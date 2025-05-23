## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

# Workaround for a Doxygen bug: we need to run python from Doxygen, and the CMake variable
# PYTHON_EXECUTABLE may contain spaces in its value, and Doxygen cannot properly handle it. So, we
# use a short python invocation command which depends on the platform.
if(APPLE)
    set(PYTHON_EXECUTABLE_FOR_DOXYGEN "python3")
else()
    set(PYTHON_EXECUTABLE_FOR_DOXYGEN "python")
endif()

set(DOXYGEN_SRC_DIR
    "${CMAKE_SOURCE_DIR}/open/vms/client/nx_vms_client_desktop/src/nx/vms/client/desktop/jsapi")

nx_configure_file(
    Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

# Make the resulting ZIP file path available in other places, e.g. adding as a binary resource.
set(js_api_documentation_zip_file "${CMAKE_CURRENT_BINARY_DIR}/js_api_doc.zip"
    CACHE INTERNAL "" FORCE)

set(doxygen_html_dir ${CMAKE_CURRENT_BINARY_DIR}/html)
set(logs_dir ${CMAKE_BINARY_DIR}/build_logs)
set(doxygen_log ${logs_dir}/js_api_doc_doxygen.log)
set(zip_log ${logs_dir}/js_api_doc_zip.log)

file(GLOB_RECURSE doxygen_src_files CONFIGURE_DEPENDS ${DOXYGEN_SRC_DIR}/*)

# OUTPUT and BYPRODUCTS must be the last arguments - see the function definition.
nx_add_custom_command(
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        ${doxygen_src_files}

    COMMAND ${CMAKE_COMMAND} -E make_directory ${logs_dir}
    COMMAND ${CMAKE_COMMAND} -E
        echo "  Running Doxygen - see ${doxygen_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}
        ${CONAN_DOXYGEN_ROOT}/bin/doxygen >${doxygen_log} 2>&1
    COMMAND ${CMAKE_COMMAND} -E
        echo "  Zipping docs - see ${zip_log}"
    COMMAND ${CMAKE_COMMAND} -E chdir ${doxygen_html_dir}
        ${CMAKE_COMMAND} -E tar cf ${js_api_documentation_zip_file} --format=zip ./ >${zip_log}

    OUTPUT ${js_api_documentation_zip_file}
    BYPRODUCTS ${doxygen_html_dir}
)

add_custom_target(js_api_documentation ALL DEPENDS ${js_api_documentation_zip_file})

set_target_properties(js_api_documentation PROPERTIES FOLDER js_api)
