/*
 *  Copyright (c) 2014-2019 Nils Christopher Brause, Philipp Kerling, Bernd Kuhls
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

  std::string sanitise(std::string str)
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
  bool allow_null;

  std::string print_enum_wire_type()
  {
    // Enums can be int or uint on the wire, except for bitfields
    if(type == "int")
      return "int32_t";
    if(type == "uint")
      return "uint32_t";
    throw std::runtime_error("Enum type must be int or uint");
  }

  std::string print_type()
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
      return "proxy_t";
    if(type == "new_id")
      return "proxy_t";
    if(type == "fd")
      return "int";
    if(type == "array")
      return "array_t";
    return type;
  }

  std::string print_short()
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

  std::string print_argument()
  {
    return print_type() + " " + sanitise(name);
  }
};

struct event_t : public element_t
{
  std::list<argument_t> args;
  int since;

  std::string print_functional()
  {
    std::stringstream ss;
    ss << "    std::function<void(";
    for(auto &arg : args)
      ss << arg.print_type() << ", ";
    if(!args.empty())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ")> " << sanitise(name) << ";";
    return ss.str();
  }

  std::string print_dispatcher(int opcode)
  {
    std::stringstream ss;
    ss << "    case " << opcode << ":" << std::endl
       << "      if(events->" << sanitise(name) << ") events->" << sanitise(name) << "(";

    int c = 0;
    for(auto &arg : args)
      if(!arg.enum_name.empty() && arg.type != "array")
        ss << arg.print_type() << "(args[" << c++ << "].get<" << arg.print_enum_wire_type() << ">()), ";
      else if(!arg.interface.empty())
        ss << arg.print_type() << "(args[" << c++ << "].get<proxy_t>()), ";
      else
        ss << "args[" << c++ << "].get<" << arg.print_type() << ">(), ";
    if(!args.empty())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ");" << std::endl
       << "      break;";
    return ss.str();
  }

  std::string print_signal_header()
  {
    std::stringstream ss;
    ss << "  /** \\brief " << summary << std::endl;
    for(auto &arg : args)
      ss << "      \\param " << arg.name << " " << arg.summary << std::endl;
    ss << description << std::endl
       << "  */" << std::endl;

    ss << "  std::function<void(";
    for(auto &arg : args)
      ss << arg.print_type() + ", ";
    if(!args.empty())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ")> &on_" <<  name << "();" << std::endl;
    return ss.str();
  }

  std::string print_signal_body(std::string interface_name)
  {
    std::stringstream ss;
    ss << "std::function<void(";
    for(auto &arg : args)
      ss << arg.print_type() << ", ";
    if(!args.empty())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ")> &" + interface_name + "_t::on_" + name + "()" << std::endl
       << "{" << std::endl
       << "  return std::static_pointer_cast<events_t>(get_events())->" + sanitise(name) + ";" << std::endl
       << "}" << std::endl;
    return ss.str();
  }
};

struct request_t : public event_t
{
  argument_t ret;
  int opcode;

  std::string availability_function_name()
  {
    if(since > 1)
      return std::string("can_") + name;
    return "";
  }

  std::string since_version_constant_name()
  {
    return name + "_since_version";
  }

  std::string print_header()
  {
    std::stringstream ss;
    ss << "  /** \\brief " << summary << std::endl;
    if(!ret.summary.empty())
      ss << "      \\return " << ret.summary << std::endl;
    for(auto &arg : args)
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

    if(ret.name.empty())
      ss << "  void ";
    else
      ss << "  " << ret.print_type() << " ";
    ss << sanitise(name) << "(";

    for(auto &arg : args)
      if(arg.type == "new_id")
        {
          if(arg.interface.empty())
            ss << "proxy_t &interface, uint32_t version, ";
        }
      else
        ss << arg.print_argument() << ", ";

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

  std::string print_body(std::string interface_name)
  {
    std::stringstream ss;
    if(ret.name.empty())
      ss <<  "void ";
    else
      ss << ret.print_type() << " ";
    ss << interface_name << "_t::" << sanitise(name) << "(";

    bool new_id_arg = false;
    for(auto &arg : args)
      if(arg.type == "new_id")
        {
          if(arg.interface.empty())
            {
              ss << "proxy_t &interface, uint32_t version, ";
              new_id_arg = true;
            }
        }
      else
        ss << arg.print_argument() << ", ";

    if(ss.str().substr(ss.str().size()-2, 2) == ", ")
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ")\n{" << std::endl;

    if(ret.name.empty())
      ss <<  "  marshal(" << opcode << "u, ";
    else if(ret.interface.empty())
      {
        ss << "  proxy_t p = marshal_constructor_versioned(" << opcode << "u, interface.interface, version, ";
      }
    else
      {
        ss << "  proxy_t p = marshal_constructor(" << opcode << "u, &" << ret.interface << "_interface, ";
      }

    for(auto &arg : args)
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

    if(!ret.name.empty())
      {
        if(new_id_arg)
          {
            ss << "  interface = interface.copy_constructor(p);" << std::endl
               << "  return interface;" << std::endl;
          }
        else
          ss << "  return " << ret.print_type() << "(p);" << std::endl;
      }
    ss << "}";

    if(!availability_function_name().empty())
    {
      ss << std::endl
         << "bool " << interface_name << "_t::" << availability_function_name() << "() const" << std::endl
         << "{" << std::endl
         << "  return (get_version() >= " << since_version_constant_name() << ");" << std::endl
         << "}";
    }

    return ss.str();
  }
};

struct enum_entry_t : public element_t
{
  std::string value;
};

struct enumeration_t : public element_t
{
  std::list<enum_entry_t> entries;
  bool bitfield;
  int id;
  uint32_t width;

  std::string print_forward(std::string iface_name)
  {
    std::stringstream ss;
    if(!bitfield)
      ss << "enum class " << iface_name << "_" << name << " : uint32_t;" << std::endl;
    else
      ss << "struct " << iface_name << "_" << name << ";" << std::endl;
    return ss.str();
  }

  std::string print_header(std::string iface_name)
  {
    std::stringstream ss;
    ss << "/** \\brief " << summary << std::endl
       << description << std::endl
       << "  */" << std::endl;

    if(!bitfield)
      ss << "enum class " << iface_name << "_" << name << " : uint32_t" << std::endl
         << "  {" << std::endl;
    else
      ss << "struct " << iface_name << "_" << name << " : public detail::bitfield<" << width << ", " << id << ">" << std::endl
         << "{" << std::endl
         << "  " << iface_name << "_" << name << "(const detail::bitfield<" << width << ", " << id << "> &b)" << std::endl
         << "    : detail::bitfield<" << width << ", " << id << ">(b) {}" << std::endl
         << "  " << iface_name << "_" << name << "(const uint32_t value)" << std::endl
         << "    : detail::bitfield<" << width << ", " << id << ">(value) {}" << std::endl;

    for(auto &entry : entries)
      {
        if(!entry.summary.empty())
          ss << "  /** \\brief " << entry.summary << " */" << std::endl;

        if(!bitfield)
          ss << "  " << sanitise(entry.name) << " = " << entry.value << "," << std::endl;
        else
          ss << "  static const detail::bitfield<" << width << ", " << id << "> " << sanitise(entry.name) << ";" << std::endl;
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

  std::string print_body(std::string iface_name)
  {
    std::stringstream ss;
    if(bitfield)
      for(auto &entry : entries)
        {
          ss << "const bitfield<" << width << ", " << id << "> " << iface_name << "_" << name
             << "::" << sanitise(entry.name) << "{" << entry.value << "};" << std::endl;
        }
    return ss.str();
  }
};

struct interface_t : public element_t
{
  int version;
  std::string orig_name;
  int destroy_opcode;
  std::list<request_t> requests;
  std::list<event_t> events;
  std::list<enumeration_t> enums;

  std::string print_forward()
  {
    std::stringstream ss;
    ss << "class " << name << "_t;" << std::endl;
    for(auto &e : enums)
      ss << e.print_forward(name);
    return ss.str();
  }

  std::string print_c_forward()
  {
    std::stringstream ss;
    ss << "struct " << orig_name << ";" << std::endl;
    return ss.str();
  }

  std::string print_header()
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

    for(auto &event : events)
      ss << event.print_functional() << std::endl;

    ss << "  };" << std::endl
       << std::endl
       << "  static int dispatcher(uint32_t opcode, std::vector<detail::any> args, std::shared_ptr<detail::events_base_t> e);" << std::endl
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

    for(auto &request : requests)
      if(request.name != "destroy")
        ss << request.print_header() << std::endl;

    for(auto &event : events)
      ss << event.print_signal_header() << std::endl;

    ss << "};" << std::endl
       << std::endl;

    for(auto &enumeration : enums)
      ss << enumeration.print_header(name) << std::endl;

    return ss.str();
  }

  std::string print_interface_header()
  {
    std::stringstream ss;
    ss << "  extern const wl_interface " << name << "_interface;" << std::endl;
    return ss.str();
  }

  std::string print_body()
  {
    std::stringstream set_events;
    set_events << "  if(proxy_has_object() && get_wrapper_type() == wrapper_type::standard)" << std::endl
               << "    {" << std::endl
               << "      set_events(std::shared_ptr<detail::events_base_t>(new events_t), dispatcher);" << std::endl;
    if(destroy_opcode != -1)
      set_events << "      set_destroy_opcode(" << destroy_opcode << "u);" << std::endl;
    set_events << "    }" << std::endl;

    std::stringstream set_interface;
    set_interface << "  interface = &" << name << "_interface;" << std::endl
                  << "  copy_constructor = [] (const proxy_t &p) -> proxy_t" << std::endl
                  << "    { return " << name << "_t(p); };" << std::endl;

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

    for(auto &request : requests)
      if(request.name != "destroy")
        ss << request.print_body(name) << std::endl
           << std::endl;

    for(auto &event : events)
      ss << event.print_signal_body(name) << std::endl;

    ss << "int " << name << "_t::dispatcher(uint32_t opcode, std::vector<any> args, std::shared_ptr<detail::events_base_t> e)" << std::endl
       << "{" << std::endl;

    if(!events.empty())
      {
        ss << "  std::shared_ptr<events_t> events = std::static_pointer_cast<events_t>(e);" << std::endl
           << "  switch(opcode)" << std::endl
           << "    {" << std::endl;

        int opcode = 0;
        for(auto &event : events)
          ss << event.print_dispatcher(opcode++) << std::endl;

        ss << "    }" << std::endl;
      }

    ss << "  return 0;" << std::endl
       << "}" << std::endl;

    for(auto &enumeration : enums)
      ss << enumeration.print_body(name) << std::endl;

    return ss.str();
  }

  std::string print_interface_body()
  {
    std::stringstream ss;
    for(auto &request : requests)
      {
        ss << "const wl_interface* " << name << "_interface_" << request.name << "_request[" << request.args.size() << "] = {" << std::endl;
        for(auto &arg : request.args)
          if(!arg.interface.empty())
            ss  << "  &" << arg.interface << "_interface," << std::endl;
          else
            ss  << "  nullptr," << std::endl;
        ss << "};" << std::endl
           << std::endl;
      }
    for(auto &event : events)
      {
        ss << "const wl_interface* " << name << "_interface_" << event.name << "_event[" << event.args.size() << "] = {" << std::endl;
        for(auto &arg : event.args)
          if(!arg.interface.empty())
            ss  << "  &" << arg.interface << "_interface," << std::endl;
          else
            ss  << "  nullptr," << std::endl;
        ss << "};" << std::endl
           << std::endl;
      }
    ss << "const wl_message " << name << "_interface_requests[" << requests.size() << "] = {" << std::endl;
    for(auto &request : requests)
      {
        ss << "  {" << std::endl
           << "    \"" << request.name << "\"," << std::endl
           << "    \"";
        if(request.since > 1)
          ss << request.since;
        for(auto &arg : request.args)
          {
            if(arg.allow_null)
              ss << "?";
            if(arg.type == "new_id" && arg.interface.empty())
              ss << "su";
            ss << arg.print_short();
          }
        ss << "\"," << std::endl
           << "    " << name << "_interface_" << request.name << "_request," << std::endl
           << "  }," << std::endl;
      }
    ss << "};" << std::endl
       << std::endl;
    ss << "const wl_message " << name << "_interface_events[" << events.size() << "] = {" << std::endl;
    for(auto &event : events)
      {
        ss << "  {" << std::endl
           << "    \"" << event.name << "\"," << std::endl
           << "    \"";
        if(event.since > 1)
          ss << event.since;
        for(auto &arg : event.args)
          {
            if(arg.allow_null)
              ss << "?";
            if(arg.type == "new_id" && arg.interface.empty())
              ss << "su";
            ss << arg.print_short();
          }
        ss << "\"," << std::endl
           << "    " << name << "_interface_" << event.name << "_event," << std::endl
           << "  }," << std::endl;
      }
    ss << "};" << std::endl
       << std::endl;
    ss << "const wl_interface wayland::detail::" << name << "_interface =" << std::endl
       << "  {" << std::endl
       << "    \"" << orig_name << "\"," << std::endl
       << "    " << version << "," << std::endl
       << "    " << requests.size() << "," << std::endl
       << "    " << name << "_interface_requests," << std::endl
       << "    " << events.size() << "," << std::endl
       << "    " << name << "_interface_events," << std::endl
       << "  };" << std::endl
       << std::endl;

    return ss.str();
  }
};

std::string unprefix(const std::string &name)
{
  auto prefix_len = name.find("_");
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
      std::string value;
      if (c + 1 < argc && argv[c+1][0] != '-')
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
                << "  " << argv[0] << " [-x extra_header.hpp] protocol1.xml [protocol2.xml ...] protocol.hpp protocol.cpp" << std::endl;
      return 1;
    }

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

                  if(argument.attribute("allow-null") && std::string(argument.attribute("allow-null").value()) == "true")
                    arg.allow_null = true;
                  else
                    arg.allow_null = false;

                  if(arg.type == "new_id")
                    req.ret = arg;
                  req.args.push_back(arg);
                }
              iface.requests.push_back(req);
            }

          for(auto const& event : interface.children("event"))
            {
              event_t ev;
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

                  if(argument.attribute("allow-null") && std::string(argument.attribute("allow-null").value()) == "true")
                    arg.allow_null = true;
                  else
                    arg.allow_null = false;

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

                  auto tmp = static_cast<uint32_t>(std::log2(stol(enum_entry.value, nullptr, 0))) + 1u;
                  if(tmp > enu.width)
                    enu.width = tmp;

                  enu.entries.push_back(enum_entry);
                }
              iface.enums.push_back(enu);
            }

          interfaces.push_back(iface);
        }
    }

  std::string hpp_file(extra[extra.size()-2]), cpp_file(extra[extra.size()-1]);
  std::fstream wayland_hpp(hpp_file, std::ios_base::out | std::ios_base::trunc);
  std::fstream wayland_cpp(cpp_file, std::ios_base::out | std::ios_base::trunc);

  // header intro
  wayland_hpp << "#pragma once" << std::endl
              << std::endl
              << "#include <array>" << std::endl
              << "#include <functional>" << std::endl
              << "#include <memory>" << std::endl
              << "#include <string>" << std::endl
              << "#include <vector>" << std::endl
              << std::endl
              << "#include <wayland-client.hpp>" << std::endl;

  for(auto const& opt : map)
    if(opt.key == std::string("x"))
      wayland_hpp << "#include <" << opt.value << ">" << std::endl;

  wayland_hpp << std::endl;

  // C forward declarations
  for(auto &iface : interfaces)
    if(iface.name != "display")
      wayland_hpp << iface.print_c_forward();
  wayland_hpp << std::endl;

  wayland_hpp << "namespace wayland" << std::endl
              << "{" << std::endl;

  // C++ forward declarations
  for(auto &iface : interfaces)
    if(iface.name != "display")
      wayland_hpp << iface.print_forward();
  wayland_hpp << std::endl;

  // interface headers
  wayland_hpp << "namespace detail" << std::endl
              << "{" << std::endl;
  for(auto &iface : interfaces)
    wayland_hpp << iface.print_interface_header();
  wayland_hpp  << "}" << std::endl
  << std::endl;

  // class declarations
  for(auto &iface : interfaces)
    if(iface.name != "display")
      wayland_hpp << iface.print_header() << std::endl;
  wayland_hpp << std::endl
              << "}" << std::endl;

  // body intro
  auto hpp_slash_pos = hpp_file.find_last_of('/');
  auto hpp_basename = (hpp_slash_pos == std::string::npos ? hpp_file : hpp_file.substr(hpp_slash_pos + 1));
  wayland_cpp << "#include <" << hpp_basename << ">" << std::endl
              << std::endl
              << "using namespace wayland;" << std::endl
              << "using namespace detail;" << std::endl
              << std::endl;

  // interface bodys
  for(auto &iface : interfaces)
    wayland_cpp << iface.print_interface_body();

  // class member definitions
  for(auto &iface : interfaces)
    if(iface.name != "display")
      wayland_cpp << iface.print_body() << std::endl;
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
   "char16_t",
   "char32_t",
   "class",
   "compl",
   "const",
   "constexpr",
   "const_cast",
   "continue",
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
