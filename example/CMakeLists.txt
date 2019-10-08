# Copyright (c) 2014-2019 Philipp Kerling, Nils Christopher Brause
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

add_executable(egl egl.cpp)
target_link_libraries(egl wayland-client++ wayland-client-extra++ wayland-cursor++ wayland-egl++ "${OPENGL_LIBRARY}" ${EGL_LDFLAGS})
target_include_directories(egl PUBLIC "${OPENGL_INCLUDE_DIR}")
target_compile_options(egl PUBLIC ${EGL_CFLAGS})

add_executable(foreign_display foreign_display.cpp)
target_link_libraries(foreign_display wayland-client++)

add_executable(proxy_wrapper proxy_wrapper.cpp)
target_link_libraries(proxy_wrapper wayland-client++ Threads::Threads)

add_executable(shm shm.cpp)
target_link_libraries(shm wayland-client++ wayland-client-extra++ wayland-cursor++)
if(LIBRT)
  target_link_libraries(shm "${LIBRT}")
endif()
