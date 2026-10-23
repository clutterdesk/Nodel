#----------------------------------------------------------------------------
# Cross-compile for Windows x64 (MSVC ABI) using clang-cl, lld-link and an
# MSVC CRT / Windows SDK splat produced by xwin (https://github.com/Jake-Shadle/xwin):
#
#   brew install llvm lld       (lld is optional if rustup is installed)
#   cargo install xwin --locked
#   xwin --accept-license splat --include-debug-libs --output ~/.xwin
#
# Override locations with -DLLVM_ROOT=... -DLLD_ROOT=... -DXWIN_DIR=...
# (or the environment variables of the same name).
#----------------------------------------------------------------------------
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

macro(_nodel_default var env_var default)
    if (NOT DEFINED ${var})
        if (DEFINED ENV{${env_var}})
            set(${var} "$ENV{${env_var}}")
        else()
            set(${var} "${default}")
        endif()
    endif()
endmacro()

_nodel_default(LLVM_ROOT LLVM_ROOT "/usr/local/opt/llvm")
_nodel_default(LLD_ROOT  LLD_ROOT  "/usr/local/opt/lld")
_nodel_default(XWIN_DIR  XWIN_DIR  "$ENV{HOME}/.xwin")
set(LLVM_ROOT "${LLVM_ROOT}" CACHE PATH "LLVM installation providing clang-cl")
set(LLD_ROOT  "${LLD_ROOT}"  CACHE PATH "LLD installation providing lld-link")
set(XWIN_DIR  "${XWIN_DIR}"  CACHE PATH "Output directory of 'xwin splat'")

# Make the cache variables visible inside try_compile projects
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_ROOT LLD_ROOT XWIN_DIR)

if (NOT EXISTS "${XWIN_DIR}/crt/include" OR NOT EXISTS "${XWIN_DIR}/sdk/include/ucrt")
    message(FATAL_ERROR "MSVC CRT/SDK not found in XWIN_DIR='${XWIN_DIR}'. "
                        "Run: xwin --accept-license splat --include-debug-libs --output ${XWIN_DIR}")
endif()

find_program(CMAKE_C_COMPILER   clang-cl  HINTS "${LLVM_ROOT}/bin" REQUIRED NO_DEFAULT_PATH)
find_program(CMAKE_CXX_COMPILER clang-cl  HINTS "${LLVM_ROOT}/bin" REQUIRED NO_DEFAULT_PATH)
# The Rust toolchain also ships a usable lld-link, so Homebrew's lld is optional
file(GLOB _nodel_rust_lld_dirs "$ENV{HOME}/.rustup/toolchains/*/lib/rustlib/*/bin/gcc-ld")
find_program(CMAKE_LINKER       lld-link  HINTS "${LLD_ROOT}/bin" "${LLVM_ROOT}/bin" ${_nodel_rust_lld_dirs} REQUIRED)
find_program(CMAKE_AR           llvm-lib  HINTS "${LLVM_ROOT}/bin" REQUIRED NO_DEFAULT_PATH)
find_program(CMAKE_RC_COMPILER  llvm-rc   HINTS "${LLVM_ROOT}/bin" REQUIRED NO_DEFAULT_PATH)
find_program(CMAKE_MT           llvm-mt   HINTS "${LLVM_ROOT}/bin" REQUIRED NO_DEFAULT_PATH)

set(CMAKE_C_COMPILER_TARGET   x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

# /imsvc marks the CRT/SDK headers as system headers
set(_nodel_xwin_cflags "")
foreach(dir crt/include sdk/include/ucrt sdk/include/um sdk/include/shared sdk/include/winrt)
    string(APPEND _nodel_xwin_cflags " /imsvc \"${XWIN_DIR}/${dir}\"")
endforeach()
string(APPEND CMAKE_C_FLAGS_INIT   "${_nodel_xwin_cflags}")
string(APPEND CMAKE_CXX_FLAGS_INIT "${_nodel_xwin_cflags}")

set(_nodel_xwin_ldflags "")
foreach(dir crt/lib/x86_64 sdk/lib/um/x86_64 sdk/lib/ucrt/x86_64)
    string(APPEND _nodel_xwin_ldflags " /libpath:\"${XWIN_DIR}/${dir}\"")
endforeach()
string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT    "${_nodel_xwin_ldflags}")
string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT "${_nodel_xwin_ldflags}")
string(APPEND CMAKE_MODULE_LINKER_FLAGS_INIT "${_nodel_xwin_ldflags}")

# Dynamic CRT, matching gtest_force_shared_crt
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# Never pick up host (macOS) libraries, headers or packages
set(CMAKE_FIND_ROOT_PATH "${XWIN_DIR}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
