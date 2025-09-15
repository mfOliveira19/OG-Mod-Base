#include "mouse.h"

MouseDevice::MouseDevice(SDL_Window* window, std::shared_ptr<game_settings::InputSettings> settings)
    : m_window(window) {
  m_settings = settings;
  // By default mouse is enabled
  enable_relative_mode(true);
}

// I don't trust SDL's key repeat stuff, do it myself to avoid bug reports...(or cause more)
bool MouseDevice::is_action_already_active(const u32 sdl_code, const bool player_movement) {
  for (const auto& action : m_active_actions) {
    if (!player_movement && action.sdl_mouse_button == sdl_code) {
      return true;
    } else if (player_movement && action.player_movement) {
      return true;
    }
  }
  return false;
}

void MouseDevice::poll_state() {
  float curr_mouse_x;
  float curr_mouse_y;
  const auto mouse_state = SDL_GetMouseState(&curr_mouse_x, &curr_mouse_y);

  const auto mouse_state_rel = SDL_GetRelativeMouseState(&m_xrel_pos, &m_yrel_pos);
  m_button_status.left = mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT);
  m_button_status.right  = mouse_state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT);
  SDL_Event e;
  scroll_y = 0.0;
  while (SDL_PeepEvents(&e, 1, SDL_GETEVENT, SDL_EVENT_MOUSE_WHEEL, SDL_EVENT_MOUSE_WHEEL) > 0) {
    scroll_y += e.wheel.y;
  }
}

void MouseDevice::clear_actions(std::shared_ptr<PadData> data) {
  for (auto it = m_active_actions.begin(); it != m_active_actions.end();) {
    it->revert_action(data, it->binding);
    it = m_active_actions.erase(it);
  }
}

void MouseDevice::process_event(const SDL_Event& event,
                                const CommandBindingGroups& commands,
                                std::shared_ptr<PadData> data,
                                std::optional<InputBindAssignmentMeta>& bind_assignment) {
  // We still want to keep track of the cursor location even if we aren't using it for inputs
  // return early
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    // https://wiki.libsdl.org/SDL3/SDL_MouseMotionEvent
    m_xcoord = event.motion.x;
    m_ycoord = event.motion.y;
    if (m_control_camera) {
      const auto xrel_amount = float(event.motion.xrel);
      const auto xadjust = std::clamp(127 + int(xrel_amount * m_xsens), 0, 255);
      if (xadjust > 0) {
        m_mouse_moved_x = true;
      }
      data->analog_data.at(2) = xadjust;
      const auto yrel_amount = float(event.motion.yrel);
      const auto yadjust = std::clamp(127 + int(yrel_amount * m_ysens), 0, 255);
      if (yadjust > 0) {
        m_mouse_moved_y = true;
      }
      data->analog_data.at(3) = yadjust;
    }
  } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
    // Mouse Button Events
    // https://wiki.libsdl.org/SDL3/SDL_MouseButtonEvent
    const auto button_event = event.button;
    // Update the internal mouse tracking, this is for GOAL reasons.
    switch (button_event.button) {
      case SDL_BUTTON_LEFT:
        m_button_status.left = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        break;
      case SDL_BUTTON_RIGHT:
        m_button_status.right = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        break;
      case SDL_BUTTON_MIDDLE:
        m_button_status.middle = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        break;
      case SDL_BUTTON_X1:
        m_button_status.mouse4 = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        break;
      case SDL_BUTTON_X2:
        m_button_status.mouse5 = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        break;
    }

    auto& binds = m_settings->mouse_binds;

    // Binding re-assignment
    if (bind_assignment && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      if (bind_assignment->device_type == InputDeviceType::MOUSE && !bind_assignment->for_analog) {
        binds.assign_button_bind(button_event.button, bind_assignment.value(), false,
                                 InputModifiers(SDL_GetModState()));
      }
      return;
    }

    // Check for commands
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
        commands.mouse_binds.find(button_event.button) != commands.mouse_binds.end()) {
      for (const auto& command : commands.mouse_binds.at(button_event.button)) {
        if (command.modifiers.has_necessary_modifiers(SDL_GetModState())) {
          if (command.event_command) {
            command.event_command(event);
          } else if (command.command) {
            command.command();
          } else {
            lg::warn("CommandBinding has no valid callback for mouse bind");
          }
        }
      }
    }
  }
}

void MouseDevice::enable_relative_mode(const bool enable) {
  // https://wiki.libsdl.org/SDL3/SDL_SetWindowRelativeMouseMode
  SDL_SetWindowRelativeMouseMode(m_window, enable);
}

void MouseDevice::enable_camera_control(const bool enable) {
  m_control_camera = enable;
  enable_relative_mode(m_control_camera);
}

void MouseDevice::set_camera_sens(const float xsens, const float ysens) {
  m_xsens = xsens;
  m_ysens = ysens;
}
