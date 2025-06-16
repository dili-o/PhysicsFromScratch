#pragma once

#include "Defines.hpp"

struct SDL_Window;

namespace hlx {
class HLX_API Platform {
public:
  Platform();

  Platform(cstring title, i32 width = 1280, i32 height = 720);

  ~Platform();

  SDL_Window *GetWindowHandle() { return m_WindowHandle; }

  void GetWindowSize(i32 *width, i32 *height);

  f64 GetAbsoluteTimeS();

  f64 GetAbsoluteTimeMS();

private:
  SDL_Window *m_WindowHandle = nullptr;
};

} // namespace hlx
