#include <core.hpp>
#include <wayland.hpp>

display_t::display_t(int fd)
  : proxy_t(reinterpret_cast<wl_proxy*>(wl_display_connect_to_fd(fd)), true)
{
}

display_t::display_t(std::string name)
  : proxy_t(reinterpret_cast<wl_proxy*>(wl_display_connect(name == "" ? NULL : name.c_str())), true)
{
}

event_queue_t display_t::create_queue()
{
  return wl_display_create_queue(reinterpret_cast<wl_display*>(proxy->proxy));
}

int display_t::get_fd()
{
  return wl_display_get_fd(reinterpret_cast<wl_display*>(proxy->proxy));
}

int display_t::roundtrip()
{
  return wl_display_roundtrip(reinterpret_cast<wl_display*>(proxy->proxy));
}    

int display_t::read_events()
{
  return wl_display_read_events(reinterpret_cast<wl_display*>(proxy->proxy));
}    

int display_t::prepare_read()
{
  return wl_display_prepare_read(reinterpret_cast<wl_display*>(proxy->proxy));
}    

void display_t::cancel_read()
{
  wl_display_cancel_read(reinterpret_cast<wl_display*>(proxy->proxy));
}    

int display_t::dispatch_queue(event_queue_t queue)
{
  return wl_display_dispatch_queue(reinterpret_cast<wl_display*>(proxy->proxy), queue.queue->queue);
}    

int display_t::dispatch_queue_pending(event_queue_t queue)
{
  return wl_display_dispatch_queue_pending(reinterpret_cast<wl_display*>(proxy->proxy), queue.queue->queue);
}    

int display_t::dispatch()
{
  return wl_display_dispatch(reinterpret_cast<wl_display*>(proxy->proxy));
}    

int display_t::dispatch_pending()
{
  return wl_display_dispatch_pending(reinterpret_cast<wl_display*>(proxy->proxy));
}    

int display_t::get_error()
{
  return wl_display_get_error(reinterpret_cast<wl_display*>(proxy->proxy));
}    

int display_t::flush()
{
  return wl_display_flush(reinterpret_cast<wl_display*>(proxy->proxy));
}    

callback_t display_t::sync()
{
  return marshal_constructor(0, &wl_callback_interface, NULL);
}

registry_t display_t::get_registry()
{
  return marshal_constructor(1, &wl_registry_interface, NULL);
}
