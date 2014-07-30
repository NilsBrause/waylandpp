#ifndef CORE_HPP
#define CORE_HPP

#include <memory>
#include <string>
#include <vector>
#include <wayland-client.h>

class event_queue_t
{
private:
  struct queue_ptr
  {
    wl_event_queue *queue;
    ~queue_ptr()
    {
      wl_event_queue_destroy(queue);
    }
  };

  std::shared_ptr<queue_ptr> queue;

  friend class display_t;

  event_queue_t(wl_event_queue *q)
    : queue(new queue_ptr({q}))
  {
  }
};

class proxy_t
{
private:
  struct proxy_ptr
  {
    wl_proxy *proxy;
    bool display;
    ~proxy_ptr()
    {
      if(!display)
        wl_proxy_destroy(proxy);
      else
        wl_display_disconnect(reinterpret_cast<wl_display*>(proxy));
    }
  };
  
  std::shared_ptr<proxy_ptr> proxy;

  friend class egl;
  friend class display_t;

  // handles integers, file descriptors and fixed point numbers
  // (this works, because wl_argument is an union)
  template <typename...T>
  proxy_t marshal_single(uint32_t opcode, const wl_interface *interface, std::vector<wl_argument> v, uint32_t i, T...args)
  {
    wl_argument arg;
    arg.u = i;
    v.push_back(arg);
    return marshal_single(opcode, interface, v, args...);
  }
  
  // handles strings
  template <typename...T>
  proxy_t marshal_single(uint32_t opcode, const wl_interface *interface, std::vector<wl_argument> v, std::string s, T...args)
  {
    wl_argument arg;
    arg.s = s.c_str();
    v.push_back(arg);
    return marshal_single(opcode, interface, v, args...);
  }
  
  // handles objects
  template <typename...T>
  proxy_t marshal_single(uint32_t opcode, const wl_interface *interface, std::vector<wl_argument> v, proxy_t p, T...args)
  {
    wl_argument arg;
    arg.o = reinterpret_cast<wl_object*>(p.proxy->proxy); // a proper C API shouldn't require this
    v.push_back(arg);
    return marshal_single(opcode, interface, v, args...);
  }
  
  // handles arrays
  template <typename...T>
  proxy_t marshal_single(uint32_t opcode, const wl_interface *interface, std::vector<wl_argument> v, std::vector<char> a, T...args)
  {
    wl_argument arg;
    wl_array arr = { a.size(), a.size(), a.data() };
    arg.a = &arr;
    v.push_back(arg);
    return marshal_single(opcode, interface, v, args...);
  }

  // no more arguments
  proxy_t marshal_single(uint32_t opcode, const wl_interface *interface, std::vector<wl_argument> v)
  {
    if(interface)
      return proxy_t(wl_proxy_marshal_array_constructor(proxy->proxy, opcode, v.data(), interface));
    wl_proxy_marshal_array(proxy->proxy, opcode, v.data());
    return proxy_t();
  }

protected:
  proxy_t()
  {
  }

public:
    proxy_t(wl_proxy *p, bool is_display = false)
      : proxy(new proxy_ptr({p, is_display}))
  {
  }
  
  proxy_t(const proxy_t& p)
    : proxy(p.proxy)
  {
  }

  template <typename...T>
  void marshal(uint32_t opcode, T...args)
  {
    if(proxy)
      marshal_single(opcode, NULL, {}, args...);
  }

  template <typename...T>
  proxy_t marshal_constructor(uint32_t opcode, const wl_interface *interface, T...args)
  {
    if(proxy)
      return marshal_single(opcode, interface, {}, args...);
    return proxy_t();
  }

  void add_dispatcher(wl_dispatcher_func_t dispatcher, void *data)
  {
    if(proxy)
      wl_proxy_add_dispatcher(proxy->proxy, dispatcher, data, NULL);
  }
};

class callback_t;
class registry_t;

class display_t : public proxy_t
{
public:
  display_t(int fd);
  display_t(std::string name = "");
  event_queue_t create_queue();
  int get_fd();
  int roundtrip();
  int read_events();
  int prepare_read();
  void cancel_read();
  int dispatch_queue(event_queue_t queue);
  int dispatch_queue_pending(event_queue_t queue);
  int dispatch();
  int dispatch_pending();
  int get_error();
  int flush();
  callback_t sync();
  registry_t get_registry();
};

enum display_error
  {
    display_error_invalid_object = 0,
    display_error_invalid_method = 1,
    display_error_no_memory = 2
  };

#endif
