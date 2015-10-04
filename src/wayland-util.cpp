/*
 * Copyright (c) 2015, Nils Christopher Brause
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

#include <wayland-client.hpp>
#include <wayland-util.hpp>

using namespace wayland;
using namespace wayland::detail;

argument_t::argument_t()
{
  argument.a = NULL;
  is_array = false;
}

argument_t::argument_t(const argument_t &arg)
{
  argument.a = NULL;
  is_array = false;
  operator=(arg);
}

argument_t &argument_t::operator=(const argument_t &arg)
{
  if(is_array)
    {
      wl_array_release(argument.a);
      delete argument.a;
    }

  if(arg.is_array)
    {
      argument.a = new wl_array;
      wl_array_init(argument.a);
      if(wl_array_copy(argument.a, arg.argument.a) < 0)
        throw std::runtime_error("wl_array_copy failed.");
    }
  else
    argument = arg.argument;

  is_array = arg.is_array;

  return *this;
}

argument_t::~argument_t()
{
  if(is_array)
    {
      wl_array_release(argument.a);
      delete argument.a;
    }
}

argument_t::argument_t(uint32_t i)
{
  argument.u = i;
  is_array = false;
}

argument_t::argument_t(std::string s)
{
  argument.s = s.c_str();
  is_array = false;
}

argument_t::argument_t(proxy_t p)
{
  argument.o = reinterpret_cast<wl_object*>(p.proxy);
  is_array = false;
}

