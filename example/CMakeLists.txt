# Copyright (c) 2014-2025 Philipp Kerling, Nils Christopher Brause
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# dependencies
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
include(FindPkgConfig)
pkg_check_modules(EGL REQUIRED egl)
find_library(LIBRT rt)
mark_as_advanced(LIBRT)

# examples
add_executable(dump dump.cpp)
target_link_libraries(dump wayland-client++)

add_executable(foreign_display foreign_display.cpp)
target_link_libraries(foreign_display wayland-client++)

add_executable(proxy_wrapper proxy_wrapper.cpp)
target_link_libraries(proxy_wrapper wayland-client++ Threads::Threads)

if (INSTALL_EXTRA_PROTOCOLS)
  add_executable(clipboard clipboard.cpp shm_common.cpp)
  target_link_libraries(clipboard wayland-client++ wayland-client-extra++ wayland-cursor++)

  if (INSTALL_UNSTABLE_PROTOCOLS)
    add_executable(shm shm.cpp shm_common.cpp)
    target_link_libraries(shm wayland-client++ wayland-client-extra++ wayland-client-unstable++ wayland-cursor++)
    if(LIBRT)
      target_link_libraries(shm "${LIBRT}")
    endif()

    add_executable(egl egl.cpp)
    target_link_libraries(egl wayland-client++ wayland-client-extra++ wayland-client-unstable++ wayland-cursor++ wayland-egl++ "${OPENGL_LIBRARY}" ${EGL_LDFLAGS})
    target_include_directories(egl PUBLIC "${OPENGL_INCLUDE_DIR}")
    target_compile_options(egl PUBLIC ${EGL_CFLAGS})
  endif()

  pkg_check_modules(LIBDECOR libdecor-0)
  if(LIBDECOR_FOUND)
    add_executable(decor decor.cpp shm_common.cpp)
    target_link_libraries(decor PRIVATE wayland-client++ wayland-client-extra++ wayland-cursor++ ${LIBDECOR_LINK_LIBRARIES})
    target_include_directories(decor PRIVATE ${LIBDECOR_INCLUDE_DIRS})
    target_compile_options(decor PRIVATE ${LIBDECOR_CFLAGS_OTHER})
  endif()
endif()

if(BUILD_SERVER)
  set(WAYLAND_SCANNERPP wayland-scanner++)
  set(PROTO_XML "${CMAKE_SOURCE_DIR}/example/pingpong.xml")
  set(CLIENT_PROTO_FILES "pingpong-client-protocol.hpp" "pingpong-client-protocol.cpp")
  set(SERVER_PROTO_FILES "pingpong-server-protocol.hpp" "pingpong-server-protocol.cpp")
  add_custom_command(OUTPUT ${CLIENT_PROTO_FILES} COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XML} ${CLIENT_PROTO_FILES} DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XML})
  add_custom_command(OUTPUT ${SERVER_PROTO_FILES} COMMAND "${WAYLAND_SCANNERPP}" "-s" "on" ${PROTO_XML} ${SERVER_PROTO_FILES} DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XML})
  add_executable(pingpong pingpong.cpp pingpong-client-protocol.cpp pingpong-server-protocol.cpp)
  target_link_libraries(pingpong wayland-client++ wayland-server++ Threads::Threads)
  target_include_directories(pingpong PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
endif()
