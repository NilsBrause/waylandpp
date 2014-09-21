/*
 * Copyright (c) 2014, Nils Christopher Brause
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

#include <typeinfo>
#include <utility>

namespace wayland
{
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
        : val(a.val->clone()) { }

      template <typename T>
      any(const T &t)
        : val(new derived<T>(t)) { }

      ~any()
      {
        delete val;
      }

      any &operator=(const any &a)
      {
        if(val) delete val;
        val = a.val->clone();
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
  }
}

#endif
