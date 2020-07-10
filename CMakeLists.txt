cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)

project(lua-rapidjson)
option (LUA_RAPIDJSON_BIT32 "32bit compile" OFF)
option (LUA_RAPIDJSON_DEBUG "Include ltests.h" OFF)
option (LUA_RAPIDJSON_EXPLICIT "Explicit error vs. Returned error message" OFF)
option (LUA_RAPIDJSON_COMPAT "Strict compatibility requirements with dkjson" ON)

# compile in release  mode by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

if(UNIX)
    if(APPLE)
        set(PLAT "macosx")
        set(LINK_FLAGS "-bundle -undefined dynamic_lookup -all_load")
    else(APPLE)
        set(PLAT "linux")
        set(LINK_FLAGS "-shared")
    endif(APPLE)

    # Compiler options taken from Lua: lua/blob/master/makefile
    add_compile_options(-fPIC
        -fmax-errors=5
        -Wall
        -Wextra
        -Wsign-compare
        -Wundef
        -Wwrite-strings
        -Wredundant-decls
        -Wdisabled-optimization
        -Wdouble-promotion
        -Wlogical-op
        -Wno-aggressive-loop-optimizations
        # the next warnings might be useful sometimes, but usually they generate too much noise
        -Werror
        -Wconversion
        -Wsign-conversion
        -Wstrict-overflow=2
        -Wformat=2
        -Wcast-qual
    )

    # The next warnings are neither valid nor needed for C++
    set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "\
        -Wdeclaration-after-statement \
        -Wmissing-prototypes \
        -Wnested-externs \
        -Wstrict-prototypes \
        -Wc++-compat \
        -Wold-style-definition"
    )

    include(CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_ARCH_NATIVE)
    if (COMPILER_SUPPORTS_ARCH_NATIVE)
        add_compile_options(-march=native)
    endif()

    if(LUA_RAPIDJSON_DEBUG)
        add_compile_options(-O0 -g)
        add_definitions(-DLUA_USER_H="ltests.h")
    else()
        add_compile_options(-O3)
    endif()

    if (LUA_RAPIDJSON_EXPLICIT)
        add_definitions(-DLUA_RAPIDJSON_EXPLICIT)
    ENDIF()

    if (LUA_RAPIDJSON_COMPAT)
        add_definitions(-DLUA_RAPIDJSON_COMPAT)
    ENDIF()

    IF(LUA_RAPIDJSON_BIT32)
       IF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
          set (CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
          set (CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
          set (CMAKE_EXE_LINKER_FLAGS "-m32 ${CMAKE_EXE_LINKER_FLAGS}")
       ELSEIF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
          set (CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
          set (CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
          set (CMAKE_EXE_LINKER_FLAGS "-m32 ${CMAKE_EXE_LINKER_FLAGS}")
       ENDIF()
    ENDIF()
else(UNIX)
    if(WIN32)
        set(PLAT "win32")
        add_definitions(-DLUA_BUILD_AS_DLL -DLUA_LIB -D_CRT_SECURE_NO_WARNINGS)
    else(WIN32)
        set(PLAT "unsupported")
    endif(WIN32)
endif(UNIX)

#-------------------------------------------------------------------------------
# Lua dependency
#-------------------------------------------------------------------------------
if(NOT LUA_INCLUDE_DIR OR (WIN32 AND NOT LUA_LIBRARIES))
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
    find_package(Lua REQUIRED)
endif()
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# rapidjson external dependency
#-------------------------------------------------------------------------------
set(LUA_RAPIDJSON ${CMAKE_BINARY_DIR}/rapidjson)

include(ExternalProject)
ExternalProject_Add(rapidjson
    PREFIX ${LUA_RAPIDJSON}
    GIT_REPOSITORY "https://github.com/Tencent/rapidjson.git"
    GIT_TAG "master"
    GIT_SHALLOW 1
    CMAKE_ARGS += -DRAPIDJSON_BUILD_TESTS=OFF
                  -DRAPIDJSON_BUILD_DOC=OFF
                  -DRAPIDJSON_BUILD_EXAMPLES=OFF
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
)

# RapidJSON is a header-only library
ExternalProject_Get_Property(rapidjson source_dir)
set(RAPIDJSON_INCLUDE_DIRS ${source_dir}/include)
#-------------------------------------------------------------------------------

include_directories(${LUA_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIRS})

set(SOURCES
    src/StringStream.hpp
    src/reader_dkcompat.hpp
    src/lua_rapidjson.hpp
    src/lua_rapidjsonlib.h
    src/lua_rapidjson.cpp
)

add_library(lua-rapidjson MODULE ${SOURCES})
add_dependencies(lua-rapidjson rapidjson)
if(${CMAKE_VERSION} VERSION_LESS "3.1.0")
    if(CMAKE_COMPILER_IS_GNUCXX)
        execute_process(COMMAND "${CMAKE_CXX_COMPILER} -dumpversion" OUTPUT_VARIABLE GCC_VERSION)
        if (GCC_VERSION VERSION_LESS 4.7)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
        endif()
    endif()
else()
    target_compile_features(lua-rapidjson PRIVATE cxx_rvalue_references)
endif()

source_group(src FILES ${SOURCES})

if (WIN32)
    target_link_libraries(lua-rapidjson ${LUA_LIBRARIES})
endif()

if (LINK_FLAGS)
    set_target_properties(lua-rapidjson PROPERTIES
        LINK_FLAGS ${LINK_FLAGS}
    )
endif()

set_target_properties(lua-rapidjson PROPERTIES
    OUTPUT_NAME "rapidjson"
    PREFIX ""
)


if (CMAKE_INSTALL_PREFIX)
  install(TARGETS lua-rapidjson DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
endif(CMAKE_INSTALL_PREFIX)
