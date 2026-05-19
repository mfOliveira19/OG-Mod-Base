#include "Crosshair.h"

#include <filesystem>

#include "common/log/log.h"
#include "third-party/stb_image/stb_image.h"
#include "third-party/imgui/imgui.h"

Crosshair::Crosshair() {
  pistol_1280.tex = load_texture("crosshair_1280_pistol.png", pistol_1280.width, pistol_1280.height);
  smg_1280.tex = load_texture("crosshair_1280_smg.png", smg_1280.width, smg_1280.height);
  gauss_1280.tex = load_texture("crosshair_1280_gauss.png", gauss_1280.width, gauss_1280.height);
  revolver_1280.tex = load_texture("crosshair_1280_revolver.png", revolver_1280.width, revolver_1280.height);
  shotgun_1280.tex = load_texture("crosshair_1280_shotgun.png", shotgun_1280.width, shotgun_1280.height);
  pistol_2560.tex = load_texture("crosshair_2560_pistol.png", pistol_2560.width, pistol_2560.height);
  smg_2560.tex = load_texture("crosshair_2560_smg.png", smg_2560.width, smg_2560.height);
  gauss_2560.tex = load_texture("crosshair_2560_gauss.png", gauss_2560.width, gauss_2560.height);
  revolver_2560.tex = load_texture("crosshair_2560_revolver.png", revolver_2560.width, revolver_2560.height);
  shotgun_2560.tex = load_texture("crosshair_2560_shotgun.png", shotgun_2560.width, shotgun_2560.height);
  pistol_low.tex = load_texture("crosshair_low_pistol.png", pistol_low.width, pistol_low.height);
  smg_low.tex = load_texture("crosshair_low_smg.png", smg_low.width, smg_low.height);
  gauss_low.tex = load_texture("crosshair_low_gauss.png", gauss_low.width, gauss_low.height);
  revolver_low.tex = load_texture("crosshair_low_revolver.png", revolver_low.width, revolver_low.height);
  shotgun_low.tex = load_texture("crosshair_low_shotgun.png", shotgun_low.width, shotgun_low.height);
}

Crosshair::~Crosshair() {

}

void Crosshair::render(DmaFollower& dma,
                       SharedRenderState* render_state,
                       ScopedProfilerNode& prof,
                       CrosshairDebugStats* debug_stats) {

  while (dma.current_tag_offset() != render_state->next_bucket)
    dma.read_and_advance();

  if (!crosshairShow()) {
    return;
  }

  draw_crosshair(render_state);
  debug_stats->num_draws = 1;

}

void Crosshair::draw_crosshair(SharedRenderState* render_state) {
  if (get_type() == CrosshairType::None)
    return;

  auto& shader = render_state->shaders[ShaderId::CROSSHAIR];
  GLuint prog = shader.id();
  if (!prog)
    return;

  shader.activate();

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  CrosshairSprite* sprite = pick_sprite(get_type(), render_state->draw_region_w);
  if (!sprite || sprite->tex == 0)
    return;

  int screen_w = render_state->draw_region_w;
  int screen_h = render_state->draw_region_h;

  float base_frac = 0.0f;
    switch (get_type()) {
      case CrosshairType::Pistol:
      case CrosshairType::Revolver:
        base_frac = 0.013f;
        break;
      case CrosshairType::SMG:
        base_frac = 0.022f;
        break;
      case CrosshairType::Gauss:
        base_frac = 0.015f;
        break;
      case CrosshairType::Shotgun:
        base_frac = 0.008f;
        break;
      default:
        base_frac = 0.022f;
        break;
    }

  float reference_h = 1080.0f;
  float exponent = 0.6f;
  float scale_factor = std::pow(reference_h / (float)screen_h, exponent);

  float target_h_px = screen_h * base_frac * scale_factor;
  target_h_px = std::max(target_h_px, 12.0f);

  float tex_aspect = (float)sprite->width / (float)sprite->height;
  float target_w_px = target_h_px * tex_aspect;

  float half_w_ndc = (target_w_px / screen_w) * 2.0f;
  float half_h_ndc = (target_h_px / screen_h) * 2.0f;

  struct Vertex {
    float pos[2];
    float uv[2];
  };

  Vertex verts[4] = {
      {{-half_w_ndc, -half_h_ndc}, {0.0f, 0.0f}},
      {{half_w_ndc, -half_h_ndc}, {1.0f, 0.0f}},
      {{half_w_ndc, half_h_ndc}, {1.0f, 1.0f}},
      {{-half_w_ndc, half_h_ndc}, {0.0f, 1.0f}},
  };

  GLuint indices[6] = {0, 1, 2, 2, 3, 0};

  GLuint vao, vbo, ebo;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
  glEnableVertexAttribArray(1);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, sprite->tex);
  glUniform1i(glGetUniformLocation(prog, "u_tex"), 0);

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  glBindVertexArray(0);
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ebo);
  glDeleteVertexArrays(1, &vao);
}

GLuint Crosshair::load_texture(const std::string& filename, int& out_w, int& out_h) {
  std::string path =
      (file_util::get_jak_project_dir() / "custom_assets" / "jak1" / "sprites" / filename).string();

  int channels;
  unsigned char* data = stbi_load(path.c_str(), &out_w, &out_h, &channels, 4);
  if (!data) {
    lg::error("Failed to load texture: {}", path);
    return 0;
  }

  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, out_w, out_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  stbi_image_free(data);
  return tex;
}

void Crosshair::draw_sprite(GLuint tex, float cx, float cy, float w, float h) {
  glBindTexture(GL_TEXTURE_2D, tex);

  float half_w = w * 0.5f;
  float half_h = h * 0.5f;

  float verts[] = {cx - half_w, cy - half_h, 0.0f, 0.0f, cx + half_w, cy - half_h, 1.0f, 0.0f,
                   cx + half_w, cy + half_h, 1.0f, 1.0f, cx - half_w, cy + half_h, 0.0f, 1.0f};

  GLuint indices[] = {0, 1, 2, 2, 3, 0};

  GLuint vao, vbo, ebo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

  glBindVertexArray(0);
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ebo);
  glDeleteVertexArrays(1, &vao);
}

CrosshairSprite* Crosshair::pick_sprite(CrosshairType type, int screen_width) {
  enum class ResTier { Low, Mid, High };
  ResTier tier;

  if (screen_width >= 2560) {
    tier = ResTier::High;
  } else if (screen_width >= 1280) {
    tier = ResTier::Mid;
  } else {
    tier = ResTier::Low;
  }

  switch (type) {
    case CrosshairType::Pistol:
      switch (tier) {
        case ResTier::High:
          return &pistol_2560;
        case ResTier::Mid:
          return &pistol_1280;
        case ResTier::Low:
          return &pistol_low;
      }
      break;

    case CrosshairType::SMG:
      switch (tier) {
        case ResTier::High:
          return &smg_2560;
        case ResTier::Mid:
          return &smg_1280;
        case ResTier::Low:
          return &smg_low;
      }
      break;
    case CrosshairType::Gauss:
      switch (tier) {
        case ResTier::High:
          return &gauss_2560;
        case ResTier::Mid:
          return &gauss_1280;
        case ResTier::Low:
          return &gauss_low;
      }
      break;
    case CrosshairType::Revolver:
      switch (tier) {
        case ResTier::High:
          return &revolver_2560;
        case ResTier::Mid:
          return &revolver_1280;
        case ResTier::Low:
          return &revolver_low;
      }
      break;
    case CrosshairType::Shotgun:
      switch (tier) {
        case ResTier::High:
          return &shotgun_2560;
        case ResTier::Mid:
          return &shotgun_1280;
        case ResTier::Low:
          return &shotgun_low;
      }
      break;

    default:
      break;
  }

  return nullptr;
}

void Crosshair::draw_debug_window(CrosshairDebugStats* debug_stats) {
  if (!ImGui::CollapsingHeader("Crosshair Debug"))
    return;

  ImGui::Text("Draws: %d", debug_stats->num_draws);
  ImGui::Text("Predicted Draws: %d", debug_stats->num_predicted_draws);
}
