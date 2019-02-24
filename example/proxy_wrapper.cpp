/*
 * Copyright (c) 2017-2019, Philipp Kerling
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

/** \example proxy_wrapper.cpp
 * This is an example of how to use the Wayland C++ bindings with multiple threads
 * binding to globals on one shared connection using proxy wrappers.
 *
 * It can run in two modes: safe or unsafe mode. In safe mode, proxy wrappers
 * are correctly used. In unsafe mode, proxy wrappers are not used, and a
 * race occurs that will lead to failures when ran often enough.
 */

#include <iostream>
#include <thread>
#include <unistd.h>

#include <wayland-client.hpp>

using namespace wayland;

class binder
{
private:
  display_t display;

  void bind(bool safe)
  {
    registry_t registry;
    seat_t seat;

    auto queue = display.create_queue();
    if(safe)
    {
      auto display_wrapper = display.proxy_create_wrapper();
      display_wrapper.set_queue(queue);
      registry = display_wrapper.get_registry();
    }
    else
    {
      registry = display.get_registry();
      // This is the race: registry will be set to the default queue for a very
      // short while between get_registry() and set_queue() - events dispatched
      // in between get stuck in the default queue and ultimately lost.
      registry.set_queue(queue);
    }

    registry.on_global() = [&seat, &registry](std::uint32_t name, std::string interface, std::uint32_t version)
    {
      if(interface == seat_t::interface_name)
        registry.bind(name, seat, version);
    };
    display.roundtrip_queue(queue);
    if(!seat)
    {
      throw std::runtime_error("Did NOT get seat interface - thread-safety issue!");
    }
    else
    {
      // Now pretend that again another part of the application wants to use the
      // seat and get the keyboard from it
      // Note that it would not be necessary to do this in this example, but
      // this code is useful for testing proxy wrappers with normal interface
      // objects (display_t is special)
      auto queue2 = display.create_queue();
      if(safe)
      {
        seat = seat.proxy_create_wrapper();
      }
      seat.set_queue(queue2);
      keyboard_t kbd = seat.get_keyboard();
      bool have_keymap = false;
      kbd.on_keymap() = [&have_keymap](keyboard_keymap_format format, int fd, std::uint32_t size)
      {
        close(fd);
        have_keymap = true;
      };
      display.roundtrip_queue(queue2);
      if(!have_keymap)
      {
        throw std::runtime_error("Did NOT get keymap - thread-safety issue!");
      }
    }
  }

  std::thread bind_thread(bool safe)
  {
    return std::thread{std::bind(&binder::bind, this, safe)};
  }

public:
  void run(int thread_count, int round_count, bool safe)
  {
    std::atomic<bool> stop{false};
    std::cout << "Using " << thread_count << " threads, safe: " << safe << std::endl;
    for(int round = 0; round < round_count; round++)
    {
      if(round % 100 == 0)
      {
        std::cout << "Round " << round << "/" << round_count << std::endl;
      }
      std::vector<std::thread> threads;
      for(int i = 0; i < thread_count; i++)
      {
        threads.emplace_back(bind_thread(safe));
      }
      for(auto& thread : threads)
      {
        thread.join();
      }
    }
    stop = true;
  }
};

int main(int argc, char** argv)
{
  if(argc != 4)
  {
    std::cerr << "Usage: " << argv[0] << " <thread count> <run count> <use safe mechanism?>" << std::endl;
    return -1;
  }
  binder b;
  b.run(std::stoi(argv[1]), std::stoi(argv[2]), std::stoi(argv[3]));
  return 0;
}
