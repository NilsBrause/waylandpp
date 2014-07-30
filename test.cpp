#include <stdexcept>
#include <iostream>
#include <wayland.hpp>

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

  surface_t surfaace = compositor.create_surface();

  return 0;
}
