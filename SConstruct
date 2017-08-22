# -*- python -*-

import os;

VERSION_MAJOR = 0
VERSION_MINOR = 1
VERSION_PATCH = 0
VERSION = "{}.{}.{}".format(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH)

hostenv = Environment(tools = ["default", "textfile"])

hostenv["CXX"] = os.environ.get("CXX", "g++")
hostenv["CXXFLAGS"] = "-std=c++11 -Wall -Werror -O2 -ggdb " + os.environ.get("CXXFLAGS", "")

targetenv = Environment()
targetenv["ENV"].update({k: v for k, v in os.environ.items() if k.startswith("PKG_CONFIG")})
pkg_config = os.environ.get("PKG_CONFIG", "pkg-config")

targetenv["CXX"] = os.environ.get("CROSSCXX", "g++")
targetenv["CXXFLAGS"] = "-std=c++11 -Wall -Werror -O2 -ggdb " + os.environ.get("CROSSCXXFLAGS", "")
targetenv["SHLIBVERSION"] = VERSION

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

version_subst = hostenv.Substfile("include/wayland-version.hpp.in", SUBST_DICT = {
  "@WAYLANDPP_VERSION_MAJOR@": VERSION_MAJOR,
  "@WAYLANDPP_VERSION_MINOR@": VERSION_MINOR,
  "@WAYLANDPP_VERSION_PATCH@": VERSION_PATCH,
  "@WAYLANDPP_VERSION@": VERSION
})

def TryPkgConfig(env, library, version=None):
  library_version_str = library if (version is None) else "{} >= {}".format(library, version)
  command = "{} --cflags --libs '{}'".format(pkg_config, library_version_str)
  ret = env.Execute(command)
  if ret == 0:
    env.ParseConfig(command)
  else:
    print("Could not configure library {} via pkg-config. Trying to continue with default paths and just the library name.".format(library))
    env["LIBS"] = env.get("LIBS", []) + [library]

wayland_client_env = targetenv.Clone()
TryPkgConfig(wayland_client_env, "wayland-client", "1.11.0")
wayland_client = wayland_client_env.SharedLibrary("src/wayland-client++",
                                                 ["src/wayland-client.cpp",
                                                  "src/wayland-client-protocol.cpp",
                                                  "src/wayland-util.cpp"],
                                                  CPPPATH = wayland_client_env.get("CPPPATH", []) + ["include"])

wayland_egl_env = targetenv.Clone()
TryPkgConfig(wayland_egl_env, "wayland-egl")
wayland_egl = wayland_egl_env.SharedLibrary("src/wayland-egl++",
                                            "src/wayland-egl.cpp",
                                            CPPPATH = wayland_egl_env.get("CPPPATH", []) + ["include"])

wayland_cursor_env = targetenv.Clone()
TryPkgConfig(wayland_cursor_env, "wayland-cursor")
wayland_cursor = wayland_cursor_env.SharedLibrary("src/wayland-cursor++",
                                                  "src/wayland-cursor.cpp",
                                                  CPPPATH = wayland_cursor_env.get("CPPPATH", []) + ["include"])

prefix = os.environ.get("PREFIX", "/usr/local")

pc_subst = {
  "@prefix@": prefix,
  "@bindir@": '$${prefix}/bin',
  "@includedir@": '$${prefix}/include',
  "@libdir@": '$${prefix}/lib',
  "@datarootdir@": '$${prefix}/share',
  "@pkgdatadir@": '$${datarootdir}/waylandpp',
  "@WAYLANDPP_VERSION@": VERSION
}

wayland_client_pc = hostenv.Substfile("wayland-client++.pc.in", SUBST_DICT = pc_subst)
wayland_egl_pc = hostenv.Substfile("wayland-egl++.pc.in", SUBST_DICT = pc_subst)
wayland_cursor_pc = hostenv.Substfile("wayland-cursor++.pc.in", SUBST_DICT = pc_subst)
wayland_scanner_pc = hostenv.Substfile("wayland-scanner++.pc.in", SUBST_DICT = pc_subst)

targetenv.InstallVersionedLib(os.path.join(prefix, "lib"), [wayland_client,
                                                            wayland_egl,
                                                            wayland_cursor])
targetenv.Install(os.path.join(prefix, "include"), ["include/wayland-client-protocol.hpp",
                                                    "include/wayland-client.hpp",
                                                    "include/wayland-cursor.hpp",
                                                    "include/wayland-egl.hpp",
                                                    "include/wayland-util.hpp",
                                                    version_subst])
targetenv.Install(os.path.join(prefix, "lib", "pkgconfig"), [wayland_client_pc, wayland_egl_pc, wayland_cursor_pc, wayland_scanner_pc])
hostenv.Install(os.path.join(prefix, "bin"), [wayland_scanner])
hostenv.Install(os.path.join(prefix, "share/waylandpp"), ["protocols/wayland.xml",
                                                          "protocols/presentation-time.xml",
                                                          "protocols/viewporter.xml"])

targetenv.Alias("install", os.path.join(prefix, "lib"))
targetenv.Alias("install", os.path.join(prefix, "include"))
hostenv.Alias("install", os.path.join(prefix, "bin"))
hostenv.Alias("install", os.path.join(prefix, "share/waylandpp"))
