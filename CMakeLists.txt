project(waylandpp)
cmake_minimum_required(VERSION 3.0)

# packages
include(FindPkgConfig)
find_package(Doxygen)

# read version information
file(STRINGS "VERSION" WAYLANDPP_VERSION LIMIT_COUNT 1)
string(REGEX MATCHALL "([0-9]+)\\.([0-9]+)\\.([0-9]+)" WAYLANDPP_VERSION_PARSED "${WAYLANDPP_VERSION}")
set(WAYLANDPP_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(WAYLANDPP_VERSION_MINOR "${CMAKE_MATCH_2}")
set(WAYLANDPP_VERSION_PATCH "${CMAKE_MATCH_3}")
configure_file(include/wayland-version.hpp.in wayland-version.hpp @ONLY)

# user options
set(INSTALL_LIB_DIR "lib${LIB_SUFFIX}" CACHE PATH "Installation directory for libraries")
set(INSTALL_BIN_DIR bin CACHE PATH "Installation directory for executables")
set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
set(INSTALL_PKGCONFIG_DIR "${INSTALL_LIB_DIR}/pkgconfig" CACHE PATH "Installation directory for pkgconfig files")
set(INSTALL_SHARE_DIR share CACHE PATH "Installation directory for shared files")
set(INSTALL_SHARE_WAYLANDPP_DIR "${INSTALL_SHARE_DIR}/waylandpp")
set(INSTALL_DOC_DIR "${INSTALL_SHARE_DIR}/doc/waylandpp" CACHE PATH "Installation directory for documentation")
set(INSTALL_MAN_DIR "${INSTALL_SHARE_DIR}/man" CACHE PATH "Installation directory for man pages")
set(BUILD_SCANNER ON CACHE BOOL "whether to build wayland-scanner++")
set(BUILD_LIBRARIES ON CACHE BOOL "whether to build the libraries")
set(BUILD_DOCUMENTATION ${DOXYGEN_FOUND} CACHE BOOL "Create and install the HTML based API documentation (requires Doxygen)")

# variables for .pc.in files
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(bindir "${prefix}/${INSTALL_BIN_DIR}")
set(datarootdir "${prefix}/${INSTALL_SHARE_DIR}")
set(pkgdatadir "${prefix}/${INSTALL_SHARE_WAYLANDPP_DIR}")
set(libdir "${prefix}/${INSTALL_LIB_DIR}")
set(includedir "${prefix}/${INSTALL_INCLUDE_DIR}")

# C++ 11
set(CMAKE_CXX_STANDARD 11)

# build scanner
if(BUILD_SCANNER)
  add_executable(wayland-scanner++ scanner/scanner.cpp scanner/pugixml.cpp)
  configure_file(wayland-scanner++.pc.in wayland-scanner++.pc @ONLY)
  install(TARGETS wayland-scanner++ RUNTIME DESTINATION "${INSTALL_BIN_DIR}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/wayland-scanner++.pc" DESTINATION "${INSTALL_PKGCONFIG_DIR}")
endif()

# build libraries
if(BUILD_LIBRARIES)
  # if we are crosscompiling, the scanner has to build and installed already
  if(CMAKE_CROSSCOMPILING OR (NOT BUILD_SCANNER))
    set(WAYLAND_SCANNERPP "WAYLAND_SCANNERPP-NOTFOUND" CACHE FILEPATH "Path to wayland-scanner++")
    if(NOT WAYLAND_SCANNERPP)
      message(SEND_ERROR "If you cross-compile or do not build the scanner, you have to specify the path to a native wayland-scanner++ executable in the WAYLAND_SCANNERPP variable.")
    endif()
  else()
    set(WAYLAND_SCANNERPP wayland-scanner++)
  endif()

  # required libraries
  pkg_check_modules(WAYLAND_CLIENT REQUIRED "wayland-client>=1.11.0")
  pkg_check_modules(WAYLAND_EGL REQUIRED wayland-egl)
  pkg_check_modules(WAYLAND_CURSOR REQUIRED wayland-cursor)

  # generate protocol source/headers from protocol XMLs
  set(PROTO_XMLS
    "${CMAKE_SOURCE_DIR}/protocols/wayland.xml"
    "${CMAKE_SOURCE_DIR}/protocols/presentation-time.xml"
    "${CMAKE_SOURCE_DIR}/protocols/viewporter.xml")
  set(PROTO_FILES
    "wayland-client-protocol.hpp"
    "wayland-client-protocol.cpp")
  add_custom_command(
    OUTPUT ${PROTO_FILES}
    COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XMLS} ${PROTO_FILES}
    DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS})

  # library building helper functions
  function(define_library TARGET CFLAGS LDFLAGS HEADERS)
    add_library("${TARGET}" SHARED ${ARGN})
    target_include_directories("${TARGET}" PUBLIC "include" "${CMAKE_CURRENT_BINARY_DIR}") # latter for wayland-version.hpp
    target_compile_options("${TARGET}" PUBLIC ${CFLAGS})
    target_link_libraries("${TARGET}" ${LDFLAGS})
    set_target_properties("${TARGET}" PROPERTIES
      PUBLIC_HEADER "${HEADERS}"
      SOVERSION "${WAYLANDPP_VERSION}")
    configure_file("${TARGET}.pc.in" "${TARGET}.pc" @ONLY)
    set_target_properties("${TARGET}" PROPERTIES RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc")
  endfunction()

  define_library(wayland-client++ "${WAYLAND_CLIENT_CFLAGS}" "${WAYLAND_CLIENT_LDFLAGS}"
    "include/wayland-client.hpp;include/wayland-util.hpp;${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol.hpp;${CMAKE_CURRENT_BINARY_DIR}/wayland-version.hpp"
    src/wayland-client.cpp src/wayland-util.cpp wayland-client-protocol.cpp wayland-client-protocol.hpp)
  define_library(wayland-egl++ "${WAYLAND_EGL_CFLAGS}" "${WAYLAND_EGL_LDFLAGS}" include/wayland-egl.hpp src/wayland-egl.cpp wayland-client-protocol.hpp)
  define_library(wayland-cursor++ "${WAYLAND_CURSOR_CFLAGS}" "${WAYLAND_CURSOR_LDFLAGS}" include/wayland-cursor.hpp src/wayland-cursor.cpp wayland-client-protocol.hpp)

  # Install libraries
  install(FILES ${PROTO_XMLS} DESTINATION "${INSTALL_SHARE_WAYLANDPP_DIR}/protocols")
  install(TARGETS wayland-client++ wayland-egl++ wayland-cursor++
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
    PUBLIC_HEADER DESTINATION "${INSTALL_INCLUDE_DIR}"
    RESOURCE DESTINATION "${INSTALL_PKGCONFIG_DIR}")
endif()

if(BUILD_DOCUMENTATION)
  if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is needed to build the documentation.")
  endif()

  configure_file(Doxyfile.in Doxyfile @ONLY)

  add_custom_target(doc ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen"
   VERBATIM)

  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/man/ DESTINATION ${INSTALL_MAN_DIR})
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html ${CMAKE_CURRENT_BINARY_DIR}/doc/latex DESTINATION ${INSTALL_DOC_DIR})
endif()
