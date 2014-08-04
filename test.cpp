#include <stdexcept>
#include <iostream>
#include <wayland.hpp>
#include <egl.hpp>
#include <GL/gl.h>

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
  egl e(display, surface, 320, 240);
  shell_surface_t shell_surface = shell.get_shell_surface(surface);
  shell_surface.on_ping() = [&] (uint32_t serial) { shell_surface.pong(serial); };
  shell_surface.set_title("Window");
  shell_surface.set_toplevel();

  // no pointer
  pointer_t pointer = seat.get_pointer();
  pointer.on_enter() = [&] (uint32_t serial, surface_t, int32_t, int32_t) { pointer.set_cursor(serial, surface_t(), 0, 0); };

  // click to exit
  bool running = true;
  pointer.on_button() = [&] (uint32_t, uint32_t, uint32_t, uint32_t) { running = false; };

  // print moving events
  pointer.on_motion() = [&] (uint32_t, int32_t x, int32_t y) { std::cout << x/256 << " " << y/256 << std::endl; };

  // draw stuff
  e.begin();
  glClearColor(1.0f, 0.1f, 0.6f, 0.5f);
  glClear(GL_COLOR_BUFFER_BIT);
  e.end();

  // event loop
  while(running) display.dispatch();

  return 0;
}
