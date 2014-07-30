#include <stdexcept>
#include <vector>
#include <egl.hpp>

#ifdef WAYLAND
egl::egl(display_t display, surface_t surface, int width, int height)
  : egl(reinterpret_cast<wl_display*>(display.proxy->proxy),
        wl_egl_window_create(reinterpret_cast<wl_surface*>(surface.proxy->proxy), width, height))
{
}
#endif

egl::egl(NativeDisplayType native_display, NativeWindowType native_window)
{
  if(!count)
    {
      display = eglGetDisplay(native_display);
      if(display == EGL_NO_DISPLAY)
        throw std::runtime_error("eglGetDisplay");
      
      EGLint major, minor;
      if(eglInitialize(display, &major, &minor) == EGL_FALSE)
        throw std::runtime_error("eglInitialize");
      if(!((major == 1 && minor >= 4) || major >= 2))
        throw std::runtime_error("EGL version too old");
    }

  count++;

  if(eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
    throw std::runtime_error("eglBindAPI");

  std::vector<EGLint> config_attibs = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
    EGL_NONE };
  EGLConfig config;
  EGLint num;
  if(eglChooseConfig(display, config_attibs.data(), &config, 1, &num) == EGL_FALSE || num == 0)
    throw std::runtime_error("eglChooseConfig");

  std::vector<EGLint> context_attribs = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE };
  context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs.data());
  if(context == EGL_NO_CONTEXT)
    throw std::runtime_error("eglCreateContext");

  std::vector<EGLint> window_attribs = {
    EGL_NONE };
  surface = eglCreateWindowSurface(display, config, native_window, window_attribs.data());
  if(surface == EGL_NO_SURFACE)
    throw std::runtime_error("eglCreateWindowSurface");  
}

egl::~egl()
{
  if(eglDestroyContext(display, context) == EGL_FALSE)
    throw std::runtime_error("eglDestroyContext");
  count--;
  if(!count)
    if(eglTerminate(display) == EGL_FALSE)
      throw std::runtime_error("eglTerminate");
}

void egl::begin()
{
  if(eglMakeCurrent(display, surface, surface, context) == EGL_FALSE)
    throw std::runtime_error("eglMakeCurrent");
}

void egl::end()
{
  if(eglSwapBuffers(display, surface) == EGL_FALSE)
    throw std::runtime_error("eglSwapBuffers");
}

EGLDisplay egl::display;
unsigned int egl::count = 0;
