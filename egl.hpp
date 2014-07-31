#ifndef EGL_HPP
#define EGL_HPP

#include <wayland.hpp>
#include <wayland-egl.h>
#include <EGL/egl.h>

class egl
{
  static EGLDisplay display;
  static unsigned int count;
  EGLSurface surface;
  EGLContext context;

public:
  egl(NativeDisplayType native_display, NativeWindowType native_window);
  egl(display_t display, surface_t surface, int width, int height);
  ~egl();
  void begin();
  void end();
};

#endif
