/*
 * Copyright (c) 2014-2019, Nils Christopher Brause, Philipp Kerling
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
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include <wayland-client-core.h>

#define wl_array_for_each_cpp(pos, array)                                                         \
  for ((pos) = static_cast<decltype(pos)>((array)->data);                                         \
       static_cast<const char*>(pos) < (static_cast<const char*>((array)->data) + (array)->size); \
       (pos)++)

namespace wayland
{
  class proxy_t;

  class array_t;

  namespace detail
  {
    /** \brief Check the return value of a C function and throw exception on
     *         failure
     *
     * \param return_value return value of the function to check
     * \param function_name name of the function, for error message
     * \return return_value if it was >= 0
     * \exception std::system_error with errno if return_value < 0
     */
    int check_return_value(int return_value, std::string const &function_name);

    /** \brief Non-refcounted wrapper for C objects
     *
     * This is by default copyable. If this is not desired, delete the
     * copy constructor and copy assignment operator in derived classes.
     */
    template<typename native_t>
    class basic_wrapper
    {
    private:
      native_t *object = nullptr;

    protected:
      basic_wrapper(native_t *object)
      : object{object}
      {
      }

    public:
      basic_wrapper()
      {
      }

      basic_wrapper(basic_wrapper const &other)
      {
        *this = other;
      }

      basic_wrapper(basic_wrapper &&other) noexcept
      {
        *this = std::move(other);
      }

      native_t *c_ptr() const
      {
        if(!object)
          throw std::runtime_error("Tried to access empty object");
        return object;
      }

      bool has_object() const
      {
        return object;
      }

      operator bool() const
      {
        return has_object();
      }

      operator native_t*() const
      {
        return c_ptr();
      }

      basic_wrapper& operator=(const basic_wrapper &right)
      {
        // Check for self-assignment
        if(this == &right)
          return *this;
        object = right.object;
        return *this;
      }

      basic_wrapper& operator=(basic_wrapper &&right) noexcept
      {
        std::swap(object, right.object);
        return *this;
      }

      bool operator==(const basic_wrapper &right) const
      {
        return object == right.object;
      }

      bool operator!=(const basic_wrapper &right) const
      {
        return !(*this == right); // Reuse equals operator
      }
    };

    /** \brief Refcounted wrapper for C objects
     *
     * This is by default copyable. If this is not desired, delete the
     * copy constructor and copy assignment operator in derived classes.
     */
    template<typename native_t>
    class refcounted_wrapper
    {
    private:
      std::shared_ptr<native_t> object;

    protected:
      refcounted_wrapper(std::shared_ptr<native_t> const &object)
      : object{object}
      {
      }

      std::shared_ptr<native_t> ref_ptr() const
      {
        return object;
      }

    public:
      refcounted_wrapper()
      {
      }

      refcounted_wrapper(refcounted_wrapper const &other)
      {
        *this = other;
      }

      refcounted_wrapper(refcounted_wrapper &&other) noexcept
      {
        *this = std::move(other);
      }

      native_t *c_ptr() const
      {
        if(!object)
          throw std::runtime_error("Tried to access empty object");
        return object.get();
      }

      bool has_object() const
      {
        return !!object;
      }

      operator bool() const
      {
        return has_object();
      }

      operator native_t*() const
      {
        return c_ptr();
      }

      refcounted_wrapper& operator=(const refcounted_wrapper &right)
      {
        // Check for self-assignment
        if(this == &right)
          return *this;
        object = right.object;
        return *this;
      }

      refcounted_wrapper& operator=(refcounted_wrapper &&right) noexcept
      {
        std::swap(object, right.object);
        return *this;
      }

      bool operator==(const refcounted_wrapper &right) const
      {
        return object == right.object;
      }

      bool operator!=(const refcounted_wrapper &right) const
      {
        return !(*this == right); // Reuse equals operator
      }
    };

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

        const std::type_info &type_info() const override
        {
          return typeid(T);
        }

        base *clone() const override
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
      bool is_array{false};
      // Uninitialized argument - only for internal use
      argument_t();

    public:
      wl_argument argument;

      argument_t(const argument_t &arg);
      argument_t &operator=(const argument_t &arg);
      ~argument_t();

      // handles integers
      argument_t(uint32_t i);
      argument_t(int32_t i);

      // handles wl_fixed_t
      argument_t(double f);

      // handles strings
      argument_t(const std::string &s);

      // handles objects
      argument_t(wl_object *o);

      // handles arrays
      argument_t(array_t a);

      // handles null objects, for example for new-id arguments
      argument_t(std::nullptr_t);

      // handles file descriptors (have same type as signed integers, so extra function)
      static argument_t fd(int fileno);
    };
  }

  class array_t
  {
  private:
    wl_array a;

    array_t(wl_array *arr);
    void get(wl_array *arr) const;

    friend class proxy_t;
    friend class detail::argument_t;

  public:
    array_t();
    array_t(const array_t &arr);
    array_t(array_t &&arr);

    template <typename T> array_t(const std::vector<T> &v)
    {
      wl_array_init(&a);
      wl_array_add(&a, v.size()*sizeof(T));
      T *p;
      unsigned int c = 0;
      wl_array_for_each_cpp(p, &a)
        *p = v.at(c++);
    }

    ~array_t();
    array_t &operator=(const array_t &arr);
    array_t &operator=(array_t &&arr);

    template <typename T> array_t &operator=(const std::vector<T> &v)
    {
      wl_array_release(&a);
      wl_array_init(&a);
      wl_array_add(&a, v.size()*sizeof(T));
      T *p;
      unsigned int c = 0;
      wl_array_for_each_cpp(p, &a)
        *p = v.at(c++);
      return *this;
    }

    template <typename T> operator std::vector<T>() const
    {
      std::vector<T> v;
      T *p;
      wl_array_for_each_cpp(p, &a)
        v.push_back(*p);
      return v;
    }
  };
}

#endif
