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

/** \example dump.cpp
 * This is an example of how to use the Wayland C++ bindings for dumping information
 * about the running Wayland compositor.
 */

#include <iostream>

#include <wayland-client.hpp>

using namespace wayland;

// Wayland protocol info dumper
class dumper
{
private:
  // global objects
  display_t display;
  registry_t registry;

public:
  dumper()
  {
  }

  ~dumper()
  {
  }

  void run()
  {
    std::vector<output_t> outputs;
    // retrieve global objects
    registry = display.get_registry();
    registry.on_global() = [&] (uint32_t name, std::string interface, uint32_t version)
      {
        std::cout << "* Global interface " << interface << " (name " << name << " version " << version << ")" << std::endl;
        if(interface == output_t::interface_name)
        {
          outputs.emplace_back();
          auto& output = outputs.back();
          registry.bind(name, output, version);
          output.on_geometry() = [=](int32_t x, int32_t y, int32_t physw, int32_t physh, output_subpixel subp, std::string make, std::string model, output_transform transform)
          {
            std::cout << "* Output geometry for " << output.get_id() << ":" << std::endl
              << "   Maker:   " << make << std::endl
              << "   Model:   " << model << std::endl
              << "   X:       " << x << std::endl
              << "   Y:       " << y << std::endl
              << "   PhysW:   " << physw << " mm" << std::endl
              << "   PhysH:   " << physh << " mm" << std::endl
              << "   Subpix:  " << static_cast<unsigned int>(subp) << std::endl
              << "   Transf:  " << static_cast<unsigned int>(transform) << std::endl;
          };
          output.on_scale() = [=](int32_t scale)
          {
            std::cout << "* Output scale for " << output.get_id() << ": " << scale << std::endl;
          };
          output.on_mode() = [=](uint32_t flags, int32_t width, int32_t height, int32_t refresh)
          {
            std::cout << "* Output mode for " << output.get_id() << ":" << std::endl
              << "   Width:   " << width << std::endl
              << "   Height:  " << height << std::endl
              << "   Refresh: " << refresh << " mHz" << std::endl
              << "   Flags:   " << flags << std::endl;
          };
        }
      };
    // Print global information
    display.roundtrip();
    std::cout << "------" << std::endl;
    // Print output information
    display.roundtrip();
  }
};

int main()
{
  dumper d;
  d.run();
  return 0;
}
