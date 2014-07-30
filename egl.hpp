#ifndef EGL_HPP
#define EGL_HPP

#ifdef WAYLAND
#include <wayland.hpp>
#include <wayland-egl.h>
#endif

#include <EGL/egl.h>

class egl
{
  static EGLDisplay display;
  static unsigned int count;
  EGLSurface surface;
  EGLContext context;

public:
  egl(NativeDisplayType native_display, NativeWindowType native_window);
#ifdef WAYLAND
  egl(display_t display, surface_t surface, int width, int height);
#endif
  ~egl();
  void begin();
  void end();
};

#endif
