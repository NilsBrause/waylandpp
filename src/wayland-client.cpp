/*
 * Copyright (c) 2014-2016, Nils Christopher Brause
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

#include <cassert>
#include <iostream>
#include <wayland-client.hpp>
#include <wayland-client-protocol.hpp>

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
    throw std::invalid_argument("event_queue is NULL");
  return queue->queue;
};

int proxy_t::c_dispatcher(const void *implementation, void *target, uint32_t opcode, const wl_message *message, wl_argument *args)
{
  if(!implementation)
    throw std::invalid_argument("proxy dispatcher: implementation is NULL.");
  if(!target)
    throw std::invalid_argument("proxy dispatcher: target is NULL.");
  if(!message)
    throw std::invalid_argument("proxy dispatcher: message is NULL.");
  if(!args)
    throw std::invalid_argument("proxy dispatcher: args is NULL.");

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
              {
                a = proxy_t();
                std::cerr << "New id is empty." << std::endl;
              }
          }
          break;
          // array
        case 'a':
          if(args[c].a)
            a = array_t(args[c].a);
          else
            a = array_t();
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

proxy_t proxy_t::marshal_single(uint32_t opcode, const wl_interface *interface, std::vector<argument_t> args)
{
  std::vector<wl_argument> v;
  for(auto &arg : args)
    v.push_back(arg.argument);
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

void proxy_t::set_destroy_opcode(int destroy_opcode)
{
  if(!display)
    data->opcode = destroy_opcode;
  else
    std::cerr << "Not setting destroy opcode on display.";
}

void proxy_t::set_events(std::shared_ptr<events_base_t> events,
                         int(*dispatcher)(int, std::vector<any>, std::shared_ptr<proxy_t::events_base_t>))
{
  // set only one time
  if(!display && !data->events)
    {
      data->events = events;
      // the dispatcher gets 'implemetation'
      if(wl_proxy_add_dispatcher(proxy, c_dispatcher, reinterpret_cast<void*>(dispatcher), data) < 0)
        throw std::runtime_error("wl_proxy_add_dispatcher failed.");
    }
}

std::shared_ptr<proxy_t::events_base_t> proxy_t::get_events()
{
  if(!display)
    return data->events;
  return std::shared_ptr<events_base_t>();
}

proxy_t::proxy_t()
  : proxy(NULL), data(NULL), display(false), interface(NULL)
{
}

proxy_t::proxy_t(wl_proxy *p, bool is_display, bool donotdestroy)
  : proxy(p), data(NULL), display(is_display), dontdestroy(donotdestroy), interface(NULL)
{
  if(!display)
    {
      data = reinterpret_cast<proxy_data_t*>(wl_proxy_get_user_data(c_ptr()));
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
  data = p.data;
  interface = p.interface;
  copy_constructor = p.copy_constructor;
  display = p.display;
  dontdestroy = p.dontdestroy;

  if(data)
    {
      data->counter++;
    }
  
  // Allowed: nothing set, proxy set & data unset (for wl_display), proxy & data set (for generic wl_proxy)
  assert((!display && !data && !proxy) || (!display && data && proxy) || (display && !data && proxy));

  return *this;
}

proxy_t::proxy_t(proxy_t &&p)
  : proxy(NULL), data(NULL), display(false), dontdestroy(NULL), interface(NULL)
{
  operator=(std::move(p));
}

proxy_t &proxy_t::operator=(proxy_t &&p)
{
  std::swap(proxy, p.proxy);
  std::swap(data, p.data);
  std::swap(display, p.display);
  std::swap(dontdestroy, p.dontdestroy);
  std::swap(interface, p.interface);
  std::swap(copy_constructor, p.copy_constructor);
  return *this;
}

proxy_t::~proxy_t()
{
  proxy_release();
}

void proxy_t::proxy_release()
{
  if(proxy && !display)
    {
      data->counter--;
      if(data->counter == 0)
        {
          if(!dontdestroy)
            {
              if(data->opcode >= 0)
                wl_proxy_marshal(proxy, data->opcode);
              wl_proxy_destroy(proxy);
            }
          delete data;
        }
  
      proxy = NULL;
      data = NULL;
    }
}


uint32_t proxy_t::get_id()
{
  return wl_proxy_get_id(c_ptr());
}

std::string proxy_t::get_class()
{
  return wl_proxy_get_class(c_ptr());
}

void proxy_t::set_queue(event_queue_t queue)
{
  wl_proxy_set_queue(c_ptr(), queue.c_ptr());
}

wl_proxy *proxy_t::c_ptr()
{
  if(!proxy)
    throw std::invalid_argument("proxy is NULL");
  return proxy;
}

bool proxy_t::proxy_has_object() const
{
  return proxy;
}

proxy_t::operator bool() const
{
  return proxy_has_object();
}

display_t::display_t(int fd)
  : proxy_t(reinterpret_cast<wl_proxy*>(wl_display_connect_to_fd(fd)), true)
{
  c_ptr(); // throws if NULL
  interface = &display_interface;
}

display_t::display_t(std::string name)
  : proxy_t(reinterpret_cast<wl_proxy*>(wl_display_connect(name == "" ? NULL : name.c_str())), true)
{
  c_ptr(); // throws if NULL
  interface = &display_interface;
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
  return wl_display_get_fd(reinterpret_cast<wl_display*>(c_ptr()));
}

int display_t::roundtrip()
{
  return wl_display_roundtrip(reinterpret_cast<wl_display*>(c_ptr()));
}

int display_t::roundtrip_queue(event_queue_t queue)
{
  return wl_display_roundtrip_queue(reinterpret_cast<wl_display*>(c_ptr()),
                                    queue.c_ptr());
}

int display_t::read_events()
{
  return wl_display_read_events(reinterpret_cast<wl_display*>(c_ptr()));
}    

int display_t::prepare_read()
{
  return wl_display_prepare_read(reinterpret_cast<wl_display*>(c_ptr()));
}    

int display_t::prepare_read_queue(event_queue_t queue)
{
  return wl_display_prepare_read_queue(reinterpret_cast<wl_display*>(c_ptr()), queue.c_ptr());
}    

void display_t::cancel_read()
{
  wl_display_cancel_read(reinterpret_cast<wl_display*>(c_ptr()));
}    

int display_t::dispatch_queue(event_queue_t queue)
{
  return wl_display_dispatch_queue(reinterpret_cast<wl_display*>(c_ptr()), queue.c_ptr());
}    

int display_t::dispatch_queue_pending(event_queue_t queue)
{
  return wl_display_dispatch_queue_pending(reinterpret_cast<wl_display*>(c_ptr()), queue.c_ptr());
}    

int display_t::dispatch()
{
  return wl_display_dispatch(reinterpret_cast<wl_display*>(c_ptr()));
}    

int display_t::dispatch_pending()
{
  return wl_display_dispatch_pending(reinterpret_cast<wl_display*>(c_ptr()));
}    

int display_t::get_error()
{
  return wl_display_get_error(reinterpret_cast<wl_display*>(c_ptr()));
}    

int display_t::flush()
{
  return wl_display_flush(reinterpret_cast<wl_display*>(c_ptr()));
}    

callback_t display_t::sync()
{
  return callback_t(marshal_constructor(0, &callback_interface, NULL));
}

registry_t display_t::get_registry()
{
  return registry_t(marshal_constructor(1, &registry_interface, NULL));
}
