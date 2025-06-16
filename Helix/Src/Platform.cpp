#include "Platform.hpp"
#include "Log.hpp"
// Vendor
#include <SDL3/SDL.h>

namespace hlx {
Platform::Platform() : Platform("Helix", 1280, 720) {}

Platform::Platform(cstring title, i32 width, i32 height) {
  if (m_WindowHandle) {
    HERROR("Failed to create window, m_WindowHandle is not null");
    return;
  }
  // Init SDL
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    HCRITICAL("SDL could not initialize! SDL error: {}", SDL_GetError());
  }

  // Create window
  SDL_WindowFlags windowFlags =
      (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE |
                        SDL_WINDOW_HIGH_PIXEL_DENSITY);

  m_WindowHandle = SDL_CreateWindow(title, width, height, windowFlags);
  HINFO("Window initialised");
}

Platform::~Platform() {
  SDL_DestroyWindow(m_WindowHandle);
  m_WindowHandle = nullptr;
  SDL_Quit();

  HINFO("Window destroyed");
}

void Platform::GetWindowSize(i32 *width, i32 *height) {
  SDL_GetWindowSize(m_WindowHandle, width, height);
}

f64 Platform::GetAbsoluteTimeS() {
  return (f64)SDL_GetPerformanceCounter() / (f64)SDL_GetPerformanceFrequency();
}

f64 Platform::GetAbsoluteTimeMS() {
  return ((f64)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency()) *
         1000.0;
}

} // namespace hlx
