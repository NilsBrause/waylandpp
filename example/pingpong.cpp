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

#include <iostream>
#include <thread>

#include <wayland-client.hpp>
#include <wayland-server.hpp>
#include <pingpong-server-protocol.hpp>
#include <pingpong-client-protocol.hpp>

int main()
{
  wayland::server::display_t server_display;
  wayland::server::global_pingpong_t global_pingpong(server_display);
  wayland::server::global_dummy_t dummy(server_display);
  wayland::server::pingpong_t server_pingpong;

  // create wayland UNIX socket.
  server_display.add_socket("pingpong");

  // Answer ping requests.
  global_pingpong.on_bind() = [&] (const wayland::server::client_t& /*client*/, wayland::server::pingpong_t pingpong)
  {
    // Don't copy the "pingpog" resource into the event handler as this creates cyclic references.
    server_pingpong = pingpong;
    pingpong.on_ping() = [&] (const std::string& msg)
    {
      std::cout << "Server received: " << msg << std::endl;
      server_pingpong.pong(msg);
    };
  };

  // Don't show anyone the dummy global.
  server_display.set_global_filter([] (const wayland::server::client_t& /*client*/, wayland::server::global_base_t global) { return !global.has_interface<wayland::server::dummy_t>(); });

  // Run server event loop in a thread.
  bool running = true;
  auto thread = std::thread([&] ()
  {
    auto el = server_display.get_event_loop();
    while(running)
    {
      el.dispatch(1);
      server_display.flush_clients();
    }
  });


  // Connect to server.
  wayland::display_t display("pingpong");

  // Bind to pingpong global.
  wayland::pingpong_t pingpong;
  auto registry = display.get_registry();
  registry.on_global() = [&] (uint32_t name, const std::string& interface, uint32_t version)
  {
    std::cout << "Found global: " << interface << std::endl;
    if(interface == wayland::pingpong_t::interface_name)
      registry.bind(name, pingpong, std::min(wayland::pingpong_t::interface_version, version));
  };
  display.roundtrip();

  // Print pong answers.
  pingpong.on_pong() = [&] (const std::string& msg)
  {
    std::cout << "Client received: " << msg << std::endl;
    running = false;
  };

  // Send ping request.
  pingpong.ping("Hello World!");
  display.roundtrip();

  thread.join();
  return 0;
}
