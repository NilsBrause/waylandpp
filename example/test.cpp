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

/** \example test.cpp
 * This is an example of how to use the Waylanbd C++ bindings.
 */

#include <stdexcept>
#include <iostream>
#include <wayland-client.hpp>
#include <wayland-egl.hpp>
#include <GL/gl.h>
#include <linux/input.h>

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

  // EGL
  egl_window_t egl_window;
  EGLDisplay egldisplay;
  EGLSurface eglsurface;
  EGLContext eglcontext;

  bool running;

  void init_egl()
  {
    egldisplay = eglGetDisplay(display);
    if(egldisplay == EGL_NO_DISPLAY)
      throw std::runtime_error("eglGetDisplay");

    EGLint major, minor;
    if(eglInitialize(egldisplay, &major, &minor) == EGL_FALSE)
      throw std::runtime_error("eglInitialize");
    if(!((major == 1 && minor >= 4) || major >= 2))
      throw std::runtime_error("EGL version too old");

    if(eglBindAPI(EGL_OPENGL_API) == EGL_FALSE)
      throw std::runtime_error("eglBindAPI");

    std::vector<EGLint> config_attribs = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_NONE
    };

    EGLConfig config;
    EGLint num;
    if(eglChooseConfig(egldisplay, config_attribs.data(), &config, 1, &num) == EGL_FALSE || num == 0)
      throw std::runtime_error("eglChooseConfig");

    std::vector<EGLint> context_attribs = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
    };

    eglcontext = eglCreateContext(egldisplay, config, EGL_NO_CONTEXT, context_attribs.data());
    if(eglcontext == EGL_NO_CONTEXT)
      throw std::runtime_error("eglCreateContext");

    eglsurface = eglCreateWindowSurface(egldisplay, config, egl_window, NULL);
    if(eglsurface == EGL_NO_SURFACE)
      throw std::runtime_error("eglCreateWindowSurface");

    if(eglMakeCurrent(egldisplay, eglsurface, eglsurface, eglcontext) == EGL_FALSE)
      throw std::runtime_error("eglMakeCurrent");
  }
  
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
    glClearColor(r, g, b, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);

    // schedule next draw
    frame_cb = surface.frame();
    frame_cb.on_done() = bind_mem_fn(&example::draw, this);

    // swap buffers
    if(eglSwapBuffers(egldisplay, eglsurface) == EGL_FALSE)
      throw std::runtime_error("eglSwapBuffers");
  }
  
public:
  example()
  {
    // retrieve global objects
    registry = display.get_registry();
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
    surface = compositor.create_surface();
    shell_surface = shell.get_shell_surface(surface);
    
    shell_surface.on_ping() = [&] (uint32_t serial) { shell_surface.pong(serial); };
    shell_surface.set_title("Window");
    shell_surface.set_toplevel();

    // Get input devices (Let's just assume they're present)
    pointer = seat.get_pointer();
    keyboard = seat.get_keyboard();

    // no pointer
    pointer.on_enter() = [&] (uint32_t serial, surface_t, int32_t, int32_t) { pointer.set_cursor(serial, surface_t(), 0, 0); };
    
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

    // intitialize egl
    egl_window = egl_window_t(surface, 320, 240);
    init_egl();

    // draw stuff
    draw();
  }

  ~example()
  {
    // finialize EGL
    if(eglDestroyContext(egldisplay, eglcontext) == EGL_FALSE)
      throw std::runtime_error("eglDestroyContext");
    if(eglTerminate(egldisplay) == EGL_FALSE)
      throw std::runtime_error("eglTerminate");
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
