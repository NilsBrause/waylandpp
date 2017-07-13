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
#include <cstdarg>
#include <cstdio>
#include <cerrno>

#include <iostream>
#include <limits>
#include <system_error>
#include <wayland-client.hpp>
#include <wayland-client-protocol.hpp>

using namespace wayland;
using namespace wayland::detail;

namespace
{

log_handler g_log_handler;

extern "C"
void _c_log_handler(const char *format, va_list args)
{
  if(!g_log_handler)
    return;
  
  // Format string
  va_list args_copy;

  // vsnprintf consumes args, so copy beforehand
  va_copy(args_copy, args);
  int length = std::vsnprintf(nullptr, 0, format, args);
  if(length < 0)
    throw std::runtime_error("Error getting length of formatted wayland-client log message");

  // check for possible overflow - could be done at runtime but the following should hold on all usual platforms
  static_assert(std::numeric_limits<std::vector<char>::size_type>::max() >= std::numeric_limits<int>::max() + 1u /* NUL */, "vector constructor must allow size big enough for vsnprintf return value");

  // for terminating NUL
  length++;

  std::vector<char> buf(static_cast<std::vector<char>::size_type>(length));
  if(std::vsnprintf(buf.data(), buf.size(), format, args_copy) < 0)
    throw std::runtime_error("Error formatting wayland-client log message");
  
  g_log_handler(buf.data());
}

}

void wayland::set_log_handler(log_handler handler)
{
  g_log_handler = handler;
  wl_log_set_handler_client(_c_log_handler);
}

event_queue_t::event_queue_t(wl_event_queue *q)
  : detail::refcounted_wrapper<wl_event_queue>({q, wl_event_queue_destroy})
{
}

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
          a = args[c].i;
          break;
          // uint32_t
        case 'u':
          a = args[c].u;
          break;
          // fd
        case 'h':
          a = args[c].h;
          break;
          // fixed
        case 'f':
          a = wl_fixed_to_double(args[c].f);
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
  typedef int(*dispatcher_func)(std::uint32_t, std::vector<any>, std::shared_ptr<proxy_t::events_base_t>);
  dispatcher_func dispatcher = reinterpret_cast<dispatcher_func>(const_cast<void*>(implementation));
  return dispatcher(opcode, vargs, p.get_events());
}

proxy_t proxy_t::marshal_single(uint32_t opcode, const wl_interface *interface, std::vector<argument_t> args, std::uint32_t version)
{
  std::vector<wl_argument> v;
  for(auto &arg : args)
    v.push_back(arg.argument);
  if(interface)
    {
      wl_proxy *p;
      if(version > 0)
        p = wl_proxy_marshal_array_constructor_versioned(proxy, opcode, v.data(), interface, version);
      else
        p = wl_proxy_marshal_array_constructor(proxy, opcode, v.data(), interface);

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
                         int(*dispatcher)(uint32_t, std::vector<any>, std::shared_ptr<proxy_t::events_base_t>))
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
{
}

proxy_t::proxy_t(wl_proxy *p, bool is_display, bool donotdestroy)
  : proxy(p), display(is_display), dontdestroy(donotdestroy)
{
  if(!display)
    {
      data = reinterpret_cast<proxy_data_t*>(wl_proxy_get_user_data(c_ptr()));
      if(!data)
        {
          data = new proxy_data_t{std::shared_ptr<events_base_t>(), -1, {0}};
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
      if(--data->counter == 0)
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


uint32_t proxy_t::get_id() const
{
  return wl_proxy_get_id(c_ptr());
}

std::string proxy_t::get_class() const
{
  return wl_proxy_get_class(c_ptr());
}

uint32_t proxy_t::get_version() const
{
  return wl_proxy_get_version(c_ptr());
}

void proxy_t::set_queue(event_queue_t queue)
{
  wl_proxy_set_queue(c_ptr(), queue.c_ptr());
}

wl_proxy *proxy_t::c_ptr() const
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

bool proxy_t::operator==(const proxy_t &right) const
{
  return proxy == right.proxy;
}

bool proxy_t::operator!=(const proxy_t &right) const
{
  return !(*this == right); // Reuse equals operator
}

read_intent::read_intent(wl_display *display, wl_event_queue *event_queue)
: display(display), event_queue(event_queue)
{
  assert(display);
}

read_intent::~read_intent()
{
  if(!finalized)
    cancel();
}

bool read_intent::is_finalized() const
{
  return finalized;
}

void read_intent::cancel()
{
  if(finalized)
    throw std::logic_error("Trying to cancel read_intent that was already finalized");
  wl_display_cancel_read(display);
  finalized = true;
}

void read_intent::read()
{
  if(finalized)
    throw std::logic_error("Trying to read with read_intent that was already finalized");
  if(wl_display_read_events(display) != 0)
    throw std::system_error(errno, std::generic_category(), "wl_display_read_events");
  finalized = true;
}


display_t::display_t(int fd)
  : proxy_t(reinterpret_cast<wl_proxy*>(wl_display_connect_to_fd(fd)), true)
{
  if(!proxy_has_object())
    throw std::runtime_error("Could not connect to Wayland display server via file-descriptor");
  interface = &display_interface;
}

display_t::display_t(std::string name)
  : proxy_t(reinterpret_cast<wl_proxy*>(wl_display_connect(name == "" ? NULL : name.c_str())), true)
{
  if(!proxy_has_object())
    throw std::runtime_error("Could not connect to Wayland display server via name");
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
  wl_display_disconnect(*this);
}

event_queue_t display_t::create_queue()
{
  wl_event_queue *queue = wl_display_create_queue(*this);
  if(!queue)
    throw std::runtime_error("wl_display_create_queue");
  return queue;
}

int display_t::get_fd()
{
  return wl_display_get_fd(*this);
}

int display_t::roundtrip()
{
  return check_return_value(wl_display_roundtrip(*this), "wl_display_roundtrip");
}

int display_t::roundtrip_queue(event_queue_t queue)
{
  return check_return_value(wl_display_roundtrip_queue(*this, queue), "wl_display_roundtrip_queue");
}

read_intent display_t::obtain_read_intent()
{
  while (wl_display_prepare_read(*this) != 0)
  {
    if(errno != EAGAIN)
      throw std::system_error(errno, std::generic_category(), "wl_display_prepare_read");
    
    dispatch_pending();
  }
  return read_intent(*this);
}

read_intent display_t::obtain_queue_read_intent(event_queue_t queue)
{
  while (wl_display_prepare_read_queue(*this, queue) != 0)
  {
    if(errno != EAGAIN)
      throw std::system_error(errno, std::generic_category(), "wl_display_prepare_read_queue");
    
    dispatch_queue_pending(queue);
  }
  return read_intent(*this, queue);
}

int display_t::dispatch_queue(event_queue_t queue)
{
  return check_return_value(wl_display_dispatch_queue(*this, queue), "wl_display_dispatch_queue");
}    

int display_t::dispatch_queue_pending(event_queue_t queue)
{
  return check_return_value(wl_display_dispatch_queue_pending(*this, queue), "wl_display_dispatch_queue_pending");
}    

int display_t::dispatch()
{
  return check_return_value(wl_display_dispatch(*this), "wl_display_dispatch");
}    

int display_t::dispatch_pending()
{
  return check_return_value(wl_display_dispatch_pending(*this), "wl_display_dispatch_pending");
}    

int display_t::get_error() const
{
  return wl_display_get_error(*this);
}    

std::tuple<int, bool> display_t::flush()
{
  int bytes_written = wl_display_flush(*this);
  if(bytes_written < 0)
  {
    if(errno == EAGAIN)
    {
      return std::make_tuple(bytes_written, false);
    }
    else
    {
      throw std::system_error(errno, std::generic_category(), "wl_display_flush");
    }
  }
  else
  {
    return std::make_tuple(bytes_written, true);
  }
}    

callback_t display_t::sync()
{
  return callback_t(marshal_constructor(0, &callback_interface, nullptr));
}

registry_t display_t::get_registry()
{
  return registry_t(marshal_constructor(1, &registry_interface, nullptr));
}

display_t::operator wl_display*() const
{
  return reinterpret_cast<wl_display*>(c_ptr());
}
