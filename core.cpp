#include <core.hpp>
#include <wayland.hpp>

event_queue_t::queue_ptr::~queue_ptr()
{
  if(queue)
    wl_event_queue_destroy(queue);
}

event_queue_t::event_queue_t(wl_event_queue *q)
  : queue(new queue_ptr({q}))
{
}

wl_event_queue *event_queue_t::c_ptr()
{
  if(!queue)
    return NULL;
  return queue->queue;
};

proxy_t::proxy_ptr::~proxy_ptr()
{
  if(proxy)
    {
      events_base_t *events = static_cast<events_base_t*>(wl_proxy_get_user_data(proxy));
      if(events)
        delete events;

      if(!display)
        wl_proxy_destroy(proxy);
      else
        wl_display_disconnect(reinterpret_cast<wl_display*>(proxy));
    }
}

proxy_t proxy_t::marshal_single(uint32_t opcode, const wl_interface *interface, std::vector<wl_argument> v)
{
  if(interface)
    {
      wl_proxy *p = wl_proxy_marshal_array_constructor(proxy->proxy, opcode, v.data(), interface);
      if(!p)
        throw std::runtime_error("wl_proxy_marshal_array_constructor");
      return proxy_t(p);
    }
  wl_proxy_marshal_array(proxy->proxy, opcode, v.data());
  return proxy_t();
}

wl_argument proxy_t::conv(uint32_t i)
{
  wl_argument arg;
  arg.u = i;
  return arg;
}

wl_argument proxy_t::conv(std::string s)
{
  wl_argument arg;
  arg.s = s.c_str();
  return arg;
}

wl_argument proxy_t::conv(proxy_t p)
{
  wl_argument arg;
  arg.o = reinterpret_cast<wl_object*>(p.c_ptr());
  return arg;
}

wl_argument proxy_t::conv(std::vector<char> a)
{
  wl_argument arg;
  wl_array arr = { a.size(), a.size(), a.data() };
  arg.a = &arr;
  return arg;
}

proxy_t::proxy_t()
{
}

void proxy_t::add_dispatcher(wl_dispatcher_func_t dispatcher, events_base_t *events)
{
  if(c_ptr())
    // save as 'data' and as 'implementation', since the dispatcher
    // only gets 'implemetation' and we can only access 'data'.
    wl_proxy_add_dispatcher(c_ptr(), dispatcher, events, events);
}

proxy_t::events_base_t *proxy_t::get_user_data()
{
  if(c_ptr())
    return static_cast<events_base_t*>(wl_proxy_get_user_data(c_ptr()));
  return NULL;
}

proxy_t::proxy_t(wl_proxy *p, bool is_display)
  : proxy(new proxy_ptr({p, is_display}))
{
}
  
proxy_t::proxy_t(const proxy_t& p)
  : proxy(p.proxy)
{
}

uint32_t proxy_t::get_id()
{
  if(c_ptr())
    return wl_proxy_get_id(c_ptr());
  return 0;
}

std::string proxy_t::get_class()
{
  if(c_ptr())
    return wl_proxy_get_class(c_ptr());
  return "";
}

void proxy_t::set_queue(event_queue_t queue)
{
  if(c_ptr() && queue.c_ptr())
    wl_proxy_set_queue(c_ptr(), queue.c_ptr());
}

wl_proxy *proxy_t::c_ptr()
{
  if(!proxy)
    return NULL;
  return proxy->proxy;
}

display_t::display_t(const proxy_t &p)
  : proxy_t(p)
{
}

// Checks a wl_display object and throws an exception
wl_proxy *check(wl_display *display)
{
  if(!display)
    throw std::runtime_error("wl_display_connect_to_fd");
  return reinterpret_cast<wl_proxy*>(display);
}

display_t::display_t(int fd)
  : proxy_t(check(wl_display_connect_to_fd(fd)), true)
{
}

display_t::display_t(std::string name)
  : proxy_t(check(wl_display_connect(name == "" ? NULL : name.c_str())), true)
{
}

event_queue_t display_t::create_queue()
{
  wl_event_queue *queue = wl_display_create_queue(reinterpret_cast<wl_display*>(c_ptr()));
  if(!queue)
    throw std::runtime_error("wl_display_create_queue");
  return queue;
}

int display_t::get_fd()
{
  if(c_ptr())
    return wl_display_get_fd(reinterpret_cast<wl_display*>(c_ptr()));
  return 0;
}

int display_t::roundtrip()
{
  if(c_ptr())
    return wl_display_roundtrip(reinterpret_cast<wl_display*>(c_ptr()));
  return 0;
}    

int display_t::read_events()
{
  if(c_ptr())
    return wl_display_read_events(reinterpret_cast<wl_display*>(c_ptr()));
  return 0;
}    

int display_t::prepare_read()
{
  if(c_ptr())
    return wl_display_prepare_read(reinterpret_cast<wl_display*>(c_ptr()));
  return 0;
}    

int display_t::prepare_read_queue(event_queue_t queue)
{
  if(c_ptr())
    return wl_display_prepare_read_queue(reinterpret_cast<wl_display*>(c_ptr()), queue.c_ptr());
  return 0;
}    

void display_t::cancel_read()
{
  if(c_ptr())
    wl_display_cancel_read(reinterpret_cast<wl_display*>(c_ptr()));
}    

int display_t::dispatch_queue(event_queue_t queue)
{
  if(c_ptr())
    return wl_display_dispatch_queue(reinterpret_cast<wl_display*>(c_ptr()), queue.c_ptr());
  return 0;
}    

int display_t::dispatch_queue_pending(event_queue_t queue)
{
  if(c_ptr())
    return wl_display_dispatch_queue_pending(reinterpret_cast<wl_display*>(c_ptr()), queue.c_ptr());
  return 0;
}    

int display_t::dispatch()
{
  if(c_ptr())
    return wl_display_dispatch(reinterpret_cast<wl_display*>(c_ptr()));
  return 0;
}    

int display_t::dispatch_pending()
{
  if(c_ptr())
    return wl_display_dispatch_pending(reinterpret_cast<wl_display*>(c_ptr()));
  return 0;
}    

int display_t::get_error()
{
  if(c_ptr())
    return wl_display_get_error(reinterpret_cast<wl_display*>(c_ptr()));
  return 0;
}    

int display_t::flush()
{
  if(c_ptr())
    return wl_display_flush(reinterpret_cast<wl_display*>(c_ptr()));
  return 0;
}    

callback_t display_t::sync()
{
  return marshal_constructor(0, &wl_callback_interface, NULL);
}

registry_t display_t::get_registry()
{
  return marshal_constructor(1, &wl_registry_interface, NULL);
}
