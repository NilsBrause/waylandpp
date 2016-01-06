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
            ["scanner/scanner", "protocols/wayland.xml"],
            "./scanner/scanner protocols/wayland.xml \
            include/wayland-client-protocol.hpp \
            src/wayland-client-protocol.cpp")

wayland_client = env.SharedLibrary("src/wayland-client++",
                                   ["src/wayland-client.cpp",
                                    "src/wayland-client-protocol.cpp",
                                    "src/wayland-util.cpp"],
                                   CPPPATH = "include",
                                   LIBS = ["wayland-client"])

wayland_egl = env.SharedLibrary("src/wayland-egl++",
                                "src/wayland-egl.cpp",
                                CPPPATH = "include",
                                LIBS = ["wayland-egl"])

wayland_cursor = env.SharedLibrary("src/wayland-cursor++",
                                   "src/wayland-cursor.cpp",
                                   CPPPATH = "include",
                                   LIBS = ["wayland-cursor"])

prefix = os.environ.get("PREFIX", "/usr/local")

env.Install(os.path.join(prefix, "lib"), [wayland_client, wayland_egl, wayland_cursor])
env.Install(os.path.join(prefix, "include"), ["include/wayland-client-protocol.hpp",
                                              "include/wayland-client.hpp",
                                              "include/wayland-cursor.hpp",
                                              "include/wayland-egl.hpp",
                                              "include/wayland-util.hpp"])

env.Alias("install", os.path.join(prefix, "lib"))
env.Alias("install", os.path.join(prefix, "include"))
