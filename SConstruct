# -*- python-mode -*-

env = Environment()

url = "http://cgit.freedesktop.org/wayland/wayland/plain/protocol/wayland.xml"

env.Command("scanner/wayland.xml",
            "",
            "wget -O scanner/wayland.xml -qc " + url)

cxxflags = "-std=c++11 -Wall -Werror -O0 -ggdb"

env.Program("scanner/scanner",
            ["scanner/scanner.cpp", "scanner/pugixml.cpp"],
            CPPPATH = "scanner",
            CXXFLAGS = cxxflags)

env.Command(["src/wayland-client-protocol.cpp",
             "include/wayland-client-protocol.hpp"],
            ["scanner/scanner", "scanner/wayland.xml"],
            "./scanner/scanner scanner/wayland.xml \
            include/wayland-client-protocol.hpp \
            src/wayland-client-protocol.cpp")

env.SharedLibrary("src/wayland-client++",
                  ["src/wayland-client.cpp", "src/wayland-client-protocol.cpp"],
                  CPPPATH = "include",
                  CXXFLAGS = cxxflags,
                  LIBS = ["wayland-client"])

env.SharedLibrary("src/wayland-egl++",
                  "src/wayland-egl.cpp",
                  CPPPATH = "include",
                  CXXFLAGS = cxxflags,
                  LIBS = ["wayland-egl"])

env.Program("example/test",
            "example/test.cpp",
            CPPPATH = ["include", "example"],
            CXXFLAGS = cxxflags,
            LIBS =  ["wayland-client++", "wayland-egl++", "EGL", "GL"],
            LIBPATH = "src")
