# build wayland-server++ library
set(PROTO_FILES
  "wayland-server-protocol.hpp"
  "wayland-server-protocol.cpp")
generate_cpp_server_files("${PROTO_XMLS}" "${PROTO_FILES}" "" "")
set(WAYLAND_SERVER_HEADERS
  "include/wayland-server.hpp"
  "include/wayland-util.hpp"
  "${CMAKE_CURRENT_BINARY_DIR}/wayland-server-protocol.hpp")
define_library(wayland-server++
  "${WAYLAND_SERVER_CFLAGS}"
  "${WAYLAND_SERVER_LINK_LIBRARIES}"
  "${WAYLAND_SERVER_HEADERS}"
  src/wayland-server.cpp
  src/wayland-util.cpp
  wayland-server-protocol.cpp
  wayland-server-protocol.hpp)
# Report undefined references only for the base library.
if(${CMAKE_VERSION} VERSION_GREATER "3.14.0")
  target_link_options(wayland-server++ PRIVATE "-Wl,--no-undefined")
endif()

if(ENABLE_WAYLAND_PROTOCOLS)
  # build wayland-server-extra++ library
  set(PROTO_FILES_EXTRA
    "wayland-server-protocol-extra.hpp"
    "wayland-server-protocol-extra.cpp")
  generate_cpp_server_files("${PROTO_XMLS_EXTRA}" "${PROTO_FILES_EXTRA}" "-x;wayland-server-protocol.hpp" "")
  set(WAYLAND_SERVER_EXTRA_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-server-protocol-extra.hpp")
  define_library(wayland-server-extra++
    "${WAYLAND_SERVER_CFLAGS}"
    "${WAYLAND_SERVER_LINK_LIBRARIES}"
    "${WAYLAND_SERVER_EXTRA_HEADERS}"
    wayland-server-protocol-extra.cpp wayland-server-protocol-extra.hpp wayland-server-protocol.hpp)
  target_link_libraries(wayland-server-extra++ INTERFACE wayland-server++)

  # build wayland-server-unstable++ library
  set(PROTO_FILES_UNSTABLE
    "wayland-server-protocol-unstable.hpp"
    "wayland-server-protocol-unstable.cpp")
  generate_cpp_server_files("${PROTO_XMLS_UNSTABLE}" "${PROTO_FILES_UNSTABLE}" "-x;wayland-server-protocol-extra.hpp" "${PROTO_FILES_EXTRA}")
  set(WAYLAND_SERVER_UNSTABLE_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-server-protocol-unstable.hpp")
  define_library(wayland-server-unstable++
    "${WAYLAND_SERVER_CFLAGS}"
    "${WAYLAND_SERVER_LINK_LIBRARIES}"
    "${WAYLAND_SERVER_UNSTABLE_HEADERS}"
    wayland-server-protocol-unstable.cpp
    wayland-server-protocol-unstable.hpp
    wayland-server-protocol.hpp)
  target_link_libraries(wayland-server-unstable++ INTERFACE wayland-server-extra++)

  # build wayland-server-staging++ library
  set(PROTO_FILES_STAGING
    "wayland-server-protocol-staging.hpp"
    "wayland-server-protocol-staging.cpp")
  generate_cpp_server_files("${PROTO_XMLS_STAGING}" "${PROTO_FILES_STAGING}" "-x;wayland-server-protocol-extra.hpp" "${PROTO_FILES_EXTRA}")
  set(WAYLAND_SERVER_STAGING_HEADERS
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-server-protocol-staging.hpp")
  define_library(wayland-server-staging++
    "${WAYLAND_SERVER_CFLAGS}"
    "${WAYLAND_SERVER_LINK_LIBRARIES}"
    "${WAYLAND_SERVER_STAGING_HEADERS}"
    wayland-server-protocol-staging.cpp
    wayland-server-protocol-staging.hpp
    wayland-server-protocol.hpp)
endif()