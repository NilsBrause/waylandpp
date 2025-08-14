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

# generate client protocol source/headers from protocol XMLs
function(generate_cpp_client_files PROTO_XMLS PROTO_FILES EXTRA_CMD_ARGS EXTRA_DEPENDS)
    list(APPEND COMMAND_LINE_ARGS ${PROTO_XMLS} ${PROTO_FILES} ${EXTRA_CMD_ARGS})
    add_custom_command(
        OUTPUT ${PROTO_FILES}
        COMMAND "${WAYLAND_SCANNERPP}" ${COMMAND_LINE_ARGS}
        DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS} ${EXTRA_DEPENDS})
endfunction()

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

# generate server protocol source/headers from protocol XMLs
function(generate_cpp_server_files PROTO_XMLS PROTO_FILES EXTRA_CMD_ARGS EXTRA_DEPENDS)
    list(APPEND COMMAND_LINE_ARGS ${PROTO_XMLS} ${PROTO_FILES} ${EXTRA_CMD_ARGS})
    add_custom_command(
        OUTPUT ${PROTO_FILES}
        COMMAND "${WAYLAND_SCANNERPP}" "-s" "on" ${COMMAND_LINE_ARGS}
        DEPENDS "${WAYLAND_SCANNERPP}" ${PROTO_XMLS} ${EXTRA_DEPENDS})
endfunction()

# get unique interface name from xml file name
function(get_interface_name base_name interface_name)
  set(_tmp_out)
  string(REGEX REPLACE "(-unstable)?-v[0-9]+$" "" _tmp_out "${base_name}")
  set(${interface_name} "${_tmp_out}" PARENT_SCOPE)
endfunction()

# avoids having several versions of the same interface, giving priority to those with the highest version number
function(filter_out_older_versions proto_files in_names accepted_xml_files out_names)
  set(_tmp_accepted_xml_files)
  set(_tmp_rejected_xml_files)
  list(REVERSE proto_files)
  foreach(file ${proto_files})
    get_filename_component(base_name ${file} NAME_WE)
    get_interface_name("${base_name}" interface_name)
    list(FIND in_names "${interface_name}" idx)
    if (idx EQUAL "-1")
      list(APPEND in_names ${interface_name})
      list(APPEND _tmp_accepted_xml_files ${file})
    else()
      get_filename_component(file_name "${file}" NAME)
      list(APPEND _tmp_rejected_xml_files ${file_name})
    endif()
  endforeach()
  list(LENGTH _tmp_rejected_xml_files length)
  if(length GREATER 0)
    message(STATUS "Reject the following protocol(s) file(s) to avoid duplicating symbols:")
    foreach(file ${_tmp_rejected_xml_files})
      message(STATUS " > ${file}")
    endforeach()
  endif()
  set(${out_names} "${in_names}" PARENT_SCOPE)
  set(${accepted_xml_files} "${_tmp_accepted_xml_files}" PARENT_SCOPE)
endfunction()