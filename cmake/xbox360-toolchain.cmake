# Xbox 360 XDK Cross-Compilation Toolchain
# Requires Microsoft Xbox 360 SDK installed

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR powerpc)
set(CMAKE_CROSSCOMPILING TRUE)

# Xbox 360 SDK path (use forward slashes)
if(NOT DEFINED XEDK)
    set(XEDK "$ENV{XEDK}")
endif()
if(NOT XEDK OR NOT EXISTS "${XEDK}")
    set(XEDK "C:/Program Files (x86)/Microsoft Xbox 360 SDK" CACHE PATH "Xbox 360 SDK root" FORCE)
endif()

# Normalize to forward slashes
string(REPLACE "\\" "/" XEDK "${XEDK}")

set(XEDK_BIN "${XEDK}/bin/win32")

# Compilers - use XDK's PowerPC cross-compiler
set(CMAKE_C_COMPILER "${XEDK_BIN}/cl.exe")
set(CMAKE_CXX_COMPILER "${XEDK_BIN}/cl.exe")
set(CMAKE_LINKER "${XEDK_BIN}/link.exe")
set(CMAKE_LIB "${XEDK_BIN}/lib.exe")

# Skip compiler test (XDK cl.exe targets PowerPC, can't produce host executables)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
