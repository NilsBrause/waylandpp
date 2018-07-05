/*
 * Copyright (c) 2018, Philipp Kerling
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

/** \example foreign_display.cpp
 * This is an example of how to use the Wayland C++ bindings with a wl_display.
 */

#include <iostream>
#include <memory>

#include <wayland-client.hpp>

using namespace wayland;

class foreign_display
{
private:
  // global objects
  wl_display* c_display = nullptr;
  std::unique_ptr<display_t> display;
  registry_t registry;

public:
  foreign_display()
  {
  }

  ~foreign_display()
  {
    // wl_display_disconnect destroys all remaining proxy instances implicitly,
    // so we have to make sure this is already gone in order to not run into a
    // double-free
    registry.proxy_release();
    // This is not called by the display_t destructor for foreign instances!
    wl_display_disconnect(c_display);
  }

  void run()
  {
    c_display = wl_display_connect(nullptr);
    if(!c_display)
      throw std::runtime_error("Cannot connect to Wayland display");
    display.reset(new display_t(c_display));
    registry = display->get_registry();
    registry.on_global() = [&] (uint32_t name, std::string interface, uint32_t version)
      {
        std::cout << "* Global interface " << interface << " (name " << name << " version " << version << ")" << std::endl;
      };
    display->roundtrip();
  }
};

int main()
{
  foreign_display d;
  d.run();
  return 0;
}
