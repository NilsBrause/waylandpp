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

#include <wayland-cursor.hpp>

using namespace wayland;

cursor_theme_t::cursor_theme_t()
{
}

cursor_theme_t::cursor_theme_t(std::string name, int size, shm_t shm)
  : detail::refcounted_wrapper<wl_cursor_theme>({wl_cursor_theme_load(name == "" ? NULL : name.c_str(),
                                                                     size, reinterpret_cast<wl_shm*>(shm.c_ptr())),
                                                 wl_cursor_theme_destroy})
{
  if(!c_ptr())
    throw std::runtime_error("wl_cursor_theme_load failed.");
}

cursor_t cursor_theme_t::get_cursor(std::string name)
{
  wl_cursor *cursor = wl_cursor_theme_get_cursor(c_ptr(), name.c_str());
  if(!cursor)
    throw std::runtime_error("wl_cursor_theme_cursor failed.");
  return cursor_t(cursor, ref_ptr());
}


cursor_t::cursor_t(wl_cursor *c, std::shared_ptr<wl_cursor_theme> const& t)
  : detail::basic_wrapper<wl_cursor>(c), cursor_theme(t)
{
}

cursor_t::cursor_t()
{
}

unsigned int cursor_t::image_count()
{
  return c_ptr()->image_count;
}

std::string cursor_t::name()
{
  return c_ptr()->name;
}

cursor_image_t cursor_t::image(unsigned int n)
{
  if(n >= image_count())
    throw std::runtime_error("n >= image count");
  return cursor_image_t(c_ptr()->images[n], cursor_theme);
}

int cursor_t::frame(uint32_t time)
{
  return wl_cursor_frame(c_ptr(), time);
}


cursor_image_t::cursor_image_t(wl_cursor_image *image, std::shared_ptr<wl_cursor_theme> const& t)
  : detail::basic_wrapper<wl_cursor_image>(image), cursor_theme(t)
{
}

cursor_image_t::cursor_image_t()
{
}

uint32_t cursor_image_t::width()
{
  return c_ptr()->width;
}

uint32_t cursor_image_t::height()
{
  return c_ptr()->height;
}

uint32_t cursor_image_t::hotspot_x()
{
  return c_ptr()->hotspot_x;
}

uint32_t cursor_image_t::hotspot_y()
{
  return c_ptr()->hotspot_y;
}

uint32_t cursor_image_t::delay()
{
  return c_ptr()->delay;
}

buffer_t cursor_image_t::get_buffer()
{
  wl_buffer *buffer = wl_cursor_image_get_buffer(c_ptr());
  // buffer will be destroyed when cursor_theme is destroyed
  return buffer_t(buffer, proxy_t::wrapper_type::foreign);
}
