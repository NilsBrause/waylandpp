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

#include <wayland-cursor.hpp>

using namespace wayland;

cursor_theme_t::cursor_theme_ptr::~cursor_theme_ptr()
{
  wl_cursor_theme_destroy(cursor_theme);
}

cursor_theme_t::cursor_theme_t()
{
}

cursor_theme_t::cursor_theme_t(std::string name, int size, shm_t shm)
  : cursor_theme(new cursor_theme_ptr({wl_cursor_theme_load(name == "" ? NULL : name.c_str(),
                                                            size, reinterpret_cast<wl_shm*>(shm.c_ptr()))}))
{
  if(!cursor_theme->cursor_theme)
    throw std::runtime_error("wl_cursor_theme_load failed.");
}

cursor_t cursor_theme_t::get_cursor(std::string name)
{
  wl_cursor *cursor = wl_cursor_theme_get_cursor(cursor_theme->cursor_theme, name.c_str());
  if(!cursor)
    throw std::runtime_error("wl_cursor_theme_cursor failed.");
  return cursor_t(cursor);
}

cursor_t::cursor_t(wl_cursor *c)
  : cursor(c)
{
}

cursor_t::cursor_t()
  : cursor(NULL)
{
}

unsigned int cursor_t::image_count()
{
  return cursor->image_count;
}

std::string cursor_t::name()
{
  return cursor->name;
}

cursor_image_t cursor_t::image(unsigned int n)
{
  if(n >= image_count())
    throw std::runtime_error("n >= image count");
  return cursor_image_t(cursor->images[n]);
}

int cursor_t::frame(uint32_t time)
{
  return wl_cursor_frame(cursor, time);
}

cursor_image_t::cursor_image_t(wl_cursor_image *image)
  : cursor_image(image)
{
}

cursor_image_t::cursor_image_t()
  : cursor_image(NULL)
{
}

uint32_t cursor_image_t::width()
{
  return cursor_image->width;
}

uint32_t cursor_image_t::height()
{
  return cursor_image->height;
}

uint32_t cursor_image_t::hotspot_x()
{
  return cursor_image->hotspot_x;
}

uint32_t cursor_image_t::hotspot_y()
{
  return cursor_image->hotspot_y;
}

uint32_t cursor_image_t::delay()
{
  return cursor_image->delay;
}

buffer_t cursor_image_t::get_buffer()
{
  wl_buffer *buffer = wl_cursor_image_get_buffer(cursor_image);
  wl_proxy *proxy = reinterpret_cast<wl_proxy*>(buffer);
  wl_proxy_set_user_data(proxy, NULL);
  // buffer will be destroyed when cursor_theme is destroyed
  return buffer_t(proxy_t(proxy, false, true));
}
