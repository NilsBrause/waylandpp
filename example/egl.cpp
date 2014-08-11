/*
 * Copyright (c) 2014, Nils Christopher Brause
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
