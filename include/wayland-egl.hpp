/*
 * Copyright (c) 2014-2022, Nils Christopher Brause, Philipp Kerling
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

#ifndef WAYLAND_EGL_HPP
#define WAYLAND_EGL_HPP

#include <wayland-egl-core.h>
#include <wayland-util.hpp>
#include <EGL/egl.h>

namespace wayland
{
  class surface_t;
}

namespace wayland
{
  /** \brief Native EGL window
   */
  class egl_window_t : public detail::refcounted_wrapper<wl_egl_window>
  {
  public:
    egl_window_t() = default;

    /** \brief Create a native egl window for use with eglCreateWindowSurface
        \param surface The Wayland surface to use
        \param width Width of the EGL buffer
        \param height height of the EGL buffer
    */
    egl_window_t(surface_t const &surface, int width, int height);

    void resize(int width, int height, int dx = 0, int dy = 0);
    void get_attached_size(int &width, int &height);
  };
}

#endif
