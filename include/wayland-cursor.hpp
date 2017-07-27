/*
 * Copyright (c) 2014-2017, Nils Christopher Brause
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

#ifndef CURSOR_HPP
#define CURSOR_HPP

#include <memory>
#include <string>
#include <wayland-cursor.h>
#include <wayland-client-protocol.hpp>
#include <wayland-util.hpp>

namespace wayland
{
  class cursor_image_t : public detail::basic_wrapper<wl_cursor_image>
  {
  private:
    cursor_image_t(wl_cursor_image *image, std::shared_ptr<wl_cursor_theme> const& t);
    friend class cursor_t;
    // Extend lifetime of wl_cursor_theme
    std::shared_ptr<wl_cursor_theme> cursor_theme;

  public:
    cursor_image_t();
    uint32_t width();
    uint32_t height();
    uint32_t hotspot_x();
    uint32_t hotspot_y();
    uint32_t delay();
    // buffer will be destroyed when cursor_theme is destroyed
    buffer_t get_buffer();
  };

  class cursor_t : public detail::basic_wrapper<wl_cursor>
  {
  private:
    cursor_t(wl_cursor *c, std::shared_ptr<wl_cursor_theme> const& t);
    friend class cursor_theme_t;
    // Extend lifetime of wl_cursor_theme
    std::shared_ptr<wl_cursor_theme> cursor_theme;

  public:
    cursor_t();
    unsigned int image_count();
    std::string name();
    cursor_image_t image(unsigned int n);
    int frame(uint32_t time);
  };

  class cursor_theme_t : public detail::refcounted_wrapper<wl_cursor_theme>
  {
  public:
    cursor_theme_t();
    cursor_theme_t(std::string name, int size, shm_t shm);
    cursor_t get_cursor(std::string name);
  };
}

#endif
