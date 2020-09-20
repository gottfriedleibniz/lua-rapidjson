cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)

project(lua-rapidjson)
IF (CMAKE_CXX_COMPILER MATCHES ".*clang")
    SET (CMAKE_COMPILER_IS_CLANGXX 1)
ENDIF ()

# compile in release  mode by default
IF (NOT CMAKE_BUILD_TYPE)
    SET (CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF ()

OPTION (LUA_RAPIDJSON_EXPLICIT "Explicit error vs. Returned error message" OFF)
IF (LUA_RAPIDJSON_EXPLICIT)
    ADD_DEFINITIONS (-DLUA_RAPIDJSON_EXPLICIT)
ENDIF ()

OPTION (LUA_RAPIDJSON_COMPAT "Strict compatibility requirements with dkjson" ON)
IF (LUA_RAPIDJSON_COMPAT)
    ADD_DEFINITIONS (-DLUA_RAPIDJSON_COMPAT)
ENDIF ()

OPTION (LUA_RAPIDJSON_SANITIZE_KEYS "Ensure key-types are string/numeric on encoding" ON)
IF (LUA_RAPIDJSON_SANITIZE_KEYS)
    ADD_DEFINITIONS (-DLUA_RAPIDJSON_SANITIZE_KEYS)
ENDIF ()

OPTION (LUA_RAPIDJSON_LUA_FLOAT "Use lua_number2str instead of internal internal::dtoa/Grisu2" OFF)
OPTION (LUA_RAPIDJSON_ROUND_FLOAT "Round decimals prior to using internal internal::dtoa/Grisu2" OFF)
IF (LUA_RAPIDJSON_LUA_FLOAT)
    ADD_DEFINITIONS (-DLUA_RAPIDJSON_LUA_FLOAT)
ELSEIF (LUA_RAPIDJSON_ROUND_FLOAT)
    ADD_DEFINITIONS (-DLUA_RAPIDJSON_ROUND_FLOAT)
ENDIF ()

OPTION (LUA_RAPIDJSON_ALLOCATOR "Use a lua_getallocf binding for the rapidjson allocator class" OFF)
IF (LUA_RAPIDJSON_ALLOCATOR)
    ADD_DEFINITIONS (-DLUA_RAPIDJSON_ALLOCATOR)
ENDIF ()

OPTION (LUA_RAPIDJSON_STATIC_NIL "Store null reference in a static variable (no longer reentrant)" OFF)
IF (LUA_RAPIDJSON_STATIC_NIL)
    ADD_DEFINITIONS (-DLUA_RAPIDJSON_STATIC_NIL)
ENDIF ()

OPTION (LUA_RAPIDJSON_EXPERIMENTAL "Experimental performance tweaks" OFF)
IF (LUA_RAPIDJSON_EXPERIMENTAL)
    ADD_DEFINITIONS (-DLUA_RAPIDJSON_EXPERIMENTAL)
ENDIF ()

IF (UNIX)
    IF (APPLE)
        SET (PLAT "macosx")
        SET (LINK_FLAGS "-bundle -undefined dynamic_lookup -all_load")
    ELSE(APPLE)
        SET (PLAT "linux")
        SET (LINK_FLAGS "-shared")
    ENDIF (APPLE)

    # Compiler options taken from Lua: lua/blob/master/makefile
    IF (CMAKE_COMPILER_IS_CLANGXX)
        ADD_COMPILE_OPTIONS (
            -ferror-limit=5
            -fno-common
            #-Weverything
            #-Wno-c++98-compat
            #-Wno-c++98-compat-pedantic
            #-Wno-gnu-zero-variadic-macro-arguments
            #-Wno-c++17-extensions
            #-Wno-covered-switch-default
            #-Wno-reserved-id-macro
        )
    elseIF (CMAKE_COMPILER_IS_GNUCXX)
        ADD_COMPILE_OPTIONS (
            -fmax-errors=5
            -Wduplicated-cond
            -Wduplicated-branches
            -Wrestrict
            -Wnull-dereference
            -Wold-style-cast
            -Wuseless-cast
            -Wlogical-op
            -Wno-useless-cast
        )
    ENDIF ()

    ADD_COMPILE_OPTIONS (
        -fPIC
        -Wall
        -Wextra
        -Wshadow
        -Wsign-compare
        -Wundef
        -Wwrite-strings
        -Wredundant-decls
        -Wdouble-promotion
        -Wunused-parameter
        -Wno-comment
        -Wno-unused-function
        -Wno-array-bounds

        #-Wconversion
        #-Wsign-conversion
        #-Wdisabled-optimization
        #-Wno-aggressive-loop-optimizations
        # the next warnings might be useful sometimes, but usually they generate too much noise
        #-Werror
        #-Wstrict-overflow=2
        #-Wformat=2
        #-Wcast-qual
    )

    INCLUDE (CheckCXXCompilerFlag)
    CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_ARCH_NATIVE)
    IF (COMPILER_SUPPORTS_ARCH_NATIVE)
        ADD_COMPILE_OPTIONS (-march=native)
    ENDIF ()

    OPTION (LUA_RAPIDJSON_DEBUG "Include ltests.h" OFF)
    IF (LUA_RAPIDJSON_DEBUG)
        ADD_COMPILE_OPTIONS (-O0 -g)
        ADD_DEFINITIONS (-DLUA_USER_H="ltests.h")
    ELSE ()
        ADD_COMPILE_OPTIONS (-O3)
    ENDIF ()

    OPTION (LUA_RAPIDJSON_BIT32 "32bit compile" OFF)
    IF (LUA_RAPIDJSON_BIT32)
       IF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
          SET (CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
          SET (CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
          SET (CMAKE_EXE_LINKER_FLAGS "-m32 ${CMAKE_EXE_LINKER_FLAGS}")
       ELSEIF ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
          SET (CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
          SET (CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
          SET (CMAKE_EXE_LINKER_FLAGS "-m32 ${CMAKE_EXE_LINKER_FLAGS}")
       ENDIF ()
    ENDIF ()
ELSE(UNIX)
    IF (WIN32)
        SET (PLAT "win32")
        ADD_DEFINITIONS (-DLUA_BUILD_AS_DLL -DLUA_LIB -D_CRT_SECURE_NO_WARNINGS)
        ADD_COMPILE_OPTIONS ( /W4 )
    ELSE(WIN32)
        SET (PLAT "unsupported")
    ENDIF (WIN32)
ENDIF (UNIX)

#-------------------------------------------------------------------------------
# Lua dependency
#-------------------------------------------------------------------------------
IF (NOT LUA_INCLUDE_DIR OR (WIN32 AND NOT LUA_LIBRARIES))
    SET (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
    FIND_PACKAGE (Lua REQUIRED)
ENDIF ()
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
# rapidjson external dependency
#-------------------------------------------------------------------------------
SET (LUA_RAPIDJSON ${CMAKE_BINARY_DIR}/rapidjson/cfx)

include(ExternalProject)
ExternalProject_Add(rapidjson
    PREFIX ${LUA_RAPIDJSON}
    GIT_REPOSITORY "https://github.com/Tencent/rapidjson.git"
    GIT_TAG "bfdcf4911047688fec49014d575433e2e5eb05be"
    GIT_SHALLOW 0
    CMAKE_ARGS += -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                  -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                  -DRAPIDJSON_BUILD_TESTS=OFF
                  -DRAPIDJSON_BUILD_DOC=OFF
                  -DRAPIDJSON_BUILD_EXAMPLES=OFF
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    UPDATE_COMMAND ""
)

# RapidJSON is a header-only library
ExternalProject_Get_Property(rapidjson source_dir)
SET (RAPIDJSON_INCLUDE_DIRS ${source_dir}/include)
#-------------------------------------------------------------------------------

INCLUDE_DIRECTORIES (${LUA_INCLUDE_DIR} ${RAPIDJSON_INCLUDE_DIRS})
FILE (GLOB_RECURSE sourceFiles ./src/*.cpp ./src/*.c)
FILE (GLOB_RECURSE headerFiles ./src/*.hpp ./src/*.h)
SET (SOURCES ${sourceFiles} ${headerFiles})

ADD_LIBRARY (lua-rapidjson MODULE ${SOURCES})
ADD_DEPENDENCIES (lua-rapidjson rapidjson)
IF (${CMAKE_VERSION} VERSION_LESS "3.1.0")
    IF (CMAKE_COMPILER_IS_GNUCXX)
        EXECUTE_PROCESS (COMMAND "${CMAKE_CXX_COMPILER} -dumpversion" OUTPUT_VARIABLE GCC_VERSION)
        IF (GCC_VERSION VERSION_LESS 4.7)
            SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
        ELSE ()
            SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
        ENDIF ()
    ENDIF ()
ELSE ()
    TARGET_COMPILE_FEATURES (lua-rapidjson PRIVATE cxx_rvalue_references)
ENDIF ()

SOURCE_GROUP (src FILES ${SOURCES})

IF (WIN32)
    TARGET_LINK_LIBRARIES (lua-rapidjson ${LUA_LIBRARIES})
ENDIF ()

IF (LINK_FLAGS)
    TARGET_INCLUDE_DIRECTORIES (lua-rapidjson PRIVATE ${INCLUDE_DIRECTORIES})
    SET_TARGET_PROPERTIES (lua-rapidjson PROPERTIES LINK_FLAGS ${LINK_FLAGS})
ENDIF ()

SET_TARGET_PROPERTIES (lua-rapidjson PROPERTIES
    OUTPUT_NAME "rapidjson"
    PREFIX ""
)

IF (CMAKE_INSTALL_PREFIX)
  INSTALL (TARGETS lua-rapidjson DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF (CMAKE_INSTALL_PREFIX)
