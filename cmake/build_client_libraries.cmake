# build wayland-client++ library
set(PROTO_FILES
  "wayland-client-protocol.hpp"
  "wayland-client-protocol.cpp")
generate_cpp_client_files("${PROTO_XMLS}" "${PROTO_FILES}" "" "")
set(WAYLAND_CLIENT_HEADERS
  "include/wayland-client.hpp"
  "include/wayland-util.hpp"
  "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol.hpp"
  "${CMAKE_CURRENT_BINARY_DIR}/wayland-version.hpp")
define_library(wayland-client++
  "${WAYLAND_CLIENT_CFLAGS}"
  "${WAYLAND_CLIENT_LINK_LIBRARIES}"
  "${WAYLAND_CLIENT_HEADERS}"
  src/wayland-client.cpp
  src/wayland-util.cpp
  wayland-client-protocol.cpp
  wayland-client-protocol.hpp)
# Report undefined references only for the base library.
if(${CMAKE_VERSION} VERSION_GREATER "3.14.0")
  target_link_options(wayland-client++ PRIVATE "-Wl,--no-undefined")
endif()

if(INSTALL_EXTRA_PROTOCOLS)
  # build wayland-extra++ library
  set(PROTO_FILES_EXTRA
    "wayland-client-protocol-extra.hpp"
    "wayland-client-protocol-extra.cpp")
  generate_cpp_client_files("${PROTO_XMLS_EXTRA}" "${PROTO_FILES_EXTRA}" "" "")
  set(WAYLAND_CLIENT_EXTRA_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-extra.hpp")
  define_library(wayland-client-extra++
    "${WAYLAND_CLIENT_CFLAGS}"
    "${WAYLAND_CLIENT_LINK_LIBRARIES}"
    "${WAYLAND_CLIENT_EXTRA_HEADERS}"
    wayland-client-protocol-extra.cpp
    wayland-client-protocol-extra.hpp
    wayland-client-protocol.hpp)
  target_link_libraries(wayland-client-extra++ INTERFACE wayland-client++)
endif()

if(INSTALL_UNSTABLE_PROTOCOLS)
  # build wayland-client-unstable++ library
  set(PROTO_FILES_UNSTABLE
    "wayland-client-protocol-unstable.hpp"
    "wayland-client-protocol-unstable.cpp")
  generate_cpp_client_files("${PROTO_XMLS_UNSTABLE}" "${PROTO_FILES_UNSTABLE}" "-x;wayland-client-protocol-extra.hpp" "${PROTO_FILES_EXTRA}")
  set(WAYLAND_CLIENT_UNSTABLE_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-unstable.hpp")
  define_library(wayland-client-unstable++
    "${WAYLAND_CLIENT_CFLAGS}"
    "${WAYLAND_CLIENT_LINK_LIBRARIES}"
    "${WAYLAND_CLIENT_UNSTABLE_HEADERS}"
    wayland-client-protocol-unstable.cpp
    wayland-client-protocol-unstable.hpp
    wayland-client-protocol.hpp)
  target_link_libraries(wayland-client-unstable++ INTERFACE wayland-client-extra++)
endif()

if(INSTALL_STAGING_PROTOCOLS)
  # build wayland-client-staging++ library
  set(PROTO_FILES_STAGING
    "wayland-client-protocol-staging.hpp"
    "wayland-client-protocol-staging.cpp")
  generate_cpp_client_files("${PROTO_XMLS_STAGING}" "${PROTO_FILES_STAGING}" "-x;wayland-client-protocol-extra.hpp" "${PROTO_FILES_EXTRA}")
  set(WAYLAND_CLIENT_STAGING_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-staging.hpp")
  define_library(wayland-client-staging++
    "${WAYLAND_CLIENT_CFLAGS}"
    "${WAYLAND_CLIENT_LINK_LIBRARIES}"
    "${WAYLAND_CLIENT_STAGING_HEADERS}"
    wayland-client-protocol-staging.cpp
    wayland-client-protocol-staging.hpp
    wayland-client-protocol.hpp)
endif()

if(INSTALL_EXPERIMENTAL_PROTOCOLS)
  # build wayland-client-experimental++ library
  set(PROTO_FILES_EXPERIMENTAL
    "wayland-client-protocol-experimental.hpp"
    "wayland-client-protocol-experimental.cpp")
  generate_cpp_client_files("${PROTO_XMLS_EXPERIMENTAL}" "${PROTO_FILES_EXPERIMENTAL}" "-x;wayland-client-protocol-extra.hpp;-x;wayland-client-protocol-unstable.hpp" "${PROTO_FILES_EXTRA}" "${PROTO_FILES_UNSTABLE}")
  set(WAYLAND_CLIENT_EXPERIMENTAL_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-experimental.hpp")
  define_library(wayland-client-experimental++
    "${WAYLAND_CLIENT_CFLAGS}"
    "${WAYLAND_CLIENT_LINK_LIBRARIES}"
    "${WAYLAND_CLIENT_EXPERIMENTAL_HEADERS}"
    wayland-client-protocol-experimental.cpp
    wayland-client-protocol-experimental.hpp
    wayland-client-protocol.hpp)
endif()

# build wayland-egl++ library
define_library(wayland-egl++
  "${WAYLAND_EGL_CFLAGS}"
  "${WAYLAND_EGL_LIBRARIES}"
  include/wayland-egl.hpp
  src/wayland-egl.cpp
  wayland-client-protocol.hpp)
target_link_libraries(wayland-egl++ INTERFACE wayland-client++)

# build wayland-cursor++ library
define_library(wayland-cursor++
  "${WAYLAND_CURSOR_CFLAGS}"
  "${WAYLAND_CURSOR_LINK_LIBRARIES}"
  include/wayland-cursor.hpp
  src/wayland-cursor.cpp
  wayland-client-protocol.hpp)
target_link_libraries(wayland-cursor++ INTERFACE wayland-client++)

if(INSTALL_WLR_PROTOCOLS)
  set(PROTO_FILES_WLR
    "wayland-client-protocol-wlr.hpp"
    "wayland-client-protocol-wlr.cpp")
  generate_cpp_client_files("${PROTO_XMLS_WLR}" "${PROTO_FILES_WLR}" "-x;wayland-client-protocol-extra.hpp" "")
  set(WAYLAND_CLIENT_WLR_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-extra.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-wlr.hpp")
  define_library(wayland-client-wlr++
    "${WAYLAND_CLIENT_CFLAGS}"
    "${WAYLAND_CLIENT_LIBRARIES};wayland-client-extra++"
    "${WAYLAND_CLIENT_WLR_HEADERS}"
    wayland-client-protocol-wlr.cpp 
    wayland-client-protocol-wlr.hpp
    wayland-client-protocol.hpp)
  target_link_libraries(wayland-client-wlr++ INTERFACE wayland-client-extra++)
endif()
if(INSTALL_PLASMA_PROTOCOLS)
  set(PROTO_FILES_PLASMA
    "wayland-client-protocol-plasma.hpp"
    "wayland-client-protocol-plasma.cpp")
  generate_cpp_client_files("${PROTO_XMLS_PLASMA}" "${PROTO_FILES_PLASMA}" "-x;wayland-client-protocol.hpp" "")
  set(WAYLAND_CLIENT_PLASMA_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-plasma.hpp")
define_library(wayland-client-plasma++
  "${WAYLAND_CLIENT_CFLAGS}"
  "${WAYLAND_CLIENT_LIBRARIES}"
  "${WAYLAND_CLIENT_PLASMA_HEADERS}"
  wayland-client-protocol-plasma.cpp
  wayland-client-protocol-plasma.hpp
  wayland-client-protocol.hpp)
  target_link_libraries(wayland-client-plasma++ INTERFACE wayland-client++)
endif()
