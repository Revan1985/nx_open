## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

add_subdirectory(nx_kit)
add_subdirectory(udt)
add_subdirectory(quazip)
add_subdirectory(qt-mustache)
add_subdirectory(stun_oauth_token_tools)

if(withAnalyticsServer)
    add_subdirectory(bytetrack)
endif()

if(withTests)
    add_subdirectory(googletest)
endif()

if(withDesktopClient)
    add_subdirectory(qthttpserver)
endif()

if(withDesktopClient OR withMobileClient)
    add_subdirectory(speex)
endif()

if((withDesktopClient OR withMobileClient) AND NOT (ANDROID OR IOS))
    add_subdirectory(qtkeychain)
endif()

if(withMediaServer OR withClouds OR withDesktopClient)
    add_subdirectory(qtsinglecoreapplication)
endif()

if(withMediaServer OR withSdk)
    add_subdirectory(ftplibpp)
endif()

if (withClouds)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../artifacts/sourcegraph_jsonrpc2
        ${CMAKE_BINARY_DIR}/open/artifacts/sourcegraph_jsonrpc2)
endif()
