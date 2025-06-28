#pragma once

#include "Defines.hpp"
#include "Math/Matrix.hpp"
// Vendor
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>

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

namespace hlx {

class Camera {
public:
  Camera(Vec3 position, f32 nearPlane, f32 farPlane, f32 fov = 45.f,
         f32 aspectRatio = 1280.f / 720.f);

  Mat4 GetRotation();
  Mat4 GetView();
  Mat4 GetProjection();
  const Vec3 &GetPosition() const;

  void HandleEvents(const SDL_Event *pEvent, SDL_Window *pWindow);
  void Update(f32 deltaTime);

private:
  void OnKeyUpEvent(SDL_Keycode keyCode);
  void OnKeyDownEvent(SDL_Keycode keyCode);
  void OnMouseMoveEvent(i16 x, i16 y);
  void OnMouseButtonEvent(bool keyDown, u16 keyCode, SDL_Window *pWindow);
  void OnMouseScrollEvent(i8 direction);
  void OnWindowResize(i32 width, i32 height);

private:
  bool m_IsActive{false};
  bool m_IsShiftDown{false};
  bool m_IsOrbiting{true};

  Vec3 m_Velocity{0.f};
  Vec3 m_Position{0.f};

  f32 m_MoveSpeed{5.f};
  f32 m_Pitch{0.f};
  f32 m_Yaw{0.f};
  f32 m_NearPlane{0.1f};
  f32 m_FarPlane{100.f};
  f32 m_Fov{45.f};
  f32 m_OrbitingDistance{15.f};
  f32 m_AspectRatio{1280.f / 720.f};
};

} // namespace hlx
