#ifndef ANY_HPP
#define ANY_HPP

#include <typeinfo>
#include <utility>

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

#endif
