#pragma once

#include <string>
#include <vector>

#include "common/math/Vector.h"

#include "game/graphics/opengl_renderer/BucketRenderer.h"
#include "game/graphics/opengl_renderer/Shader.h"
#include "game/graphics/opengl_renderer/opengl_utils.h"

namespace math {

inline Matrix4f translate(float x, float y, float z) {
  Matrix4f m = Matrix4f::identity();
  m(0, 3) = x;
  m(1, 3) = y;
  m(2, 3) = z;
  return m;
}

inline Matrix4f scale(float x, float y, float z) {
  Matrix4f m = Matrix4f::identity();
  m(0, 0) = x;
  m(1, 1) = y;
  m(2, 2) = z;
  return m;
}

inline Matrix4f rotateX(float rad) {
  Matrix4f m = Matrix4f::identity();
  float c = std::cos(rad), s = std::sin(rad);
  m(1, 1) = c;
  m(1, 2) = -s;
  m(2, 1) = s;
  m(2, 2) = c;
  return m;
}

inline Matrix4f rotateY(float rad) {
  Matrix4f m = Matrix4f::identity();
  float c = std::cos(rad), s = std::sin(rad);
  m(0, 0) = c;
  m(0, 2) = s;
  m(2, 0) = -s;
  m(2, 2) = c;
  return m;
}

inline Matrix4f rotateZ(float rad) {
  Matrix4f m = Matrix4f::identity();
  float c = std::cos(rad), s = std::sin(rad);
  m(0, 0) = c;
  m(0, 1) = -s;
  m(1, 0) = s;
  m(1, 1) = c;
  return m;
}

// Perspective helper
inline Matrix4f perspective(float fov_deg, float aspect, float znear, float zfar) {
  float f = 1.0f / std::tan(fov_deg * 3.14159265f / 180.0f / 2.0f);
  Matrix4f m = Matrix4f::zero();
  m(0, 0) = f / aspect;
  m(1, 1) = f;
  // reversed-Z (far at 0, near at 1)
  m(2, 2) = znear / (zfar - znear);
  m(2, 3) = (zfar * znear) / (zfar - znear);
  m(3, 2) = -1.0f;
  return m;
}

// Orthographic helper
inline Matrix4f orthographic(float left,
                             float right,
                             float bottom,
                             float top,
                             float znear,
                             float zfar) {
  Matrix4f m = Matrix4f::identity();
  m(0, 0) = 2.0f / (right - left);
  m(1, 1) = 2.0f / (top - bottom);
  m(2, 2) = -2.0f / (zfar - znear);
  m(0, 3) = -(right + left) / (right - left);
  m(1, 3) = -(top + bottom) / (top - bottom);
  m(2, 3) = -(zfar + znear) / (zfar - znear);
  return m;
}

// ------------------- Quaternion slerp -------------------
inline math::Vector4f slerp(const math::Vector4f& a, const math::Vector4f& b, float t) {
  float dot = a.x() * b.x() + a.y() * b.y() + a.z() * b.z() + a.w() * b.w();
  const float DOT_THRESHOLD = 0.9995f;

  if (dot > DOT_THRESHOLD) {
    // linear interpolate
    math::Vector4f result = a * (1 - t) + b * t;
    return result / result.length();
  }

  dot = std::clamp(dot, -1.0f, 1.0f);
  float theta_0 = std::acos(dot);
  float theta = theta_0 * t;

  math::Vector4f b_ortho = b - a * dot;
  b_ortho = b_ortho / b_ortho.length();

  return a * std::cos(theta) + b_ortho * std::sin(theta);
}

inline Matrix4f mirrorX() {
  Matrix4f m = Matrix4f::identity();
  m(0, 0) = -1.0f;
  return m;
}
}  // namespace math

// ----------------------------------
// Debug info
// ----------------------------------
struct ViewmodelDebugStats {
  int num_draws = 0;
  int num_predicted_draws = 0;
};

// ----------------------------------
// Animation data (already in your code)
// ----------------------------------
struct AnimationChannel {
  int target_node;
  std::string path;
  std::vector<float> times;
  std::vector<math::Vector4f> values;
  std::string interpolation;
};

struct Animation {
  std::string name;
  float duration = 0.0f;
  std::vector<AnimationChannel> channels;
};

// ----------------------------------
// Skeleton / skinning support
// ----------------------------------
struct Joint {
  std::string name;
  int parent = -1;
  math::Matrix4f inverse_bind_matrix;
  math::Matrix4f local_transform;
  math::Matrix4f global_transform;
};

struct Skeleton {
  std::vector<Joint> joints;
  std::vector<int> joint_nodes;
};

struct Mesh {
  GLuint vao = 0;
  GLuint vbo = 0;
  GLuint ibo = 0;
  GLuint index_count = 0;
  GLuint texture = 0;
  float normal[3];
  float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  bool useNormalUV = false;
};

struct ViewmodelResource {
  std::string name;
  std::vector<Mesh> mesh;
  Skeleton skeleton;
  std::vector<Animation> animations;
};

enum ViewmodelModels {
  crowbar = 0,
  pistol = 1,
  smg = 2,
  gauss = 3,
  revolver = 4,
  shotgun = 5,
  MAX_MODEL
};

enum ViewmodelAnimations {
  draw = 0,
  crowbar_attack1 = 1,
  crowbar_attack1_miss = 2,
  crowbar_attack2 = 3,
  crowbar_attack2_hit = 4,
  crowbar_attack3 = 5,
  crowbar_attack3_hit = 6,
  pistol_idle1 = 7,
  pistol_idle2 = 8,
  pistol_idle3 = 9,
  pistol_shoot = 10,
  pistol_shoot_empty = 11,
  pistol_reload = 12,
  pistol_reload_noshot = 13,
  smg_idle1 = 14,
  smg_longidle = 15,
  smg_shoot = 16,
  smg_shoot2 = 17,
  smg_shoot3 = 18,
  smg_reload = 19,
  smg_grenade = 20,
  gauss_fidget = 21,
  gauss_fire = 22,
  gauss_fire2 = 23,
  gauss_idle1 = 24,
  gauss_idle2 = 25,
  gauss_spin = 26,
  gauss_spinup = 27,
  holster = 28,
  revolver_fidget = 29,
  revolver_fire = 30,
  revolver_idle1 = 31,
  revolver_idle2 = 32,
  revolver_idle3 = 33,
  revolver_reload = 34,
  shotgun_start_reload = 35,
  shotgun_shoot_big = 36,
  shotgun_shoot = 37,
  shotgun_reload = 38,
  shotgun_reholster = 39,
  shotgun_pump = 40,
  shotgun_idle4 = 41,
  shotgun_deepidle = 42,
  none = 43,
  MAX_ANIMATIONS
};

// Map enum to string
static const std::array<const char*, MAX_ANIMATIONS> g_viewmodelAnimationNames = {
    "draw",
    // Crowbar
    "attack1",
    "attack1miss",
    "attack2",
    "attack2hit",
    "attack3",
    "attack3hit",
    // Pistol
    "idle1",
    "idle2",
    "idle3",
    "shoot",
    "shoot_empty",
    "reload",
    "reload_noshot",
    // SMG
    "idle1",
    "longidle",
    "shoot",
    "shoot_2",
    "shoot_3",
    "reload",
    "grenade",
    // Gauss
    "fidget",
    "fire",
    "fire2",
    "idle1",
    "idle2",
    "spin",
    "spinup",
    "holster",
    // Revolver
    "fidget1",
    "fire1",
    "idle1",
    "idle2",
    "idle3",
    "reload",
    // Shotgun
    "start_reload",
    "shoot_big",
    "shoot",
    "reload",
    "reholster",
    "pump",
    "idle4",
    "deepidle"
    };

inline const char* viewmodelAnimationName(ViewmodelAnimations anim) {
  int idx = static_cast<int>(anim);
  if (idx < 0 || idx >= MAX_ANIMATIONS)
    return "unknown";
  return g_viewmodelAnimationNames[idx];
}

struct Sprite {
  GLuint texture = 0;
  float size = 0.2f;
  bool visible = false;
  math::Vector3f position = {0.0f, 0.0f, 0.0f};
  float rotation = 0.0f;
  float alpha = 1.0f;
};

// ----------------------------------
// Viewmodel class
// ----------------------------------
class Viewmodel {
 public:
  Viewmodel();
  ~Viewmodel();

  // Render
  void render(DmaFollower& dma,
              SharedRenderState* render_state,
              ScopedProfilerNode& prof,
              ViewmodelDebugStats* debug_stats);
  void draw_debug_window(ViewmodelDebugStats* debug_stats);
  void play_animation_name(const std::string& name);
  int viewmodelActiveModel() { return Gfx::g_global_settings.viewmodel_active_model; };
  float viewmodelAnimationSpeed() { return Gfx::g_global_settings.viewmodel_animation_speed; };
  bool viewmodelShow() { return Gfx::g_global_settings.viewmodel_show; };
  bool viewmodelPause() { return Gfx::g_global_settings.viewmodel_pause; };
  float fogIntensity() { return Gfx::g_global_settings.fog_intensity; };
  ViewmodelAnimations viewmodelActiveAnimation() {
    int val = Gfx::g_global_settings.viewmodel_active_animation;
    if (val < 0 || val >= MAX_ANIMATIONS) {
      return none;
    }
    return static_cast<ViewmodelAnimations>(val);
  }
  float viewmodelOffsetZ() { return Gfx::g_global_settings.viewmodel_offset_z; };
  float viewmodelRotationX() { return Gfx::g_global_settings.viewmodel_rotation_x; };

  private:
    // Mesh storage
    std::vector<Mesh> m_mesh;
   // ----------------------------
   // Debug transforms
   // ----------------------------
   float offset_x = 0.0f;
   float offset_y = 0.0f;
   float offset_z = 0.0f;
   float scale = 1.0f;

   float cam_x = 0.0f;
   float cam_y = 1.00f;
   float cam_z = 1.685f;

   float DEG2RAD = 3.14159265f / 180.0f;

   float rot_x = -1.0f * DEG2RAD;
   float rot_y = 180.0f * DEG2RAD;
   float rot_z = 0.0f * DEG2RAD;

   // Animations
   std::vector<Animation> m_animations;
   float m_current_time = 0.0f;
   int m_active_animation = 0;
   bool m_playing_animation = false;
   ViewmodelAnimations m_current_anim = ViewmodelAnimations::draw;

   // Skeleton
   Skeleton m_skeleton;

   // Models
   std::vector<ViewmodelResource> m_models;
   int m_active_model = -1;
   void update_animation();
   void play_animation(int index);
   bool load_from_file(const std::string& filename, ViewmodelResource& out);
   void set_active_model(int index);
   GLuint load_texture(const std::string& filename);
   void draw_muzzle_flash(SharedRenderState* render_state,
                          GLuint sprite_texture_id,
                          float size,
                          const math::Vector3f& muzzle_pos_model,
                          const math::Matrix4f& model,
                          const math::Matrix4f& view,
                          const math::Matrix4f& projection);

   // Muzzle flash
   Sprite m_muzzle_flash;
   GLuint m_sprite_vao = 0;
   GLuint m_sprite_vbo = 0;
   GLuint m_muzzle_flash_texture_pistol = 0;
   GLuint m_muzzle_flash_texture_smg = 0;
   GLuint m_muzzle_flash_texture_smg_2 = 0;
   float m_muzzle_flash_timer = 0.0f;
   float m_muzzle_flash_duration = 0.040f;
   GLuint m_muzzle_flash_texture_current = 0;
   math::Vector3f m_muzzle_flash_local;
};
