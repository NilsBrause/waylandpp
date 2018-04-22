project(waylandpp)
cmake_minimum_required(VERSION 3.1)

# packages
include(FindPkgConfig)
include(GNUInstallDirs)
find_package(Doxygen)

# version information
set(WAYLANDPP_VERSION_MAJOR 0)
set(WAYLANDPP_VERSION_MINOR 2)
set(WAYLANDPP_VERSION_PATCH 3)
set(WAYLANDPP_VERSION "${WAYLANDPP_VERSION_MAJOR}.${WAYLANDPP_VERSION_MINOR}.${WAYLANDPP_VERSION_PATCH}")
configure_file(include/wayland-version.hpp.in wayland-version.hpp @ONLY)

# path shorthands
set(INSTALL_FULL_PKGCONFIGDIR "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig")
set(INSTALL_FULL_PKGDATADIR "${CMAKE_INSTALL_FULL_DATADIR}/waylandpp")

# user options
set(BUILD_SCANNER ON CACHE BOOL "whether to build wayland-scanner++")
set(BUILD_LIBRARIES ON CACHE BOOL "whether to build the libraries")
set(BUILD_DOCUMENTATION ${DOXYGEN_FOUND} CACHE BOOL "Create and install the HTML based API documentation (requires Doxygen)")
set(BUILD_EXAMPLES OFF CACHE BOOL "whether to build the examples (requires BUILD_LIBRARIES to be ON)")

# variables for .pc.in files
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(bindir "${CMAKE_INSTALL_FULL_BINDIR}")
set(datarootdir "${CMAKE_INSTALL_FULL_DATAROOTDIR}")
set(pkgdatadir "${INSTALL_FULL_PKGDATADIR}")
set(libdir "${CMAKE_INSTALL_FULL_LIBDIR}")
set(includedir "${CMAKE_INSTALL_FULL_INCLUDEDIR}")

# C++ 11
set(CMAKE_CXX_STANDARD 11)

# build scanner
if(BUILD_SCANNER)
  add_executable(wayland-scanner++ scanner/scanner.cpp scanner/pugixml.cpp)
  configure_file(wayland-scanner++.pc.in wayland-scanner++.pc @ONLY)
  install(TARGETS wayland-scanner++ RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/wayland-scanner++.pc" DESTINATION "${INSTALL_FULL_PKGCONFIGDIR}")
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
  set(PROTO_XMLS "${CMAKE_SOURCE_DIR}/protocols/wayland.xml")
  set(PROTO_XMLS_EXTRA "${CMAKE_SOURCE_DIR}/protocols/presentation-time.xml"
    "${CMAKE_SOURCE_DIR}/protocols/viewporter.xml"
    "${CMAKE_SOURCE_DIR}/protocols/xdg-shell.xml")
  set(PROTO_FILES
    "wayland-client-protocol.hpp"
    "wayland-client-protocol.cpp")
  set(PROTO_FILES_EXTRA
    "wayland-client-protocol-extra.hpp"
    "wayland-client-protocol-extra.cpp")
  add_custom_command(
    OUTPUT ${PROTO_FILES}
    COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XMLS} ${PROTO_FILES}
    DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS})
  add_custom_command(
    OUTPUT ${PROTO_FILES_EXTRA}
    COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_EXTRA} ${PROTO_FILES_EXTRA}
    DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_EXTRA})

  # library building helper functions
  function(define_library TARGET CFLAGS LDFLAGS HEADERS)
    add_library("${TARGET}" SHARED ${ARGN})
    target_include_directories("${TARGET}" PUBLIC "include" "${CMAKE_CURRENT_BINARY_DIR}") # latter for wayland-version.hpp
    target_compile_options("${TARGET}" PUBLIC ${CFLAGS})
    target_link_libraries("${TARGET}" ${LDFLAGS})
    set_target_properties("${TARGET}" PROPERTIES
      PUBLIC_HEADER "${HEADERS}"
      SOVERSION "${WAYLANDPP_VERSION_MAJOR}.${WAYLANDPP_VERSION_MINOR}")
    configure_file("${TARGET}.pc.in" "${TARGET}.pc" @ONLY)
    set_target_properties("${TARGET}" PROPERTIES RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc")
  endfunction()

  define_library(wayland-client++ "${WAYLAND_CLIENT_CFLAGS}" "${WAYLAND_CLIENT_LDFLAGS}"
    "include/wayland-client.hpp;include/wayland-util.hpp;${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol.hpp;${CMAKE_CURRENT_BINARY_DIR}/wayland-version.hpp"
    src/wayland-client.cpp src/wayland-util.cpp wayland-client-protocol.cpp wayland-client-protocol.hpp)
  define_library(wayland-client-extra++ "${WAYLAND_CLIENT_CFLAGS}" "${WAYLAND_CLIENT_LDFLAGS}"
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-extra.hpp"
    wayland-client-protocol-extra.cpp wayland-client-protocol-extra.hpp wayland-client-protocol.hpp)
  define_library(wayland-egl++ "${WAYLAND_EGL_CFLAGS}" "${WAYLAND_EGL_LDFLAGS}" include/wayland-egl.hpp src/wayland-egl.cpp wayland-client-protocol.hpp)
  define_library(wayland-cursor++ "${WAYLAND_CURSOR_CFLAGS}" "${WAYLAND_CURSOR_LDFLAGS}" include/wayland-cursor.hpp src/wayland-cursor.cpp wayland-client-protocol.hpp)

  # Install libraries
  install(FILES ${PROTO_XMLS} ${PROTO_XMLS_EXTRA} DESTINATION "${INSTALL_FULL_PKGDATADIR}/protocols")
  install(TARGETS wayland-client++ wayland-client-extra++ wayland-egl++ wayland-cursor++
    LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}"
    RESOURCE DESTINATION "${INSTALL_FULL_PKGCONFIGDIR}")
endif()

if(BUILD_EXAMPLES)
  if(NOT BUILD_LIBRARIES)
    message(FATAL_ERROR "Cannot build examples without building libraries")
  endif()
  add_subdirectory(example)
endif()

if(BUILD_DOCUMENTATION)
  if(NOT DOXYGEN_FOUND)
    message(FATAL_ERROR "Doxygen is needed to build the documentation.")
  endif()

  set(WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc")
  set(WAYLANDPP_BUILD_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  configure_file(Doxyfile.in Doxyfile @ONLY)

  add_custom_command(
    OUTPUT "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/html/index.html"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" ${PROTO_FILES} ${PROTO_FILES_EXTRA}
    COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    COMMENT "Generating API documentation with Doxygen"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM
  )
  add_custom_target(doc ALL DEPENDS "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/html/index.html")

  install(DIRECTORY "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/man/" DESTINATION ${CMAKE_INSTALL_FULL_MANDIR})
  install(DIRECTORY "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/html" "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/latex" DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR})
endif()
