# Copyright (c) 2014-2025 Philipp Kerling, Nils Christopher Brause, Craig Andrews, Tobias Kortkamp, Balint Reczey
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

cmake_minimum_required(VERSION 3.4...4.0)
project(waylandpp VERSION 1.0.0 LANGUAGES CXX)

# packages
include(FindPkgConfig)
include(GNUInstallDirs)
include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
find_package(Doxygen)

# version information
configure_file(include/wayland-version.hpp.in wayland-version.hpp @ONLY)

# path shorthands
set(INSTALL_FULL_PKGCONFIGDIR "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig")

# user options
option(BUILD_SCANNER "whether to build wayland-scanner++" ON)
option(BUILD_LIBRARIES "whether to build the libraries" ON)
cmake_dependent_option(BUILD_SHARED_LIBS "Build shared libraries" ON
  "BUILD_LIBRARIES" OFF)
option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})
option(INSTALL_UNSTABLE_PROTOCOLS "whether to install the unstable protocols" ON)
option(INSTALL_STAGING_PROTOCOLS "whether to install the staging protocols" ON)
option(INSTALL_EXPERIMENTAL_PROTOCOLS "whether to install the experimental protocols" ON)
cmake_dependent_option(BUILD_EXAMPLES
  "whether to build the examples (requires BUILD_LIBRARIES to be ON)" OFF
  "BUILD_LIBRARIES" OFF)
option(BUILD_SERVER "whether to build the server bindings." ON)

# Do not report undefined references in libraries, since the protocol libraries cannot be used on their own.
if(CMAKE_SHARED_LINKER_FLAGS)
  string(REPLACE "-Wl,--no-undefined" " " CMAKE_SHARED_LINKER_FLAGS ${CMAKE_SHARED_LINKER_FLAGS})
endif()

# variables for .pc.in files
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(bindir "\${prefix}/${CMAKE_INSTALL_BINDIR}")
set(datarootdir "\${prefix}/${CMAKE_INSTALL_DATAROOTDIR}")
set(pkgdatadir "\${prefix}/${CMAKE_INSTALL_DATADIR}/waylandpp")
set(libdir "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")

set(install_namespace "Waylandpp")

# C++ 11
set(CMAKE_CXX_STANDARD 11)

# sets ${PREFIX}_LIBRARIES to the libraries' full path
function(pkg_libs_full_path PREFIX)
  if(${PREFIX}_FOUND)
    unset(LIBS_TMP CACHE)
    foreach(LIB ${${PREFIX}_LIBRARIES})
      unset(LIB_TMP CACHE)
      find_library(LIB_TMP ${LIB} PATHS ${${PREFIX}_LIBRARY_DIRS})
      list(APPEND LIBS_TMP ${LIB_TMP})
    endforeach()
    set(${PREFIX}_LIBRARIES ${LIBS_TMP} PARENT_SCOPE)
  endif()
endfunction()

# AdressSanitizer support
set(CMAKE_C_FLAGS_ASAN "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O0")
set(CMAKE_CXX_FLAGS_ASAN "-fsanitize=address -fno-optimize-sibling-calls -fsanitize-address-use-after-scope -fno-omit-frame-pointer -g -O0")

# build scanner
if(BUILD_SCANNER)
  pkg_check_modules(PUGIXML REQUIRED "pugixml>=1.4")
  pkg_libs_full_path(PUGIXML)
  add_executable(wayland-scanner++ scanner/scanner.cpp)
  target_link_libraries(wayland-scanner++ ${PUGIXML_LIBRARIES})
  target_compile_options(wayland-scanner++ PUBLIC ${PUGIXML_CFLAGS})
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
  if(BUILD_SERVER)
    pkg_check_modules(WAYLAND_SERVER REQUIRED "wayland-server>=1.18.0")
  endif()
  pkg_check_modules(WAYLAND_EGL REQUIRED wayland-egl)
  pkg_libs_full_path(WAYLAND_EGL)
  pkg_check_modules(WAYLAND_CURSOR REQUIRED wayland-cursor)
  pkg_libs_full_path(WAYLAND_CURSOR)

  # generate protocol source/headers from protocol XMLs
  set(PROTO_XMLS "${CMAKE_SOURCE_DIR}/protocols/wayland.xml")
  file(GLOB PROTO_XMLS_EXTRA "${CMAKE_SOURCE_DIR}/protocols/extra/*.xml")
  file(GLOB PROTO_XMLS_UNSTABLE "${CMAKE_SOURCE_DIR}/protocols/unstable/*.xml")
  file(GLOB PROTO_XMLS_STAGING "${CMAKE_SOURCE_DIR}/protocols/staging/*.xml")
  file(GLOB PROTO_XMLS_EXPERIMENTAL "${CMAKE_SOURCE_DIR}/protocols/experimental/*.xml")
  set(PROTO_FILES
    "wayland-client-protocol.hpp"
    "wayland-client-protocol.cpp")
  set(PROTO_FILES_EXTRA
    "wayland-client-protocol-extra.hpp"
    "wayland-client-protocol-extra.cpp")
  set(PROTO_FILES_UNSTABLE
    "wayland-client-protocol-unstable.hpp"
    "wayland-client-protocol-unstable.cpp")
  set(PROTO_FILES_STAGING
    "wayland-client-protocol-staging.hpp"
    "wayland-client-protocol-staging.cpp")
  set(PROTO_FILES_EXPERIMENTAL
    "wayland-client-protocol-experimental.hpp"
    "wayland-client-protocol-experimental.cpp")
  add_custom_command(
    OUTPUT ${PROTO_FILES}
    COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XMLS} ${PROTO_FILES}
    DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS})
  add_custom_command(
    OUTPUT ${PROTO_FILES_EXTRA}
    COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_EXTRA} ${PROTO_FILES_EXTRA}
    DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_EXTRA})
  add_custom_command(
    OUTPUT ${PROTO_FILES_UNSTABLE}
    COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_UNSTABLE} ${PROTO_FILES_UNSTABLE} "-x" "wayland-client-protocol-extra.hpp"
    DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_UNSTABLE} ${PROTO_FILES_EXTRA})
  add_custom_command(
    OUTPUT ${PROTO_FILES_STAGING}
    COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_STAGING} ${PROTO_FILES_STAGING} "-x" "wayland-client-protocol-extra.hpp"
    DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_STAGING} ${PROTO_FILES_EXTRA})
  add_custom_command(
    OUTPUT ${PROTO_FILES_EXPERIMENTAL}
    COMMAND "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_EXPERIMENTAL} ${PROTO_FILES_EXPERIMENTAL} "-x" "wayland-client-protocol-unstable.hpp"
    DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_EXPERIMENTAL} ${PROTO_FILES_UNSTABLE})

  if(BUILD_SERVER)
    set(PROTO_FILES
      "wayland-server-protocol.hpp"
      "wayland-server-protocol.cpp")
    set(PROTO_FILES_EXTRA
      "wayland-server-protocol-extra.hpp"
      "wayland-server-protocol-extra.cpp")
    set(PROTO_FILES_UNSTABLE
      "wayland-server-protocol-unstable.hpp"
      "wayland-server-protocol-unstable.cpp")
    set(PROTO_FILES_STAGING
      "wayland-server-protocol-staging.hpp"
      "wayland-server-protocol-staging.cpp")
    set(PROTO_FILES_EXPERIMENTAL
      "wayland-server-protocol-experimental.hpp"
      "wayland-server-protocol-experimental.cpp")
    add_custom_command(
      OUTPUT ${PROTO_FILES}
      COMMAND "${WAYLAND_SCANNERPP}" "-s" "on" ${PROTO_XMLS} ${PROTO_FILES}
      DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS})
    add_custom_command(
      OUTPUT ${PROTO_FILES_EXTRA}
      COMMAND "${WAYLAND_SCANNERPP}" "-s" "on" ${PROTO_XMLS_EXTRA} ${PROTO_FILES_EXTRA} "-x" "wayland-server-protocol.hpp"
      DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_EXTRA})
    add_custom_command(
      OUTPUT ${PROTO_FILES_UNSTABLE}
      COMMAND "${WAYLAND_SCANNERPP}" "-s" "on" ${PROTO_XMLS_UNSTABLE} ${PROTO_FILES_UNSTABLE} "-x" "wayland-server-protocol-extra.hpp"
      DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_UNSTABLE} ${PROTO_FILES_EXTRA})
    add_custom_command(
      OUTPUT ${PROTO_FILES_STAGING}
      COMMAND "${WAYLAND_SCANNERPP}" "-s" "on" ${PROTO_XMLS_STAGING} ${PROTO_FILES_STAGING} "-x" "wayland-server-protocol-extra.hpp"
      DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_STAGING} ${PROTO_FILES_EXTRA})
    add_custom_command(
      OUTPUT ${PROTO_FILES_EXPERIMENTAL}
      COMMAND "${WAYLAND_SCANNERPP}" "-s" "on" ${PROTO_XMLS_EXPERIMENTAL} ${PROTO_FILES_EXPERIMENTAL} "-x" "wayland-server-protocol-unstable.hpp"
      DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS_EXPERIMENTAL} ${PROTO_FILES_UNSTABLE})
  endif()

  # library building helper functions
  function(define_library TARGET CFLAGS LIBRARIES HEADERS)
    add_library("${TARGET}" ${ARGN})
    add_library(${install_namespace}::${TARGET} ALIAS ${TARGET})
    get_target_property(libtype "${TARGET}" TYPE)
    target_include_directories("${TARGET}" PUBLIC
      "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include;${CMAKE_CURRENT_BINARY_DIR}>" # latter for wayland-version.hpp
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
      )
    target_compile_options("${TARGET}" PUBLIC ${CFLAGS})
    target_link_libraries("${TARGET}" PUBLIC ${LIBRARIES})
    set_target_properties("${TARGET}" PROPERTIES
      PUBLIC_HEADER "${HEADERS}"
      VERSION "${PROJECT_VERSION}"
      SOVERSION ${PROJECT_VERSION_MAJOR})
    configure_file("${TARGET}.pc.in" "${TARGET}.pc" @ONLY)
    set_target_properties("${TARGET}" PROPERTIES RESOURCE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.pc")
  endfunction()

  define_library(wayland-client++ "${WAYLAND_CLIENT_CFLAGS}" "${WAYLAND_CLIENT_LIBRARIES}"
    "include/wayland-client.hpp;include/wayland-util.hpp;${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol.hpp;${CMAKE_CURRENT_BINARY_DIR}/wayland-version.hpp"
    src/wayland-client.cpp src/wayland-util.cpp wayland-client-protocol.cpp wayland-client-protocol.hpp)
  # Report undefined references only for the base library.
  if(${CMAKE_VERSION} VERSION_GREATER "3.14.0")
    target_link_options(wayland-client++ PRIVATE "-Wl,--no-undefined")
  endif()
  if(BUILD_SERVER)
    define_library(wayland-server++ "${WAYLAND_SERVER_CFLAGS}" "${WAYLAND_SERVER_LIBRARIES}"
      "include/wayland-server.hpp;include/wayland-util.hpp;${CMAKE_CURRENT_BINARY_DIR}/wayland-server-protocol.hpp"
      src/wayland-server.cpp src/wayland-util.cpp wayland-server-protocol.cpp wayland-server-protocol.hpp)
    # Report undefined references only for the base library.
    if(${CMAKE_VERSION} VERSION_GREATER "3.14.0")
      target_link_options(wayland-server++ PRIVATE "-Wl,--no-undefined")
    endif()
    define_library(wayland-server-extra++ "${WAYLAND_SERVER_CFLAGS}" "${WAYLAND_SERVER_LIBRARIES}"
      "${CMAKE_CURRENT_BINARY_DIR}/wayland-server-protocol-extra.hpp"
      wayland-server-protocol-extra.cpp wayland-server-protocol-extra.hpp wayland-server-protocol.hpp)
    target_link_libraries(wayland-server-extra++ INTERFACE wayland-server++)
    define_library(wayland-server-unstable++ "${WAYLAND_SERVER_CFLAGS}" "${WAYLAND_SERVER_LIBRARIES}"
      "${CMAKE_CURRENT_BINARY_DIR}/wayland-server-protocol-unstable.hpp"
      wayland-server-protocol-unstable.cpp wayland-server-protocol-unstable.hpp wayland-server-protocol.hpp)
    target_link_libraries(wayland-server-unstable++ INTERFACE wayland-server-extra++)
    define_library(wayland-server-staging++ "${WAYLAND_SERVER_CFLAGS}" "${WAYLAND_SERVER_LIBRARIES}"
      "${CMAKE_CURRENT_BINARY_DIR}/wayland-server-protocol-staging.hpp"
      wayland-server-protocol-staging.cpp wayland-server-protocol-staging.hpp wayland-server-protocol.hpp)
    define_library(wayland-server-experimental++ "${WAYLAND_SERVER_CFLAGS}" "${WAYLAND_SERVER_LIBRARIES}"
      "${CMAKE_CURRENT_BINARY_DIR}/wayland-server-protocol-experimental.hpp"
      wayland-server-protocol-experimental.cpp wayland-server-protocol-experimental.hpp wayland-server-protocol.hpp)
  endif()
  define_library(wayland-client-extra++ "${WAYLAND_CLIENT_CFLAGS}" "${WAYLAND_CLIENT_LIBRARIES}"
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-extra.hpp"
    wayland-client-protocol-extra.cpp wayland-client-protocol-extra.hpp wayland-client-protocol.hpp)
  target_link_libraries(wayland-client-extra++ INTERFACE wayland-client++)
  define_library(wayland-client-unstable++ "${WAYLAND_CLIENT_CFLAGS}" "${WAYLAND_CLIENT_LIBRARIES}"
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-unstable.hpp"
    wayland-client-protocol-unstable.cpp wayland-client-protocol-unstable.hpp wayland-client-protocol.hpp)
  target_link_libraries(wayland-client-unstable++ INTERFACE wayland-client-extra++)
  define_library(wayland-client-staging++ "${WAYLAND_CLIENT_CFLAGS}" "${WAYLAND_CLIENT_LIBRARIES}"
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-staging.hpp"
    wayland-client-protocol-staging.cpp wayland-client-protocol-staging.hpp wayland-client-protocol.hpp)
  define_library(wayland-client-experimental++ "${WAYLAND_CLIENT_CFLAGS}" "${WAYLAND_CLIENT_LIBRARIES}"
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-client-protocol-experimental.hpp"
    wayland-client-protocol-experimental.cpp wayland-client-protocol-experimental.hpp wayland-client-protocol.hpp)
  define_library(wayland-egl++ "${WAYLAND_EGL_CFLAGS}" "${WAYLAND_EGL_LIBRARIES}" include/wayland-egl.hpp src/wayland-egl.cpp wayland-client-protocol.hpp)
  target_link_libraries(wayland-egl++ INTERFACE wayland-client++)
  define_library(wayland-cursor++ "${WAYLAND_CURSOR_CFLAGS}" "${WAYLAND_CURSOR_LIBRARIES}" include/wayland-cursor.hpp src/wayland-cursor.cpp wayland-client-protocol.hpp)
  target_link_libraries(wayland-cursor++ INTERFACE wayland-client++)

  # Install libraries
  install(FILES ${PROTO_XMLS} ${PROTO_XMLS_EXTRA} ${PROTO_XMLS_UNSTABLE} ${PROTO_XMLS_STAGING} ${PROTO_XMLS_EXPERIMENTAL} DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/waylandpp/protocols")
  list(APPEND INSTALL_TARGETS wayland-client++ wayland-client-extra++ wayland-egl++ wayland-cursor++)
  if (INSTALL_UNSTABLE_PROTOCOLS)
	  list(APPEND INSTALL_TARGETS wayland-client-unstable++)
    if(BUILD_SERVER)
	    list(APPEND INSTALL_TARGETS wayland-server-unstable++)
    endif()
  endif()
  if (INSTALL_STAGING_PROTOCOLS)
	  list(APPEND INSTALL_TARGETS wayland-client-staging++)
    if(BUILD_SERVER)
	    list(APPEND INSTALL_TARGETS wayland-server-staging++)
    endif()
  endif()
  if (INSTALL_EXPERIMENTAL_PROTOCOLS)
	  list(APPEND INSTALL_TARGETS wayland-client-experimental++)
    if(BUILD_SERVER)
	    list(APPEND INSTALL_TARGETS wayland-server-experimental++)
    endif()
  endif()
  if(BUILD_SERVER)
    list(APPEND INSTALL_TARGETS wayland-server++ wayland-server-extra++)
  endif()
  install(TARGETS ${INSTALL_TARGETS} EXPORT ${CMAKE_PROJECT_NAME}-targets
    LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}"
    PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}"
    RESOURCE DESTINATION "${INSTALL_FULL_PKGCONFIGDIR}")

  # create and install CMake-Config files
  set(_CMAKECONFIGDIR "${CMAKE_INSTALL_LIBDIR}/cmake/${CMAKE_PROJECT_NAME}")

  install(EXPORT ${CMAKE_PROJECT_NAME}-targets
        DESTINATION "${_CMAKECONFIGDIR}"
        NAMESPACE ${install_namespace}::)
  write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}-config-version.cmake
    COMPATIBILITY AnyNewerVersion)
  configure_package_config_file(waylandpp-config.cmake.in
                              ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}-config.cmake
                              INSTALL_DESTINATION "${_CMAKECONFIGDIR}")

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}-config-version.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}-config.cmake
          DESTINATION "${_CMAKECONFIGDIR}")
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
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" ${PROTO_FILES} ${PROTO_FILES_EXTRA} ${PROTO_FILES_UNSTABLE} ${PROTO_FILES_STAGING} ${PROTO_FILES_EXPERIMENTAL}
    COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    COMMENT "Generating API documentation with Doxygen"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM
  )
  add_custom_target(doc ALL DEPENDS "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/html/index.html")

  install(DIRECTORY "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/man/" DESTINATION ${CMAKE_INSTALL_FULL_MANDIR})
  install(DIRECTORY "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/html" "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/latex" DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR})
endif()
