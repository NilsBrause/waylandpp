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

/** \example shm.cpp
 * This is an example of how to use the Wayland C++ bindings with SHM.
 */

#include <stdexcept>
#include <iostream>
#include <array>
#include <memory>
#include <sstream>
#include <ctime>
#include <algorithm>

#include <wayland-client.hpp>
#include <linux/input.h>
#include <wayland-cursor.hpp>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace wayland;

// helper to create a std::function out of a member function and an object
template <typename R, typename T, typename... Args>
std::function<R(Args...)> bind_mem_fn(R(T::* func)(Args...), T *t)
{
  return [func, t] (Args... args)
    {
      return (t->*func)(args...);
    };
}

// shared memory helper class
class shared_mem_t
{
private:
  std::string name;
  int fd = 0;
  size_t len = 0;
  void *mem = nullptr;

public:
  shared_mem_t()
  {
  }

  shared_mem_t(size_t size)
  : len(size)
  {
    // Very simple shared memory wrapper - do not use this in production code!
    // The generated memory regions are visible in the file system and can be
    // stolen by other running processes.
    // Linux code should use memfd_create when possible (ommited here for
    // simplicity).
    
    // create random filename
    std::stringstream ss;
    std::srand(std::time(0));
    ss << '/' << std::rand();
    name = ss.str();

    // open shared memory file
    fd = shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
    if(fd < 0)
      throw std::runtime_error("shm_open failed.");

    // set size
    if(ftruncate(fd, size) < 0)
      throw std::runtime_error("ftruncate failed.");

    // map memory
    mem = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(mem == MAP_FAILED)
      throw std::runtime_error("mmap failed.");
  }

  ~shared_mem_t() noexcept(false)
  {
    if(fd)
      {
        if(munmap(mem, len) < 0)
          throw std::runtime_error("munmap failed.");
        if(close(fd) < 0)
          throw std::runtime_error("close failed.");
        if(shm_unlink(name.c_str()) < 0)
          throw std::runtime_error("shm_unlink failed");
      }
  }

  int get_fd()
  {
    return fd;
  }

  void *get_mem()
  {
    return mem;
  }
};

// example Wayland client
class example
{
private:
  // global objects
  display_t display;
  registry_t registry;
  compositor_t compositor;
  shell_t shell;
  seat_t seat;
  shm_t shm;
  
  // local objects
  surface_t surface;
  shell_surface_t shell_surface;
  pointer_t pointer;
  keyboard_t keyboard;
  callback_t frame_cb;
  cursor_theme_t cursor_theme;
  cursor_image_t cursor_image;
  buffer_t cursor_buffer;
  surface_t cursor_surface;

  std::shared_ptr<shared_mem_t> shared_mem;
  buffer_t buffer[2];
  int cur_buf;

  bool running;
  bool has_pointer;
  bool has_keyboard;

  void draw(uint32_t serial = 0)
  {
    float h = ((serial >> 4) & 0xFF)/255.0;
    float s = 1, v = 1;

    int hi = h*6;
    float f = h*6 - hi;
    float p = v*(1-s);
    float q = v*(1-s*f);
    float t = v*(1-s*(1-f));
    float r, g, b;

    switch(hi)
      {
      case 1:
        r = q; g = v; b = p;
        break;
      case 2:
        r = p; g = v; b = t;
        break;
      case 3:
        r = p; g = q; b = v;
        break;
      case 4:
        r = t; g = p; b = v;
        break;
      case 5:
        r = v; g = p; b = q;
        break;
      default: // 0,6
        r = v; g = t; b = p;
        break;
      }

    // draw stuff
    uint32_t pixel = (0x80 << 24)
      | (static_cast<uint32_t>(r * 255.0) << 16)
      | (static_cast<uint32_t>(g * 255.0) << 8)
      | static_cast<uint32_t>(b * 255.0);

    std::fill_n(static_cast<uint32_t*>(shared_mem->get_mem())+cur_buf*320*240, 320*240, pixel);
    surface.attach(buffer[cur_buf], 0, 0);
    surface.damage(0, 0, 320, 240);
    if(!cur_buf)
      cur_buf = 1;
    else
      cur_buf = 0;

    // schedule next draw
    frame_cb = surface.frame();
    frame_cb.on_done() = bind_mem_fn(&example::draw, this);
    surface.commit();
  }

public:
  example()
  {
    // retrieve global objects
    registry = display.get_registry();
    registry.on_global() = [&] (uint32_t name, std::string interface, uint32_t version)
      {
        if(interface == compositor_t::interface_name)
          registry.bind(name, compositor, version);
        else if(interface == shell_t::interface_name)
          registry.bind(name, shell, version);
        else if(interface == seat_t::interface_name)
          registry.bind(name, seat, version);
        else if(interface == shm_t::interface_name)
          registry.bind(name, shm, version);
      };
    display.roundtrip();

    seat.on_capabilities() = [&] (seat_capability capability)
      {
        has_keyboard = capability & seat_capability::keyboard;
        has_pointer = capability & seat_capability::pointer;
      };
    display.roundtrip();

    if(!has_keyboard)
      throw std::runtime_error("No keyboard found.");
    if(!has_pointer)
      throw std::runtime_error("No pointer found.");

    // create shared memory
    shared_mem = std::shared_ptr<shared_mem_t>(new shared_mem_t(2*320*240*4));
    auto pool = shm.create_pool(shared_mem->get_fd(), 2*320*240*4);
    for(unsigned int c = 0; c < 2; c++)
      buffer[c] = pool.create_buffer(c*320*240*4, 320, 240, 320*4, shm_format::argb8888);
    cur_buf = 0;

    // create a surface
    surface = compositor.create_surface();
    shell_surface = shell.get_shell_surface(surface);
    
    shell_surface.on_ping() = [&] (uint32_t serial) { shell_surface.pong(serial); };
    shell_surface.set_title("Window");
    shell_surface.set_toplevel();

    // Get input devices
    pointer = seat.get_pointer();
    keyboard = seat.get_keyboard();

    // load cursor theme
    cursor_theme = cursor_theme_t("default", 16, shm);
    cursor_t cursor = cursor_theme.get_cursor("cross");
    cursor_image = cursor.image(0);
    cursor_buffer = cursor_image.get_buffer();

    // create cursor surface
    cursor_surface = compositor.create_surface();

    // draw cursor
    pointer.on_enter() = [&] (uint32_t serial, surface_t, int32_t, int32_t)
      {
        cursor_surface.attach(cursor_buffer, 0, 0);
        cursor_surface.damage(0, 0, cursor_image.width(), cursor_image.height());
        cursor_surface.commit();
        pointer.set_cursor(serial, cursor_surface, 0, 0);
      };
    
    // window movement
    pointer.on_button() = [&] (uint32_t serial, uint32_t time, uint32_t button, pointer_button_state state)
      {
        if(button == BTN_LEFT && state == pointer_button_state::pressed)
          shell_surface.move(seat, serial);
      };

    // press 'q' to exit
    keyboard.on_key() = [&] (uint32_t, uint32_t, uint32_t key, keyboard_key_state state)
      {
        if(key == KEY_Q && state == keyboard_key_state::pressed)
          running = false;
      };

    // draw stuff
    draw();
  }

  ~example()
  {
  }

  void run()
  {
    // event loop
    running = true;
    while(running)
      display.dispatch();
  }
};

int main()
{
  example e;
  e.run();
  return 0;
}
