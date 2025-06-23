#include "Camera.hpp"
#include "Math/Quat.hpp"

namespace hlx {
static SDL_Keycode sLeftButton = SDLK_A;
static SDL_Keycode sRightButton = SDLK_D;
static SDL_Keycode sForwardButton = SDLK_W;
static SDL_Keycode sBackwardButton = SDLK_S;
static SDL_Keycode sUpButton = SDLK_SPACE;
static SDL_Keycode sDownButton = SDLK_LCTRL;
static SDL_Keycode sToggleOrbit = SDLK_O;

enum Buttons {
  BUTTON_PADDING,
  BUTTON_LEFT,
  BUTTON_MIDDLE,
  BUTTON_RIGHT,
  BUTTON_SIDE_1,
  BUTTON_SIDE_2,
  BUTTON_PADDING2,
  BUTTON_MAX_BUTTONS
};

Camera::Camera(Vec3 position, f32 nearPlane, f32 farPlane, f32 fov,
               f32 aspectRatio)
    : m_IsActive(false), m_IsShiftDown(false), m_Velocity(Vec3(0.f)),
      m_Position(position), m_MoveSpeed(1.f), m_Pitch(0.f), m_Yaw(0.f),
      m_NearPlane(nearPlane), m_FarPlane(farPlane), m_Fov(fov),
      m_AspectRatio(aspectRatio) {}

Mat4 Camera::GetRotation() {
  Quat pitchRotation =
      glm::angleAxis(glm::radians(m_Pitch), Vec3{1.f, 0.f, 0.f});
  Quat yawRotation = glm::angleAxis(glm::radians(m_Yaw), Vec3{0.f, -1.f, 0.f});
  return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

Mat4 Camera::GetView() {
  if (m_IsOrbiting) {
    Vec3 direction;
    direction.x = cos(glm::radians(m_Pitch)) * sin(glm::radians(m_Yaw));
    direction.y = sin(glm::radians(m_Pitch));
    direction.z = cos(glm::radians(m_Pitch)) * cos(glm::radians(m_Yaw));

    m_Position = direction * m_OrbitingDistance; // Orbit around origin
    Vec3 up = Vec3(0, 1, 0);

    return glm::lookAt(m_Position, Vec3(0), up);
  }
  Mat4 cameraTranslation = glm::translate(Mat4(1.f), m_Position);
  Mat4 cameraRotation = GetRotation();
  return glm::inverse(cameraTranslation * cameraRotation);
}

Mat4 Camera::GetProjection() {
  Mat4 projection = glm::perspective(glm::radians(m_Fov), m_AspectRatio,
                                     m_NearPlane, m_FarPlane);
  projection[1][1] *= -1.f;
  return projection;
}

void Camera::HandleEvents(const SDL_Event *pEvent, SDL_Window *pWindow) {
  switch (pEvent->type) {
  case SDL_EVENT_WINDOW_RESIZED: {
    i32 width = 0, height = 0;
    SDL_GetWindowSize(pWindow, &width, &height);
    OnWindowResize(width, height);
  } break;
  case SDL_EVENT_KEY_UP: {
    SDL_Keycode key = pEvent->key.key;
    OnKeyUpEvent(key);
  } break;
  case SDL_EVENT_KEY_DOWN: {
    SDL_Keycode key = pEvent->key.key;
    OnKeyDownEvent(key);
  } break;
  case SDL_EVENT_MOUSE_MOTION: {
    i32 x_pos = pEvent->motion.xrel;
    i32 y_pos = pEvent->motion.yrel;
    OnMouseMoveEvent(x_pos, y_pos);
  } break;
  case SDL_EVENT_MOUSE_WHEEL: {
    i32 z_delta = pEvent->wheel.y;
    if (z_delta != 0) {
      z_delta = z_delta < 0 ? -1 : 1;
    }
    OnMouseScrollEvent(z_delta);
  } break;
  case SDL_EVENT_MOUSE_BUTTON_UP:
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    u8 button = pEvent->button.button;
    bool pressed = pEvent->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    OnMouseButtonEvent(pressed, button, pWindow);
  } break;
  default:
    break;
  };
}

void Camera::Update(f32 deltaTime) {
  if (m_IsOrbiting)
    return;
  Mat4 cameraRotation = GetRotation();
  m_Position +=
      Vec3(cameraRotation * Vec4(m_Velocity * m_MoveSpeed, 1.f)) * deltaTime;
}

void Camera::OnKeyUpEvent(SDL_Keycode keyCode) {
  if (!m_IsActive || m_IsOrbiting)
    return;

  if (keyCode == sLeftButton) {
    m_Velocity.x = 0;
  } else if (keyCode == sRightButton) {
    m_Velocity.x = 0;
  } else if (keyCode == sForwardButton) {
    m_Velocity.z = 0;
  } else if (keyCode == sBackwardButton) {
    m_Velocity.z = 0;
  } else if (keyCode == sUpButton) {
    m_Velocity.y = 0;
  } else if (keyCode == sDownButton) {
    m_Velocity.y = 0;
  } else if (keyCode == SDLK_LSHIFT) {
    m_IsShiftDown = false;
  }
}

void Camera::OnKeyDownEvent(SDL_Keycode keyCode) {
  if (!m_IsActive || m_IsOrbiting) {
    if (keyCode == sToggleOrbit) {
      m_IsOrbiting = !m_IsOrbiting;
    }
    return;
  }

  if (keyCode == sLeftButton) {
    m_Velocity.x = -1;
  } else if (keyCode == sRightButton) {
    m_Velocity.x = 1;
  } else if (keyCode == sForwardButton) {
    m_Velocity.z = -1;
  } else if (keyCode == sBackwardButton) {
    m_Velocity.z = 1;
  } else if (keyCode == sUpButton) {
    m_Velocity.y = 1;
  } else if (keyCode == sDownButton) {
    m_Velocity.y = -1;
  } else if (keyCode == SDLK_LSHIFT) {
    m_IsShiftDown = true;
  }
}

void Camera::OnMouseMoveEvent(i16 x, i16 y) {
  if (!m_IsActive)
    return;

  if (m_IsOrbiting) {
    m_Yaw += (f32)x / 20.f;
    m_Pitch -= (f32)y / 20.f;
  } else {
    m_Yaw += (f32)x / 5.f;
    m_Pitch -= (f32)y / 5.f;
  }
  m_Pitch = glm::clamp(m_Pitch, -89.0f, 89.0f);
}

void Camera::OnMouseButtonEvent(bool keyDown, u16 keyCode,
                                SDL_Window *pWindow) {
  if (keyDown) {
    if (keyCode == BUTTON_RIGHT) {
      SDL_SetWindowRelativeMouseMode(pWindow, true);
      m_IsActive = true;
    }
  } else {
    if (keyCode == BUTTON_RIGHT) {
      SDL_SetWindowRelativeMouseMode(pWindow, false);
      m_IsActive = false;
      m_Velocity = Vec3(0.f);
    }
  }
}

void Camera::OnMouseScrollEvent(i8 direction) {
  if (m_IsShiftDown) {
    m_MoveSpeed = direction > 0 ? (m_MoveSpeed + 0.5f) : (m_MoveSpeed - 0.5f);
    m_MoveSpeed = glm::clamp(m_MoveSpeed, 0.5f, 100.f);
  } else {
    constexpr f32 scrollFactor = 2.f;
    if (m_IsOrbiting) {
      m_OrbitingDistance += (direction * -scrollFactor);
      m_OrbitingDistance = glm::clamp(m_OrbitingDistance, 2.f, 180.f);
    } else {
      m_Fov += (direction * -scrollFactor);
      m_Fov = glm::clamp(m_Fov, 2.f, 90.f);
    }
  }
}

void Camera::OnWindowResize(i32 width, i32 height) {
  m_AspectRatio = (f32)width / height;
}
} // namespace hlx
