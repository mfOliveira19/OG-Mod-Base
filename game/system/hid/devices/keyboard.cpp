#include "keyboard.h"
#include "game/system/hid/sdl_util.h"

KeyboardDevice::KeyboardDevice(std::shared_ptr<game_settings::InputSettings> settings) {
  m_settings = settings;
}

// I don't trust SDL's key repeat stuff, do it myself to avoid bug reports...(or cause more)
bool KeyboardDevice::is_action_already_active(const u32 sdl_keycode) {
  for (const auto& action : m_active_actions) {
    if (action.sdl_keycode == sdl_keycode) {
      return true;
    }
  }
  return false;
}

void KeyboardDevice::poll_state(std::shared_ptr<PadData> data) {
  auto& binds = m_settings->keyboard_binds;
  const auto keyboard_state = SDL_GetKeyboardState(NULL);
  auto keyboard_modifier_state = SDL_GetModState();

  // Iterate binds, see if there are any new actions we need to track
  // - Normal Buttons
  for (const auto& [sdl_keycode, bind_list] : binds.buttons) {
    // Changed for hl mod, skip these keys which are used for the controller
    if (sdl_keycode == SDLK_UP || sdl_keycode == SDLK_DOWN || sdl_keycode == SDLK_LEFT ||
        sdl_keycode == SDLK_RIGHT || sdl_keycode == SDLK_O || sdl_keycode == SDLK_P ||
        sdl_keycode == SDLK_RETURN) {
      continue;
    }
    for (const auto& bind : bind_list) {
      if (keyboard_state[SDL_GetScancodeFromKey(sdl_keycode, &keyboard_modifier_state)] &&
          bind.modifiers.has_necessary_modifiers(keyboard_modifier_state) &&
          !is_action_already_active(sdl_keycode)) {
        data->button_data.at(bind.pad_data_index) = true;  // press the button
        const auto pressure_index = data->button_index_to_pressure_index(
            static_cast<PadData::ButtonIndex>(bind.pad_data_index));
        if (pressure_index != PadData::PressureIndex::INVALID_PRESSURE) {
          data->pressure_data.at(pressure_index) = 255;
        }
        m_active_actions.push_back(
            {sdl_keycode, bind, [](std::shared_ptr<PadData> data, InputBinding bind) {
               // let go of the button
               data->button_data.at(bind.pad_data_index) = false;
               const auto pressure_index = data->button_index_to_pressure_index(
                   static_cast<PadData::ButtonIndex>(bind.pad_data_index));
               if (pressure_index != PadData::PressureIndex::INVALID_PRESSURE) {
                 data->pressure_data.at(pressure_index) = 0;
               }
             }});
      }
    }
  }
  // - Analog Buttons
  for (const auto& [sdl_keycode, bind_list] : binds.button_axii) {
    for (const auto& bind : bind_list) {
      if (keyboard_state[SDL_GetScancodeFromKey(sdl_keycode, &keyboard_modifier_state)] &&
          bind.modifiers.has_necessary_modifiers(keyboard_modifier_state) &&
          !is_action_already_active(sdl_keycode)) {
        data->button_data.at(bind.pad_data_index) = true;  // press the button
        const auto pressure_index = data->button_index_to_pressure_index(
            static_cast<PadData::ButtonIndex>(bind.pad_data_index));
        if (pressure_index != PadData::PressureIndex::INVALID_PRESSURE) {
          data->pressure_data.at(pressure_index) = 255;
        }
        m_active_actions.push_back(
            {sdl_keycode, bind, [](std::shared_ptr<PadData> data, InputBinding bind) {
               // let go of the button
               data->button_data.at(bind.pad_data_index) = false;
               const auto pressure_index = data->button_index_to_pressure_index(
                   static_cast<PadData::ButtonIndex>(bind.pad_data_index));
               if (pressure_index != PadData::PressureIndex::INVALID_PRESSURE) {
                 data->pressure_data.at(pressure_index) = 0;
               }
             }});
      }
    }
  }
  // - Analog Sticks
  for (const auto& [sdl_keycode, bind_list] : binds.analog_axii) {
    for (const auto& bind : bind_list) {
      if (keyboard_state[SDL_GetScancodeFromKey(sdl_keycode, &keyboard_modifier_state)] &&
          bind.modifiers.has_necessary_modifiers(keyboard_modifier_state) &&
          !is_action_already_active(sdl_keycode)) {
        data->analog_data.at(bind.pad_data_index) += bind.minimum_in_range ? -127 : 127;
        data->update_analog_sim_tracker(false);
        m_active_actions.push_back(
            {sdl_keycode, bind, [](std::shared_ptr<PadData> data, InputBinding bind) {
               data->analog_data.at(bind.pad_data_index) += bind.minimum_in_range ? 127 : -127;
               data->update_analog_sim_tracker(true);
             }});
      }
    }
  }

  // Check if any previously tracked actions are now invalidated by the new state of the keyboard
  // if so, we'll run their revert code and remove them
  for (auto it = m_active_actions.begin(); it != m_active_actions.end();) {
    // Modifiers are easy, if the action required one and it's not pressed anymore, evict it
    // Alternatively, was the primary key released
    if (!keyboard_state[SDL_GetScancodeFromKey(it->sdl_keycode, &keyboard_modifier_state)] ||
        !it->binding.modifiers.has_necessary_modifiers(keyboard_modifier_state)) {
      it->revert_action(data, it->binding);
      it = m_active_actions.erase(it);
    } else {
      it++;
    }
  }

  // To check the keyboard status without mapping to a controller
  m_key_status.w = keyboard_state[SDL_SCANCODE_W];
  m_key_status.a = keyboard_state[SDL_SCANCODE_A];
  m_key_status.s = keyboard_state[SDL_SCANCODE_S];
  m_key_status.d = keyboard_state[SDL_SCANCODE_D];
  m_key_status.r = keyboard_state[SDL_SCANCODE_R];
  m_key_status.q = keyboard_state[SDL_SCANCODE_Q];
  m_key_status.e = keyboard_state[SDL_SCANCODE_E];
  m_key_status.space = keyboard_state[SDL_SCANCODE_SPACE];
  m_key_status.shift = (keyboard_state[SDL_SCANCODE_LSHIFT] || keyboard_state[SDL_SCANCODE_RSHIFT]);
  m_key_status.ctrl = (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL]);
  m_key_status.alt = (keyboard_state[SDL_SCANCODE_LALT] || keyboard_state[SDL_SCANCODE_RALT]);
  m_key_status.esc = keyboard_state[SDL_SCANCODE_ESCAPE];

  m_key_status.k_1 = keyboard_state[SDL_SCANCODE_1];
  m_key_status.k_2 = keyboard_state[SDL_SCANCODE_2];
  m_key_status.k_3 = keyboard_state[SDL_SCANCODE_3];
  m_key_status.k_4 = keyboard_state[SDL_SCANCODE_4];
  m_key_status.enter = keyboard_state[SDL_SCANCODE_RETURN];
  m_key_status.backspace = keyboard_state[SDL_SCANCODE_BACKSPACE];
  m_key_status.arrow_left = keyboard_state[SDL_SCANCODE_LEFT];
  m_key_status.arrow_right = keyboard_state[SDL_SCANCODE_RIGHT];
  m_key_status.arrow_down = keyboard_state[SDL_SCANCODE_DOWN];
  m_key_status.arrow_up = keyboard_state[SDL_SCANCODE_UP];
  m_key_status.period = keyboard_state[SDL_SCANCODE_PERIOD];
  m_key_status.tab = keyboard_state[SDL_SCANCODE_TAB];
}

void KeyboardDevice::clear_actions(std::shared_ptr<PadData> data) {
  for (auto it = m_active_actions.begin(); it != m_active_actions.end();) {
    it->revert_action(data, it->binding);
    it = m_active_actions.erase(it);
  }
}

void KeyboardDevice::process_event(const SDL_Event& event,
                                   const CommandBindingGroups& commands,
                                   std::shared_ptr<PadData> /*data*/,
                                   std::optional<InputBindAssignmentMeta>& bind_assignment) {
  if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
    const auto key_event = event.key;
    if (m_ignore_key_on_keyup && m_ignore_key_on_keyup.value() == (u32)key_event.key &&
        event.type == SDL_EVENT_KEY_UP) {
      m_ignore_key_on_keyup = std::nullopt;
      return;
    }

    auto& binds = m_settings->keyboard_binds;

    // Binding re-assignment
    if (bind_assignment) {
      if (bind_assignment->device_type != InputDeviceType::KEYBOARD) {
        return;
      }
      // We do not allow rebinding these keys
      if (key_event.key == SDLK_RETURN || key_event.key == SDLK_ESCAPE || key_event.key == SDLK_1 ||
          key_event.key == SDLK_2 || key_event.key == SDLK_3 || key_event.key == SDLK_E) {
        return;
      }
      // Backspace cancels
      if (key_event.key == SDLK_BACKSPACE) {
        bind_assignment = std::nullopt;
        return;
      }
      // A normal key down event (a new key was pressed) and it's not a modifier
      if (event.type == SDL_EVENT_KEY_DOWN && !sdl_util::is_modifier_key(key_event.key)) {
        if (bind_assignment->for_analog) {
          m_ignore_key_on_keyup = key_event.key;
          binds.assign_analog_bind(key_event.key, bind_assignment.value(),
                                   InputModifiers(key_event.mod));
        } else {
          binds.assign_button_bind(key_event.key, bind_assignment.value(), false,
                                   InputModifiers(key_event.mod));
        }
      } else if (event.type == SDL_EVENT_KEY_UP) {
        // modifiers are instead inspected on a KEYUP, however if it's one of the keys
        // for triggering the binding assignment, and it's the first time we've seen it -- we ignore
        // it
        if (!bind_assignment->seen_keyboard_confirm_up) {
          for (const auto& confirm_bind : bind_assignment->keyboard_confirmation_binds) {
            if (confirm_bind.sdl_idx == key_event.key) {
              bind_assignment->seen_keyboard_confirm_up = true;
              return;
            }
          }
        }
        // otherwise, set the bind
        if (bind_assignment->for_analog) {
          binds.assign_analog_bind(key_event.key, bind_assignment.value(),
                                   InputModifiers(key_event.mod));
        } else {
          binds.assign_button_bind(key_event.key, bind_assignment.value(), false,
                                   InputModifiers(key_event.mod));
        }
      }
      return;
    }

    // Check for commands
    if (event.type == SDL_EVENT_KEY_DOWN &&
        commands.keyboard_binds.find(key_event.key) != commands.keyboard_binds.end()) {
      for (const auto& command : commands.keyboard_binds.at(key_event.key)) {
        if (command.modifiers.has_necessary_modifiers(key_event.mod)) {
          if (command.event_command) {
            command.event_command(event);
          } else if (command.command) {
            command.command();
          } else {
            lg::warn("CommandBinding has no valid callback for keyboard bind");
          }
        }
      }
    }
  }
}
