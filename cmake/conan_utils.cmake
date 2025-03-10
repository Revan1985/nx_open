## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

include_guard(GLOBAL)

include(${CMAKE_CURRENT_LIST_DIR}/output_directories.cmake)

option(runConanAutomatically "Run Conan automatically when configuring the project." ON)
set(conanLockFile "" CACHE STRING "Install Conan packages using specified lock file.")
set(extraConanArgs "" CACHE STRING "Extra command line arguments for Conan.")
set(customConanHome "" CACHE STRING "Custom Conan home directory.")

string(CONCAT installSystemRequirementsDoc
    "Allow installation of system requirements by Conan. "
    "This option does only work in an interactive shell.")
option(installSystemRequirements ${installSystemRequirementsDoc} OFF)
# Due to security reasons `installSystemRequirements` should be reset to OFF after every CMake run.
cmake_language(DEFER
    DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL set installSystemRequirements OFF CACHE BOOL ${installSystemRequirementsDoc} FORCE
)

function(nx_init_conan)
    find_program(CONAN_EXECUTABLE NAMES conan NO_CMAKE_SYSTEM_PATH NO_CMAKE_PATH)
    if(NOT CONAN_EXECUTABLE)
        message(FATAL_ERROR "Conan executable is not found.")
    endif()

    if(customConanHome)
        message(STATUS "Using custom CONAN_USER_HOME: ${customConanHome}")
        set(ENV{CONAN_USER_HOME} ${customConanHome})
    else()
        message(STATUS "Using local CONAN_USER_HOME: ${CMAKE_BINARY_DIR}")
        set(ENV{CONAN_USER_HOME} ${CMAKE_BINARY_DIR})
    endif()

    if(NOT customConanHome)
        # Sometimes Conan cannot update this file even if it was not modified. A simple solution is
        # just to delete it and let Conan regenerate it.
        file(REMOVE ${CMAKE_BINARY_DIR}/.conan/settings.yml)
    endif()

    _configure_conan()
endfunction()

function(_configure_conan)
    if(NOT customConanHome)
        set(conan_installed_config_list_file "$ENV{CONAN_USER_HOME}/.conan/config_install.json")
        file(REMOVE ${conan_installed_config_list_file})

        set(download_cache_parameter_string "")
        if(DEFINED ENV{NX_CONAN_DOWNLOAD_CACHE})
            message(STATUS "Conan download cache: $ENV{NX_CONAN_DOWNLOAD_CACHE}")
            set(download_cache_parameter_string "download_cache = $ENV{NX_CONAN_DOWNLOAD_CACHE}")
        endif()

        cmake_path(SET conan_user_home_short "$ENV{CONAN_USER_HOME}/.conan_short")

        set(conan_config_dir_src "${open_source_root}/conan_config")
        set(conan_config_dir "${CMAKE_BINARY_DIR}/conan_config")
        nx_configure_directory(${conan_config_dir_src} ${conan_config_dir})

        nx_execute_process_or_fail(
            COMMAND ${CONAN_EXECUTABLE} config install ${conan_config_dir}
            ERROR_MESSAGE "Can not apply Conan configuration from ${conan_config_dir}"
        )
    else()
        # Check if "nx" remote is already added.
        set(conan_remotes_file "$ENV{CONAN_USER_HOME}/.conan/remotes.json")
        if(EXISTS ${conan_remotes_file})
            file(READ ${conan_remotes_file} conan_remotes)
            string(JSON conan_remotes_count LENGTH ${conan_remotes} "remotes")
            math(EXPR last_conan_remote_index "${conan_remotes_count} - 1")
            foreach(remote_index RANGE 0 ${last_conan_remote_index})
                string(JSON remote_url GET ${conan_remotes} "remotes" ${remote_index} "url")
                string(JSON remote_name GET ${conan_remotes} "remotes" ${remote_index} "name")
                if("${remote_name}" STREQUAL "nx" AND "${remote_url}" STREQUAL "${conanNxRemote}")
                    return()
                endif()
            endforeach()
        endif()

        nx_execute_process_or_fail(
            COMMAND ${CONAN_EXECUTABLE} remote add nx ${conanNxRemote} True --force
            ERROR_MESSAGE "Can not add Nx Conan remote."
        )
    endif()

    if(DEFINED ENV{NX_ARTIFACTORY_USERNAME} AND DEFINED ENV{NX_ARTIFACTORY_PASSWORD})
        message(STATUS "Using custom conan credentials $ENV{NX_ARTIFACTORY_USERNAME}:******")
        set(ENV{CONAN_LOGIN_USERNAME_NX} $ENV{NX_ARTIFACTORY_USERNAME})
        set(ENV{CONAN_PASSWORD_NX} $ENV{NX_ARTIFACTORY_PASSWORD})
    endif()
endfunction()

function(nx_run_conan)
    cmake_parse_arguments(CONAN "" "BUILD_TYPE;PROFILE;HOME_DIR" "FLAGS" ${ARGN})

    set(flags)

    if("${conanLockFile}" STREQUAL "")
        if(CONAN_BUILD_TYPE)
            list(APPEND flags "-s build_type=${CONAN_BUILD_TYPE}")
        endif()

        if(CONAN_PROFILE)
            list(APPEND flags "--profile:build=default" "--profile:host=${CONAN_PROFILE}")
        endif()

        list(APPEND flags ${CONAN_FLAGS})
    else()
        if(NOT EXISTS ${conanLockFile})
            message(FATAL_ERROR "Custom Conan lock file \"${conanLockFile}\" does not exist.")
        endif()
        list(APPEND flags "--lockfile \"${conanLockFile}\"")
        list(APPEND flags "--lockfile-out \"${CMAKE_BINARY_DIR}/conan.lock\"")
    endif()

    list(APPEND flags ${extraConanArgs})

    set(CONAN_USER_HOME $ENV{CONAN_USER_HOME})

    set(run_conan_script ${CMAKE_BINARY_DIR}/run_conan.cmake)
    nx_configure_file(${open_source_root}/cmake/run_conan.cmake.in ${run_conan_script} @ONLY)

    if(runConanAutomatically OR NOT EXISTS ${CMAKE_BINARY_DIR}/conan_paths.cmake)
        nx_execute_process_or_fail(
            COMMAND ${CMAKE_COMMAND}
                -DinstallSystemRequirements=${installSystemRequirements}
                -P ${run_conan_script}
            ERROR_MESSAGE "Conan execution failed."
        )
    else()
        message(WARNING
            "Automatic Conan execution is disabled! "
            "If you need to update Conan packages, either enable automatic updates with "
            "`-DrunConanAutomatically=ON` or run update manually using "
            "`cmake -P ${run_conan_script}`")
    endif()
endfunction()

nx_init_conan()
