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
#include <iostream>
#include <wayland-client.hpp>
#include <wayland-egl.hpp>
#include <egl.hpp>
#include <GL/gl.h>
#include <linux/input.h>

int main()
{
  // retrieve global objects
  display_t display;
  registry_t registry = display.get_registry();
  compositor_t compositor;
  shell_t shell;
  seat_t seat;
  shm_t shm;
  registry.on_global() = [&] (uint32_t name, std::string interface, uint32_t version)
    {
      if(interface == "wl_compositor")
        registry.bind(name, compositor, version);
      else if(interface == "wl_shell")
        registry.bind(name, shell, version);
      else if(interface == "wl_seat")
        registry.bind(name, seat, version);
      else if(interface == "wl_shm")
        registry.bind(name, shm, version);
    };
  display.dispatch();

  // create a surface
  surface_t surface = compositor.create_surface();
  shell_surface_t shell_surface = shell.get_shell_surface(surface);
  shell_surface.on_ping() = [&] (uint32_t serial) { shell_surface.pong(serial); };
  shell_surface.set_title("Window");
  shell_surface.set_toplevel();

  // Get input devices (I'll just assume they're present)
  pointer_t pointer = seat.get_pointer();
  keyboard_t keyboard = seat.get_keyboard();

  // no pointer
  pointer.on_enter() = [&] (uint32_t serial, surface_t, int32_t, int32_t) { pointer.set_cursor(serial, surface_t(), 0, 0); };

  // window movement
  pointer.on_button() = [&] (uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
    {
      if(button == BTN_LEFT && state == pointer_t::button_state_pressed)
        shell_surface.move(seat, serial);
    };

  // press 'q' to exit
  bool running = true;
  keyboard.on_key() = [&] (uint32_t, uint32_t, uint32_t key, uint32_t state)
    {
      if(key == KEY_Q && state == keyboard_t::key_state_pressed)
        running = false;
    };

  // intitialize egl
  egl_window_t egl_window(surface, 320, 240);
  egl e(display, egl_window);

  // draw stuff
  e.begin();
  glClearColor(1.0f, 0.1f, 0.6f, 0.5f);
  glClear(GL_COLOR_BUFFER_BIT);
  e.end();

  // event loop
  while(running) display.dispatch();

  return 0;
}
