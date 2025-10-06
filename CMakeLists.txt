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

cmake_minimum_required(VERSION 3.6...4.0)
project(waylandpp VERSION 1.0.0 LANGUAGES CXX)

# packages
include(FindPkgConfig)
include(GNUInstallDirs)
include(CMakeDependentOption)
include(CMakePackageConfigHelpers)
find_package(Doxygen)

# internal cmake scripts
set(CMAKETOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKETOOLS_DIR}")
include(waylandpp_helper_functions)

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
option(INSTALL_EXTRA_PROTOCOLS "whether to install the additional stable protocols" ON)
cmake_dependent_option(INSTALL_UNSTABLE_PROTOCOLS "whether to install the unstable protocols" ON
  INSTALL_EXTRA_PROTOCOLS OFF)
cmake_dependent_option(INSTALL_STAGING_PROTOCOLS "whether to install the staging protocols" ON
  INSTALL_EXTRA_PROTOCOLS OFF)
cmake_dependent_option(INSTALL_EXPERIMENTAL_PROTOCOLS "whether to install the experimental protocols" ON
  INSTALL_UNSTABLE_PROTOCOLS OFF)
cmake_dependent_option(USE_SYSTEM_PROTOCOLS "whether to build libraries from the wayland-protocols package instead of bundled protocols" OFF
  "(NOT INSTALL_EXPERIMENTAL_PROTOCOLS)" OFF)
cmake_dependent_option(BUILD_EXAMPLES
  "whether to build the examples (requires BUILD_LIBRARIES to be ON)" OFF
  "BUILD_LIBRARIES" OFF)
option(BUILD_SERVER "whether to build the server bindings." ON)
# To activate the following option, it is necessary to deactivate option INSTALL_EXPERIMENTAL_PROTOCOLS and activate option USE_SYSTEM_PROTOCOLS.
cmake_dependent_option(INSTALL_WLR_PROTOCOLS "whether to build the library based on the wlr protocols" OFF
  USE_SYSTEM_PROTOCOLS OFF)
cmake_dependent_option(INSTALL_PLASMA_PROTOCOLS "whether to build the library based on the plasma protocols" OFF
  USE_SYSTEM_PROTOCOLS OFF)

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
  if(USE_SYSTEM_PROTOCOLS)
    pkg_get_variable(WAYLAND_PKGDATADIR wayland-client pkgdatadir)
    if(NOT WAYLAND_PKGDATADIR)
      set(WAYLAND_PKGDATADIR "/usr/share/wayland")
      message(WARNING "pkgdatadir field not found in wayland.pc, fallback to ${WAYLAND_PKGDATADIR}")
    else()
      message(STATUS "Use [wayland] protocols found at ${WAYLAND_PKGDATADIR}")
    endif()
  endif()
  if(BUILD_SERVER)
    pkg_check_modules(WAYLAND_SERVER REQUIRED "wayland-server>=1.18.0")
  endif()
  pkg_check_modules(WAYLAND_EGL REQUIRED wayland-egl)
  pkg_libs_full_path(WAYLAND_EGL)
  pkg_check_modules(WAYLAND_CURSOR REQUIRED wayland-cursor)
  pkg_libs_full_path(WAYLAND_CURSOR)

  if(USE_SYSTEM_PROTOCOLS)
    if (INSTALL_EXTRA_PROTOCOLS OR INSTALL_UNSTABLE_PROTOCOLS OR INSTALL_STAGING_PROTOCOLS)
      pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols)
      pkg_get_variable(WAYLAND_PROTOCOLS_PKGDATADIR wayland-protocols pkgdatadir)
      if(NOT WAYLAND_PROTOCOLS_PKGDATADIR)
        set(WAYLAND_PROTOCOLS_PKGDATADIR "/usr/share/wayland-protocols")
        message(WARNING "pkgdatadir field not found in wayland-protocols.pc, fallback to ${WAYLAND_PROTOCOLS_PKGDATADIR}")
      else()
        message(STATUS "Use [wayland-protocols] protocols found at ${WAYLAND_PROTOCOLS_PKGDATADIR}")
      endif()
    endif()
  endif()

  if(INSTALL_WLR_PROTOCOLS)
    pkg_check_modules(WLR_PROTOCOLS REQUIRED wlr-protocols)
    pkg_get_variable(WLR_PROTOCOLS_PKGDATADIR wlr-protocols pkgdatadir)
    message(STATUS "Use [wlr-protocols] protocols found at ${WLR_PROTOCOLS_PKGDATADIR}")
  endif()
  if(INSTALL_PLASMA_PROTOCOLS)
    find_package(PlasmaWaylandProtocols REQUIRED)
    message(STATUS "Use [plasma-wayland-protocols] protocols found at ${PLASMA_WAYLAND_PROTOCOLS_DIR}")
  endif()

  # generate protocol source/headers from protocol XMLs
  if(USE_SYSTEM_PROTOCOLS)
    set(PROTO_XMLS "${WAYLAND_PKGDATADIR}/wayland.xml")
    set(_unique_interface_list)
    if(INSTALL_EXTRA_PROTOCOLS)
      file(GLOB _PROTO_XMLS_EXTRA "${WAYLAND_PROTOCOLS_PKGDATADIR}/stable/**/*.xml")
      filter_out_older_versions("${_PROTO_XMLS_EXTRA}" "${_unique_interface_list}" PROTO_XMLS_EXTRA _unique_interface_list)
    endif()
    if(INSTALL_UNSTABLE_PROTOCOLS)
      file(GLOB _PROTO_XMLS_UNSTABLE "${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/**/*.xml")
      filter_out_older_versions("${_PROTO_XMLS_UNSTABLE}" "${_unique_interface_list}" PROTO_XMLS_UNSTABLE _unique_interface_list)
    endif()
    if(INSTALL_STAGING_PROTOCOLS)
      file(GLOB _PROTO_XMLS_STAGING "${WAYLAND_PROTOCOLS_PKGDATADIR}/staging/**/*.xml")
      filter_out_older_versions("${_PROTO_XMLS_STAGING}" "${_unique_interface_list}" PROTO_XMLS_STAGING _unique_interface_list)
    endif()
  else()
    set(PROTO_XMLS "${CMAKE_CURRENT_SOURCE_DIR}/protocols/wayland.xml")
    file(GLOB PROTO_XMLS_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/protocols/extra/*.xml")
    file(GLOB PROTO_XMLS_UNSTABLE "${CMAKE_CURRENT_SOURCE_DIR}/protocols/unstable/*.xml")
    file(GLOB PROTO_XMLS_STAGING "${CMAKE_CURRENT_SOURCE_DIR}/protocols/staging/*.xml")
    file(GLOB PROTO_XMLS_EXPERIMENTAL "${CMAKE_CURRENT_SOURCE_DIR}/protocols/experimental/*.xml")
  endif()
  if(INSTALL_WLR_PROTOCOLS)
    file(GLOB _PROTO_XMLS_WLR "${WLR_PROTOCOLS_PKGDATADIR}/**/*.xml")
    filter_out_older_versions("${_PROTO_XMLS_WLR}" "${_unique_interface_list}" PROTO_XMLS_WLR _unique_interface_list)
  endif()

  if(INSTALL_PLASMA_PROTOCOLS)
    file(GLOB _PROTO_XMLS_PLASMA "${PLASMA_WAYLAND_PROTOCOLS_DIR}/*.xml")
    # conflicts with plasma-virtual-desktop.xml
    list(FILTER _PROTO_XMLS_PLASMA EXCLUDE REGEX "org-kde-plasma-virtual-desktop.xml")
    # conflicts with screencast.xml
    list(FILTER _PROTO_XMLS_PLASMA EXCLUDE REGEX "zkde-screencast-unstable-v1.xml")
    filter_out_older_versions("${_PROTO_XMLS_PLASMA}" "${_unique_interface_list}" PROTO_XMLS_PLASMA _unique_interface_list)
  endif()

  include(build_client_libraries)

  if(BUILD_SERVER)
    include(build_server_libraries)
  endif()

  # Install libraries
  list(APPEND INSTALL_TARGETS wayland-client++ wayland-egl++ wayland-cursor++)
  if(BUILD_SERVER)
    list(APPEND INSTALL_TARGETS wayland-server++)
  endif()
  if (INSTALL_EXTRA_PROTOCOLS)
    list(APPEND INSTALL_TARGETS wayland-client-extra++)
    if(BUILD_SERVER)
      list(APPEND INSTALL_TARGETS wayland-server-extra++)
    endif()
  endif()
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
  if(INSTALL_WLR_PROTOCOLS)
    list(APPEND INSTALL_TARGETS wayland-client-wlr++)
    if(BUILD_SERVER)
      list(APPEND INSTALL_TARGETS wayland-server-wlr++)
    endif()
  endif()
  if(INSTALL_PLASMA_PROTOCOLS)
    list(APPEND INSTALL_TARGETS wayland-client-plasma++)
    if(BUILD_SERVER)
      list(APPEND INSTALL_TARGETS wayland-server-plasma++)
    endif()
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

  set(DOCUMENTATION_DEPENDENCIES ${PROTO_FILES})
  if (INSTALL_EXTRA_PROTOCOLS)
    set(DOCUMENTATION_DEPENDENCIES ${DOCUMENTATION_DEPENDENCIES} ${PROTO_FILES_EXTRA})
  endif()
  if (INSTALL_UNSTABLE_PROTOCOLS)
    set(DOCUMENTATION_DEPENDENCIES ${DOCUMENTATION_DEPENDENCIES} ${PROTO_FILES_UNSTABLE})
  endif()
  if (INSTALL_STAGING_PROTOCOLS)
    set(DOCUMENTATION_DEPENDENCIES ${DOCUMENTATION_DEPENDENCIES} ${PROTO_FILES_STAGING})
  endif()
  if (INSTALL_EXPERIMENTAL_PROTOCOLS)
    set(DOCUMENTATION_DEPENDENCIES ${DOCUMENTATION_DEPENDENCIES} ${PROTO_FILES_EXPERIMENTAL})
  endif()
  if (INSTALL_WLR_PROTOCOLS)
    set(DOCUMENTATION_DEPENDENCIES ${DOCUMENTATION_DEPENDENCIES} ${PROTO_FILES_WLR})
  endif()
  if (INSTALL_PLASMA_PROTOCOLS)
    set(DOCUMENTATION_DEPENDENCIES ${DOCUMENTATION_DEPENDENCIES} ${PROTO_FILES_PLASMA})
  endif()

  add_custom_command(
    OUTPUT "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/html/index.html"
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" ${DOCUMENTATION_DEPENDENCIES}
    COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
    COMMENT "Generating API documentation with Doxygen"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM
  )
  add_custom_target(doc ALL DEPENDS "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/html/index.html")

  install(DIRECTORY "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/man/" DESTINATION ${CMAKE_INSTALL_FULL_MANDIR})
  install(DIRECTORY "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/html" "${WAYLANDPP_DOXYGEN_OUTPUT_DIRECTORY}/latex" DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR})
endif()
