#include <stdexcept>
#include <vector>
#include <egl.hpp>

egl::egl(display_t &disp, egl_window_t &win)
{
  display = eglGetDisplay(disp);
  if(display == EGL_NO_DISPLAY)
    throw std::runtime_error("eglGetDisplay");
  
  EGLint major, minor;
  if(eglInitialize(display, &major, &minor) == EGL_FALSE)
    throw std::runtime_error("eglInitialize");
  if(!((major == 1 && minor >= 4) || major >= 2))
    throw std::runtime_error("EGL version too old");

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
  surface = eglCreateWindowSurface(display, config, win, window_attribs.data());
  if(surface == EGL_NO_SURFACE)
    throw std::runtime_error("eglCreateWindowSurface");  
}

egl::~egl()
{
  if(eglDestroyContext(display, context) == EGL_FALSE)
    throw std::runtime_error("eglDestroyContext");
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
