/*
 * Copyright (c) 2022, Nils Christopher Brause
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

#include <stdexcept>
#include <iostream>
#include <limits>
#include <wayland-server-core.h>
#include <wayland-server.hpp>

using namespace wayland::server;
using namespace wayland::server::detail;
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
    static_assert(std::numeric_limits<std::vector<char>::size_type>::max() >= std::numeric_limits<int>::max() + 1U /* NUL */, "vector constructor must allow size big enough for vsnprintf return value");

    // for terminating NUL
    length++;

    std::vector<char> buf(static_cast<std::vector<char>::size_type>(length));
    if(std::vsnprintf(buf.data(), buf.size(), format, args_copy) < 0)
      throw std::runtime_error("Error formatting wayland-client log message");

    va_end(args_copy);

    g_log_handler(buf.data());
  }

}

void wayland::server::set_log_handler(const log_handler& handler)
{
  g_log_handler = handler;
  wl_log_set_handler_server(_c_log_handler);
}

//-----------------------------------------------------------------------------

display_t::data_t *display_t::wl_display_get_user_data(wl_display *display)
{
  wl_listener *listener = wl_display_get_destroy_listener(display, destroy_func);
  if(listener)
    return reinterpret_cast<data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  return nullptr;
}

void display_t::destroy_func(wl_listener *listener, void */*unused*/)
{
  auto *data = reinterpret_cast<display_t::data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  if(data->destroy)
    data->destroy();
}

void display_t::client_created_func(wl_listener *listener, void *cl)
{
  auto *data = reinterpret_cast<display_t::data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  client_t client(reinterpret_cast<wl_client*>(cl));
  if(data->client_created)
    data->client_created(client);
}

void display_t::init()
{
  data = new data_t;
  data->counter = 1;
  data->destroy_listener.user = data;
  data->client_created_listener.user = data;
  data->destroy_listener.listener.notify = destroy_func;
  data->client_created_listener.listener.notify = client_created_func;
  wl_display_add_destroy_listener(display, reinterpret_cast<wl_listener*>(&data->destroy_listener));
  wl_display_add_client_created_listener(display, reinterpret_cast<wl_listener*>(&data->client_created_listener));
}

void display_t::fini()
{
  data->counter--;
  if(data->counter == 0)
  {
    wl_display_destroy_clients(c_ptr());
    wl_display_destroy(c_ptr());
    delete data;
  }
}

display_t::display_t()
{
  display = wl_display_create();
  if(!display)
    throw std::runtime_error("Failed to create display.");
  init();
}

display_t::display_t(wl_display *c)
{
  display = c;
  data = wl_display_get_user_data(c_ptr());
  if(!data)
    init();
  else
    data->counter++;
}

display_t::~display_t()
{
  fini();
}

display_t::display_t(const display_t& d)
{
  display = d.display;
  data = d.data;
  data->counter++;
}

display_t::display_t(display_t&& d) noexcept
{
  operator=(std::move(d));
}

display_t &display_t::operator=(const display_t& d)
{
  if(&d == this)
    return *this;
  fini();
  display = d.display;
  data = d.data;
  data->counter++;
  return *this;
}

display_t &display_t::operator=(display_t&& d) noexcept
{
  std::swap(display, d.display);
  std::swap(data, d.data);
  return *this;
}

bool display_t::operator==(const display_t& d) const
{
  return c_ptr() == d.c_ptr();
}

wl_display *display_t::c_ptr() const
{
  if(!display)
    throw std::runtime_error("display is null.");
  return display;
}

wayland::detail::any &display_t::user_data()
{
  return data->user_data;
}

event_loop_t display_t::get_event_loop() const
{
  return wl_display_get_event_loop(c_ptr());
}

int display_t::add_socket(const std::string& name) const
{
  return wl_display_add_socket(c_ptr(), name.c_str());
}

std::string display_t::add_socket_auto() const
{
  return wl_display_add_socket_auto(c_ptr());
}

int display_t::add_socket_fd(int sock_fd) const
{
  return wl_display_add_socket_fd(c_ptr(), sock_fd);
}

void display_t::terminate() const
{
  wl_display_terminate(c_ptr());
}

void display_t::run() const
{
  wl_display_run(c_ptr());
}

void display_t::flush_clients() const
{
  wl_display_flush_clients(c_ptr());
}

uint32_t display_t::get_serial() const
{
  return wl_display_get_serial(c_ptr());
}

uint32_t display_t::next_serial() const
{
  return wl_display_next_serial(c_ptr());
}

std::function<void()> &display_t::on_destroy()
{
  return data->destroy;
}

std::function<void(client_t&)> &display_t::on_client_created()
{
  return data->client_created;
}

std::list<client_t> display_t::get_client_list() const
{
  std::list<client_t> clients;
  wl_client *client = nullptr;
  wl_list *list = wl_display_get_client_list(c_ptr());
  wl_client_for_each(client, list)
    clients.emplace_back(client_t(client));
  return clients;
}

bool display_t::c_filter_func(const wl_client *client, const wl_global *global, void *data)
{
  return static_cast<display_t::data_t*>(data)->filter_func(client_t(const_cast<wl_client*>(client)), global_base_t(const_cast<wl_global*>(global)));
}

void display_t::set_global_filter(const std::function<bool(client_t, global_base_t)>& filter)
{
  data->filter_func = filter;
  wl_display_set_global_filter(c_ptr(), c_filter_func, data);
}

void display_t::set_default_max_buffer_size(size_t max_buffer_size)
{
  wl_display_set_default_max_buffer_size(c_ptr(), max_buffer_size);
}

//-----------------------------------------------------------------------------

void client_t::destroy_func(wl_listener *listener, void */*unused*/)
{
  auto *data = reinterpret_cast<data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  if(data->destroy)
    data->destroy();
}

void client_t::destroy_late_func(wl_listener *listener, void */*unused*/)
{
  auto *data = reinterpret_cast<data_t*>(reinterpret_cast<listener_t*>(listener)->user);
    if(data->destroy_late)
      data->destroy_late();
}

void client_t::resource_created_func(wl_listener *listener, void *resource_ptr)
{
  auto *data = reinterpret_cast<data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  resource_t resource(static_cast<wl_resource*>(resource_ptr));
  if(data->resource_created)
    data->resource_created(resource);
}

void client_t::user_data_destroy_func(void *data)
{
  delete static_cast<data_t*>(data);
}

void client_t::init()
{
  data = new data_t;
  data->client = client;
  data->counter = 1;
  data->destroyed = false;
  data->destroy_listener.user = data;
  data->destroy_listener.listener.notify = destroy_func;
  data->destroy_late_listener.user = data;
  data->destroy_late_listener.listener.notify = destroy_late_func;
  data->resource_created_listener.user = data;
  data->resource_created_listener.listener.notify = resource_created_func;
  wl_client_set_user_data(client, data, user_data_destroy_func);
  wl_client_add_destroy_listener(client, reinterpret_cast<wl_listener*>(&data->destroy_listener));
  wl_client_add_destroy_late_listener(client, reinterpret_cast<wl_listener*>(&data->destroy_late_listener));
  wl_client_add_resource_created_listener(client, reinterpret_cast<wl_listener*>(&data->resource_created_listener));
}

client_t::client_t(display_t &d, int fd)
{
  client = wl_client_create(d.display, fd);
  init();
}

client_t::client_t(wl_client *c)
{
  client = c;
  data = static_cast<data_t*>(wl_client_get_user_data(c_ptr()));
  if(!data)
    init();
  else
    data->counter++;
}

client_t::~client_t()
{
}

client_t::client_t(const client_t &c)
{
  client = c.client;
  data = c.data;
  data->counter++;
}

client_t::client_t(client_t&& c) noexcept
{
  operator=(std::move(c));
}

client_t &client_t::operator=(const client_t& c)
{
  if(&c == this)
    return *this;
  client = c.client;
  data = c.data;
  data->counter++;
  return *this;
}

client_t &client_t::operator=(client_t&& c) noexcept
{
  std::swap(client, c.client);
  std::swap(data, c.data);
  return *this;
}

bool client_t::operator==(const client_t& c) const
{
  return c_ptr() == c.c_ptr();
}

wl_client *client_t::c_ptr() const
{
  if(!client)
    throw std::runtime_error("client is null.");
  return client;
}

wayland::detail::any &client_t::user_data()
{
  return data->user_data;
}

void client_t::flush() const
{
  wl_client_flush(c_ptr());
}

void client_t::get_credentials(pid_t &pid, uid_t &uid, gid_t &gid) const
{
  wl_client_get_credentials(c_ptr(), &pid, &uid, &gid);
}

int client_t::get_fd() const
{
  return wl_client_get_fd(c_ptr());
}

std::function<void()> &client_t::on_destroy()
{
  return data->destroy;
}

resource_t client_t::get_object(uint32_t id)
{
  wl_resource *resource = wl_client_get_object(client, id);
  if(resource)
    return resource_t(resource);
  return resource_t();
}

void client_t::post_no_memory() const
{
  wl_client_post_no_memory(c_ptr());
}

void client_t::post_implementation_error(const std::string &msg) const
{
  wl_client_post_implementation_error(c_ptr(), "%s", msg.c_str());
}

display_t client_t::get_display() const
{
  return wl_client_get_display(c_ptr());
}

enum wl_iterator_result client_t::resource_iterator(wl_resource *resource, void *data)
{
  reinterpret_cast<std::list<resource_t>*>(data)->push_back(resource_t(resource));
  return WL_ITERATOR_CONTINUE;
}

std::list<resource_t> client_t::get_resource_list() const
{
  std::list<resource_t> resources;
  wl_client_for_each_resource(c_ptr(), resource_iterator, &resources);
  return resources;
}

void client_t::set_max_buffer_size(size_t max_buffer_size)
{
  wl_client_set_max_buffer_size(client, max_buffer_size);
}

std::function<void()> &client_t::on_destroy_late()
{
  return data->destroy_late;
}

std::function<void(resource_t&)> &client_t::on_resource_created()
{
  return data->resource_created;
}

//-----------------------------------------------------------------------------

void resource_t::destroy_func(wl_listener *listener, void */*unused*/)
{
  auto *data = reinterpret_cast<resource_t::data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  if(data->destroy)
    data->destroy();
  reinterpret_cast<listener_t*>(listener)->user = nullptr;
  delete data;
}

int resource_t::dummy_dispatcher(int /*opcode*/, const std::vector<wayland::detail::any>& /*args*/, const std::shared_ptr<resource_t::events_base_t>& /*events*/)
{
  return 0;
}

void resource_t::init()
{
  data = new data_t;
  data->counter = 1;
  data->destroy_listener.user = data;
  data->destroy_listener.listener.notify = destroy_func;
  wl_resource_set_user_data(resource, data);
  wl_resource_add_destroy_listener(resource, reinterpret_cast<wl_listener*>(&data->destroy_listener));
  wl_resource_set_dispatcher(resource, c_dispatcher, reinterpret_cast<void*>(dummy_dispatcher), data, nullptr); // dummy dispatcher
}

resource_t::resource_t(const client_t& client, const wl_interface *interface, int version, uint32_t id)
{
  resource = wl_resource_create(client.c_ptr(), interface, version, id);
  init();
}

resource_t::resource_t(wl_resource *c)
{
  resource = c;
  data = static_cast<data_t*>(wl_resource_get_user_data(c_ptr()));
  if(!data)
    init();
  else
    data->counter++;
}

resource_t::~resource_t()
{
}

resource_t::resource_t(const resource_t &r)
{
  resource = r.resource;
  data = r.data;

  // If data is nullptr, this is a empty dummy resource created by the c_dispatcher.
  if(data)
    data->counter++;
}

resource_t::resource_t(resource_t &&r) noexcept
{
  operator=(std::move(r));
}

resource_t &resource_t::operator=(const resource_t& r)
{
  if(&r == this)
    return *this;
  resource = r.resource;

  // If data is nullptr, this is a empty dummy resource created by the c_dispatcher.
  if (r.data)
  {
      data = r.data;
      data->counter++;
  }
  return *this;
}

resource_t &resource_t::operator=(resource_t&& r) noexcept
{
  std::swap(resource, r.resource);
  std::swap(data, r.data);
  return *this;
}

bool resource_t::operator==(const resource_t& r) const
{
  return resource == r.resource;
}

resource_t::operator bool() const
{
  return resource != nullptr;
}

wl_resource *resource_t::c_ptr() const
{
  if(!resource)
    throw std::runtime_error("resource is null.");
  return resource;
}

bool resource_t::proxy_has_object() const
{
  return resource;
}

wayland::detail::any &resource_t::user_data()
{
  return data->user_data;
}

int resource_t::c_dispatcher(const void *implementation, void *target, uint32_t opcode, const wl_message *message, wl_argument *args)
{
  if(!implementation)
    throw std::invalid_argument("resource dispatcher: implementation is NULL.");
  if(!target)
    throw std::invalid_argument("resource dispatcher: target is NULL.");
  if(!message)
    throw std::invalid_argument("resource dispatcher: message is NULL.");
  if(!args)
    throw std::invalid_argument("resource dispatcher: args is NULL.");

  resource_t p(reinterpret_cast<wl_resource*>(target));
  client_t cl = p.get_client();

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
      // resource
    case 'o':
      if(args[c].o)
        a = resource_t(reinterpret_cast<wl_resource*>(args[c].o));
      else
        a = resource_t();
      break;
      // new id
    case 'n':
    {
      if(args[c].n)
        a = resource_t(cl, message->types[c], message->types[c]->version, args[c].n);
      else
        a = resource_t();
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

  using dispatcher_func = int(*)(int, std::vector<any>, std::shared_ptr<resource_t::events_base_t>);
  auto dispatcher = reinterpret_cast<dispatcher_func>(const_cast<void*>(implementation));
  return dispatcher(opcode, vargs, p.get_events());
}

void resource_t::set_events(const std::shared_ptr<events_base_t>& events,
                            int(*dispatcher)(int, const std::vector<any>&, const std::shared_ptr<resource_t::events_base_t>&))
{
  // set only one time
  if(!data->events)
  {
    data->events = events;
    // the dispatcher gets 'implemetation'
    wl_resource_set_dispatcher(c_ptr(), c_dispatcher, reinterpret_cast<void*>(dispatcher), data, nullptr);
  }
}

std::shared_ptr<resource_t::events_base_t> resource_t::get_events() const
{
  return data->events;
}

void resource_t::post_event_array(uint32_t opcode, const std::vector<argument_t>& v) const
{
  auto *args = new wl_argument[v.size()];
  for(unsigned int c = 0; c < v.size(); c++)
    args[c] = v[c].get_c_argument();
  wl_resource_post_event_array(c_ptr(), opcode, args);
  delete[] args;
}

void resource_t::queue_event_array(uint32_t opcode, const std::vector<argument_t>& v) const
{
  auto *args = new wl_argument[v.size()];
  for(unsigned int c = 0; c < v.size(); c++)
    args[c] = v[c].get_c_argument();
  wl_resource_queue_event_array(c_ptr(), opcode, args);
  delete[] args;
}

void resource_t::post_error(uint32_t code, const std::string& msg) const
{
  wl_resource_post_error(c_ptr(), code, "%s", msg.c_str());
}

void resource_t::post_no_memory() const
{
  wl_resource_post_no_memory(c_ptr());
}

uint32_t resource_t::get_id() const
{
  return wl_resource_get_id(c_ptr());
}

client_t resource_t::get_client() const
{
  return client_t(wl_resource_get_client(c_ptr()));
}

unsigned int resource_t::get_version() const
{
  return wl_resource_get_version(resource);
}

std::string resource_t::get_class()
{
  return wl_resource_get_class(resource);
}

std::function<void()> &resource_t::on_destroy()
{
  return data->destroy;
}

//-----------------------------------------------------------------------------

bool global_base_t::has_interface(const wl_interface *interface) const
{
  return interface == wl_global_get_interface(c_ptr());
}

global_base_t::global_base_t(display_t &display, const wl_interface* interface, int version, data_t *dat, wl_global_bind_func_t func)
{
  data = dat;
  data->counter = 1;
  global = wl_global_create(display.c_ptr(), interface, version, data, func);
}

void global_base_t::fini()
{
  if(data)
  {
    data->counter--;
    if(data->counter == 0)
    {
      wl_global_destroy(c_ptr());
      delete data;
    }
  }
}

global_base_t::~global_base_t()
{
  fini();
}

global_base_t::global_base_t(wl_global *g)
{
  global = g;
  data = static_cast<data_t*>(wl_global_get_user_data(c_ptr()));
  data->counter++;
}

global_base_t::global_base_t(const global_base_t& g)
{
  global = g.global;
  data = g.data;
  data->counter++;
}

global_base_t::global_base_t(global_base_t&& g) noexcept
{
  operator=(std::move(g));
}

global_base_t &global_base_t::operator=(const global_base_t& g)
{
  if(&g == this)
    return *this;
  fini();
  global = g.global;
  data = g.data;
  data->counter++;
  return *this;
}

global_base_t &global_base_t::operator=(global_base_t&& g) noexcept
{
  std::swap(global, g.global);
  std::swap(data, g.data);
  return *this;
}

bool global_base_t::operator==(const global_base_t& g) const
{
  return c_ptr() == g.c_ptr();
}

wl_global *global_base_t::c_ptr() const
{
  if(!global)
    throw std::runtime_error("global is null.");
  return global;
}

wayland::detail::any &global_base_t::user_data()
{
  return data->user_data;
}

//-----------------------------------------------------------------------------

const bitfield<2, -1> fd_event_mask_t::readable{WL_EVENT_READABLE};
const bitfield<2, -1> fd_event_mask_t::writable{WL_EVENT_WRITABLE};
const bitfield<2, -1> fd_event_mask_t::hangup{WL_EVENT_HANGUP};
const bitfield<2, -1> fd_event_mask_t::error{WL_EVENT_ERROR};

event_loop_t::data_t *event_loop_t::wl_event_loop_get_user_data(wl_event_loop *client)
{
  wl_listener *listener = wl_event_loop_get_destroy_listener(client, destroy_func);
  if(listener)
    return reinterpret_cast<data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  return nullptr;
}

void event_loop_t::destroy_func(wl_listener *listener, void */*unused*/)
{
  auto *data = reinterpret_cast<event_loop_t::data_t*>(reinterpret_cast<listener_t*>(listener)->user);
  if(data->destroy)
    data->destroy();
  delete data;
}

int event_loop_t::event_loop_fd_func(int fd, uint32_t mask, void *data)
{
  auto *f = reinterpret_cast<std::function<int(int, uint32_t)>*>(data);
  return (*f)(fd, mask);
}

int event_loop_t::event_loop_timer_func(void *data)
{
  auto *f = reinterpret_cast<std::function<int()>*>(data);
  return (*f)();
}

int event_loop_t::event_loop_signal_func(int signal_number, void *data)
{
  auto *f = reinterpret_cast<std::function<int(int)>*>(data);
  return (*f)(signal_number);
}

void event_loop_t::event_loop_idle_func(void *data)
{
  auto *f = reinterpret_cast<std::function<void()>*>(data);
  (*f)();
}

void event_loop_t::init()
{
  data = new data_t;
  data->counter = 1;
  data->destroy_listener.user = data;
  data->destroy_listener.listener.notify = destroy_func;
  wl_event_loop_add_destroy_listener(event_loop, reinterpret_cast<wl_listener*>(&data->destroy_listener));
}

void event_loop_t::fini()
{
  data->counter--;
  if(data->counter == 0)
  {
    if (data->do_delete)
      wl_event_loop_destroy(event_loop);
  }
}

event_loop_t::event_loop_t()
{
  event_loop = wl_event_loop_create();
  init();
}

event_loop_t::event_loop_t(wl_event_loop *p)
  : event_loop(p)
{
  data = wl_event_loop_get_user_data(c_ptr());
  if(!data)
  {
    // default event loop is managed by the display;
    init();
    data->do_delete = false;
  }
  else
    data->counter++;
}

event_loop_t::~event_loop_t()
{
  fini();
}

event_loop_t::event_loop_t(const event_loop_t& e)
{
  event_loop = e.event_loop;
  data = e.data;
  data->counter++;
}

event_loop_t::event_loop_t(event_loop_t&& e) noexcept
{
  operator=(std::move(e));
}

event_loop_t &event_loop_t::operator=(const event_loop_t& e)
{
  if(&e == this)
    return *this;
  fini();
  event_loop = e.event_loop;
  data = e.data;
  data->counter++;
  return *this;
}

event_loop_t &event_loop_t::operator=(event_loop_t&& e) noexcept
{
  std::swap(event_loop, e.event_loop);
  std::swap(data, e.data);
  return *this;
}

bool event_loop_t::operator==(const event_loop_t& e) const
{
  return c_ptr() == e.c_ptr();
}

wl_event_loop *event_loop_t::c_ptr() const
{
  if(!event_loop)
    throw std::runtime_error("event_loop is null.");
  return event_loop;
}

wayland::detail::any &event_loop_t::user_data()
{
  return data->user_data;
}

event_source_t event_loop_t::add_fd(int fd, const fd_event_mask_t& mask, const std::function<int(int, uint32_t)> &func)
{
  data->fd_funcs.push_back(func);
  return wl_event_loop_add_fd(event_loop, fd, mask, event_loop_t::event_loop_fd_func, &data->fd_funcs.back());
}

event_source_t event_loop_t::add_timer(const std::function<int()> &func)
{
  data->timer_funcs.push_back(func);
  return wl_event_loop_add_timer(event_loop, event_loop_t::event_loop_timer_func, &data->timer_funcs.back());
}

event_source_t event_loop_t::add_signal(int signal_number, const std::function<int(int)> &func)
{
  data->signal_funcs.push_back(func);
  return wl_event_loop_add_signal(event_loop, signal_number, event_loop_t::event_loop_signal_func, &data->signal_funcs.back());
}

event_source_t event_loop_t::add_idle(const std::function<void()> &func)
{
  data->idle_funcs.push_back(func);
  return wl_event_loop_add_idle(event_loop, event_loop_t::event_loop_idle_func, &data->idle_funcs.back());
}

const std::function<void()> &event_loop_t::on_destroy()
{
  return data->destroy;
}

int event_loop_t::dispatch(int timeout) const
{
  return wl_event_loop_dispatch(c_ptr(), timeout);
}

void event_loop_t::dispatch_idle() const
{
  wl_event_loop_dispatch_idle(c_ptr());
}

int event_loop_t::get_fd() const
{
  return wl_event_loop_get_fd(c_ptr());
}

//-----------------------------------------------------------------------------

event_source_t::event_source_t(wl_event_source *p)
  : wayland::detail::refcounted_wrapper<wl_event_source>({p, wl_event_source_remove})
{
}

wl_event_source *event_source_t::c_ptr() const
{
  if(!event_source)
    throw std::runtime_error("event_source is null.");
  return event_source;
}

int event_source_t::timer_update(int ms_delay) const
{
  return wl_event_source_timer_update(c_ptr(), ms_delay);
}

int event_source_t::fd_update(const fd_event_mask_t& mask) const
{
  return wl_event_source_fd_update(c_ptr(), mask);
}

void event_source_t::check() const
{
  wl_event_source_check(c_ptr());
}
