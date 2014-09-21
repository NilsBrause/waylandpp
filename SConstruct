# -*- python -*-

import os;

env = Environment()

env["CXX"] = os.environ.get("CXX", "g++")
env["CXXFLAGS"] = "-std=c++11 -Wall -Werror -O2 -ggdb"

env.Program("scanner/scanner",
            ["scanner/scanner.cpp", "scanner/pugixml.cpp"],
            CPPPATH = "scanner")

env.Command(["src/wayland-client-protocol.cpp",
             "include/wayland-client-protocol.hpp"],
            ["scanner/scanner", "scanner/wayland.xml"],
            "./scanner/scanner scanner/wayland.xml \
            include/wayland-client-protocol.hpp \
            src/wayland-client-protocol.cpp")

env.SharedLibrary("src/wayland-client++",
                  ["src/wayland-client.cpp", "src/wayland-client-protocol.cpp"],
                  CPPPATH = "include",
                  LIBS = ["wayland-client"])

env.SharedLibrary("src/wayland-egl++",
                  "src/wayland-egl.cpp",
                  CPPPATH = "include",
                  LIBS = ["wayland-egl"])

env.SharedLibrary("src/wayland-cursor++",
                  "src/wayland-cursor.cpp",
                  CPPPATH = "include",
                  LIBS = ["wayland-cursor"])

env.Program("example/test",
            "example/test.cpp",
            CPPPATH = ["include", "example"],
            LIBS =  ["wayland-client++", "wayland-egl++", "wayland-cursor++", "EGL", "GL"],
            LIBPATH = "src")
