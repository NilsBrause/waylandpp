/*
 * Copyright (c) 2025, Nils Christopher Brause
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

/** \example clipboard.cpp
 * This is an example of how to use the Clipboard with the Wayland C++ bindings.
 */

#include <stdexcept>
#include <array>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <set>

#include <unistd.h>

#include <wayland-client.hpp>
#include <wayland-client-protocol-extra.hpp>
#include <linux/input.h>
#include <wayland-cursor.hpp>

#include "shm_common.hpp"

using namespace wayland;

auto hash_proxy = [] (proxy_t const& proxy) { return reinterpret_cast<size_t>(proxy.c_ptr()); };
using hash_proxy_t = decltype(hash_proxy);

// example Wayland client
class example
{
private:
  // global objects
  display_t display;
  registry_t registry;
  compositor_t compositor;
  shell_t shell;
  xdg_wm_base_t xdg_wm_base;
  seat_t seat;
  shm_t shm;
  data_device_manager_t data_device_manager;

  // local objects
  surface_t surface;
  shell_surface_t shell_surface;
  xdg_surface_t xdg_surface;
  xdg_toplevel_t xdg_toplevel;
  pointer_t pointer;
  keyboard_t keyboard;
  callback_t frame_cb;
  cursor_image_t cursor_image;
  buffer_t cursor_buffer;
  surface_t cursor_surface;

  data_device_t data_device;
  std::unordered_map<data_offer_t, std::set<std::string>, hash_proxy_t> data_offer_mime_types;
  data_source_t data_source;

  std::shared_ptr<shared_mem_t> shared_mem;
  buffer_t buffer;

  bool running;
  bool has_pointer;
  bool has_keyboard;
  int width = 640;
  int height = 480;

  void create_buffers(int w, int h)
  {
    if (w != 0)
      width = w;
    if (h != 0)
      height = h;
    shared_mem.reset(new shared_mem_t(width * height * 4));
    auto pool = shm.create_pool(shared_mem->get_fd(), width * height * 4);
    buffer = pool.create_buffer(0, width, height, width * 4, shm_format::argb8888);
  }

  void draw(uint32_t serial = 0)
  {
    // draw stuff
    std::fill_n(static_cast<uint32_t*>(shared_mem->get_mem()), width*height, 0xFF008080);
    surface.attach(buffer, 0, 0);
    surface.damage(0, 0, width, height);

    // schedule next draw
    frame_cb = surface.frame();
    frame_cb.on_done() = [this] (uint32_t serial) { draw(serial); };
    surface.commit();
  }

public:
  example(const example&) = delete;
  example(example&&) noexcept = delete;
  ~example() noexcept = default;
  example& operator=(const example&) = delete;
  example& operator=(example&&) noexcept = delete;

  example()
    : data_offer_mime_types(0, hash_proxy)
  {
    // retrieve global objects
    registry = display.get_registry();
    registry.on_global() = [&] (uint32_t name, const std::string& interface, uint32_t version)
    {
      if(interface == compositor_t::interface_name)
        registry.bind(name, compositor, std::min(compositor_t::interface_version, version));
      else if(interface == shell_t::interface_name)
        registry.bind(name, shell, std::min(shell_t::interface_version, version));
      else if(interface == xdg_wm_base_t::interface_name)
        registry.bind(name, xdg_wm_base, std::min(xdg_wm_base_t::interface_version, version));
      else if(interface == seat_t::interface_name)
        registry.bind(name, seat, std::min(seat_t::interface_version, version));
      else if(interface == shm_t::interface_name)
        registry.bind(name, shm, std::min(shm_t::interface_version, version));
      else if (interface == data_device_manager_t::interface_name)
        registry.bind(name, data_device_manager, std::min(data_device_manager_t::interface_version, version));
    };
    display.roundtrip();

    seat.on_capabilities() = [&] (const seat_capability& capability)
    {
      has_keyboard = capability & seat_capability::keyboard;
      has_pointer = capability & seat_capability::pointer;
    };

    // create a surface
    surface = compositor.create_surface();

    // create a shell surface
    if(xdg_wm_base)
    {
      xdg_wm_base.on_ping() = [&] (uint32_t serial) { xdg_wm_base.pong(serial); };
      xdg_surface = xdg_wm_base.get_xdg_surface(surface);
      xdg_surface.on_configure() = [&] (uint32_t serial) { xdg_surface.ack_configure(serial); };
      xdg_toplevel = xdg_surface.get_toplevel();
      xdg_toplevel.set_title("Window");
      xdg_toplevel.on_close() = [&] () { running = false; };
      xdg_toplevel.on_configure() = [&] (int32_t w, int32_t h, array_t)
      {
        create_buffers(w, h);
        // Don't immediately redraw, as this would slow down resizes considerably.
      };
    }
    else
    {
      shell_surface = shell.get_shell_surface(surface);
      shell_surface.on_ping() = [&] (uint32_t serial) { shell_surface.pong(serial); };
      shell_surface.set_title("Window");
      shell_surface.set_toplevel();
      shell_surface.on_configure() = [&] (wayland::shell_surface_resize, int32_t w, int32_t h)
      {
        create_buffers(w, h);
        // Don't immediately redraw, as this would slow down resizes considerably.
      };
    }
    surface.commit();

    data_device = data_device_manager.get_data_device(seat);

    // The data_offer objects needs to be saved, otherwise it gets rejected.
    data_device.on_data_offer() = [&] (data_offer_t offer)
    {
      data_offer_mime_types[offer] = {};

      // The MIME typed are checked later.
      offer.on_offer() = [this, offer] (std::string mime_type)
      {
        data_offer_mime_types[offer].insert(mime_type);
      };
    };

    data_device.on_selection() = [&] (data_offer_t offer)
    {
      // Clipboard is empty.
      if(!offer)
        return;

      // No previous data_offer event.
      if (!data_offer_mime_types.count(offer))
        return;

      // Only accept text.
      if (!data_offer_mime_types[offer].count("text/plain") )
      {
        data_offer_mime_types.erase(offer);
        return;
      }

      std::array<int, 2> fds;
      pipe(fds.data());
      offer.receive("text/plain", fds[1]);
      close(fds[1]);

      display.roundtrip();

      std::cout << "Pasted from clipbaord: " << std::flush;
      while (true)
      {
        std::string buf(1024, 0);
        auto n = read(fds[0], const_cast<char*>(buf.c_str()), buf.size());
        if (n <= 0)
          break;
        buf.resize(n);
        std::cout << buf;
      }
      std::cout << std::endl;
      close(fds[0]);

      data_offer_mime_types.erase(offer);
    };

    display.roundtrip();

    // Get input devices
    if(!has_keyboard)
      throw std::runtime_error("No keyboard found.");
    if(!has_pointer)
      throw std::runtime_error("No pointer found.");

    pointer = seat.get_pointer();
    keyboard = seat.get_keyboard();

    // create shared memory
    create_buffers(width, height);

    // load cursor theme
    cursor_theme_t cursor_theme = cursor_theme_t("default", 16, shm);
    cursor_t cursor = cursor_theme.get_cursor("cross");
    cursor_image = cursor.image(0);
    cursor_buffer = cursor_image.get_buffer();

    // create cursor surface
    cursor_surface = compositor.create_surface();

    // draw cursor
    pointer.on_enter() = [&] (uint32_t serial, const surface_t& /*unused*/, int32_t /*unused*/, int32_t /*unused*/)
    {
      cursor_surface.attach(cursor_buffer, 0, 0);
      cursor_surface.damage(0, 0, cursor_image.width(), cursor_image.height());
      cursor_surface.commit();
      pointer.set_cursor(serial, cursor_surface, cursor_image.hotspot_x(), cursor_image.hotspot_y());
    };

    // window movement
    pointer.on_button() = [&] (uint32_t serial, uint32_t /*unused*/, uint32_t button, pointer_button_state state)
    {
      if(button == BTN_LEFT && state == pointer_button_state::pressed)
      {
        if(xdg_toplevel)
          xdg_toplevel.move(seat, serial);
        else
          shell_surface.move(seat, serial);
      }
    };

    std::cout << "Press 'C' to copy a message to the clipboard." << std::endl;

    // press 'q' to exit
    keyboard.on_key() = [&] (uint32_t serial, uint32_t /*unused*/, uint32_t key, keyboard_key_state state)
    {
      if (state != keyboard_key_state::pressed)
        return;

      if(key == KEY_Q)
      {
        running = false;
      }
      else if(key == KEY_C)
      {
        data_source = data_device_manager.create_data_source();
        data_source.on_send() = [] (std::string mime_type, int fd)
        {
          std::string message = "Hello Wayland++!";
          write(fd, message.c_str(), message.size());
          close(fd);
        };
        data_source.offer("text/plain");
        data_device.set_selection(data_source, serial);
      }
    };

    // draw stuff
    draw();
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
