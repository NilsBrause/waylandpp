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
    ~queue_ptr();
  };

  std::shared_ptr<queue_ptr> queue;

public:
  event_queue_t(wl_event_queue *q);
  wl_event_queue *c_ptr();
};

class proxy_t
{
protected:
  struct events_base_t
  {
    virtual ~events_base_t() { }
  };

private:
  struct proxy_data_t
  {
    std::shared_ptr<events_base_t> events;
    int opcode;
    bool display;
    unsigned int counter;
  };

  wl_proxy *proxy;

  // marshal request
  proxy_t marshal_single(uint32_t opcode, const wl_interface *interface, std::vector<wl_argument> v);

  // handles integers, file descriptors and fixed point numbers
  // (this works, because wl_argument is an union)
  wl_argument conv(uint32_t i);
  // handles strings
  wl_argument conv(std::string s);
  // handles objects
  wl_argument conv(proxy_t p);
  // handles arrays
  wl_argument conv(std::vector<char> a);

protected:
  proxy_t();

  template <typename...T>
  void marshal(uint32_t opcode, T...args)
  {
    std::vector<wl_argument> v = { conv(args)... };
    if(c_ptr())
      marshal_single(opcode, NULL, v);
  }

  template <typename...T>
  proxy_t marshal_constructor(uint32_t opcode, const wl_interface *interface, T...args)
  {
    std::vector<wl_argument> v = { conv(args)... };
    if(c_ptr())
      return marshal_single(opcode, interface, v);
    return proxy_t();
  }

  void add_dispatcher(wl_dispatcher_func_t dispatcher, std::shared_ptr<events_base_t> events);
  void set_destroy_opcode(int destroy_opcode);
  std::shared_ptr<proxy_t::events_base_t> get_events();

public:
  proxy_t(wl_proxy *p, bool is_display = false);
  proxy_t(const proxy_t& p);
  proxy_t &operator=(const proxy_t &p);
  ~proxy_t();
  uint32_t get_id();
  std::string get_class();
  void set_queue(event_queue_t queue);
  wl_proxy *c_ptr();
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
  int prepare_read_queue(event_queue_t queue);
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
