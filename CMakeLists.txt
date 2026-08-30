cmake_minimum_required(VERSION 3.10)
project(NetXbox VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(SOURCES
    src/main.c
    src/app.c
    src/core/types.c
    src/core/translate.c
    src/core/logo_data.c
    src/html/html.c
    src/network/http.c
    src/renderer/renderer.c
    src/browser/browser.c
    src/ui/ui.c
    src/graphics/framebuffer.c
    src/graphics/font.c
    src/graphics/stb_truetype_impl.c
    src/graphics/image.c
    src/adblock/adblock.c
)

set(PLATFORM_WIN32_SOURCES
    src/platform/win32/platform_win32.c
)

set(PLATFORM_XBOX360_SOURCES
    src/platform/xbox360/platform_xbox360.c
    src/ui/xui_ui.cpp
)

set(INCLUDES
    include
    thirdparty
)

option(BUILD_XBOX360 "Build for Xbox 360" OFF)

if(BUILD_XBOX360)
    message(STATUS "Building for Xbox 360 (XDK)")

    # Xbox 360 SDK paths
    if(NOT DEFINED XEDK)
        set(XEDK "$ENV{XEDK}")
    endif()
    if(NOT XEDK OR NOT EXISTS "${XEDK}")
        set(XEDK "C:/Program Files (x86)/Microsoft Xbox 360 SDK")
    endif()
    message(STATUS "XEDK: ${XEDK}")

    set(XEDK_BIN "${XEDK}/bin/win32")
    set(XEDK_INC "${XEDK}/include/xbox")
    set(XEDK_INC_SYS "${XEDK}/include/xbox/sys")
    set(XEDK_LIB "${XEDK}/lib/xbox")

    add_definitions(-DPLATFORM_XBOX360 -D_XBOX -DWIN32_LEAN_AND_MEAN -D_CRT_SECURE_NO_WARNINGS)

    add_executable(netxbox ${SOURCES} ${PLATFORM_XBOX360_SOURCES})
    target_include_directories(netxbox PRIVATE
        ${CMAKE_SOURCE_DIR}/include/xbox_compat
        ${INCLUDES}
        ${XEDK_INC}
        ${XEDK_INC_SYS}
    )

    # XDK compiler flags
    # /TP = compile as C++ (allows C99-style declarations, XDK cl.exe is C89 only)
    target_compile_options(netxbox PRIVATE
        /TP
        /D_XBOX
        /MT
        /GS-
        /GF
        /GR-
        /fp:fast
        /Zi
        /W3
        /nologo
    )

    # XDK linker flags
    target_link_options(netxbox PRIVATE
        /SUBSYSTEM:XBOX
        /NODEFAULTLIB
        /INCREMENTAL:NO
        /NOLOGO
        /MAP:netxbox.map
    )

    # Link Xbox 360 XDK libraries (full paths)
    target_link_directories(netxbox PRIVATE ${XEDK_LIB})

    target_link_libraries(netxbox
        ${XEDK_LIB}/d3d9.lib
        ${XEDK_LIB}/d3dx9.lib
        ${XEDK_LIB}/xonline.lib
        ${XEDK_LIB}/xinput2.lib
        ${XEDK_LIB}/xnet.lib
        ${XEDK_LIB}/xboxkrnl.lib
        ${XEDK_LIB}/xapilib.lib
        ${XEDK_LIB}/libcMT.lib
        ${XEDK_LIB}/libcpMT.lib
        ${XEDK_LIB}/xgraphics.lib
        ${XEDK_LIB}/xuirun.lib
        ${XEDK_LIB}/xuirender.lib
        ${XEDK_LIB}/xmedia2.lib
        ${XEDK_LIB}/xmcore.lib
        ${XEDK_LIB}/xaudio2.lib
        ${XEDK_LIB}/xact3.lib
        ${XEDK_LIB}/x3daudio.lib
        ${XEDK_LIB}/xbdm.lib
    )

    # Post-build: convert PE to XEX using imagexex
    add_custom_command(TARGET netxbox POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:netxbox>/media"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/media/xui/netxbox.xzp"
            "$<TARGET_FILE_DIR:netxbox>/media/netxbox.xzp"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/media/xui/xarialuni.ttf"
            "$<TARGET_FILE_DIR:netxbox>/media/xarialuni.ttf"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_SOURCE_DIR}/media/xui/cursor.png"
            "$<TARGET_FILE_DIR:netxbox>/media/cursor.png"
        COMMAND "${XEDK_BIN}/imagexex.exe" /IN:"$<TARGET_FILE:netxbox>" /OUT:"$<TARGET_FILE_DIR:netxbox>/netxbox.xex"
        COMMENT "Generating netxbox.xex via imagexex"
    )

else()
    message(STATUS "Building for Windows")
    enable_language(RC)
    add_executable(netxbox ${SOURCES} ${PLATFORM_WIN32_SOURCES} resources/app.rc)
    target_include_directories(netxbox PRIVATE ${INCLUDES} resources)
    target_link_libraries(netxbox ws2_32 user32 gdi32 kernel32 opengl32 winhttp shlwapi comdlg32)
    target_compile_definitions(netxbox PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
endif()
