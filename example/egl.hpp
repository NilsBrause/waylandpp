#ifndef EGL_HPP
#define EGL_HPP

#include <wayland-egl.hpp>
#include <EGL/egl.h>

class egl
{
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;

  egl(const egl &) { }

public:
  egl(display_t &disp, egl_window_t &win);
  ~egl();
  void begin();
  void end();
};

#endif
