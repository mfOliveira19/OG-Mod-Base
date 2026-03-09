#include "Viewmodel.h"

#include <filesystem>

#include "common/log/log.h"

#include "third-party/stb_image/stb_image.h"
#include "third-party/imgui/imgui.h"
#include "third-party/tiny_gltf/tiny_gltf.h"

static math::Vector3f s_light_dir(-0.3f, -0.6f, -0.8f);
static math::Vector3f s_light_color(1.0f, 0.9f, 0.7f);
static math::Vector3f s_ambient(0.6f, 0.55f, 0.5f);
static float s_chrome_spec_power = 48.0f;
static float s_chrome_intensity = 0.9f;
static float s_chrome_diffuse_strength = 0.4f;
static math::Vector2f s_chrome_uv_scale(0.9f, 0.38f);
static math::Vector2f s_chrome_uv_offset(-0.72f, -0.76f);
static float muzzle_flash_pistol_offset_x = -0.405f;
static float muzzle_flash_pistol_offset_y = 0.430f;
static float muzzle_flash_smg_offset_x = -0.205f;
static float muzzle_flash_smg_offset_y = 0.55f;

static const std::array<std::string, (size_t)ViewmodelModels::MAX_MODEL> kViewmodelPaths = {
"vm_crowbar.glb", "vm_9mmhandgun.glb", "vm_9mmar.glb"};

Viewmodel::Viewmodel() {
  namespace fs = std::filesystem;
  std::string folder =
      (file_util::get_jak_project_dir() / "custom_assets" / "jak1" / "models" / "viewmodel")
          .string();

    m_models.resize((size_t)ViewmodelModels::MAX_MODEL);

    for (size_t i = 0; i < (size_t)ViewmodelModels::MAX_MODEL; ++i) {
      const auto& filename = kViewmodelPaths[i];
      ViewmodelResource res;

      if (load_from_file(filename, res)) {
        m_models[i] = std::move(res);
      } else {
        lg::error("Failed to load viewmodel: {}", filename);
      }
    }

    set_active_model(ViewmodelModels::crowbar);
    // Load sprites
    m_muzzle_flash_texture_pistol = load_texture("muzzle_flash_pistol.png");
    m_muzzle_flash_texture_smg = load_texture("muzzle_flash_smg.png");
    m_muzzle_flash_texture_smg_2 = load_texture("muzzle_flash_smg2.png");
}

Viewmodel::~Viewmodel() {
  for (auto& mesh : m_mesh) {
    if (mesh.vao)
      glDeleteVertexArrays(1, &mesh.vao);
    if (mesh.vbo)
      glDeleteBuffers(1, &mesh.vbo);
    if (mesh.ibo)
      glDeleteBuffers(1, &mesh.ibo);
    if (mesh.texture)
      glDeleteTextures(1, &mesh.texture);
  }
}

void Viewmodel::render(DmaFollower& dma,
                       SharedRenderState* render_state,
                       ScopedProfilerNode& prof,
                       ViewmodelDebugStats* debug_stats) {

  while (dma.current_tag_offset() != render_state->next_bucket)
    dma.read_and_advance();

  if (m_models.empty())
    return;

  if (!viewmodelShow()) {
    if (m_playing_animation) {
      auto& model = m_models[m_active_model];
      auto& anim = model.animations[m_active_animation];
      m_current_time = anim.duration;  // jump to end
      update_animation();
    }
    return;
  }

  static int once = 0;

  if (once == 0) {
    ViewmodelResource res;
    load_from_file("vm_crowbar.glb", res);
    once++;
  }

  // Handle Fog
  render_state->fog_intensity = fogIntensity();

  // Handle model switch
  if (viewmodelActiveModel() != m_active_model) {
    m_active_model = viewmodelActiveModel();
    play_animation_name("draw");
  }

    // --- MODEL / VIEW / PROJECTION ---
  math::Matrix4f model = math::rotateZ(rot_z) * math::rotateY(rot_y) *
                         math::rotateX(rot_x + viewmodelRotationX()) *
                         math::translate(offset_x, offset_y, offset_z + viewmodelOffsetZ()) *
                         math::scale(scale, scale, scale);

  math::Matrix4f view = math::translate(-cam_x, -cam_y, -cam_z);

  float aspect = static_cast<float>(render_state->render_fb_w) / render_state->render_fb_h;
  math::Matrix4f projection = math::perspective(72.0f, aspect, 0.1f, 50.0f);
  math::Vector3f muzzle_local;
  math::Matrix4f muzzle_model = math::rotateZ(rot_z) * math::rotateY(rot_y) *
                         math::rotateX(rot_x + viewmodelRotationX()) *
                         math::translate(offset_x, offset_y, offset_z) *
                         math::scale(scale, scale, scale);
  static float muzzle_flash_size = 0.25f;
  // Handle animation play
  ViewmodelAnimations anim = viewmodelActiveAnimation();
  if (anim != ViewmodelAnimations::draw) {
    play_animation_name(viewmodelAnimationName(anim));

    switch (anim) {
      case ViewmodelAnimations::pistol_shoot:
        muzzle_flash_size = 0.25f;
        m_muzzle_flash_local = {muzzle_flash_pistol_offset_x, muzzle_flash_pistol_offset_y, 0.8f};
        m_muzzle_flash_texture_current = m_muzzle_flash_texture_pistol;
        m_muzzle_flash_timer = m_muzzle_flash_duration;
        break;

      case ViewmodelAnimations::smg_shoot2:
        muzzle_flash_size = 0.40f;
        m_muzzle_flash_local = {muzzle_flash_smg_offset_x, muzzle_flash_smg_offset_y, 0.8f};
        m_muzzle_flash_texture_current = m_muzzle_flash_texture_smg_2;
        m_muzzle_flash_timer = m_muzzle_flash_duration;
        break;
      case ViewmodelAnimations::smg_shoot:
      case ViewmodelAnimations::smg_shoot3:
        muzzle_flash_size = 0.40f;
        m_muzzle_flash_local = {muzzle_flash_smg_offset_x, muzzle_flash_smg_offset_y, 0.8f};
        m_muzzle_flash_texture_current = m_muzzle_flash_texture_smg;
        m_muzzle_flash_timer = m_muzzle_flash_duration;
        break;
    }

    Gfx::g_global_settings.viewmodel_active_animation = 0;
  }

  // Each frame:
  if (m_muzzle_flash_timer > 0.0f) {
    m_muzzle_flash_timer -= ImGui::GetIO().DeltaTime;
    if (m_muzzle_flash_timer > 0.0f) {
      draw_muzzle_flash(render_state, m_muzzle_flash_texture_current, muzzle_flash_size,
                        m_muzzle_flash_local, muzzle_model, view, projection);
    }
  }

  // select active model
  auto& modelRes = m_models[m_active_model];

  auto& shader = render_state->shaders[ShaderId::VIEWMODEL];
  GLuint prog = shader.id();
  if (prog == 0) {
    lg::error("Viewmodel::render - VIEWMODEL shader not compiled/linked!");
    return;
  }
  shader.activate();

  // --- ANIMATION UPDATE ---
  if (!viewmodelPause()) {
    update_animation();
  }

  auto set_matrix = [&](const char* name, const math::Matrix4f& mat) {
    GLint loc = glGetUniformLocation(prog, name);
    if (loc >= 0)
      glUniformMatrix4fv(loc, 1, GL_FALSE, mat.data());
  };

  set_matrix("uModel", model);
  set_matrix("uView", view);
  set_matrix("uProjection", projection);

  // --- Upload joint matrices ---
  if (!modelRes.skeleton.joints.empty()) {
    std::vector<float> joint_mats;
    joint_mats.reserve(modelRes.skeleton.joints.size() * 16);
    for (const auto& j : modelRes.skeleton.joints) {
      auto skin_mat = j.global_transform * j.inverse_bind_matrix;
      joint_mats.insert(joint_mats.end(), skin_mat.data(), skin_mat.data() + 16);
    }
    GLint loc = glGetUniformLocation(prog, "uBones");
    if (loc >= 0)
      glUniformMatrix4fv(loc, (GLsizei)modelRes.skeleton.joints.size(), GL_FALSE,
                         joint_mats.data());
  }

  // --- Save relevant GL state (culling, blend, depth) ---
  GLboolean prevCull = glIsEnabled(GL_CULL_FACE);
  GLint prevCullFaceMode = GL_BACK;
  glGetIntegerv(GL_CULL_FACE_MODE, &prevCullFaceMode);
  GLint prevFrontFace = GL_CCW;
  glGetIntegerv(GL_FRONT_FACE, &prevFrontFace);

  GLboolean prevBlend = glIsEnabled(GL_BLEND);
  GLint prevBlendSrcRGB = GL_ONE, prevBlendDstRGB = GL_ZERO;
  GLint prevBlendSrcAlpha = GL_ONE, prevBlendDstAlpha = GL_ZERO;
  glGetIntegerv(GL_BLEND_SRC_RGB, &prevBlendSrcRGB);
  glGetIntegerv(GL_BLEND_DST_RGB, &prevBlendDstRGB);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &prevBlendSrcAlpha);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &prevBlendDstAlpha);

  GLboolean prevDepthTest = glIsEnabled(GL_DEPTH_TEST);
  GLint prevDepthFunc = GL_LESS;
  glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
  GLboolean prevDepthMask = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &prevDepthMask);
  GLdouble prevDepthRange[2] = {0.0, 1.0};
  glGetDoublev(GL_DEPTH_RANGE, prevDepthRange);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glDepthMask(GL_TRUE);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glDisable(GL_BLEND);

  // --- DRAW EACH MESH ---
  for (auto& prim : modelRes.mesh) {
    glBindVertexArray(prim.vao);

    GLint locHasTex = glGetUniformLocation(prog, "uHasTexture");
    if (locHasTex >= 0)
      glUniform1i(locHasTex, prim.texture != 0);

    if (prim.texture != 0) {
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, prim.texture);
      GLint locTex = glGetUniformLocation(prog, "uTexture");
      if (locTex >= 0)
        glUniform1i(locTex, 0);
    }

    GLint locColor = glGetUniformLocation(prog, "uColor");
    if (locColor >= 0)
      glUniform4fv(locColor, 1, prim.color);

    GLint locUseNormalUV = glGetUniformLocation(prog, "uUseNormalUV");
    if (locUseNormalUV >= 0) {
      glUniform1i(locUseNormalUV, prim.useNormalUV ? 1 : 0);
    }

    GLint locLightDir = glGetUniformLocation(prog, "uLightDir");
    if (locLightDir >= 0) {
      math::Vector3f dir = s_light_dir.normalized();
      glUniform3fv(locLightDir, 1, dir.data());
    }

    GLint locLightColor = glGetUniformLocation(prog, "uLightColor");
    if (locLightColor >= 0)
      glUniform3fv(locLightColor, 1, s_light_color.data());

    GLint locChromeUVScale = glGetUniformLocation(prog, "uChromeUVScale");
    if (locChromeUVScale >= 0)
      glUniform2fv(locChromeUVScale, 1, s_chrome_uv_scale.data());

    GLint locChromeUVOffset = glGetUniformLocation(prog, "uChromeUVOffset");
    if (locChromeUVOffset >= 0)
      glUniform2fv(locChromeUVOffset, 1, s_chrome_uv_offset.data());

    GLint locAmbient = glGetUniformLocation(prog, "uAmbient");
    if (locAmbient >= 0)
      glUniform3fv(locAmbient, 1, s_ambient.data());

    GLint locSpecPower = glGetUniformLocation(prog, "uChromeSpecPower");
    if (locSpecPower >= 0)
      glUniform1f(locSpecPower, s_chrome_spec_power);

    GLint locIntensity = glGetUniformLocation(prog, "uChromeIntensity");
    if (locIntensity >= 0)
      glUniform1f(locIntensity, s_chrome_intensity);

    GLint locDiffuse = glGetUniformLocation(prog, "uChromeDiffuseStrength");
    if (locDiffuse >= 0)
      glUniform1f(locDiffuse, s_chrome_diffuse_strength);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prim.ibo);
    glDrawElements(GL_TRIANGLES, prim.index_count, GL_UNSIGNED_INT, 0);

    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
      lg::error("OpenGL error: {}", err);
    }
  }
  glBindVertexArray(0);

  // --- Restore previous GL state ---
  // Depth range / mask / func / test
  glDepthRange(prevDepthRange[0], prevDepthRange[1]);
  if (!prevDepthMask)
    glDepthMask(GL_FALSE);
  else
    glDepthMask(GL_TRUE);

  if (!prevDepthTest)
    glDisable(GL_DEPTH_TEST);
  else
    glEnable(GL_DEPTH_TEST);
  glDepthFunc((GLenum)prevDepthFunc);

  // Cull
  if (!prevCull)
    glDisable(GL_CULL_FACE);
  else {
    glEnable(GL_CULL_FACE);
    glCullFace((GLenum)prevCullFaceMode);
  }
  glFrontFace((GLenum)prevFrontFace);

  // Blend
  if (!prevBlend)
    glDisable(GL_BLEND);
  else {
    glEnable(GL_BLEND);
    glBlendFuncSeparate((GLenum)prevBlendSrcRGB, (GLenum)prevBlendDstRGB, (GLenum)prevBlendSrcAlpha,
                        (GLenum)prevBlendDstAlpha);
  }

  debug_stats->num_draws = (int)modelRes.mesh.size();
  debug_stats->num_predicted_draws = debug_stats->num_draws;
}

void Viewmodel::draw_muzzle_flash(SharedRenderState* render_state,
                                  GLuint sprite_texture_id,
                                  float size,
                                  const math::Vector3f& muzzle_pos_model,
                                  const math::Matrix4f& model,
                                  const math::Matrix4f& view,
                                  const math::Matrix4f& projection) {
  auto& shader = render_state->shaders[ShaderId::MUZZLE_FLASH];
  GLuint muzzle_flash_shader_id = shader.id();
  if (muzzle_flash_shader_id == 0) {
    lg::error("Viewmodel::draw_muzzle_flash - MUZZLE_FLASH shader not compiled/linked!");
    return;
  }
  shader.activate();

  // --- Project muzzle position into clip space ---
  math::Vector4f clip =
      projection * view * model *
      math::Vector4f(muzzle_pos_model.x(), muzzle_pos_model.y(), muzzle_pos_model.z(), 1.0f);

  if (clip.w() <= 0.0f)
    return;  // behind camera

  // Perspective divide → Normalized Device Coordinates [-1, 1]
  float ndc_x = clip.x() / clip.w();
  float ndc_y = clip.y() / clip.w();

  // --- Get texture size ---
  int tex_w = 1, tex_h = 1;
  glBindTexture(GL_TEXTURE_2D, sprite_texture_id);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tex_w);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &tex_h);
  glBindTexture(GL_TEXTURE_2D, 0);

  // --- Screen dimensions ---
  int screen_w = render_state->render_fb_w;
  int screen_h = render_state->render_fb_h;

  // Sprite scale relative to screen height
  float target_h = screen_h * size;
  float scale = target_h / (float)tex_h;
  float target_w = tex_w * scale;

  // Convert sprite size from pixels → NDC
  float half_w_ndc = (target_w / (float)screen_w);
  float half_h_ndc = (target_h / (float)screen_h);

  // --- Vertex data (in NDC coordinates) ---
  struct Vertex {
    float pos[2];
    float uv[2];
  };

  Vertex vertices[4] = {
      {{ndc_x - half_w_ndc, ndc_y - half_h_ndc}, {0.0f, 0.0f}},  // bottom-left
      {{ndc_x + half_w_ndc, ndc_y - half_h_ndc}, {1.0f, 0.0f}},  // bottom-right
      {{ndc_x + half_w_ndc, ndc_y + half_h_ndc}, {1.0f, 1.0f}},  // top-right
      {{ndc_x - half_w_ndc, ndc_y + half_h_ndc}, {0.0f, 1.0f}},  // top-left
  };

  GLuint indices[6] = {0, 1, 2, 2, 3, 0};

  // --- VAO/VBO setup ---
  GLuint vao, vbo, ebo;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);  // position
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void*)(2 * sizeof(float)));  // UV
  glEnableVertexAttribArray(1);

  // --- Bind texture + uniforms ---
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sprite_texture_id);

  GLint loc_tex = glGetUniformLocation(muzzle_flash_shader_id, "tex_T0");
  if (loc_tex >= 0)
    glUniform1i(loc_tex, 0);

  // --- Enable transparency blending ---
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  // --- Draw ---
  glBindVertexArray(vao);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  // --- Cleanup ---
  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glBindVertexArray(0);
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ebo);
  glDeleteVertexArrays(1, &vao);
}

bool Viewmodel::load_from_file(const std::string& filename, ViewmodelResource& out) {
  std::string model_path = fs::path(file_util::get_jak_project_dir() / "custom_assets" / "jak1" /
                                    "models" / "viewmodel" / filename)
                               .string();
  lg::info("Loading viewmodel from file: {}", model_path);

  tinygltf::TinyGLTF loader;
  tinygltf::Model model;
  std::string err, warn;

  bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, model_path);
  if (!warn.empty())
    lg::warn("tinygltf warning: {}", warn);
  if (!err.empty())
    lg::error("tinygltf error: {}", err);
  if (!ret) {
    lg::error("Failed to load GLTF: {}", model_path);
    return false;
  }

  if (model.meshes.empty()) {
    lg::error("No meshes in glTF file!");
    return false;
  }

  // ------------------------------
  // Skeleton (skins)
  // ------------------------------
  out.skeleton.joints.clear();
  out.skeleton.joint_nodes.clear();

  if (!model.skins.empty()) {
    const auto& skin = model.skins[0];
    lg::info("Skin found with {} joints", skin.joints.size());

    // Load inverse bind matrices
    std::vector<math::Matrix4f> inv_bind_mats;
    if (skin.inverseBindMatrices >= 0) {
      const auto& accessor = model.accessors[skin.inverseBindMatrices];
      const auto& view = model.bufferViews[accessor.bufferView];
      const auto& buf = model.buffers[view.buffer];
      const float* data =
          reinterpret_cast<const float*>(buf.data.data() + view.byteOffset + accessor.byteOffset);
      for (size_t i = 0; i < accessor.count; i++) {
        math::Matrix4f m;
        memcpy(m.data(), data + i * 16, sizeof(float) * 16);
        inv_bind_mats.push_back(m);
      }
    }

    // Build joint list
    for (size_t i = 0; i < skin.joints.size(); i++) {
      int node_idx = skin.joints[i];
      const auto& node = model.nodes[node_idx];

      Joint j;
      j.name = node.name;
      j.parent = -1;
      j.inverse_bind_matrix =
          (i < inv_bind_mats.size()) ? inv_bind_mats[i] : math::Matrix4f::identity();
      j.local_transform = math::Matrix4f::identity();
      j.global_transform = math::Matrix4f::identity();

      out.skeleton.joints.push_back(j);
      out.skeleton.joint_nodes.push_back(node_idx);
    }

    // Resolve parents (GLTF node hierarchy)
    for (size_t i = 0; i < out.skeleton.joints.size(); i++) {
      int node_idx = out.skeleton.joint_nodes[i];
      for (size_t p = 0; p < model.nodes.size(); p++) {
        const auto& node = model.nodes[p];
        for (int child : node.children) {
          if (child == node_idx) {
            auto it =
                std::find(out.skeleton.joint_nodes.begin(), out.skeleton.joint_nodes.end(), (int)p);
            if (it != out.skeleton.joint_nodes.end()) {
              out.skeleton.joints[i].parent =
                  (int)std::distance(out.skeleton.joint_nodes.begin(), it);
            }
          }
        }
      }
    }
  } else {
    lg::warn("No skin in GLTF, skeleton will be empty");
  }

  // ------------------------------
  // Mesh loading
  // ------------------------------
  struct Vertex {
    float pos[3];
    float uv[2];
    unsigned short joints[4] = {0, 0, 0, 0};
    float weights[4] = {0, 0, 0, 0};
    float normal[3] = {0, 0, 0};
  };

  out.mesh.clear();
  for (const auto& mesh : model.meshes) {
    for (const auto& prim : mesh.primitives) {
      std::vector<Vertex> vertices;
      std::vector<uint32_t> indices;
      GLuint index_offset = 0;

      // Positions
      const auto& posAccessor = model.accessors[prim.attributes.at("POSITION")];
      const auto& posView = model.bufferViews[posAccessor.bufferView];
      const auto& posBuffer = model.buffers[posView.buffer];
      const float* posData = reinterpret_cast<const float*>(
          posBuffer.data.data() + posView.byteOffset + posAccessor.byteOffset);
      size_t vertexCount = posAccessor.count;

      // UVs
      std::vector<float> uvs;
      if (prim.attributes.find("TEXCOORD_0") != prim.attributes.end()) {
        const auto& uvAccessor = model.accessors[prim.attributes.at("TEXCOORD_0")];
        const auto& uvView = model.bufferViews[uvAccessor.bufferView];
        const auto& uvBuffer = model.buffers[uvView.buffer];
        const float* uvData = reinterpret_cast<const float*>(
            uvBuffer.data.data() + uvView.byteOffset + uvAccessor.byteOffset);
        uvs.assign(uvData, uvData + uvAccessor.count * 2);
      }

      // Normals
      std::vector<float> normals;
      if (prim.attributes.find("NORMAL") != prim.attributes.end()) {
        const auto& normalAccessor = model.accessors[prim.attributes.at("NORMAL")];
        const auto& normalView = model.bufferViews[normalAccessor.bufferView];
        const auto& normalBuffer = model.buffers[normalView.buffer];
        const float* normalData = reinterpret_cast<const float*>(
            normalBuffer.data.data() + normalView.byteOffset + normalAccessor.byteOffset);
        normals.assign(normalData, normalData + normalAccessor.count * 3);
      }

      for (size_t i = 0; i < vertexCount; i++) {
        Vertex v;
        v.pos[0] = posData[i * 3 + 0];
        v.pos[1] = posData[i * 3 + 1];
        v.pos[2] = posData[i * 3 + 2];
        if (!uvs.empty()) {
          v.uv[0] = uvs[i * 2 + 0];
          v.uv[1] = uvs[i * 2 + 1];
        } else {
          v.uv[0] = v.uv[1] = 0.0f;
        }

        if (!normals.empty()) {
          v.normal[0] = normals[i * 3 + 0];
          v.normal[1] = normals[i * 3 + 1];
          v.normal[2] = normals[i * 3 + 2];
        } else {
          v.normal[0] = v.normal[1] = v.normal[2] = 0.0f;
        }
        vertices.push_back(v);
      }

      // Indices
      const auto& idxAccessor = model.accessors[prim.indices];
      const auto& idxView = model.bufferViews[idxAccessor.bufferView];
      const auto& idxBuffer = model.buffers[idxView.buffer];
      const void* idxData = idxBuffer.data.data() + idxView.byteOffset + idxAccessor.byteOffset;
      size_t indexCount = idxAccessor.count;

      if (idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT) {
        const uint16_t* buf = reinterpret_cast<const uint16_t*>(idxData);
        for (size_t i = 0; i < indexCount; i++)
          indices.push_back(static_cast<uint32_t>(buf[i] + index_offset));
      } else if (idxAccessor.componentType == TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT) {
        const uint32_t* buf = reinterpret_cast<const uint32_t*>(idxData);
        for (size_t i = 0; i < indexCount; i++)
          indices.push_back(buf[i] + index_offset);
      } else {
        lg::error("Unsupported index type: {}", idxAccessor.componentType);
        return false;
      }

      index_offset += static_cast<GLuint>(vertexCount);

      // Joints
      if (prim.attributes.find("JOINTS_0") != prim.attributes.end()) {
        const auto& jAccessor = model.accessors[prim.attributes.at("JOINTS_0")];
        const auto& jView = model.bufferViews[jAccessor.bufferView];
        const auto& jBuffer = model.buffers[jView.buffer];
        const unsigned char* jData = jBuffer.data.data() + jView.byteOffset + jAccessor.byteOffset;

        if (jAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
          const uint8_t* data = reinterpret_cast<const uint8_t*>(jData);
          for (size_t i = 0; i < vertexCount; i++)
            for (int k = 0; k < 4; k++)
              vertices[i].joints[k] = data[i * 4 + k];
        } else if (jAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
          const uint16_t* data = reinterpret_cast<const uint16_t*>(jData);
          for (size_t i = 0; i < vertexCount; i++)
            for (int k = 0; k < 4; k++)
              vertices[i].joints[k] = data[i * 4 + k];
        } else {
          lg::warn("Unsupported JOINTS_0 component type: {}", jAccessor.componentType);
        }
      }

      // Weights
      if (prim.attributes.find("WEIGHTS_0") != prim.attributes.end()) {
        const auto& wAccessor = model.accessors[prim.attributes.at("WEIGHTS_0")];
        const auto& wView = model.bufferViews[wAccessor.bufferView];
        const auto& wBuffer = model.buffers[wView.buffer];
        const float* wData = reinterpret_cast<const float*>(wBuffer.data.data() + wView.byteOffset +
                                                            wAccessor.byteOffset);
        for (size_t i = 0; i < vertexCount; i++)
          for (int k = 0; k < 4; k++)
            vertices[i].weights[k] = wData[i * 4 + k];
      }

      // Upload to OpenGL
      Mesh mp;
      glGenVertexArrays(1, &mp.vao);
      glBindVertexArray(mp.vao);

      glGenBuffers(1, &mp.vbo);
      glBindBuffer(GL_ARRAY_BUFFER, mp.vbo);
      glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(),
                   GL_STATIC_DRAW);

      glGenBuffers(1, &mp.ibo);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mp.ibo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(),
                   GL_STATIC_DRAW);

      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
      glEnableVertexAttribArray(1);
      glVertexAttribIPointer(2, 4, GL_UNSIGNED_SHORT, sizeof(Vertex),
                             (void*)offsetof(Vertex, joints));
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            (void*)offsetof(Vertex, weights));
      glEnableVertexAttribArray(3);


      // Normals
      glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                            (void*)offsetof(Vertex, normal));
      glEnableVertexAttribArray(4);

      glBindVertexArray(0);
      mp.index_count = static_cast<GLuint>(indices.size());

      // Texture / color
      mp.texture = 0;
      mp.useNormalUV = false;
      if (prim.material >= 0 && prim.material < model.materials.size()) {
        const auto& mat = model.materials[prim.material];

        auto nameLower = mat.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (nameLower.find("chrome") != std::string::npos) {
          mp.useNormalUV = true;
        }
        if (mat.pbrMetallicRoughness.baseColorTexture.index >= 0) {
          int texIndex = mat.pbrMetallicRoughness.baseColorTexture.index;
          const auto& tex = model.textures[texIndex];
          if (tex.source >= 0) {
            const auto& img = model.images[tex.source];
            glGenTextures(1, &mp.texture);
            glBindTexture(GL_TEXTURE_2D, mp.texture);

            GLenum format = GL_RGBA;
            if (img.component == 1)
              format = GL_RED;
            else if (img.component == 2)
              format = GL_RG;
            else if (img.component == 3)
              format = GL_RGB;
            else if (img.component == 4)
              format = GL_RGBA;

            GLenum internalFormat = (format == GL_RGB) ? GL_SRGB8 : GL_SRGB8_ALPHA8;

            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, img.width, img.height, 0, format,
                         GL_UNSIGNED_BYTE, img.image.data());

            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
          }
        } else {
          const auto& factor = mat.pbrMetallicRoughness.baseColorFactor;
          if (factor.size() == 4) {
            mp.color[0] = factor[0];
            mp.color[1] = factor[1];
            mp.color[2] = factor[2];
            mp.color[3] = factor[3];
          }
        }
      }

      out.mesh.push_back(mp);
    }
  }

  // ------------------------------
  // Animations
  // ------------------------------
  out.animations.clear();
  for (const auto& anim : model.animations) {
    Animation a;
    a.name = anim.name;

    for (size_t i = 0; i < anim.channels.size(); i++) {
      const auto& channel = anim.channels[i];
      const auto& sampler = anim.samplers[channel.sampler];

      AnimationChannel ch;
      ch.target_node = channel.target_node;
      ch.path = channel.target_path;
      ch.interpolation = sampler.interpolation;

      // Times
      const auto& timeAccessor = model.accessors[sampler.input];
      const auto& timeView = model.bufferViews[timeAccessor.bufferView];
      const auto& timeBuffer = model.buffers[timeView.buffer];
      const float* timeData = reinterpret_cast<const float*>(
          timeBuffer.data.data() + timeView.byteOffset + timeAccessor.byteOffset);
      ch.times.assign(timeData, timeData + timeAccessor.count);

      // Values
      const auto& valAccessor = model.accessors[sampler.output];
      const auto& valView = model.bufferViews[valAccessor.bufferView];
      const auto& valBuffer = model.buffers[valView.buffer];
      const float* valData = reinterpret_cast<const float*>(
          valBuffer.data.data() + valView.byteOffset + valAccessor.byteOffset);

      int components = (ch.path == "rotation") ? 4 : 3;
      for (size_t j = 0; j < valAccessor.count; j++) {
        math::Vector4f v(0, 0, 0, 0);
        for (int c = 0; c < components; c++)
          v[c] = valData[j * components + c];
        ch.values.push_back(v);
      }

      if (!ch.times.empty())
        a.duration = std::max(a.duration, ch.times.back());

      a.channels.push_back(ch);
    }

    out.animations.push_back(a);
  }
  out.name = fs::path(filename).stem().string();
  lg::info("Loaded GLTF: {} ({} primitives, {} joints, {} animations)", filename, out.mesh.size(),
           out.skeleton.joints.size(), out.animations.size());

  if (m_models.empty()) {
    m_animations = out.animations;
    if (!m_animations.empty()) {
      play_animation(0);
    }
  }
  return true;
}

GLuint Viewmodel::load_texture(const std::string& filename) {
  std::string path =
      (file_util::get_jak_project_dir() / "custom_assets" / "jak1" / "sprites" / filename)
          .string();

  int width, height, channels;
  unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
  if (!data) {
    lg::error("Failed to load texture: {}", path);
    return 0;
  }

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  stbi_image_free(data);
  return tex;
}

void Viewmodel::draw_debug_window(ViewmodelDebugStats* debug_stats) {
  if (!ImGui::CollapsingHeader("Viewmodel Debug"))
    return;

  ImGui::Text("Draws: %d", debug_stats->num_draws);
  ImGui::Text("Predicted Draws: %d", debug_stats->num_predicted_draws);
  ImGui::Separator();

  // --- Model Selection ---
  if (!m_models.empty()) {
    ImGui::Text("Active model Index: %d", m_active_model);
    static int selected_model = m_active_model;
    if (ImGui::BeginCombo("Model", m_models[selected_model].name.c_str())) {
      for (int i = 0; i < (int)m_models.size(); i++) {
        bool is_selected = (i == selected_model);
        if (ImGui::Selectable(m_models[i].name.c_str(), is_selected)) {
          selected_model = i;
          set_active_model(i);  // switch meshes, skeleton, animations
        }
        if (is_selected)
          ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
  }

  // --- Model Transform ---
  ImGui::Text("Model Transform:");
  ImGui::SliderFloat("Offset X", &offset_x, -100.0f, 100.0f);
  ImGui::SliderFloat("Offset Y", &offset_y, -100.0f, 100.0f);
  ImGui::SliderFloat("Offset Z", &offset_z, -100.0f, 100.0f);
  ImGui::SliderFloat("Scale", &scale, 0.1f, 10.0f);

  // --- Camera Controls ---
  ImGui::Separator();
  ImGui::Text("Camera:");
  ImGui::SliderFloat("Cam X", &cam_x, -100.0f, 100.0f);
  ImGui::SliderFloat("Cam Y", &cam_y, -100.0f, 100.0f);
  ImGui::SliderFloat("Cam Z", &cam_z, 0.0f, 100.0f);

  ImGui::Separator();
  ImGui::SliderAngle("Rotate X", &rot_x, -180.0f, 180.0f);
  ImGui::SliderAngle("Rotate Y", &rot_y, -180.0f, 180.0f);
  ImGui::SliderAngle("Rotate Z", &rot_z, -180.0f, 180.0f);

  ImGui::Separator();
  ImGui::SliderFloat2("UV Scale", s_chrome_uv_scale.data(), 0.1f, 3.0f);
  ImGui::SliderFloat2("UV Offset", s_chrome_uv_offset.data(), -1.0f, 1.0f);
  ImGui::SliderFloat("Chrome diffuse", &s_chrome_diffuse_strength, 0.0f, 2.0f);

  ImGui::Separator();
  ImGui::Text("Lighting Controls:");

  ImGui::SliderFloat3("Light Dir", s_light_dir.data(), -1.0f, 1.0f);
  ImGui::SliderFloat3("Light Color", s_light_color.data(), 0.0f, 3.0f);
  ImGui::SliderFloat3("Ambient", s_ambient.data(), 0.0f, 1.0f);

    // --- Camera Controls ---
  ImGui::Separator();
  ImGui::Text("Muzzle Flash:");
  ImGui::SliderFloat("Pistol Offset X", &muzzle_flash_pistol_offset_x, -1.0f, 1.0f);
  ImGui::SliderFloat("Pistol Offset Y", &muzzle_flash_pistol_offset_y, -1.0f, 1.0f);
  ImGui::SliderFloat("SMG Offset X", &muzzle_flash_smg_offset_x, -1.0f, 1.0f);
  ImGui::SliderFloat("SMG Offset Y", &muzzle_flash_smg_offset_y, -1.0f, 1.0f);

  // --- Animations for current model ---
  ImGui::Separator();
  ImGui::Text("Animations:");

  if (!m_animations.empty()) {
    for (int i = 0; i < (int)m_animations.size(); i++) {
      const char* name = m_animations[i].name.empty() ? ("Anim " + std::to_string(i)).c_str()
                                                      : m_animations[i].name.c_str();
      if (ImGui::Button(name)) {
        play_animation(i);
      }
      ImGui::SameLine();
      ImGui::Text("Duration: %.2f", m_animations[i].duration);
    }

    ImGui::NewLine();
    ImGui::Text("Current Time: %.2f / %.2f", m_current_time,
                m_animations[m_active_animation].duration);
  }
}

void Viewmodel::play_animation(int index) {
  if (m_models.empty())
    return;

  auto& model = m_models[m_active_model];

  if (index < 0 || index >= (int)model.animations.size())
    return;

  m_active_animation = index;
  m_current_time = 0.0f;
  m_playing_animation = true;
}

void Viewmodel::play_animation_name(const std::string& name) {
  if (m_models.empty())
    return;

  auto& model = m_models[m_active_model];

  for (int i = 0; i < (int)model.animations.size(); i++) {
    if (model.animations[i].name == name) {
      m_active_animation = i;
      m_current_time = 0.0f;
      m_playing_animation = true;
      return;
    }
  }

  // optional: log if not found
  fmt::print("Animation '{}' not found in model '{}'\n", name, model.name);
}

void Viewmodel::update_animation() {
  if (m_models.empty())
    return;

  auto& model = m_models[m_active_model];

  if (model.animations.empty() || model.skeleton.joints.empty())
    return;

  if (!m_playing_animation)
    return;

  const auto& anim = model.animations[m_active_animation];
  float delta_time = ImGui::GetIO().DeltaTime * viewmodelAnimationSpeed();
  m_current_time += delta_time;

  if (m_current_time >= anim.duration) {
    m_current_time = anim.duration;
    m_playing_animation = false;
  }

  // Reset local transforms
  for (auto& j : model.skeleton.joints) {
    j.local_transform = math::Matrix4f::identity();
  }

  // Per-joint temp storage for TRS
  std::vector<math::Matrix4f> T(model.skeleton.joints.size(), math::Matrix4f::identity());
  std::vector<math::Matrix4f> R(model.skeleton.joints.size(), math::Matrix4f::identity());
  std::vector<math::Matrix4f> S(model.skeleton.joints.size(), math::Matrix4f::identity());

  for (const auto& ch : anim.channels) {
    // Map from glTF node index → joint index
    auto it = std::find(model.skeleton.joint_nodes.begin(), model.skeleton.joint_nodes.end(),
                        ch.target_node);
    if (it == model.skeleton.joint_nodes.end())
      continue;  // channel animates something else

    int joint_index = (int)std::distance(model.skeleton.joint_nodes.begin(), it);

    // --- Find keyframes ---
    int idx = 0;
    while (idx < (int)ch.times.size() - 1 && m_current_time > ch.times[idx + 1])
      idx++;
    int idx1 = std::min(idx + 1, (int)ch.times.size() - 1);
    float t0 = ch.times[idx];
    float t1 = ch.times[idx1];
    float factor = (t1 > t0) ? (m_current_time - t0) / (t1 - t0) : 0.0f;

    const auto& v0 = ch.values[idx];
    const auto& v1 = ch.values[idx1];

    // --- Interpolate ---
    if (ch.path == "translation") {
      math::Vector3f interp = v0.xyz() * (1 - factor) + v1.xyz() * factor;
      T[joint_index] = math::translate(interp.x(), interp.y(), interp.z());
    } else if (ch.path == "rotation") {
      math::Vector4f q = math::slerp(v0, v1, factor);
      float x = q.x(), y = q.y(), z = q.z(), w = q.w();
      math::Matrix4f rot = math::Matrix4f::identity();
      rot(0, 0) = 1 - 2 * y * y - 2 * z * z;
      rot(0, 1) = 2 * x * y - 2 * z * w;
      rot(0, 2) = 2 * x * z + 2 * y * w;
      rot(1, 0) = 2 * x * y + 2 * z * w;
      rot(1, 1) = 1 - 2 * x * x - 2 * z * z;
      rot(1, 2) = 2 * y * z - 2 * x * w;
      rot(2, 0) = 2 * x * z - 2 * y * w;
      rot(2, 1) = 2 * y * z + 2 * x * w;
      rot(2, 2) = 1 - 2 * x * x - 2 * y * y;
      R[joint_index] = rot;
    } else if (ch.path == "scale") {
      math::Vector3f interp = v0.xyz() * (1 - factor) + v1.xyz() * factor;
      S[joint_index] = math::scale(interp.x(), interp.y(), interp.z());
    }
  }

  // Build final local transforms = T * R * S
  for (int i = 0; i < (int)model.skeleton.joints.size(); i++) {
    model.skeleton.joints[i].local_transform = T[i] * R[i] * S[i];
  }

  // Compute global transforms (parent → child)
  for (int i = 0; i < (int)model.skeleton.joints.size(); i++) {
    auto& j = model.skeleton.joints[i];
    if (j.parent >= 0) {
      j.global_transform = model.skeleton.joints[j.parent].global_transform * j.local_transform;
    } else {
      j.global_transform = j.local_transform;
    }
  }
}

void Viewmodel::set_active_model(int index) {
  if (index < 0 || index >= (int)m_models.size())
    return;
  m_active_model = index;
  m_current_time = 0.0f;
  m_playing_animation = false;
  play_animation_name("draw");
}
