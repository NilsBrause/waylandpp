#ifndef PROXY_HPP
#define PROXY_HPP

class proxy_t
{
private:
  struct proxy_ptr
  {
    wl_proxy *proxy;
    ~proxy_ptr()
    {
      wl_proxy_destroy(proxy);
    }
  };
  
  std::shared_ptr<proxy_ptr> proxy;

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
  proxy_t(wl_proxy *p)
    : proxy(new proxy_ptr({p}))
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

#endif
