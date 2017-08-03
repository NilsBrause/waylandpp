# -*- python -*-

import os;

hostenv = Environment()

hostenv["CXX"] = os.environ.get("CXX", "g++")
hostenv["CXXFLAGS"] = "-std=c++11 -Wall -Werror -O2 -ggdb " + os.environ.get("CXXFLAGS", "")

targetenv = Environment()
targetenv["ENV"].update({"PKG_CONFIG_PATH": os.environ.get("PKG_CONFIG_PATH", "")})

targetenv["CXX"] = os.environ.get("CROSSCXX", "g++")
targetenv["CXXFLAGS"] = "-std=c++11 -Wall -Werror -O2 -ggdb " + os.environ.get("CROSSCXXFLAGS", "")

wayland_scanner = hostenv.Program("scanner/wayland-scanner++",
                                  ["scanner/scanner.cpp", "scanner/pugixml.cpp"],
                                  CPPPATH = "scanner")

hostenv.Command(["src/wayland-client-protocol.cpp",
                 "include/wayland-client-protocol.hpp"],
                ["scanner/wayland-scanner++",
                 "protocols/wayland.xml",
                 "protocols/presentation-time.xml",
                 "protocols/viewporter.xml"],
                "./scanner/wayland-scanner++ \
                protocols/wayland.xml \
                protocols/presentation-time.xml \
                protocols/viewporter.xml \
                include/wayland-client-protocol.hpp \
                src/wayland-client-protocol.cpp")

wayland_client_env = targetenv.Clone().ParseConfig("pkg-config --cflags --libs 'wayland-client >= 1.11.0'")
wayland_client = wayland_client_env.SharedLibrary("src/wayland-client++",
                                                 ["src/wayland-client.cpp",
                                                  "src/wayland-client-protocol.cpp",
                                                  "src/wayland-util.cpp"],
                                                  CPPPATH = wayland_client_env.get("CPPPATH", []) + ["include"])

wayland_egl_env = targetenv.Clone().ParseConfig("pkg-config --cflags --libs wayland-egl")
wayland_egl = wayland_egl_env.SharedLibrary("src/wayland-egl++",
                                            "src/wayland-egl.cpp",
                                            CPPPATH = wayland_egl_env.get("CPPPATH", []) + ["include"])

wayland_cursor_env = targetenv.Clone().ParseConfig("pkg-config --cflags --libs wayland-cursor")
wayland_cursor = wayland_cursor_env.SharedLibrary("src/wayland-cursor++",
                                                  "src/wayland-cursor.cpp",
                                                  CPPPATH = wayland_cursor_env.get("CPPPATH", []) + ["include"])

prefix = os.environ.get("PREFIX", "/usr/local")

targetenv.Install(os.path.join(prefix, "lib"), [wayland_client, wayland_egl, wayland_cursor])
targetenv.Install(os.path.join(prefix, "include"), ["include/wayland-client-protocol.hpp",
                                                    "include/wayland-client.hpp",
                                                    "include/wayland-cursor.hpp",
                                                    "include/wayland-egl.hpp",
                                                    "include/wayland-util.hpp"])
hostenv.Install(os.path.join(prefix, "bin"), [wayland_scanner])
hostenv.Install(os.path.join(prefix, "share/waylandpp"), ["protocols/wayland.xml",
                                                          "protocols/presentation-time.xml",
                                                          "protocols/viewporter.xml"])

targetenv.Alias("install", os.path.join(prefix, "lib"))
targetenv.Alias("install", os.path.join(prefix, "include"))
hostenv.Alias("install", os.path.join(prefix, "bin"))
hostenv.Alias("install", os.path.join(prefix, "share/waylandpp"))
