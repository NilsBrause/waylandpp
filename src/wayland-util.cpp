/*
 * Copyright (c) 2015-2017, Nils Christopher Brause
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

#include <wayland-util.hpp>

#include <cerrno>
#include <limits>
#include <system_error>

using namespace wayland;
using namespace wayland::detail;

namespace wayland
{
  namespace detail
  {
    int check_return_value(int return_value, const std::string &function_name)
    {
      if(return_value < 0)
      {
        throw std::system_error(errno, std::generic_category(), function_name);
      }
      else
      {
        return return_value;
      }
    }
  }
}

argument_t::argument_t(const argument_t &arg)
{
  operator=(arg);
}

argument_t &argument_t::operator=(const argument_t &arg)
{
  if(is_array)
    {
      wl_array_release(argument.a);
      delete argument.a;
    }

  is_array = arg.is_array;

  if(arg.is_array)
    {
      argument.a = new wl_array;
      wl_array_init(argument.a);
      if(wl_array_copy(argument.a, arg.argument.a) < 0)
        throw std::runtime_error("wl_array_copy failed.");
    }
  else
    argument = arg.argument;

  return *this;
}

argument_t::argument_t()
{
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
}

argument_t::argument_t(int32_t i)
{
  argument.i = i;
}

argument_t::argument_t(double f)
{
  argument.f = wl_fixed_from_double(f);
}

argument_t::argument_t(const std::string &s)
{
  argument.s = s.c_str();
}

argument_t::argument_t(wl_object *o)
{
  argument.o = o;
  is_array = false;
}

argument_t::argument_t(std::nullptr_t)
{
  argument.n = 0;
}

argument_t::argument_t(array_t a)
{
  argument.a = new wl_array;
  a.get(argument.a);
  is_array = true;
}

argument_t argument_t::fd(int fileno)
{
  if(fileno > std::numeric_limits<int32_t>::max())
    throw std::invalid_argument("FD number too big");
  argument_t arg;
  arg.argument.h = static_cast<int32_t> (fileno);
  return arg;
}

array_t::array_t(wl_array *arr)
{
  wl_array_init(&a);
  wl_array_copy(&a, arr);
}

void array_t::get(wl_array *arr) const
{
  wl_array_init(arr);
  wl_array_copy(arr, const_cast<wl_array*>(&a));
}

array_t::array_t()
{
  wl_array_init(&a);
}

array_t::array_t(const array_t &arr)
{
  wl_array_init(&a);
  wl_array_copy(&a, const_cast<wl_array*>(&arr.a));
}

array_t::array_t(array_t &&arr)
{
  wl_array_init(&a);
  std::swap(a, arr.a);
}

array_t::~array_t()
{
  wl_array_release(&a);
}

array_t &array_t::operator=(const array_t &arr)
{
  wl_array_release(&a);
  wl_array_init(&a);
  wl_array_copy(&a, const_cast<wl_array*>(&arr.a));
  return *this;
}

array_t &array_t::operator=(array_t &&arr)
{
  std::swap(a, arr.a);
  return *this;
}
