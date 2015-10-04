/*
 * Copyright (c) 2014-2015, Nils Christopher Brause
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

#ifndef WAYLAND_UTIL_HPP
#define WAYLAND_UTIL_HPP

#include <algorithm>
#include <typeinfo>
#include <utility>

namespace wayland
{
  class proxy_t;

  namespace detail
  {
    class any
    {
    private:
      class base
      {
      public:
        virtual ~base() { }
        virtual const std::type_info &type_info() const = 0;
        virtual base *clone() const = 0;
      };

      template <typename T>
      class derived : public base
      {
      private:
        T val;
        friend class any;

      public:
        derived(const T &t)
          : val(t) { }

        virtual const std::type_info &type_info() const override
        {
          return typeid(T);
        }

        virtual base *clone() const override
        {
          return new derived<T>(val);
        }
      };

      base *val;

    public:
      any()
        : val(nullptr) { }

      any(const any &a)
        : val(a.val ? a.val->clone() : nullptr) { }

      template <typename T>
      any(const T &t)
        : val(new derived<T>(t)) { }

      ~any()
      {
        delete val;
      }

      any &operator=(const any &a)
      {
        delete val;
        val = a.val ? a.val->clone() : nullptr;
        return *this;
      }

      template <typename T>
      any &operator=(const T &t)
      {
        if(val && typeid(T) == val->type_info())
          static_cast<derived<T>*>(val)->val = t;
        else
          {
            delete val;
            val = new derived<T>(t);
          }
        return *this;
      }

      template <typename T>
      T &get()
      {
        if(val && typeid(T) == val->type_info())
          return static_cast<derived<T>*>(val)->val;
        else
          throw std::bad_cast();
      }
    };

    template<unsigned int size, int id = 0>
    class bitfield
    {
      uint32_t v;
      static const uint32_t mask = (1 << size) - 1;

    public:
      explicit bitfield(const uint32_t value = 0)
        : v(value)
      {
      }

      explicit operator uint32_t() const
      {
        return v;
      }

      operator bool() const
      {
        return v;
      }

      bitfield(const bitfield<size, id> &b)
      {
        operator=(b);
      }

      bool operator==(const bitfield<size, id> &b)
      {
        return v == b.v;
      }

      bool operator!=(const bitfield<size, id> &b)
      {
        return !operator==(b);
      }

      bitfield<size, id> &operator=(const bitfield<size, id> &b)
      {
        v = static_cast<uint32_t>(b);
        return *this;
      }

      bitfield<size, id> operator|(const bitfield<size, id> &b) const
      {
        return bitfield<size, id>(v | static_cast<uint32_t>(b));
      }

      bitfield<size, id> operator&(const bitfield<size, id> &b) const
      {
        return bitfield<size, id>(v & static_cast<uint32_t>(b));
      }

      bitfield<size, id> operator^(const bitfield<size, id> &b) const
      {
        return bitfield<size, id>((v ^ static_cast<uint32_t>(b)) & mask);
      }

      bitfield<size, id> operator~() const
      {
        return bitfield<size, id>(~v & mask);
      }

      bitfield<size, id> &operator|=(const bitfield<size, id> &b)
      {
        operator=(*this | b);
        return *this;
      }

      bitfield<size, id> &operator&=(const bitfield<size, id> &b)
      {
        operator=(*this & b);
        return *this;
      }

      bitfield<size, id> &operator^=(const bitfield<size, id> &b)
      {
        operator=(*this ^ b);
        return *this;
      }
    };

    class argument_t
    {
    private:
      bool is_array;

    public:
      wl_argument argument;

      argument_t();
      argument_t(const argument_t &arg);
      argument_t &operator=(const argument_t &arg);
      ~argument_t();

      // handles integers, file descriptors and fixed point numbers
      // (this works, because wl_argument is an union)
      argument_t(uint32_t i);

      // handles strings
      argument_t(std::string s);

      // handles objects
      argument_t(proxy_t p);

      // handles arrays
      template <typename T>
      argument_t(std::vector<T> a)
      {
        argument.a = new wl_array;
        wl_array_init(argument.a);
        if(!wl_array_add(argument.a, a.size()*sizeof(T)))
          throw std::runtime_error("wl_array_add failed.");
        std::copy_n(a.begin(), a.end(), static_cast<T*>(argument.a->data));
        is_array = true;
      }
    };
  }
}

#endif
