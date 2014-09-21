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

#include <iostream>
#include <wayland-client.hpp>

using namespace wayland;
using namespace wayland::detail;

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

int proxy_t::c_dispatcher(const void *implementation, void *target, uint32_t opcode, const wl_message *message, wl_argument *args)
{
  std::string signature(message->signature);
  std::vector<any> vargs;
  unsigned int c = 0;
  for(char ch : signature)
    {
      if(ch == '?' || isdigit(ch))
        continue;

      any a;
      switch(ch)
        {
          // int_32_t
        case 'i':
        case 'h':
        case 'f':
          a = args[c].i;
          break;
          // uint32_t
        case 'u':
          a = args[c].u;
          break;
          // string
        case 's':
          if(args[c].s)
            a = std::string(args[c].s);
          else
            a = std::string("");
          break;
          // proxy
        case 'o':
          if(args[c].o)
            a = proxy_t(reinterpret_cast<wl_proxy*>(args[c].o));
          else
            a = proxy_t();
          break;
          // new id
        case 'n':
          {
            if(args[c].o)
              {
                wl_proxy *proxy = reinterpret_cast<wl_proxy*>(args[c].o);
                wl_proxy_set_user_data(proxy, NULL); // Wayland leaves the user data uninitialized
                a = proxy_t(proxy);
              }
            else
              a = proxy_t();
          }
          break;
          // array
        case 'a':
          if(args[c].a)
            a = std::vector<char>(reinterpret_cast<char*>(args[c].a->data),
                                  reinterpret_cast<char*>(args[c].a->data) + args[c].a->size);
          else
            a = std::vector<char>();
          break;
        default:
          a = 0;
          break;
        }
      vargs.push_back(a);
      c++;
    }
  proxy_t p(reinterpret_cast<wl_proxy*>(target), false);
  typedef int(*dispatcher_func)(int, std::vector<any>, std::shared_ptr<proxy_t::events_base_t>);
  dispatcher_func dispatcher = reinterpret_cast<dispatcher_func>(const_cast<void*>(implementation));
  return dispatcher(opcode, vargs, p.get_events());
}

proxy_t proxy_t::marshal_single(uint32_t opcode, const wl_interface *interface, std::vector<wl_argument> v)
{
  if(interface)
    {
      wl_proxy *p = wl_proxy_marshal_array_constructor(proxy, opcode, v.data(), interface);
      if(!p)
        throw std::runtime_error("wl_proxy_marshal_array_constructor");
      wl_proxy_set_user_data(p, NULL); // Wayland leaves the user data uninitialized
      return proxy_t(p);
    }
  wl_proxy_marshal_array(proxy, opcode, v.data());
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
  arg.o = reinterpret_cast<wl_object*>(p.proxy);
  return arg;
}

wl_argument proxy_t::conv(std::vector<char> a)
{
  wl_argument arg;
  wl_array arr = { a.size(), a.size(), a.data() };
  arg.a = &arr;
  return arg;
}

void proxy_t::set_destroy_opcode(int destroy_opcode)
{
  if(proxy && !display)
    {
      proxy_data_t *data = reinterpret_cast<proxy_data_t*>(wl_proxy_get_user_data(proxy));
      data->opcode = destroy_opcode;
    }
}

void proxy_t::set_events(std::shared_ptr<events_base_t> events,
                         int(*dispatcher)(int, std::vector<any>, std::shared_ptr<proxy_t::events_base_t>))
{
  if(proxy && !display)
    {
      proxy_data_t *data = reinterpret_cast<proxy_data_t*>(wl_proxy_get_user_data(proxy));
      // set only one time
      if(!data->events)
        {
          data->events = events;
          // the dispatcher gets 'implemetation'
          wl_proxy_add_dispatcher(proxy, c_dispatcher, reinterpret_cast<void*>(dispatcher), data);
        }
    }
}

std::shared_ptr<proxy_t::events_base_t> proxy_t::get_events()
{
  if(proxy && !display)
    {
      proxy_data_t *data = reinterpret_cast<proxy_data_t*>(wl_proxy_get_user_data(proxy));
      return data->events;
    }
  return std::shared_ptr<events_base_t>();
}

proxy_t::proxy_t()
  : proxy(NULL), display(false), interface(NULL)
{
}

proxy_t::proxy_t(wl_proxy *p, bool is_display)
  : proxy(p), display(is_display), interface(NULL)
{
  if(proxy && !display)
    {
      proxy_data_t *data = reinterpret_cast<proxy_data_t*>(wl_proxy_get_user_data(proxy));
      if(!data)
        {
          data = new proxy_data_t{std::shared_ptr<events_base_t>(), -1, 0};
          wl_proxy_set_user_data(proxy, data);
        }
      data->counter++;
    } 
}
  
proxy_t::proxy_t(const proxy_t &p)
{
  operator=(p);
}

proxy_t &proxy_t::operator=(const proxy_t& p)
{
  proxy = p.proxy;
  interface = p.interface;
  display = p.display;
  if(proxy && !display)
    {
      proxy_data_t *data = reinterpret_cast<proxy_data_t*>(wl_proxy_get_user_data(proxy));
      if(!data)
        {
          std::cout << "Found proxy_t without meta data." << std::endl;
          data = new proxy_data_t{std::shared_ptr<events_base_t>(), -1, 0};
          wl_proxy_set_user_data(proxy, data);
        }
      data->counter++;
    }
  return *this;
}

proxy_t::proxy_t(proxy_t &&p)
  : proxy(NULL), display(false), interface(NULL)
{
  operator=(std::move(p));
}

proxy_t &proxy_t::operator=(proxy_t &&p)
{
  std::swap(proxy, p.proxy);
  std::swap(display, p.display);
  std::swap(interface, p.interface);
  return *this;
}

proxy_t::~proxy_t()
{
  if(proxy && !display)
    {
      proxy_data_t *data = reinterpret_cast<proxy_data_t*>(wl_proxy_get_user_data(proxy));
      data->counter--;
      if(data->counter == 0)
        {
          if(data->opcode >= 0)
            wl_proxy_marshal(proxy, data->opcode);
          wl_proxy_destroy(proxy);
          delete data;
        }
    }
}

uint32_t proxy_t::get_id()
{
  if(proxy)
    return wl_proxy_get_id(proxy);
  return 0;
}

std::string proxy_t::get_class()
{
  if(proxy)
    return wl_proxy_get_class(proxy);
  return "";
}

void proxy_t::set_queue(event_queue_t queue)
{
  if(proxy && queue.c_ptr())
    wl_proxy_set_queue(proxy, queue.c_ptr());
}

wl_proxy *proxy_t::c_ptr()
{
  return proxy;
}

display_t::display_t(int fd)
  : proxy_t(reinterpret_cast<wl_proxy*>(wl_display_connect_to_fd(fd)), true)
{
  if(!c_ptr())
    throw std::runtime_error("wl_display_connect_to_fd");
  interface = &wl_display_interface;
}

display_t::display_t(std::string name)
  : proxy_t(reinterpret_cast<wl_proxy*>(wl_display_connect(name == "" ? NULL : name.c_str())), true)
{
  if(!c_ptr())
    throw std::runtime_error("wl_display_connect");
  interface = &wl_display_interface;
}

display_t::display_t(display_t &&d)
{
  operator=(std::move(d));
}

display_t &display_t::operator=(display_t &&d)
{
  proxy_t::operator=(std::move(d));
  return *this;
}

display_t::~display_t()
{
  wl_display_disconnect(reinterpret_cast<wl_display*>(c_ptr()));
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
  return callback_t(marshal_constructor(0, &wl_callback_interface, NULL));
}

registry_t display_t::get_registry()
{
  return registry_t(marshal_constructor(1, &wl_registry_interface, NULL));
}
