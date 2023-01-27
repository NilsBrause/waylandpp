/*
 *  Copyright (c) 2014-2022 Nils Christopher Brause, Philipp Kerling, Bernd Kuhls
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdint>
#include <fstream>
#include <iostream>
#include <list>
#include <set>
#include <sstream>
#include <vector>
#include <cctype>
#include <cmath>
#include <stdexcept>

#include "pugixml.hpp"

using namespace pugi;

std::list<std::string> interface_names;

struct element_t
{
  std::string name;
  std::string summary;
  std::string description;
  static const std::set<std::string> keywords;

  static std::string sanitise(std::string str)
  {
    if(keywords.count(str))
      return "_" + str;
    return str;
  }
};

struct argument_t : public element_t
{
  std::string type;
  std::string interface;
  std::string enum_iface;
  std::string enum_name;
  bool allow_null = false;

  std::string print_enum_wire_type() const
  {
    // Enums can be int or uint on the wire, except for bitfields
    if(type == "int")
      return "int32_t";
    if(type == "uint")
      return "uint32_t";
    throw std::runtime_error("Enum type must be int or uint");
  }

  std::string print_type(bool server) const
  {
    if(!interface.empty())
      return interface + "_t";
    if(!enum_iface.empty())
      return enum_iface + "_" + enum_name;
    if(type == "int")
      return "int32_t";
    if(type == "uint")
      return "uint32_t";
    if(type == "fixed")
      return "double";
    if(type == "string")
      return "std::string";
    if(type == "object")
      return server ? "resource_t" : "proxy_t";
    if(type == "new_id")
      return server ? "resource_t" : "proxy_t";
    if(type == "fd")
      return "int";
    if(type == "array")
      return "array_t";
    return type;
  }

  std::string print_short() const
  {
    if(type == "int")
      return "i";
    if(type == "uint")
      return "u";
    if(type == "fixed")
      return "f";
    if(type == "string")
      return "s";
    if(type == "object")
      return "o";
    if(type == "new_id")
      return "n";
    if(type == "array")
      return "a";
    if(type == "fd")
      return "h";
    return "x";
  }

  std::string print_argument(bool server) const
  {
    return print_type(server) + (!interface.empty() || !enum_iface.empty() || type == "string" || type == "array" ? " const& " : " ") + sanitise(name);
  }
};

struct event_t : public element_t
{
  std::list<argument_t> args;
  int since = 0;
  argument_t ret;
  int opcode = 0;

  std::string print_functional(bool server) const
  {
    std::stringstream ss;
    ss << "    std::function<void(";
    for(auto const& arg : args)
      ss << arg.print_type(server) << ", ";
    if(!args.empty())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ")> " << sanitise(name) << ";";
    return ss.str();
  }

  std::string print_dispatcher(int opcode, bool server) const
  {
    std::stringstream ss;
    ss << "    case " << opcode << ":" << std::endl
       << "      if(events->" << sanitise(name) << ") events->" << sanitise(name) << "(";

    int c = 0;
    for(auto const& arg : args)
      if(!arg.enum_name.empty() && arg.type != "array")
        ss << arg.print_type(server) << "(args[" << c++ << "].get<" << arg.print_enum_wire_type() << ">()), ";
      else if(!arg.interface.empty())
      {
        if(server)
          ss << arg.print_type(server) << "(args[" << c++ << "].get<resource_t>()), ";
        else
          ss << arg.print_type(server) << "(args[" << c++ << "].get<proxy_t>()), ";
      }
      else
        ss << "args[" << c++ << "].get<" << arg.print_type(server) << ">(), ";
    if(!args.empty())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ");" << std::endl
       << "      break;";
    return ss.str();
  }

  std::string print_signal_header(bool server) const
  {
    std::stringstream ss;
    ss << "  /** \\brief " << summary << std::endl;
    for(auto const& arg : args)
      ss << "      \\param " << arg.name << " " << arg.summary << std::endl;
    ss << description << std::endl
       << "  */" << std::endl;

    ss << "  std::function<void(";
    for(auto const& arg : args)
      ss << arg.print_type(server) + ", ";
    if(!args.empty())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ")> &on_" <<  name << "();" << std::endl;
    return ss.str();
  }

  std::string print_signal_body(const std::string& interface_name, bool server) const
  {
    std::stringstream ss;
    ss << "std::function<void(";
    for(auto const& arg : args)
      ss << arg.print_type(server) << ", ";
    if(!args.empty())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ")> &" + interface_name + "_t::on_" + name + "()" << std::endl
       << "{" << std::endl
       << "  return std::static_pointer_cast<events_t>(get_events())->" + sanitise(name) + ";" << std::endl
       << "}" << std::endl;
    return ss.str();
  }

  std::string availability_function_name() const
  {
    if(since > 1)
      return std::string("can_") + name;
    return "";
  }

  std::string since_version_constant_name() const
  {
    return name + "_since_version";
  }

  std::string print_header(bool server) const
  {
    std::stringstream ss;
    ss << "  /** \\brief " << summary << std::endl;
    if(!ret.summary.empty())
      ss << "      \\return " << ret.summary << std::endl;
    for(auto const& arg : args)
    {
      if(arg.type == "new_id")
      {
        if(arg.interface.empty())
          ss << "      \\param interface Interface to bind" << std::endl
             << "      \\param version Interface version" << std::endl;
      }
      else
        ss << "      \\param " << sanitise(arg.name) << " " << arg.summary << std::endl;
    }
    ss << description << std::endl
       << "  */" << std::endl;

    if(ret.name.empty() || server)
      ss << "  void ";
    else
      ss << "  " << ret.print_type(server) << " ";
    ss << sanitise(name) << "(";

    for(auto const& arg : args)
      if(arg.type == "new_id")
      {
        if(arg.interface.empty())
          ss << "proxy_t &interface, uint32_t version, ";
      }
      else
        ss << arg.print_argument(server) << ", ";

    if(server)
      ss << "bool post = true";

    if(ss.str().substr(ss.str().size()-2, 2) == ", ")
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ");" << std::endl;

    ss << std::endl
       << "  /** \\brief Minimum protocol version required for the \\ref " << sanitise(name) << " function" << std::endl
       << "  */" << std::endl
       << "  static constexpr std::uint32_t " << since_version_constant_name() << " = " << since << ";" << std::endl;

    if(!availability_function_name().empty())
    {
      ss << std::endl
         << "  /** \\brief Check whether the \\ref " << name << " function is available with" << std::endl
         << "      the currently bound version of the protocol" << std::endl
         << "  */" << std::endl
         << "  bool " << availability_function_name() << "() const;" << std::endl;
    }

    return ss.str();
  }

  std::string print_body(const std::string& interface_name, bool server) const
  {
    std::stringstream ss;
    if(ret.name.empty() || server)
      ss <<  "void ";
    else
      ss << ret.print_type(server) << " ";
    ss << interface_name << "_t::" << sanitise(name) << "(";

    bool new_id_arg = false;
    for(auto const& arg : args)
      if(arg.type == "new_id")
      {
        if(arg.interface.empty())
        {
          ss << "proxy_t &interface, uint32_t version, ";
          new_id_arg = true;
        }
      }
      else
        ss << arg.print_argument(server) << ", ";

    if(server)
      ss << "bool post";

    if(ss.str().substr(ss.str().size()-2, 2) == ", ")
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ")" << std::endl
       << "{" << std::endl;

    if(server)
      ss <<  "  send_event(post, " << opcode << ", ";
    else if(ret.name.empty())
      ss <<  "  marshal(" << opcode << "U, ";
    else if(ret.interface.empty())
    {
      ss << "  proxy_t p = marshal_constructor_versioned(" << opcode << "U, interface.interface, version, ";
    }
    else
    {
      ss << "  proxy_t p = marshal_constructor(" << opcode << "U, &" << ret.interface << "_interface, ";
    }

    for(auto const& arg : args)
    {
      if(arg.type == "new_id")
      {
        if(arg.interface.empty())
          ss << "std::string(interface.interface->name), version, ";
        ss << "nullptr, ";
      }
      else if(arg.type == "fd")
        ss << "argument_t::fd(" << sanitise(arg.name) << "), ";
      else if(arg.type == "object")
        ss << sanitise(arg.name) << ".proxy_has_object() ? reinterpret_cast<wl_object*>(" << sanitise(arg.name) << ".c_ptr()) : nullptr, ";
      else if(!arg.enum_name.empty())
        ss << "static_cast<" << arg.print_enum_wire_type() << ">(" << sanitise(arg.name) + "), ";
      else
        ss << sanitise(arg.name) + ", ";
    }

    ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ");" << std::endl;

    if(!ret.name.empty() && !server)
    {
      if(new_id_arg)
      {
        ss << "  interface = interface.copy_constructor(p);" << std::endl
           << "  return interface;" << std::endl;
      }
      else
        ss << "  return " << ret.print_type(server) << "(p);" << std::endl;
    }
    ss << "}" << std::endl;

    if(!availability_function_name().empty())
    {
      ss << std::endl
         << "bool " << interface_name << "_t::" << availability_function_name() << "() const" << std::endl
         << "{" << std::endl
         << "  return (get_version() >= " << since_version_constant_name() << ");" << std::endl
         << "}" << std::endl;
    }

    return ss.str();
  }
};

struct request_t : public event_t
{
};

struct enum_entry_t : public element_t
{
  std::string value;
};

struct enumeration_t : public element_t
{
  std::list<enum_entry_t> entries;
  bool bitfield = false;
  int id = 0;
  uint32_t width = 0;

  std::string print_forward(const std::string& iface_name) const
  {
    std::stringstream ss;
    if(!bitfield)
      ss << "enum class " << iface_name << "_" << name << " : uint32_t;" << std::endl;
    else
      ss << "struct " << iface_name << "_" << name << ";" << std::endl;
    return ss.str();
  }

  std::string print_header(const std::string& iface_name) const
  {
    std::stringstream ss;
    ss << "/** \\brief " << summary << std::endl
       << description << std::endl
       << "  */" << std::endl;

    if(!bitfield)
      ss << "enum class " << iface_name << "_" << name << " : uint32_t" << std::endl
         << "  {" << std::endl;
    else
      ss << "struct " << iface_name << "_" << name << " : public wayland::detail::bitfield<" << width << ", " << id << ">" << std::endl
         << "{" << std::endl
         << "  " << iface_name << "_" << name << "(const wayland::detail::bitfield<" << width << ", " << id << "> &b)" << std::endl
         << "    : wayland::detail::bitfield<" << width << ", " << id << ">(b) {}" << std::endl
         << "  " << iface_name << "_" << name << "(const uint32_t value)" << std::endl
         << "    : wayland::detail::bitfield<" << width << ", " << id << ">(value) {}" << std::endl;

    for(auto const& entry : entries)
    {
      if(!entry.summary.empty())
        ss << "  /** \\brief " << entry.summary << " */" << std::endl;

      if(!bitfield)
        ss << "  " << sanitise(entry.name) << " = " << entry.value << "," << std::endl;
      else
        ss << "  static const wayland::detail::bitfield<" << width << ", " << id << "> " << sanitise(entry.name) << ";" << std::endl;
    }

    if(!bitfield)
    {
      ss.str(ss.str().substr(0, ss.str().size()-2));
      ss.seekp(0, std::ios_base::end);
      ss << std::endl;
    }

    ss << "};" << std::endl;
    return ss.str();
  }

  std::string print_body(const std::string& iface_name) const
  {
    std::stringstream ss;
    if(bitfield)
      for(auto const& entry : entries)
      {
        ss << "const bitfield<" << width << ", " << id << "> " << iface_name << "_" << name
           << "::" << sanitise(entry.name) << "{" << entry.value << "};" << std::endl;
      }
    return ss.str();
  }
};

struct post_error_t : public element_t
{
  std::string print_server_header() const
  {
    std::stringstream ss;
    ss << "  /** \\brief Post error: " << summary << std::endl;
    if(!description.empty())
      ss << "  " << description << std::endl;
    ss << "  */" << std::endl;
    ss << "  void post_" << name << "(std::string const& msg);" << std::endl;
    return ss.str();
  }

  std::string print_server_body(std::string iface_name) const
  {
    std::stringstream ss;
    ss << "void " << iface_name << "_t::post_" << name << "(std::string const& msg)" << std::endl
       << "{" << std::endl
       << "  post_error(static_cast<uint32_t>(" << iface_name << "_error::" << sanitise(name) << "), msg);" << std::endl
       << "}" << std::endl;
    return ss.str();
  }
};

struct interface_t : public element_t
{
  int version = 0;
  std::string orig_name;
  int destroy_opcode = 0;
  std::list<request_t> requests;
  std::list<event_t> events;
  std::list<enumeration_t> enums;
  std::list<post_error_t> errors;

  std::string print_forward() const
  {
    std::stringstream ss;
    ss << "class " << name << "_t;" << std::endl;
    for(auto const& e : enums)
      ss << e.print_forward(name);
    return ss.str();
  }

  std::string print_c_forward() const
  {
    std::stringstream ss;
    ss << "struct " << orig_name << ";" << std::endl;
    return ss.str();
  }

  std::string print_client_header() const
  {
    std::stringstream ss;
    ss << "/** \\brief " << summary << std::endl
       << description << std::endl
       << "*/" << std::endl;

    ss << "class " << name << "_t : public proxy_t" << std::endl
       << "{" << std::endl
       << "private:" << std::endl
       << "  struct events_t : public detail::events_base_t" << std::endl
       << "  {" << std::endl;

    for(auto const& event : events)
      ss << event.print_functional(false) << std::endl;

    ss << "  };" << std::endl
       << std::endl
       << "  static int dispatcher(uint32_t opcode, const std::vector<detail::any>& args, const std::shared_ptr<detail::events_base_t>& e);" << std::endl
       << std::endl
       << "  " << name << "_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/);" << std::endl
       << std::endl;

    ss << "public:" << std::endl
       << "  " << name << "_t();" << std::endl
       << "  explicit " << name << "_t(const proxy_t &proxy);" << std::endl
       << "  " << name << "_t(" << orig_name << " *p, wrapper_type t = wrapper_type::standard);" << std::endl
       << std::endl
       << "  " << name << "_t proxy_create_wrapper();" << std::endl
       << std::endl
       << "  static const std::string interface_name;" << std::endl
       << std::endl
       << "  operator " << orig_name << "*() const;" << std::endl
       << std::endl;

    for(auto const& request : requests)
      if(request.name != "destroy")
        ss << request.print_header(false) << std::endl;

    for(auto const& event : events)
      ss << event.print_signal_header(false) << std::endl;

    ss << "};" << std::endl
       << std::endl;

    for(auto const& enumeration : enums)
      ss << enumeration.print_header(name) << std::endl;

    return ss.str();
  }

  std::string print_server_header() const
  {
    std::stringstream ss;
    ss << "/** \\brief " << summary << std::endl
       << description << std::endl
       << "*/" << std::endl;

    ss << "class " << name << "_t : public resource_t" << std::endl
       << "{" << std::endl
       << "private:" << std::endl
       << "  struct events_t : public resource_t::events_base_t" << std::endl
       << "  {" << std::endl;

    for(auto const& request : requests)
      ss << request.print_functional(true) << std::endl;

    ss << "  };" << std::endl
       << std::endl
       << "  static int dispatcher(int opcode, const std::vector<wayland::detail::any>& args, const std::shared_ptr<resource_t::events_base_t>& e);" << std::endl
       << std::endl;

    ss << "protected:" << std::endl
       << "  static constexpr const wl_interface *interface = &wayland::server::detail::" << name << "_interface;" << std::endl
       << "  static constexpr const unsigned int max_version = " << version << ";" << std::endl
       << std::endl
       << "  friend class global_t<" << name << "_t>;" << std::endl
       << "  friend class global_base_t;" << std::endl
       << std::endl;

    ss << "public:" << std::endl
       << "  " << name << "_t() = default;" << std::endl
       << "  " << name << "_t(const client_t& client, uint32_t id, int version = " << version << ");" << std::endl
       << "  " << name << "_t(const resource_t &resource);" << std::endl
       << std::endl
       << "  static const std::string interface_name;" << std::endl
       << std::endl
       << "  operator " << orig_name << "*() const;" << std::endl
       << std::endl;

    for(auto const& request : requests)
      ss << request.print_signal_header(true) << std::endl;

    for(auto const& event : events)
      ss << event.print_header(true) << std::endl;

    for(auto const& error : errors)
      ss << error.print_server_header() << std::endl;

    ss << "};" << std::endl
       << std::endl
       << "using global_" << name << "_t = global_t<" << name << "_t>;" << std::endl
       << std::endl;

    for(auto const& enumeration : enums)
      ss << enumeration.print_header(name) << std::endl;

    return ss.str();
  }

  std::string print_interface_header() const
  {
    std::stringstream ss;
    ss << "  extern const wl_interface " << name << "_interface;" << std::endl;
    return ss.str();
  }

  std::string print_client_body() const
  {
    std::stringstream set_events;
    set_events << "  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)" << std::endl
               << "    {" << std::endl
               << "      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);" << std::endl;
    if(destroy_opcode != -1)
      set_events << "      set_destroy_opcode(" << destroy_opcode << "U);" << std::endl;
    set_events << "    }" << std::endl;

    std::stringstream set_interface;
    set_interface << "  set_interface(&" << name << "_interface);" << std::endl
                  << "  set_copy_constructor([] (const proxy_t &p) -> proxy_t" << std::endl
                  << "    { return " << name << "_t(p); });" << std::endl;

    std::stringstream ss;
    ss << name << "_t::" << name << "_t(const proxy_t &p)" << std::endl
       << "  : proxy_t(p)" << std::endl
       << "{" << std::endl
       << set_events.str()
       << set_interface.str()
       << "}" << std::endl
       << std::endl
       << name << "_t::" << name << "_t()" << std::endl
       << "{" << std::endl
       << set_interface.str()
       << "}" << std::endl
       << std::endl
       << name << "_t::" << name << "_t(" << orig_name << " *p, wrapper_type t)" << std::endl
       << "  : proxy_t(reinterpret_cast<wl_proxy*> (p), t)"
       << "{" << std::endl
       << set_events.str()
       << set_interface.str()
       << "}" << std::endl
       << std::endl
       << name << "_t::" << name << "_t(proxy_t const &wrapped_proxy, construct_proxy_wrapper_tag /*unused*/)" << std::endl
       << "  : proxy_t(wrapped_proxy, construct_proxy_wrapper_tag())"
       << "{" << std::endl
       << set_interface.str()
       << "}" << std::endl
       << std::endl
       << name << "_t " << name << "_t::proxy_create_wrapper()" << std::endl
       << "{" << std::endl
       << "  return {*this, construct_proxy_wrapper_tag()};" << std::endl
       << "}" << std::endl
       << std::endl
       << "const std::string " << name << "_t::interface_name = \"" << orig_name << "\";" << std::endl
       << std::endl
       << name << "_t::operator " << orig_name << "*() const" << std::endl
       << "{" << std::endl
       << "  return reinterpret_cast<" << orig_name << "*> (c_ptr());" << std::endl
       << "}" << std::endl
       << std::endl;

    for(auto const& request : requests)
      if(request.name != "destroy")
        ss << request.print_body(name, false) << std::endl
           << std::endl;

    for(auto const& event : events)
      ss << event.print_signal_body(name, false) << std::endl;

    ss << "int " << name << "_t::dispatcher(uint32_t opcode, const std::vector<any>& args, const std::shared_ptr<detail::events_base_t>& e)" << std::endl
       << "{" << std::endl;

    if(!events.empty())
    {
      ss << "  std::shared_ptr<events_t> events = std::static_pointer_cast<events_t>(e);" << std::endl
         << "  switch(opcode)" << std::endl
         << "    {" << std::endl;

      int opcode = 0;
      for(auto const& event : events)
        ss << event.print_dispatcher(opcode++, false) << std::endl;

      ss << "    }" << std::endl;
    }

    ss << "  return 0;" << std::endl
       << "}" << std::endl;

    for(auto const& enumeration : enums)
      ss << enumeration.print_body(name) << std::endl;

    return ss.str();
  }

  std::string print_server_body() const
  {
    std::stringstream ss;
    ss << name << "_t::" << name << "_t(const client_t& client, uint32_t id, int version)" << std::endl
       << "  : resource_t(client, &server::detail::" << name << "_interface, id, version)" << std::endl
       << "{" << std::endl
       << "  set_events(std::shared_ptr<resource_t::events_base_t>(new events_t), dispatcher);" << std::endl
       << "}" << std::endl
       << std::endl
       << name << "_t::" << name << "_t(const resource_t &resource)" << std::endl
       << "  : resource_t(resource)" << std::endl
       << "{" << std::endl
       << "  set_events(std::shared_ptr<resource_t::events_base_t>(new events_t), dispatcher);" << std::endl
       << "}" << std::endl
       << std::endl
       << "const std::string " << name << "_t::interface_name = \"" << orig_name << "\";" << std::endl
       << std::endl
       << name << "_t::operator " << orig_name << "*() const" << std::endl
       << "{" << std::endl
       << "  return reinterpret_cast<" << orig_name << "*> (c_ptr());" << std::endl
       << "}" << std::endl
       << std::endl;

    for(auto const& request : requests)
      ss << request.print_signal_body(name, true) << std::endl
         << std::endl;

    for(auto const& event : events)
      ss << event.print_body(name, true) << std::endl;

    for(auto const& error : errors)
      ss << error.print_server_body(name) << std::endl;

    ss << "int " << name << "_t::dispatcher(int opcode, const std::vector<any>& args, const std::shared_ptr<resource_t::events_base_t>& e)" << std::endl
       << "{" << std::endl;

    if(!requests.empty())
    {
      ss << "  std::shared_ptr<events_t> events = std::static_pointer_cast<events_t>(e);" << std::endl
         << "  switch(opcode)" << std::endl
         << "    {" << std::endl;

      int opcode = 0;
      for(auto const& request : requests)
        ss << request.print_dispatcher(opcode++, true) << std::endl;

      ss << "    }" << std::endl;
    }

    ss << "  return 0;" << std::endl
       << "}" << std::endl;

    for(auto const& enumeration : enums)
      ss << enumeration.print_body(name) << std::endl;

    return ss.str();
  }

  std::string print_interface_body(bool server) const
  {
    std::stringstream ss;
    for(auto const& request : requests)
    {
      ss << "const wl_interface* " << name << "_interface_" << request.name << "_request" << (server ? "_server" : "") << "[" << request.args.size() << "] = {" << std::endl;
      for(auto const& arg : request.args)
        if(!arg.interface.empty())
          ss  << "  &" << arg.interface << "_interface," << std::endl;
        else
          ss  << "  nullptr," << std::endl;
      ss << "};" << std::endl
         << std::endl;
    }
    for(auto const& event : events)
    {
      ss << "const wl_interface* " << name << "_interface_" << event.name << "_event" << (server ? "_server" : "") << "[" << event.args.size() << "] = {" << std::endl;
      for(auto const& arg : event.args)
        if(!arg.interface.empty())
          ss  << "  &" << arg.interface << "_interface," << std::endl;
        else
          ss  << "  nullptr," << std::endl;
      ss << "};" << std::endl
         << std::endl;
    }
    ss << "const wl_message " << name << "_interface_requests" << (server ? "_server" : "") << "[" << requests.size() << "] = {" << std::endl;
    for(auto const& request : requests)
    {
      ss << "  {" << std::endl
         << "    \"" << request.name << "\"," << std::endl
         << "    \"";
      if(request.since > 1)
        ss << request.since;
      for(auto const& arg : request.args)
      {
        if(arg.allow_null)
          ss << "?";
        if(arg.type == "new_id" && arg.interface.empty())
          ss << "su";
        ss << arg.print_short();
      }
      ss << "\"," << std::endl
         << "    " << name << "_interface_" << request.name << "_request" << (server ? "_server" : "") << "," << std::endl
         << "  }," << std::endl;
    }
    ss << "};" << std::endl
       << std::endl;
    ss << "const wl_message " << name << "_interface_events" << (server ? "_server" : "") << "[" << events.size() << "] = {" << std::endl;
    for(auto const& event : events)
    {
      ss << "  {" << std::endl
         << "    \"" << event.name << "\"," << std::endl
         << "    \"";
      if(event.since > 1)
        ss << event.since;
      for(auto const& arg : event.args)
      {
        if(arg.allow_null)
          ss << "?";
        if(arg.type == "new_id" && arg.interface.empty())
          ss << "su";
        ss << arg.print_short();
      }
      ss << "\"," << std::endl
         << "    " << name << "_interface_" << event.name << "_event" << (server ? "_server" : "") << "," << std::endl
         << "  }," << std::endl;
    }
    ss << "};" << std::endl
       << std::endl;
    if(server)
      ss << "const wl_interface wayland::server::detail::" << name << "_interface =" << std::endl;
    else
      ss << "const wl_interface wayland::detail::" << name << "_interface =" << std::endl;
    ss << "  {" << std::endl
       << "    \"" << orig_name << "\"," << std::endl
       << "    " << version << "," << std::endl
       << "    " << requests.size() << "," << std::endl
       << "    " << name << "_interface_requests" << (server ? "_server" : "") << "," << std::endl
       << "    " << events.size() << "," << std::endl
       << "    " << name << "_interface_events" << (server ? "_server" : "") << "," << std::endl
       << "  };" << std::endl
       << std::endl;

    return ss.str();
  }
};

std::string unprefix(const std::string &name)
{
  auto prefix_len = name.find('_');
  if(prefix_len != std::string::npos)
  {
    auto prefix = name.substr(0, prefix_len);
    if(prefix == "wl" || prefix == "wp")
      return name.substr(prefix_len+1, name.size());
  }
  return name;
}

struct arg_t
{
  std::string key;
  std::string value;
};

void parse_args(int argc, char **argv, std::vector<arg_t>& map, std::vector<std::string>& extra)
{
  bool opts_end = false;
  for(unsigned int c = 1; c < argc; c++)
  {
    std::string str(argv[c]);
    if(opts_end || str[0] != '-')
      extra.push_back(str);
    else if(str == "--")
      opts_end = true;
    else
    {
      std::string key = str.substr(1);
      std::string value;
      if(c + 1 < argc && argv[c+1][0] != '-')
        value = argv[++c];
      map.push_back(arg_t{str.substr(1), value});
    }
  }
}

int main(int argc, char *argv[])
{
  std::vector<arg_t> map;
  std::vector<std::string> extra;
  parse_args(argc, argv, map, extra);

  if(extra.size() < 3)
  {
    std::cerr << "Usage:" << std::endl
              << "  " << argv[0] << " [-s on] [-x extra_header.hpp] protocol1.xml [protocol2.xml ...] protocol.hpp protocol.cpp" << std::endl;
    return 1;
  }

  // generate server headers?
  auto const server = [&map] ()
  {
    for(auto const& opt : map)
      if(opt.key == "s")
        return true;
    return false;
  }();

  std::list<interface_t> interfaces;
  int enum_id = 0;

  for(int c = 0; c < extra.size()-2; c++)
  {
    xml_document doc;
    doc.load_file(extra[c].c_str());
    auto protocol = doc.child("protocol");

    for(auto const& interface : protocol.children("interface"))
    {
      interface_t iface;
      iface.destroy_opcode = -1;
      iface.orig_name = interface.attribute("name").value();
      iface.name = unprefix(iface.orig_name);
      if(interface.attribute("version"))
        iface.version = std::stoi(std::string(interface.attribute("version").value()), nullptr, 0);
      else
        iface.version = 1;
      if(interface.child("description"))
      {
        auto description = interface.child("description");
        iface.summary = description.attribute("summary").value();
        iface.description = description.text().get();
      }

      interface_names.push_back(iface.name);

      int opcode = 0; // Opcodes are in order of the XML. (Sadly undocumented)
      for(auto const& request : interface.children("request"))
      {
        request_t req;
        req.opcode = opcode++;
        req.name = request.attribute("name").value();

        if(request.attribute("since"))
          req.since = std::stoi(std::string(request.attribute("since").value()), nullptr, 0);
        else
          req.since = 1;

        if(request.child("description"))
        {
          auto description = request.child("description");
          req.summary = description.attribute("summary").value();
          req.description = description.text().get();
        }

        // destruction takes place through the class destuctor
        if(req.name == "destroy")
          iface.destroy_opcode = req.opcode;
        for(auto const& argument : request.children("arg"))
        {
          argument_t arg;
          arg.type = argument.attribute("type").value();
          arg.name = argument.attribute("name").value();

          if(argument.attribute("summary"))
            arg.summary = argument.attribute("summary").value();

          if(argument.attribute("interface"))
            arg.interface = unprefix(argument.attribute("interface").value());

          if(argument.attribute("enum"))
          {
            std::string tmp = argument.attribute("enum").value();
            if(tmp.find('.') == std::string::npos)
            {
              arg.enum_iface = iface.name;
              arg.enum_name = tmp;
            }
            else
            {
              arg.enum_iface = unprefix(tmp.substr(0, tmp.find('.')));
              arg.enum_name = tmp.substr(tmp.find('.')+1);
            }
          }

          arg.allow_null = argument.attribute("allow-null") && std::string(argument.attribute("allow-null").value()) == "true";

          if(arg.type == "new_id")
            req.ret = arg;
          req.args.push_back(arg);
        }
        iface.requests.push_back(req);
      }

      opcode = 0;
      for(auto const& event : interface.children("event"))
      {
        event_t ev;
        ev.opcode = opcode++;
        ev.name = event.attribute("name").value();

        if(event.attribute("since"))
          ev.since = std::stoi(std::string(event.attribute("since").value()), nullptr, 0);
        else
          ev.since = 1;

        if(event.child("description"))
        {
          auto description = event.child("description");
          ev.summary = description.attribute("summary").value();
          ev.description = description.text().get();
        }

        for(auto const& argument : event.children("arg"))
        {
          argument_t arg;
          arg.type = argument.attribute("type").value();
          arg.name = argument.attribute("name").value();

          if(argument.attribute("summary"))
            arg.summary = argument.attribute("summary").value();

          if(argument.attribute("interface"))
            arg.interface = unprefix(argument.attribute("interface").value());

          if(argument.attribute("enum"))
          {
            std::string tmp = argument.attribute("enum").value();
            if(tmp.find('.') == std::string::npos)
            {
              arg.enum_iface = iface.name;
              arg.enum_name = tmp;
            }
            else
            {
              arg.enum_iface = unprefix(tmp.substr(0, tmp.find('.')));
              arg.enum_name = tmp.substr(tmp.find('.')+1);
            }
          }

          arg.allow_null = argument.attribute("allow-null") && std::string(argument.attribute("allow-null").value()) == "true";

          if(arg.type == "new_id")
            ev.ret = arg;
          ev.args.push_back(arg);
        }
        iface.events.push_back(ev);
      }

      for(auto const& enumeration : interface.children("enum"))
      {
        enumeration_t enu;
        enu.name = enumeration.attribute("name").value();
        if(enumeration.child("description"))
        {
          auto description = enumeration.child("description");
          enu.summary = description.attribute("summary").value();
          enu.description = description.text().get();
        }

        if(enumeration.attribute("bitfield"))
        {
          std::string tmp = enumeration.attribute("bitfield").value();
          enu.bitfield = (tmp == "true");
        }
        else
          enu.bitfield = false;
        enu.id = enum_id++;
        enu.width = 0;

        for(auto entry = enumeration.child("entry"); entry;
            entry = entry.next_sibling("entry"))
        {
          enum_entry_t enum_entry;
          enum_entry.name = entry.attribute("name").value();
          if(enum_entry.name == "default"
             || isdigit(enum_entry.name.at(0)))
            enum_entry.name.insert(0, 1, '_');
          enum_entry.value = entry.attribute("value").value();

          if(entry.attribute("summary"))
            enum_entry.summary = entry.attribute("summary").value();

          auto tmp = static_cast<uint32_t>(std::log2(stol(enum_entry.value, nullptr, 0))) + 1U;
          if(tmp > enu.width)
            enu.width = tmp;

          enu.entries.push_back(enum_entry);
          if(enu.name == "error")

          {
            post_error_t error;
            error.name = enum_entry.name;
            error.summary = enum_entry.summary;
            error.description = enum_entry.description;
            iface.errors.push_back(error);
          }
        }
        iface.enums.push_back(enu);
      }

      interfaces.push_back(iface);
    }
  }

  std::string hpp_file(extra[extra.size()-2]);
  std::string cpp_file(extra[extra.size()-1]);
  std::fstream wayland_hpp(hpp_file, std::ios_base::out | std::ios_base::trunc);
  std::fstream wayland_cpp(cpp_file, std::ios_base::out | std::ios_base::trunc);

  // header intro
  wayland_hpp << "#pragma once" << std::endl
              << std::endl
              << "#include <array>" << std::endl
              << "#include <cstdint>" << std::endl
              << "#include <functional>" << std::endl
              << "#include <memory>" << std::endl
              << "#include <string>" << std::endl
              << "#include <vector>" << std::endl
              << std::endl
              << (server ? "#include <wayland-server.hpp>" : "#include <wayland-client.hpp>") << std::endl;

  std::fstream wayland_server_hpp;
  std::fstream wayland_server_cpp;
  if(server)
  {
    std::string server_hpp_file(extra[extra.size()-2]), server_cpp_file(extra[extra.size()-1]);
    wayland_server_hpp.open(server_hpp_file, std::ios_base::out | std::ios_base::trunc);
    wayland_server_cpp.open(server_cpp_file, std::ios_base::out | std::ios_base::trunc);

    // header intro
    wayland_server_hpp << "#pragma once" << std::endl
                       << std::endl
                       << "#include <array>" << std::endl
                       << "#include <cstdint>" << std::endl
                       << "#include <functional>" << std::endl
                       << "#include <memory>" << std::endl
                       << "#include <string>" << std::endl
                       << "#include <vector>" << std::endl
                       << std::endl
                       << "#include <wayland-server.hpp>" << std::endl
                       << std::endl;

    // body intro
    auto server_hpp_slash_pos = server_hpp_file.find_last_of('/');
    auto server_hpp_basename = (server_hpp_slash_pos == std::string::npos ? server_hpp_file : server_hpp_file.substr(server_hpp_slash_pos + 1));
    wayland_server_cpp << "#include <" << server_hpp_basename << ">" << std::endl
                       << std::endl
                       << "using namespace wayland::detail;" << std::endl
                       << "using namespace wayland::server;" << std::endl
                       << "using namespace wayland::server::detail;" << std::endl
                       << std::endl;
  }

  for(auto const& opt : map)
    if(opt.key == std::string("x"))
      wayland_hpp << "#include <" << opt.value << ">" << std::endl;

  wayland_hpp << std::endl;

  // C forward declarations
  for(auto const& iface : interfaces)
    if(iface.name != "display")
      wayland_hpp << iface.print_c_forward();
  wayland_hpp << std::endl;

  wayland_hpp << "namespace wayland" << std::endl
              << "{" << std::endl;
  if(server)
    wayland_hpp << "namespace server" << std::endl
                << "{" << std::endl;

  // C++ forward declarations
  for(auto const& iface : interfaces)
    if(iface.name != "display")
      wayland_hpp << iface.print_forward();
  wayland_hpp << std::endl;

  // interface headers
  wayland_hpp << "namespace detail" << std::endl
              << "{" << std::endl;
  for(auto const& iface : interfaces)
    wayland_hpp << iface.print_interface_header();
  wayland_hpp  << "}" << std::endl
               << std::endl;

  // class declarations
  for(auto const& iface : interfaces)
    if(iface.name != "display")
    {
      if(server)
        wayland_hpp << iface.print_server_header() << std::endl;
      else
        wayland_hpp << iface.print_client_header() << std::endl;
    }
  wayland_hpp << std::endl
              << "}" << std::endl;
  if(server)
    wayland_hpp << "}" << std::endl;

  // body intro
  auto hpp_slash_pos = hpp_file.find_last_of('/');
  auto hpp_basename = (hpp_slash_pos == std::string::npos ? hpp_file : hpp_file.substr(hpp_slash_pos + 1));
  wayland_cpp << "#include <" << hpp_basename << ">" << std::endl
              << std::endl
              << "using namespace wayland;" << std::endl
              << "using namespace wayland::detail;" << std::endl;
  if(server)
    wayland_cpp << "using namespace wayland::server;" << std::endl
                << "using namespace wayland::server::detail;" << std::endl;
  wayland_cpp << std::endl;

  // interface bodys
  for(auto const& iface : interfaces)
    wayland_cpp << iface.print_interface_body(server);

  // class member definitions
  for(auto const& iface : interfaces)
    if(iface.name != "display")
    {
      if(server)
        wayland_cpp << iface.print_server_body() << std::endl;
      else
        wayland_cpp << iface.print_client_body() << std::endl;
    }
  wayland_cpp << std::endl;

  // clean up
  wayland_hpp.close();
  wayland_cpp.close();

  return 0;
}

// set of C++-only keywords not to use as names
const std::set<std::string> element_t::keywords =
{
  "alignas",
  "alignof",
  "and",
  "and_eq",
  "asm",
  "auto",
  "bitand",
  "bitor",
  "bool",
  "break",
  "case",
  "catch",
  "char",
  "char8_t",
  "char16_t",
  "char32_t",
  "class",
  "compl",
  "concept",
  "const",
  "consteval",
  "constexpr",
  "constinit",
  "const_cast",
  "continue",
  "co_await",
  "co_return",
  "co_yield",
  "decltype",
  "default",
  "delete",
  "do",
  "double",
  "dynamic_cast",
  "else",
  "enum",
  "explicit",
  "export",
  "extern",
  "false",
  "float",
  "for",
  "friend",
  "goto",
  "if",
  "inline",
  "int",
  "long",
  "mutable",
  "namespace",
  "new",
  "noexcept",
  "not",
  "not_eq",
  "nullptr",
  "operator",
  "or",
  "or_eq",
  "private",
  "protected",
  "public",
  "register",
  "reinterpret_cast",
  "requires",
  "return",
  "short",
  "signed",
  "sizeof",
  "static",
  "static_assert",
  "static_cast",
  "struct",
  "switch",
  "template",
  "this",
  "thread_local",
  "throw",
  "true",
  "try",
  "typedef",
  "typeid",
  "typename",
  "union",
  "unsigned",
  "using",
  "virtual",
  "void",
  "volatile",
  "wchar_t",
  "while",
  "xor",
  "xor_eq",
};
