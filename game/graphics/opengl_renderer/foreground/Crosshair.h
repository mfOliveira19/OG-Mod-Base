#pragma once

#include <string>
#include <vector>

#include "common/math/Vector.h"

#include "game/graphics/opengl_renderer/BucketRenderer.h"
#include "game/graphics/opengl_renderer/Shader.h"
#include "game/graphics/opengl_renderer/opengl_utils.h"


struct CrosshairSprite {
  GLuint tex = 0;
  int width = 0;
  int height = 0;
};

struct CrosshairDebugStats {
  int num_draws = 0;
  int num_predicted_draws = 0;
};

enum class CrosshairType {
  None,
  Pistol,
  SMG,
  Gauss,
  Revolver,
  Shotgun
};

inline CrosshairType int_to_crosshair(int value) {
  switch (value) {
    case 1:
      return CrosshairType::Pistol;
    case 2:
      return CrosshairType::SMG;
    case 3:
      return CrosshairType::Gauss;
    case 4:
      return CrosshairType::Revolver;
    case 5:
      return CrosshairType::Shotgun;
    case 0:
    default:
      return CrosshairType::None;
  }
}

class Crosshair {
 public:
  Crosshair();
  ~Crosshair();

  void render(DmaFollower& dma,
              SharedRenderState* render_state,
              ScopedProfilerNode& prof,
              CrosshairDebugStats* debug_stats);

  void draw_debug_window(CrosshairDebugStats* debug_stats);
  CrosshairType get_type() const {
    int raw_value = Gfx::g_global_settings.current_crosshair;
    return int_to_crosshair(raw_value);
  }
  bool crosshairShow() { return Gfx::g_global_settings.crosshair_show; };

 private:
  void draw_crosshair(SharedRenderState* render_state);
  CrosshairType m_type = CrosshairType::Pistol;
  CrosshairSprite pistol_1280;
  CrosshairSprite smg_1280;
  CrosshairSprite gauss_1280;
  CrosshairSprite revolver_1280;
  CrosshairSprite shotgun_1280;
  CrosshairSprite pistol_2560;
  CrosshairSprite smg_2560;
  CrosshairSprite gauss_2560;
  CrosshairSprite revolver_2560;
  CrosshairSprite shotgun_2560;
  CrosshairSprite pistol_low;
  CrosshairSprite smg_low;
  CrosshairSprite gauss_low;
  CrosshairSprite revolver_low;
  CrosshairSprite shotgun_low;

  void draw_sprite(GLuint tex, float cx, float cy, float w, float h);
  GLuint load_texture(const std::string& filename, int& out_w, int& out_h);
  CrosshairSprite* pick_sprite(CrosshairType type, int screen_width);
};
