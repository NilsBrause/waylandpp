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