# SPDX-License-Identifier: GPL-3.0-or-later
# Include after project(), so stale localized compiler-detection caches cannot
# override the prefix expected by Ninja. Works even with Chinese-only VS installs.
if(MSVC)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(CMAKE_CL_SHOWINCLUDES_PREFIX "Note: including file: ")
    set(CMAKE_C_CL_SHOWINCLUDES_PREFIX "Note: including file: ")
    set(CMAKE_CXX_CL_SHOWINCLUDES_PREFIX "Note: including file: ")
    set(CMAKE_C_COMPILER_LAUNCHER "${Python3_EXECUTABLE};${CMAKE_CURRENT_LIST_DIR}/normalize-msvc.py")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${Python3_EXECUTABLE};${CMAKE_CURRENT_LIST_DIR}/normalize-msvc.py")
endif()
