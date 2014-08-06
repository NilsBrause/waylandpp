env = Environment()

url = "http://cgit.freedesktop.org/wayland/wayland/plain/protocol/wayland.xml"

env.Command("wayland.xml",
            "",
            "wget -qc " + url)

cxxflags = "-std=c++11 -Wall -Werror -O0 -ggdb"

env.Program("scanner",
            ["main.cpp", "pugixml.cpp"],
            CPPPATH = ".",
            CXXFLAGS = cxxflags)

env.Command(["wayland.cpp", "wayland.hpp"],
            ["scanner", "wayland.xml"],
            "./scanner")

env.SharedLibrary("wayland++",
                  ["core.cpp", "wayland.cpp"],
                  CPPPATH = ".",
                  CXXFLAGS = cxxflags,
                  LIBS = ["wayland-client"])

env.Program("test",
            ["test.cpp", "egl.cpp"],
            CPPPATH = ".",
            CXXFLAGS = cxxflags,
            LIBS =  ["wayland++", "wayland-egl", "EGL", "GL"],
            LIBPATH = ".")
