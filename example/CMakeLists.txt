# dependencies
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)
include(FindPkgConfig)
pkg_check_modules(EGL REQUIRED egl)
find_library(LIBRT rt)
mark_as_advanced(LIBRT)

# protocols
set(PROTO_XMLS "${CMAKE_SOURCE_DIR}/protocols/xdg-shell.xml")
set(PROTO_FILES
  "wayland-extra-client-protocol.hpp"
  "wayland-extra-client-protocol.cpp")
add_custom_command(
  OUTPUT ${PROTO_FILES}
  COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XMLS} ${PROTO_FILES}
  DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS})
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# examples
add_executable(dump dump.cpp)
target_link_libraries(dump wayland-client++)

add_executable(egl egl.cpp wayland-extra-client-protocol.cpp)
target_link_libraries(egl wayland-client++ wayland-cursor++ wayland-egl++ "${OPENGL_gl_LIBRARY}" ${EGL_LDFLAGS})
target_include_directories(egl PUBLIC "${OPENGL_INCLUDE_DIR}")
target_compile_options(egl PUBLIC ${EGL_CFLAGS})

add_executable(proxy_wrapper proxy_wrapper.cpp)
target_link_libraries(proxy_wrapper wayland-client++ Threads::Threads)

add_executable(shm shm.cpp wayland-extra-client-protocol.cpp)
target_link_libraries(shm wayland-client++ wayland-cursor++)
if(LIBRT)
  target_link_libraries(shm "${LIBRT}")
endif()
