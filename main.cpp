/*
 * (C) Copyright 2014 Nils Brause
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
#include <list>
#include <sstream>

#include <pugixml.hpp>

using namespace pugi;

struct argument_t
{
  std::string type;
  std::string name;
  std::string interface;
  bool allow_null;

  std::string print_type()
  {
    if(interface != "")
      return interface.substr(3, interface.size()) + "_t";
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
      return "std::vector<char>";
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

struct event_t
{
  std::string name;
  std::list<argument_t> args;

  std::string print_functional()
  {
    std::string tmp = "std::function<void(";
    for(auto arg : args)
      tmp += arg.print_type() + ", ";
    if(args.size())
      tmp = tmp.substr(0, tmp.size()-2);
    tmp += ")> " + name + ";";
    return tmp;
  }

  std::string print_dispatcher(int opcode)
  {
    std::stringstream ss;
    ss << "    case " << opcode << ":" << std::endl
       << "      if(events->" << name << ") events->" << name << "(";

    int c = 0;
    for(auto arg : args)
      {
        if(arg.type == "array")
          {
            ss << "std::vector<char>((char*)args[" << c << "].a->data, "
               << "(char*)args[" << c << "].a->data + args[" << c << "].a->size), ";
            c++;
          }
        else if(arg.type == "object" || arg.type == "new_id") // wayland/src/connection.c:886
          {
            ss << "proxy_t((wl_proxy*)args[" << c << "].o), ";
            c++;
          }
        else
          ss << "args[" << c++ << "]." << arg.print_short() << ", ";
      }
    if(args.size())
      ss.str(ss.str().substr(0, ss.str().size()-2));
    ss.seekp(0, std::ios_base::end);
    ss << ");" << std::endl
       << "      break;";
    return ss.str();
  }

  std::string print_signal_header()
  {
    std::string tmp = "std::function<void(";
    for(auto arg : args)
      tmp += arg.print_type() + ", ";
    if(args.size())
      tmp = tmp.substr(0, tmp.size()-2);
    tmp += ")> &on_" + name + "();";
    return tmp;
  }

  std::string print_signal_body(std::string interface_name)
  {
    std::string tmp = "std::function<void(";
    for(auto arg : args)
      tmp += arg.print_type() + ", ";
    if(args.size())
      tmp = tmp.substr(0, tmp.size()-2);
    tmp += ")> &" + interface_name + "_t::on_" + name + "()\n";
    tmp += "{\n";
    tmp += "  return std::static_pointer_cast<events_t>(get_events())->" + name + ";\n";
    tmp += "}";
    return tmp;
  }
};

struct request_t : public event_t
{
  argument_t ret;
  int opcode;

  std::string print_header()
  {
    std::string tmp;
    if(ret.name == "")
      tmp += "void ";
    else
      tmp += ret.print_type() + " ";
    tmp += name +  "(";

    for(auto arg : args)
      if(arg.type == "new_id")
        {
          if(arg.interface == "")
            tmp += "proxy_t &interface, uint32_t version, ";
        }
      else
        tmp += arg.print_argument() + ", ";

    if(tmp.substr(tmp.size()-2, 2) == ", ")
      tmp = tmp.substr(0, tmp.size()-2);
    tmp += ");";
    return tmp;
  }

  std::string print_body(std::string interface_name)
  {
    std::string tmp;
    if(ret.name == "")
      tmp += "void ";
    else
      tmp += ret.print_type() + " ";
    tmp += interface_name + "_t::" + name + "(";

    bool new_id_arg = false;
    for(auto arg : args)
      if(arg.type == "new_id")
        {
          if(arg.interface == "")
            {
              tmp += "proxy_t &interface, uint32_t version, ";
              new_id_arg = true;
            }
        }
      else
        tmp += arg.print_argument() + ", ";

    if(tmp.substr(tmp.size()-2, 2) == ", ")
      tmp = tmp.substr(0, tmp.size()-2);
    tmp += ")\n{\n";

    if(ret.name == "")
      tmp += "  marshal(" + std::to_string(opcode) + ", ";
    else
      {
        tmp += "  proxy_t p = marshal_constructor(" + std::to_string(opcode) + ", ";
        if(ret.interface == "")
          tmp += "interface.interface";
        else
          tmp += "&" + ret.interface + "_interface";
        tmp += ", ";
      }

    for(auto arg : args)
      {
        if(arg.type == "new_id")
          {
            if(arg.interface == "")
              tmp += "std::string(interface.interface->name), version, ";
            tmp += "NULL, ";
          }
        else
          tmp += arg.name + ", ";
      }

    tmp = tmp.substr(0, tmp.size()-2);
    tmp += ");\n";

    if(ret.name != "")
      {
        if(new_id_arg)
          tmp += "  interface = p;\n";
        tmp += "  return p;\n";
      }
    tmp += "}";
    return tmp;
  }
};

struct enum_entry_t
{
  std::string name;
  std::string value;
};

struct enumeration_t
{
  std::string name;
  std::list<enum_entry_t> entries;
  
  std::string print(std::string interface_name)
  {
    std::string tmp = "enum " + interface_name + "_" + name + "\n";
    tmp += + "  {\n";
    for(auto &entry : entries)
      tmp += "    " + interface_name + "_" + name + "_" + entry.name + " = " + entry.value + ",\n";
    tmp = tmp.substr(0, tmp.size()-2);
    tmp += "\n  };";
    return tmp;
  }
};

struct interface_t
{
  std::string version;
  std::string name;
  int destroy_opcode;
  std::list<request_t> requests;
  std::list<event_t> events;
  std::list<enumeration_t> enums;

  std::string print_forward()
  {
    std::string tmp = "class ";
    tmp += name + "_t;";
    return tmp;
  }

  std::string print_header()
  {
    std::stringstream ss;
    ss << "class " << name << "_t : public proxy_t" << std::endl
       << "{" << std::endl
       << "private:" << std::endl
       << "  struct events_t : public proxy_t::events_base_t" << std::endl
       << "  {" << std::endl;

    for(auto &event : events)
      ss << "    " << event.print_functional() << std::endl;

    ss << "  };" << std::endl
       << std::endl
       << "  static int dispatcher(const void *implementation, void *target, uint32_t opcode, const wl_message *message, wl_argument *args);" << std::endl
       << std::endl
       << "public:" << std::endl
       << "  " + name + "_t(const proxy_t &proxy);" << std::endl
       << "  " + name + "_t();" << std::endl;

    for(auto &request : requests)
      ss << "  " << request.print_header() << std::endl;

    for(auto &event : events)
      ss << "  " << event.print_signal_header() << std::endl;

    ss << "};" << std::endl
       << std::endl;
    
    for(auto &enumeration : enums)
      ss << enumeration.print(name) << std::endl
       << std::endl;

    return ss.str();
  }

  std::string print_body()
  {
    std::stringstream ss;
    ss << name << "_t::" << name << "_t(const proxy_t &p)" << std::endl
       << "  : proxy_t(p)" << std::endl
       << "{" << std::endl
       << "  add_dispatcher(dispatcher, std::shared_ptr<proxy_t::events_base_t>(new events_t));" << std::endl
       << "  set_destroy_opcode(" << destroy_opcode << ");" << std::endl
       << "  interface = &wl_" << name << "_interface;" << std::endl
       << "}" << std::endl
       << std::endl
       << name << "_t::" << name << "_t()" << std::endl
       << "{" << std::endl
       << "  interface = &wl_" << name << "_interface;" << std::endl
       << "}" << std::endl
       << std::endl;

    for(auto &request : requests)
      ss << request.print_body(name) << std::endl
         << std::endl;

    for(auto &event : events)
      ss << event.print_signal_body(name) << std::endl
         << std::endl;

    ss << "int " << name << "_t::dispatcher(const void *implementation, void *target, uint32_t opcode, const wl_message *message, wl_argument *args)" << std::endl
       << "{" << std::endl;

    if(events.size())
      {
        ss << "  const events_t *events = static_cast<const events_t*>(implementation);" << std::endl
           << "  switch(opcode)" << std::endl
           << "    {" << std::endl;
        
        int opcode = 0;
        for(auto &event : events)
          ss << event.print_dispatcher(opcode++) << std::endl;
        
        ss << "    }" << std::endl;
      }

    ss << "  return 0;" << std::endl
       << "}" << std::endl;

    return ss.str();
  }
};

int main()
{
  xml_document doc;
  doc.load_file("wayland.xml");
  xml_node protocol = doc.child("protocol");

  std::list<interface_t> interfaces;

  for(xml_node &interface : protocol.children("interface"))
    {
      interface_t iface;
      iface.destroy_opcode = -1;
      iface.name = interface.attribute("name").value();
      if(iface.name == "wl_display") continue; // skip in favor of hand written version
      iface.name = iface.name.substr(3, iface.name.size());
      iface.version = interface.attribute("version").value();

      int opcode = 0; // Opcodes are in order of the XML. (Sadly undocumented)
      for(xml_node &request : interface.children("request"))
        {
          request_t req;
          req.opcode = opcode++;
          req.name = request.attribute("name").value();
          // destruction takes place through the class destuctor
          if(req.name == "destroy")
            {
              iface.destroy_opcode = req.opcode;
              continue;
            }
          for(xml_node &argument : request.children("arg"))
            {
              argument_t arg;
              arg.type = argument.attribute("type").value();
              arg.name = argument.attribute("name").value();
              arg.interface = argument.attribute("interface") ? argument.attribute("interface").value() : "";
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
          for(xml_node &argument : event.children("arg"))
            {
              argument_t arg;
              arg.type = argument.attribute("type").value();
              arg.name = argument.attribute("name").value();
              arg.interface = argument.attribute("interface") ? argument.attribute("interface").value() : "";
              arg.allow_null = argument.attribute("interface") && std::string(argument.attribute("interface").value()) == "true";
              ev.args.push_back(arg);
            }
          iface.events.push_back(ev);
        }

      for(xml_node &enumeration : interface.children("enum"))
        {
          enumeration_t enu;
          enu.name = enumeration.attribute("name").value();
          for(xml_node entry = enumeration.child("entry"); entry;
              entry = entry.next_sibling("entry"))
            {
              enum_entry_t enum_entry;
              enum_entry.name = entry.attribute("name").value();
              enum_entry.value = entry.attribute("value").value();
              enu.entries.push_back(enum_entry);
            }
          iface.enums.push_back(enu);
        }
          
      interfaces.push_back(iface);
    }

  std::fstream wayland_hpp("wayland.hpp", std::ios_base::out | std::ios_base::trunc);
  std::fstream wayland_cpp("wayland.cpp", std::ios_base::out | std::ios_base::trunc);

  // header intro
  wayland_hpp << "#ifndef WAYLAND_HPP" << std::endl
              << "#define WAYLAND_HPP" << std::endl
              << std::endl
              << "#include <array>" << std::endl
              << "#include <functional>" << std::endl
              << "#include <memory>" << std::endl
              << "#include <string>" << std::endl
              << "#include <vector>" << std::endl
              << std::endl
              << "#include <core.hpp>" << std::endl
              << std::endl;

  // forward declarations
  for(auto &iface : interfaces)
    wayland_hpp << iface.print_forward() << std::endl;
  wayland_hpp << std::endl;

  // class declarations
  for(auto &iface : interfaces)
    wayland_hpp << iface.print_header() << std::endl;
  wayland_hpp << std::endl
              << "#endif" << std::endl;

  // body intro
  wayland_cpp << "#include <wayland.hpp>" << std::endl
              << std::endl;
  
  // class member definitions
  for(auto &iface : interfaces)
    wayland_cpp << iface.print_body() << std::endl;
  wayland_cpp << std::endl;

  // clean up
  wayland_hpp.close();
  wayland_cpp.close();
  
  return 0;
}
