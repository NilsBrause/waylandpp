project(waylandpp)
cmake_minimum_required(VERSION 3.0)

file(STRINGS "VERSION" WAYLANDPP_VERSION LIMIT_COUNT 1)
string(REGEX MATCHALL "([0-9]+)\\.([0-9]+)\\.([0-9]+)" WAYLANDPP_VERSION_PARSED "${WAYLANDPP_VERSION}")
set(WAYLANDPP_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(WAYLANDPP_VERSION_MINOR "${CMAKE_MATCH_2}")
set(WAYLANDPP_VERSION_PATCH "${CMAKE_MATCH_3}")

set(INSTALL_LIB_DIR "lib${LIB_SUFFIX}" CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
set(INSTALL_PKGCONFIG_DIR "${INSTALL_LIB_DIR}/pkgconfig" CACHE PATH "Installation directory for pkgconfig files")
set(INSTALL_SHARE_DIR share CACHE PATH "Installation directory for shared files")
set(BUILD_SCANNER ON CACHE BOOL "whether to build wayland-scanner++")
set(BUILD_LIBRARIES ON CACHE BOOL "whether to build the libraries")

set(INSTALL_SHARE_WAYLANDPP_DIR "${INSTALL_SHARE_DIR}/waylandpp")

set(CMAKE_CXX_STANDARD 11)

# variables for .pc.in files
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(bindir "${prefix}/${INSTALL_BIN_DIR}")
set(datarootdir "${prefix}/${INSTALL_SHARE_DIR}")
set(pkgdatadir "${prefix}/${INSTALL_SHARE_WAYLANDPP_DIR}")
set(libdir "${prefix}/${INSTALL_LIB_DIR}")
set(includedir "${prefix}/${INSTALL_INCLUDE_DIR}")

configure_file(Doxyfile.in Doxyfile @ONLY)

if(BUILD_SCANNER)
  add_executable(wayland-scanner++
    scanner/pugiconfig.hpp
    scanner/pugixml.hpp
    scanner/pugixml.cpp
    scanner/scanner.cpp
  )
  configure_file(wayland-scanner++.pc.in wayland-scanner++.pc @ONLY)
  install(TARGETS wayland-scanner++ RUNTIME DESTINATION "${INSTALL_BIN_DIR}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/wayland-scanner++.pc" DESTINATION "${INSTALL_PKGCONFIG_DIR}")
endif()

if(BUILD_LIBRARIES)
  if(CMAKE_CROSSCOMPILING OR (NOT BUILD_SCANNER))
    set(WAYLAND_SCANNERPP "WAYLAND_SCANNERPP-NOTFOUND" CACHE FILEPATH "Path to wayland-scanner++")
    if(NOT WAYLAND_SCANNERPP)
      message(SEND_ERROR "If you cross-compile or do not build the scanner, you have to specify the path to a native wayland-scanner++ executable in the WAYLAND_SCANNERPP variable.")
    endif()
  else()
    set(WAYLAND_SCANNERPP wayland-scanner++)
  endif()

  include(FindPkgConfig)
  pkg_check_modules(WAYLAND_CLIENT REQUIRED "wayland-client>=1.11.0")
  pkg_check_modules(WAYLAND_EGL REQUIRED wayland-egl)
  pkg_check_modules(WAYLAND_CURSOR REQUIRED wayland-cursor)

  set(PROTO_XMLS "${CMAKE_SOURCE_DIR}/protocols/wayland.xml" "${CMAKE_SOURCE_DIR}/protocols/presentation-time.xml" "${CMAKE_SOURCE_DIR}/protocols/viewporter.xml")
  add_custom_command(
    OUTPUT wayland-client-protocol.hpp wayland-client-protocol.cpp
    COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XMLS} wayland-client-protocol.hpp wayland-client-protocol.cpp
    DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS}
  )

  configure_file(include/wayland-version.hpp.in wayland-version.hpp @ONLY)

  function(add_pc_as_resource TARGET)
    configure_file("${TARGET}.pc.in" "${TARGET}.pc" @ONLY)
    set_target_properties("${TARGET}" PROPERTIES RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc")
  endfunction()
  function(define_library TARGET CFLAGS LDFLAGS HEADERS)
    add_library("${TARGET}" SHARED ${ARGN})
    target_compile_options("${TARGET}" PUBLIC ${CFLAGS})
    target_link_libraries("${TARGET}" ${LDFLAGS})
    set_target_properties("${TARGET}" PROPERTIES
      PUBLIC_HEADER "${HEADERS}"
      SOVERSION "${WAYLANDPP_VERSION}"
    )
    add_pc_as_resource("${TARGET}")
  endfunction()

  include_directories("include")
  # for wayland-version.hpp
  include_directories("${CMAKE_CURRENT_BINARY_DIR}")
  define_library(wayland-client++ "${WAYLAND_CLIENT_CFLAGS}" "${WAYLAND_CLIENT_LDFLAGS}" "include/wayland-client.hpp;include/wayland-util.hpp;${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol.hpp;${CMAKE_CURRENT_BINARY_DIR}/wayland-version.hpp"
    src/wayland-client.cpp
    src/wayland-util.cpp
    wayland-client-protocol.cpp
    wayland-client-protocol.hpp
  )
  define_library(wayland-egl++ "${WAYLAND_EGL_CFLAGS}" "${WAYLAND_EGL_LDFLAGS}" include/wayland-egl.hpp src/wayland-egl.cpp wayland-client-protocol.hpp)
  define_library(wayland-cursor++ "${WAYLAND_CURSOR_CFLAGS}" "${WAYLAND_CURSOR_LDFLAGS}" include/wayland-cursor.hpp src/wayland-cursor.cpp wayland-client-protocol.hpp)

  install(FILES ${PROTO_XMLS} DESTINATION "${INSTALL_SHARE_WAYLANDPP_DIR}/protocols")
  install(
    TARGETS wayland-client++ wayland-egl++ wayland-cursor++
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
    PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}"
    RESOURCE DESTINATION "${INSTALL_PKGCONFIG_DIR}"
  )
endif()
