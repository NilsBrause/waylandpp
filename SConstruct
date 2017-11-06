# -*- python -*-

import os;

AddOption("--documentation",
          dest="documentation",
          action='store_true',
          help="Build and install documentation",
          default=False if WhereIs("doxygen") == None else True)

with open("VERSION", "r") as versionfile:
  VERSION = versionfile.read().strip()
VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH = VERSION.split(".")

hostenv = Environment(tools = ["default", "textfile"])
hostenv.Replace(CXX = os.environ.get("CXX", "g++"))
hostenv.Replace(CXXFLAGS = "-std=c++11 -Wall -Werror -O2 -ggdb ")
hostenv.Append(CXXFLAGS = os.environ.get("CXXFLAGS", ""))

targetenv = Environment()
targetenv.Append(ENV = {k: v for k, v in os.environ.items() if k.startswith("PKG_CONFIG")})
targetenv.Replace(CXX = os.environ.get("CROSSCXX", os.environ.get("CXX", "g++")))
targetenv.Replace(CXXFLAGS = "-std=c++11 -Wall -Werror -O2 -ggdb ")
targetenv.Append(CXXFLAGS = os.environ.get("CROSSCXXFLAGS", os.environ.get("CXXFLAGS", "")))
targetenv.Replace(CPPPATH = "include")
targetenv.Replace(SHLIBVERSION = VERSION)
targetenv.Replace(SHLIBVERSIONFLAGS = "")

pkg_config = os.environ.get("PKG_CONFIG", "pkg-config")

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

doxyfile_subst = hostenv.Substfile("Doxyfile.in", SUBST_DICT = {
  "@WAYLANDPP_VERSION@": VERSION,
  "@WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY@": "doc"
})

def TryPkgConfig(env, library, version=None):
  library_version_str = library if (version is None) else "{} >= {}".format(library, version)
  command = "{} --cflags --libs '{}'".format(pkg_config, library_version_str)
  ret = env.Execute(command)
  if ret == 0:
    env.ParseConfig(command)
  else:
    print("Could not configure library {} via pkg-config. Trying to continue with default paths and just the library name.".format(library))
    env.Append(LIBS = library)

wayland_client_env = targetenv.Clone()
TryPkgConfig(wayland_client_env, "wayland-client", "1.11.0")
wayland_client = wayland_client_env.SharedLibrary("src/wayland-client++",
                                                 ["src/wayland-client.cpp",
                                                  "src/wayland-client-protocol.cpp",
                                                  "src/wayland-util.cpp"])

wayland_egl_env = targetenv.Clone()
TryPkgConfig(wayland_egl_env, "wayland-egl")
wayland_egl = wayland_egl_env.SharedLibrary("src/wayland-egl++",
                                            "src/wayland-egl.cpp")

wayland_cursor_env = targetenv.Clone()
TryPkgConfig(wayland_cursor_env, "wayland-cursor")
wayland_cursor = wayland_cursor_env.SharedLibrary("src/wayland-cursor++",
                                                  "src/wayland-cursor.cpp")

root = os.environ.get("ROOT", "/")
prefix = os.environ.get("PREFIX", "/usr/local")
libdir = os.environ.get("LIBDIR", "lib" + os.environ.get("LIB_SUFFIX", ""))
includedir = os.environ.get("INCLUDEDIR", "include")
bindir = os.environ.get("BINDIR", "bin")
sharedir = os.environ.get("SHAREDIR", "share")
docdir = os.environ.get("DOCDIR", os.path.join(sharedir, "doc"))
mandir = os.environ.get("MANDIR", os.path.join(sharedir, "man"))

libdir = libdir.strip('/')
includedir = includedir.strip('/')
bindir = bindir.strip('/')
sharedir = sharedir.strip('/')

pc_subst = {
  "@prefix@": prefix,
  "@bindir@": os.path.join('$${prefix}', bindir),
  "@includedir@": os.path.join('$${prefix}', includedir),
  "@libdir@": os.path.join('$${prefix}', libdir),
  "@datarootdir@": os.path.join('$${prefix}', sharedir),
  "@pkgdatadir@": os.path.join('$${datarootdir}', 'waylandpp'),
  "@WAYLANDPP_VERSION@": VERSION
}

prefix = prefix.strip('/')

wayland_client_pc = hostenv.Substfile("wayland-client++.pc.in", SUBST_DICT = pc_subst)
wayland_egl_pc = hostenv.Substfile("wayland-egl++.pc.in", SUBST_DICT = pc_subst)
wayland_cursor_pc = hostenv.Substfile("wayland-cursor++.pc.in", SUBST_DICT = pc_subst)
wayland_scanner_pc = hostenv.Substfile("wayland-scanner++.pc.in", SUBST_DICT = pc_subst)

targetenv.InstallVersionedLib(os.path.join(root, prefix, libdir), [wayland_client,
                                                                  wayland_egl,
                                                                  wayland_cursor])
targetenv.Install(os.path.join(root, prefix, includedir), ["include/wayland-client-protocol.hpp",
                                                          "include/wayland-client.hpp",
                                                          "include/wayland-cursor.hpp",
                                                          "include/wayland-egl.hpp",
                                                          "include/wayland-util.hpp",
                                                          version_subst])
targetenv.Install(os.path.join(root, prefix, libdir, "pkgconfig"), [wayland_client_pc, wayland_egl_pc, wayland_cursor_pc, wayland_scanner_pc])
hostenv.Install(os.path.join(root, prefix, bindir), [wayland_scanner])
hostenv.Install(os.path.join(root, prefix, sharedir, "waylandpp"), ["protocols/wayland.xml",
                                                                   "protocols/presentation-time.xml",
                                                                   "protocols/viewporter.xml"])

targetenv.Alias("install", os.path.join(root, prefix, libdir))
targetenv.Alias("install", os.path.join(root, prefix, includedir))
hostenv.Alias("install", os.path.join(root, prefix, bindir))
hostenv.Alias("install", os.path.join(root, prefix, sharedir, "waylandpp"))

if GetOption("documentation"):
  y = hostenv.Command(["doc/html/index.html", "doc/latex/index.tex", "doc/man/man3/wayland_display_t.3"],
                      [wayland_client, wayland_egl, wayland_cursor, "Doxyfile"],
                      "doxygen >/dev/null 2>/dev/null")
  Clean(y, ["doc/html", "doc/latex", "doc/man"])
  hostenv.Install(os.path.join(root, prefix, docdir, "waylandpp"),
                  [os.path.join("doc", "html"), os.path.join("doc", "latex")])
  hostenv.Install(os.path.join(root, prefix, mandir, "man3"),
                  Glob(os.path.join("doc", "man", "man3", "*")))
  hostenv.Alias("install", os.path.join(root, prefix, docdir))
  hostenv.Alias("install", os.path.join(root, prefix, mandir))
