#include <stdexcept>
#include <iostream>
#include <wayland.hpp>
#include <egl.hpp>
#include <GL/gl.h>

int main()
{
  wl_display *d = wl_display_connect(NULL);
  display_t display(reinterpret_cast<wl_proxy*>(d));

  registry_t registry = display.get_registry();
  compositor_t compositor;
  shell_t shell;
  seat_t seat;
  shm_t shm;
  registry.on_global() = [&] (uint32_t name, std::string interface, uint32_t version)
    {
      std::cout << name << ": " << interface << " v" << version << std::endl;
      if(interface == "wl_compositor")
        compositor = registry.bind(name, &wl_compositor_interface, version);
      else if(interface == "wl_shell")
        shell = registry.bind(name, &wl_shell_interface, version);
      else if(interface == "wl_seat")
        seat = registry.bind(name, &wl_seat_interface, version);
      else if(interface == "wl_shm")
        shm = registry.bind(name, &wl_shm_interface, version);
    };

  wl_display_dispatch(d);

  surface_t surface = compositor.create_surface();
  egl e(display, surface, 320, 240);
  shell_surface_t shell_surface = shell.get_shell_surface(surface);
  shell_surface.on_ping() = [&] (uint32_t serial) { shell_surface.pong(serial); };
  shell_surface.set_title("Window");
  shell_surface.set_toplevel();
  
  e.begin();
  glClearColor(1.0f, 0.1f, 0.6f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  e.end();

  while(true) wl_display_dispatch(d);

  return 0;
}
