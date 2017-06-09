/*
 * (C) Copyright 2014-2016 Nils Brause
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
#include <cctype>
#include <cmath>

#include <pugixml.hpp>

using namespace pugi;

std::list<std::string> interface_names;

struct element_t
{
  std::string summary;
  std::string description;
};

struct argument_t : public element_t
{
  std::string type;
  std::string name;
  std::string interface;
  std::string enum_iface;
  std::string enum_name;
  bool allow_null;

  std::string print_enum_wire_type()
  {
    // Enums can be int or uint on the wire, except for bitfields
    if(type == "int")
      return "int32_t";
    else if(type == "uint")
      return "uint32_t";
    else
      throw std::runtime_error("Enum type must be int or uint");
  }

  std::string print_type()
  {
    if(interface != "")
      return interface + "_t";
    else if(enum_iface != "")
      return enum_iface + "_" + enum_name;
    else if(type == "int")
      return "int32_t";
    else if(type == "uint")
      return "uint32_t";
    else if(type == "fixed")
      return "int32_t";
    else if(type == "string")
      return "std::string";
    else if(type == "object")
      return "proxy_t";
    else if(type == "new_id")
      return "proxy_t";
    else if(type == "fd")
      return "int";
    else if(type == "array")
      return "array_t";
    else
      return type;
  }

  std::string print_short()
  {
    if(type == "int")
      return "i";
    else if(type == "uint")
      return "u";
    else if(type == "fixed")
      return "f";
    else if(type == "string")
      return "s";
    else if(type == "object")
      return "o";
    else if(type == "new_id")
      return "n";
    else if(type == "array")
      return "a";
    else if(type == "fd")
      return "h";
    else
      return "x";
  }

  std::string print_argument()
  {
    return print_type() + " " + name;
  }
};

struct event_t : public element_t
{
  std::string name;
  std::list<argument_t> args;
  int since;

  std::string print_functional()
  {
    std::stringstream ss;
    ss << "    std::function<void(";
    for(auto &arg : args)
      ss << arg.print_type() << ", ";
    if(args.size())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ")> " << name << ";";
    return ss.str();
  }

  std::string print_dispatcher(int opcode)
  {
    std::stringstream ss;
    ss << "    case " << opcode << ":" << std::endl
       << "      if(events->" << name << ") events->" << name << "(";

    int c = 0;
    for(auto &arg : args)
      if(arg.enum_name != "")
        ss << arg.print_type() << "(args[" << c++ << "].get<" << arg.print_enum_wire_type() << ">()), ";
      else if(arg.interface != "")
        ss << arg.print_type() << "(args[" << c++ << "].get<proxy_t>()), ";
      else
        ss << "args[" << c++ << "].get<" << arg.print_type() << ">(), ";
    if(args.size())
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
    if(args.size())
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
    if(args.size())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ")> &" + interface_name + "_t::on_" + name + "()" << std::endl
       << "{" << std::endl
       << "  return std::static_pointer_cast<events_t>(get_events())->" + name + ";" << std::endl
       << "}" << std::endl;
    return ss.str();
  }
};

struct request_t : public event_t
{
  argument_t ret;
  int opcode;

  std::string print_header()
  {
    std::stringstream ss;
    ss << "  /** \\brief " << summary << std::endl;
    if(ret.summary != "")
      ss << "      \\return " << ret.summary << std::endl;
    for(auto &arg : args)
      {
        if(arg.type == "new_id")
          {
            if(arg.interface == "")
              ss << "      \\param interface Interface to bind" << std::endl
                 << "      \\param version Interface version" << std::endl;
          }
        else
          ss << "      \\param " << arg.name << " " << arg.summary << std::endl;
      }
    ss << description << std::endl
       << "  */" << std::endl;

    if(ret.name == "")
      ss << "  void ";
    else
      ss << "  " << ret.print_type() << " ";
    ss << name << "(";

    for(auto &arg : args)
      if(arg.type == "new_id")
        {
          if(arg.interface == "")
            ss << "proxy_t &interface, uint32_t version, ";
        }
      else
        ss << arg.print_argument() << ", ";

    if(ss.str().substr(ss.str().size()-2, 2) == ", ")
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ");" << std::endl;
    return ss.str();
  }

  std::string print_body(std::string interface_name)
  {
    std::stringstream ss;
    if(ret.name == "")
      ss <<  "void ";
    else
      ss << ret.print_type() << " ";
    ss << interface_name << "_t::" << name << "(";

    bool new_id_arg = false;
    for(auto &arg : args)
      if(arg.type == "new_id")
        {
          if(arg.interface == "")
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

    if(ret.name == "")
      ss <<  "  marshal(" << opcode << ", ";
    else
      {
        ss << "  proxy_t p = marshal_constructor(" << opcode << ", ";
        if(ret.interface == "")
          ss << "interface.interface";
        else
          ss << "&" << ret.interface << "_interface";
        ss << ", ";
      }

    for(auto &arg : args)
      {
        if(arg.type == "new_id")
          {
            if(arg.interface == "")
              ss << "std::string(interface.interface->name), version, ";
            ss << "NULL, ";
          }
        else if(arg.enum_name != "")
          ss << "static_cast<" << arg.print_enum_wire_type() << ">(" << arg.name + "), ";
        else
          ss << arg.name + ", ";
      }

    ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ");" << std::endl;

    if(ret.name != "")
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
    return ss.str();
  }
};

struct enum_entry_t : public element_t
{
  std::string name;
  std::string value;
};

struct enumeration_t : public element_t
{
  std::string name;
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
        if(entry.summary != "")
          ss << "  /** \\brief " << entry.summary << " */" << std::endl;

        if(!bitfield)
          ss << "  " << entry.name << " = " << entry.value << "," << std::endl;
        else
          ss << "  static const detail::bitfield<" << width << ", " << id << "> " << entry.name << ";" << std::endl;
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
             << "::" << entry.name << "{" << entry.value << "};" << std::endl;
        }
    return ss.str();
  }
};

struct interface_t : public element_t
{
  int version;
  std::string name;
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

  std::string print_header()
  {
    std::stringstream ss;
    ss << "/** \\brief " << summary << std::endl
       << description << std::endl
       << "*/" << std::endl;

    ss << "class " << name << "_t : public proxy_t" << std::endl
       << "{" << std::endl
       << "private:" << std::endl
       << "  struct events_t : public proxy_t::events_base_t" << std::endl
       << "  {" << std::endl;

    for(auto &event : events)
      ss << event.print_functional() << std::endl;

    ss << "  };" << std::endl
       << std::endl
       << "  static int dispatcher(int opcode, std::vector<detail::any> args, std::shared_ptr<proxy_t::events_base_t> e);" << std::endl
       << std::endl;

    ss << "public:" << std::endl
       << "  " << name << "_t();" << std::endl
       << "  explicit " << name << "_t(const proxy_t &proxy);" << std::endl
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
    std::stringstream ss;
    ss << name << "_t::" << name << "_t(const proxy_t &p)" << std::endl
       << "  : proxy_t(p)" << std::endl
       << "{" << std::endl
       << "  set_events(std::shared_ptr<proxy_t::events_base_t>(new events_t), dispatcher);" << std::endl
       << "  set_destroy_opcode(" << destroy_opcode << ");" << std::endl
       << "  interface = &" << name << "_interface;" << std::endl
       << "  copy_constructor = [] (const proxy_t &p) -> proxy_t" << std::endl
       << "    { return " << name << "_t(p); };" << std::endl
       << "}" << std::endl
       << std::endl
       << name << "_t::" << name << "_t()" << std::endl
       << "{" << std::endl
       << "  interface = &" << name << "_interface;" << std::endl
       << "  copy_constructor = [] (const proxy_t &p) -> proxy_t" << std::endl
       << "    { return " << name << "_t(p); };" << std::endl
       << "}" << std::endl
       << std::endl;

    for(auto &request : requests)
      if(request.name != "destroy")
        ss << request.print_body(name) << std::endl
           << std::endl;

    for(auto &event : events)
      ss << event.print_signal_body(name) << std::endl;

    ss << "int " << name << "_t::dispatcher(int opcode, std::vector<any> args, std::shared_ptr<proxy_t::events_base_t> e)" << std::endl
       << "{" << std::endl;

    if(events.size())
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
    ss << "const wl_interface wayland::detail::" << name << "_interface =" << std::endl
       << "  {" << std::endl
       << "    \"" << orig_name << "\"," << std::endl
       << "    " << version << "," << std::endl
       << "    " << requests.size() << "," << std::endl
       << "    (const wl_message[]) {" << std::endl;
    for(auto &request : requests)
      {
        ss << "      {" << std::endl
           << "        \"" << request.name << "\"," << std::endl
           << "        \"";
        if(request.since > 1)
          ss << request.since;
        for(auto &arg : request.args)
          {
            if(arg.allow_null)
              ss << "?";
            if(arg.type == "new_id" && arg.interface == "")
              ss << "su";
            ss << arg.print_short();
          }
        ss << "\"," << std::endl
           << "        (const wl_interface*[]) {" << std::endl;
        for(auto &arg : request.args)
          if(arg.interface != "")
            ss  << "          &" << arg.interface << "_interface," << std::endl;
          else
            ss  << "          NULL," << std::endl;
        ss << "        }," << std::endl
           << "      }," << std::endl;
      }
    ss << "    }," << std::endl
       << "    " << events.size() << "," << std::endl
       << "    (const wl_message[]) {" << std::endl;
    for(auto &event : events)
      {
        ss << "      {" << std::endl
           << "        \"" << event.name << "\"," << std::endl
           << "        \"";
        if(event.since > 1)
          ss << event.since;
        for(auto &arg : event.args)
          {
            if(arg.allow_null)
              ss << "?";
            if(arg.type == "new_id" && arg.interface == "")
              ss << "su";
            ss << arg.print_short();
          }
        ss << "\"," << std::endl
           << "        (const wl_interface*[]) {" << std::endl;
        for(auto &arg : event.args)
          if(arg.interface != "")
            ss  << "          &" << arg.interface << "_interface," << std::endl;
          else
            ss  << "          NULL," << std::endl;
        ss << "        }," << std::endl
           << "      }," << std::endl;
      }
    ss << "    }" << std::endl
       << "  };" << std::endl
       << std::endl;

    return ss.str();
  }
};

int main(int argc, char *argv[])
{
  if(argc < 4)
    {
      std::cerr << "Usage:" << std::endl
                << "  " << argv[0] << " protocol1.xml [protocol2.xml ...] protocol.hpp protocol.cpp" << std::endl;
      return 1;
    }

  std::list<interface_t> interfaces;
  int enum_id = 0;

  for(int c = 1; c < argc-2; c++)
    {
      xml_document doc;
      doc.load_file(argv[c]);
      xml_node protocol = doc.child("protocol");

      for(xml_node &interface : protocol.children("interface"))
        {
          interface_t iface;
          iface.destroy_opcode = -1;
          iface.orig_name = interface.attribute("name").value();
          if(iface.orig_name.substr(0, 3) == "wl_")
            iface.name = iface.orig_name.substr(3, iface.orig_name.size());
          else
            iface.name = iface.orig_name;

          if(interface.attribute("version"))
            iface.version = std::stoi(std::string(interface.attribute("version").value()));
          else
            iface.version = 1;
          if(interface.child("description"))
            {
              xml_node description = interface.child("description");
              iface.summary = description.attribute("summary").value();
              iface.description = description.text().get();
            }

          interface_names.push_back(iface.name);

          int opcode = 0; // Opcodes are in order of the XML. (Sadly undocumented)
          for(xml_node &request : interface.children("request"))
            {
              request_t req;
              req.opcode = opcode++;
              req.name = request.attribute("name").value();

              if(request.attribute("since"))
                req.since = std::stoi(std::string(request.attribute("since").value()));
              else
                req.since = 1;

              if(request.child("description"))
                {
                  xml_node description = request.child("description");
                  req.summary = description.attribute("summary").value();
                  req.description = description.text().get();
                }

              // destruction takes place through the class destuctor
              if(req.name == "destroy")
                iface.destroy_opcode = req.opcode;
              for(xml_node &argument : request.children("arg"))
                {
                  argument_t arg;
                  arg.type = argument.attribute("type").value();
                  arg.name = argument.attribute("name").value();

                  if(argument.attribute("summary"))
                    arg.summary = argument.attribute("summary").value();

                  if(argument.attribute("interface"))
                    {
                      arg.interface = argument.attribute("interface").value();
                      if(arg.interface.substr(0, 3) == "wl_")
                        arg.interface = arg.interface.substr(3, arg.interface.size());
                    }

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
                          arg.enum_iface = tmp.substr(0, tmp.find('.'));
                          if(arg.enum_iface.substr(0, 3) == "wl_")
                            arg.enum_iface = arg.enum_iface.substr(3, arg.enum_iface.size());
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

          for(xml_node &event : interface.children("event"))
            {
              event_t ev;
              ev.name = event.attribute("name").value();

              if(event.attribute("since"))
                ev.since = std::stoi(std::string(event.attribute("since").value()));
              else
                ev.since = 1;

              if(event.child("description"))
                {
                  xml_node description = event.child("description");
                  ev.summary = description.attribute("summary").value();
                  ev.description = description.text().get();
                }

              for(xml_node &argument : event.children("arg"))
                {
                  argument_t arg;
                  arg.type = argument.attribute("type").value();
                  arg.name = argument.attribute("name").value();

                  if(argument.attribute("summary"))
                    arg.summary = argument.attribute("summary").value();

                  if(argument.attribute("interface"))
                    {
                      arg.interface = argument.attribute("interface").value();
                      if(arg.interface.substr(0, 3) == "wl_")
                        arg.interface = arg.interface.substr(3, arg.interface.size());
                    }

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
                          arg.enum_iface = tmp.substr(0, tmp.find('.'));
                          if(arg.enum_iface.substr(0, 3) == "wl_")
                            arg.enum_iface = arg.enum_iface.substr(3, arg.enum_iface.size());
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

          for(xml_node &enumeration : interface.children("enum"))
            {
              enumeration_t enu;
              enu.name = enumeration.attribute("name").value();
              if(enumeration.child("description"))
                {
                  xml_node description = enumeration.child("description");
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

              for(xml_node entry = enumeration.child("entry"); entry;
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

                  uint32_t tmp = std::floor(std::log2(stol(enum_entry.value)))+1;
                  if(tmp > enu.width)
                    enu.width = tmp;

                  enu.entries.push_back(enum_entry);
                }
              iface.enums.push_back(enu);
            }
          
          interfaces.push_back(iface);
        }
    }

  std::string hpp_file(argv[argc-2]), cpp_file(argv[argc-1]);
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
              << "#include <wayland-client.hpp>" << std::endl
              << std::endl
              << "namespace wayland" << std::endl
              << "{" << std::endl;

  // forward declarations
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
