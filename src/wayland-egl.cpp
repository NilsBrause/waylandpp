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
#include <utility>
#include <wayland-egl.hpp>
#include <wayland-client-protocol.hpp>

using namespace wayland;

egl_window_t::egl_window_t(surface_t &surface, int width, int height)
{
  window = wl_egl_window_create(reinterpret_cast<wl_surface*>(surface.c_ptr()), width, height);
  if(!window)
    throw std::runtime_error("Failed to create native wl_egl_window");
}

egl_window_t::egl_window_t()
  : window(nullptr)
{
}

egl_window_t::~egl_window_t()
{
  if(window)
    wl_egl_window_destroy(window);
}

egl_window_t::egl_window_t(egl_window_t &&w)
{
  operator=(std::move(w));
}

egl_window_t &egl_window_t::operator=(egl_window_t &&w)
{
  std::swap(window, w.window);
  return *this;
}

wl_egl_window *egl_window_t::c_ptr() const
{
  return window;
}

egl_window_t::operator wl_egl_window*() const
{
  return c_ptr();
}

void egl_window_t::resize(int width, int height, int dx, int dy)
{
  if(window)
    wl_egl_window_resize(window, width, height, dx, dy);
}

void egl_window_t::get_attached_size(int &width, int &height)
{
  if(window)
    wl_egl_window_get_attached_size(window, &width, &height);
  else
    {
      width = 0;
      height = 0;
    }
}

// C++ Overrides

EGLDisplay eglGetDisplay(display_t &display)
{
  return eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(display.c_ptr()));
}
