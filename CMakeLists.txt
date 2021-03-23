CMAKE_MINIMUM_REQUIRED( VERSION 3.9.0 )
project( luarapidjson CXX )

INCLUDE(CheckCXXCompilerFlag)
IF( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
  SET(CMAKE_COMPILER_IS_CLANGXX 1) # using Clang or AppleClang
ENDIF()

# Lua
IF( NOT LUA_INCLUDE_DIR OR (WIN32 AND NOT LUA_LIBRARIES) )
  SET( CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/" )
  FIND_PACKAGE( Lua REQUIRED )
ENDIF()

################################################################################
# Path/Library Setup.
################################################################################

SET(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/etc/")
IF( NOT CMAKE_BUILD_TYPE )
  SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, \
  options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
ENDIF()

SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/" CACHE STRING "Library build output directory")
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/")
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/")

SET(CMAKE_INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Installation directory for libraries")
SET(CMAKE_INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")

################################################################################
# Compiler Setup
################################################################################

# Enables the use of Intel AVX2 if/when possible
OPTION(LUA_NATIVE_ARCH "Enable compiler optimizations for the native processor architecture" OFF)

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Windows systems
IF( MSVC )
  OPTION(LUA_USE_WINDOWS "Windows specific build" ON)
  OPTION(LUA_BUILD_AS_DLL "Build Lua library as DLL" ON)
  IF( NOT( ${CMAKE_COMPILER_IS_CLANG} ) )
    MESSAGE(WARNING "MSVC detected! For MSVC/msbuild environments, it is advised to use the LLVM/ClangCL toolset")
  ENDIF()

  IF(MSVC AND NOT "${CMAKE_BUILD_TYPE}" MATCHES "Debug")
    SET(MSVC_OPT_FLAG "/Ox /DNDEBUG")
  ELSE()
    SET(MSVC_OPT_FLAG "")
  ENDIF()

  IF( LUA_NATIVE_ARCH )
    CHECK_CXX_COMPILER_FLAG("/arch:AVX2" COMPILER_OPT_ARCH_AVX2_SUPPORTED)
    IF( LUA_NATIVE_ARCH AND COMPILER_OPT_ARCH_AVX2_SUPPORTED )
      ADD_COMPILE_OPTIONS(/arch:AVX2)
    ENDIF()
  ENDIF()

  ADD_COMPILE_OPTIONS(/nologo /Zc:__cplusplus)
  ADD_COMPILE_DEFINITIONS(NOMINMAX)
  ADD_COMPILE_DEFINITIONS(_CRT_SECURE_NO_DEPRECATE)
  ADD_COMPILE_DEFINITIONS(_CRT_NONSTDC_NO_DEPRECATE)

  SET(LUA_CXX_FLAGS "${MSVC_OPT_FLAG} /Ob2 /Oi /Ot /GT /GF /GS- /MP /EHsc /GL")

  # Visual Studio 2019 feature
  CHECK_CXX_COMPILER_FLAG("/Ob3" COMPILER_OPT_OB3)
  IF( COMPILER_OPT_OB3 )
    STRING(REPLACE "/Ob2" "/Ob3" LUA_CXX_FLAGS "${LUA_CXX_FLAGS}")
  ENDIF()

  SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oy ${LUA_CXX_FLAGS} /DNDEBUG")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Zi ${LUA_CXX_FLAGS} /DNDEBUG")
  SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /EHs /RTC1")
  # SET(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")

  # W4 to turn compiler warnings all the way up...
  STRING(REGEX REPLACE "[-/]W[1-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")

# Posix systems (including msys2/cygwin)
ELSE()
  OPTION(LUA_BIT32 "32bit compile" OFF)
  OPTION(LUA_USE_POSIX "Use POSIX functionality" ON)

  IF( APPLE )
    SET(PLAT "macosx")
    SET(LINK_FLAGS "-bundle -undefined dynamic_lookup -all_load")
  ENDIF()

  IF( LUA_BIT32 )
    IF( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR CMAKE_COMPILER_IS_CLANGXX )
      SET(CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
    ENDIF()
  ENDIF()

  # Compile everything with -march=native
  IF( LUA_NATIVE_ARCH )
    CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_OPT_ARCH_NATIVE_SUPPORTED)
    IF( LUA_NATIVE_ARCH AND COMPILER_OPT_ARCH_NATIVE_SUPPORTED )
      ADD_COMPILE_OPTIONS(-march=native)
    ENDIF()
  ENDIF()

  # Temporary fix: CMake no longer defines WIN32 on MSYS.
  IF( CYGWIN OR MSYS )
    SET(LUA_BUILD_AS_DLL ON)
    ADD_COMPILE_DEFINITIONS(_WIN32)
  ENDIF()

  # Compiler options taken from Lua: lua/blob/master/makefile
  ADD_COMPILE_OPTIONS(
    -Wall -Wextra -Wsign-compare -Wundef -Wwrite-strings
    -Wredundant-decls -Wdisabled-optimization -Wdouble-promotion -Wno-inline
    -Wdouble-promotion -Wunused-parameter -Wno-comment -Wno-unused-function
    -Wno-array-bounds -Wno-sign-conversion -Wstrict-overflow=2 -Wformat=2
    -Wno-format-nonliteral
    # -Wshadow
    # -Werror
    # -Wconversion
    # -Wsign-conversion
    # -Wcast-qual
  )

  IF( CMAKE_COMPILER_IS_CLANGXX )
    ADD_COMPILE_OPTIONS(-ferror-limit=5 -fno-common -Wno-deprecated
      # Clang Everything support;
      #-Weverything
      #-Wno-gnu-zero-variadic-macro-arguments
      #-Wno-covered-switch-default
      #-Wno-reserved-id-macro
      #-Wno-unused-macros
      #-Wno-padded
      #-Wno-old-style-cast
      #-Wno-extra-semi-stmt
      #-Wno-c++98-compat-pedantic
      #-Wno-zero-as-null-pointer-constant
      #-Wno-gnu-anonymous-struct
      #-Wno-gnu-label-as-value
      #-Wno-nested-anon-types
      #-Wno-enum-enum-conversion
    )
  ELSEIF( CMAKE_COMPILER_IS_GNUCXX )
    ADD_COMPILE_OPTIONS(-fmax-errors=5 -Wduplicated-cond -Wrestrict -Wlogical-op
      -Wno-aggressive-loop-optimizations
      -Wno-ignored-qualifiers
      #-Wnull-dereference
      #-Wduplicated-branches
    )
  ENDIF()
ENDIF()

################################################################################
# Build Options
################################################################################

OPTION(LUA_COMPILED_AS_HPP "Library compiled for Lua with C++ linkage" OFF)
OPTION(LUA_USE_LONGJMP_HPP "C++ compiled Lua still uses longjmp/setjmp for exception handling" OFF)
OPTION(LUA_INCLUDE_TEST "Include ltests.h" OFF)

OPTION(LUA_RAPIDJSON_COMPAT "Strict compatibility requirements with dkjson" OFF)
OPTION(LUA_RAPIDJSON_EXPLICIT "Explicit error vs. Returned error message" OFF)
OPTION(LUA_RAPIDJSON_SANITIZE_KEYS "Ensure key-types are string/numeric on encoding" ON)
OPTION(LUA_RAPIDJSON_LUA_FLOAT "Use lua_number2str instead of internal::dtoa/Grisu2" OFF)
OPTION(LUA_RAPIDJSON_ROUND_FLOAT "Round decimals prior to using internal::dtoa/Grisu2" OFF)
OPTION(LUA_RAPIDJSON_ALLOCATOR "Use a lua_getallocf binding for the rapidjson allocator class" ON)
SET(LUA_RAPIDJSON_TABLE_CUTOFF CACHE STRING
  "Threshold for table_is_json_array. If a table of only integer keys has a \
  key greater than this value: ensure at least half of the keys within the \
  table have non-nil objects to be encoded as an array."
)

IF( CMAKE_BUILD_TYPE STREQUAL Debug )
  SET(LUA_INCLUDE_TEST ON)
ENDIF()

IF( LUA_COMPILED_AS_HPP )
  ADD_COMPILE_DEFINITIONS(LUA_COMPILED_AS_HPP)
  IF ( LUA_USE_LONGJMP_HPP )
    ADD_COMPILE_DEFINITIONS(LUA_USE_LONGJMP_HPP)
  ENDIF()
ENDIF()

IF( LUA_INCLUDE_TEST )
  ADD_COMPILE_DEFINITIONS(LUA_USER_H="ltests.h")
ENDIF()

IF( LUA_RAPIDJSON_COMPAT )
  ADD_COMPILE_DEFINITIONS(LUA_RAPIDJSON_COMPAT)
ENDIF()

IF( LUA_RAPIDJSON_EXPLICIT )
  ADD_COMPILE_DEFINITIONS(LUA_RAPIDJSON_EXPLICIT)
ENDIF()

IF( LUA_RAPIDJSON_SANITIZE_KEYS )
  ADD_COMPILE_DEFINITIONS(LUA_RAPIDJSON_SANITIZE_KEYS)
ENDIF()

IF( LUA_RAPIDJSON_LUA_FLOAT )
  ADD_COMPILE_DEFINITIONS(LUA_RAPIDJSON_LUA_FLOAT)
ELSEIF( LUA_RAPIDJSON_ROUND_FLOAT )
  ADD_COMPILE_DEFINITIONS(LUA_RAPIDJSON_ROUND_FLOAT)
ENDIF()

IF( LUA_RAPIDJSON_ALLOCATOR )
  ADD_COMPILE_DEFINITIONS(LUA_RAPIDJSON_ALLOCATOR)
ENDIF()

IF( LUA_RAPIDJSON_TABLE_CUTOFF )
  ADD_COMPILE_DEFINITIONS(LUA_RAPIDJSON_TABLE_CUTOFF=${LUA_RAPIDJSON_TABLE_CUTOFF})
ENDIF()

################################################################################
# rapidjson external dependency
################################################################################

include(ExternalProject)
ExternalProject_Add(rapidjson
  PREFIX rapidjson
  GIT_REPOSITORY "https://github.com/Tencent/rapidjson.git"
  GIT_TAG "master"
  GIT_SHALLOW 1
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  UPDATE_COMMAND ""
  TEST_COMMAND ""
  UPDATE_COMMAND ""
  PATCH_COMMAND ""
  EXCLUDE_FROM_ALL TRUE
  CMAKE_ARGS +=
    -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=true
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}
    -DRAPIDJSON_BUILD_TESTS=OFF
    -DRAPIDJSON_BUILD_DOC=OFF
    -DRAPIDJSON_BUILD_EXAMPLES=OFF
)

# RapidJSON is a header-only library
ExternalProject_Get_Property(rapidjson source_dir)
SET(RAPIDJSON_INCLUDE_DIRS ${source_dir}/include)

################################################################################
# Compilation
################################################################################

INCLUDE_DIRECTORIES( ./src/ )
INCLUDE_DIRECTORIES(${LUA_INCLUDE_DIR})
INCLUDE_DIRECTORIES(${RAPIDJSON_INCLUDE_DIRS})

FILE(GLOB_RECURSE SRC_LUARAPIDJSON ./src/*.cpp ./src/*.c)
SET_SOURCE_FILES_PROPERTIES(${SRC_LUARAPIDJSON} PROPERTIES LANGUAGE CXX )
SET(PUB_LUARAPIDJSON src/lua_rapidjsonlib.h )

# 51 release does not bundle lua.hpp; must explicitly handle in code
INCLUDE(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX("${LUA_INCLUDE_DIR}/lua.hpp" HAVE_LUA_HPP)
IF((${HAVE_LUA_HPP}) AND NOT ${LUA_COMPILED_AS_HPP})
  ADD_COMPILE_OPTIONS(-DLUA_INCLUDE_HPP)
ELSE()
  MESSAGE(WARNING "lua.hpp not bundled, explicitly including headers")
ENDIF()

ADD_LIBRARY(luarapidjson MODULE ${SRC_LUARAPIDJSON})
ADD_DEPENDENCIES(luarapidjson rapidjson)

SET_TARGET_PROPERTIES(luarapidjson PROPERTIES OUTPUT_NAME "rapidjson" PREFIX "")
IF( LUA_BIT32 )
  SET_TARGET_PROPERTIES(luarapidjson PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
ENDIF()

IF( LINK_FLAGS )
  SET_TARGET_PROPERTIES(luarapidjson PROPERTIES LINK_FLAGS "${LINK_FLAGS}")
ENDIF()

IF( LUA_BUILD_AS_DLL )
  TARGET_COMPILE_DEFINITIONS(luarapidjson PUBLIC LUA_BUILD_AS_DLL)
ENDIF()

# Win32 modules need to be linked to the Lua library.
IF( WIN32 OR CYGWIN OR MSYS )
  TARGET_INCLUDE_DIRECTORIES(luarapidjson PRIVATE ${INCLUDE_DIRECTORIES})
  TARGET_LINK_LIBRARIES(luarapidjson ${LUA_LIBRARIES})
ENDIF()

IF( CMAKE_INSTALL_PREFIX )
  INSTALL(TARGETS luarapidjson DESTINATION "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF(CMAKE_INSTALL_PREFIX)

################################################################################
# Install
################################################################################

# Don't allow installation if including test builds
SET(SKIP_INSTALL SKIP_INSTALL_ALL OR LUA_INCLUDE_TEST)

IF( NOT SKIP_INSTALL_HEADERS AND NOT ${SKIP_INSTALL} )
  INSTALL(FILES ${PUB_LUARAPIDJSON} DESTINATION "${CMAKE_INSTALL_INC_DIR}")
ENDIF()

IF( NOT SKIP_INSTALL_LIBRARIES AND NOT ${SKIP_INSTALL} )
  IF( UNIX AND NOT APPLE )
    SET_TARGET_PROPERTIES(luarapidjson PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIB_DIR}" )
  ENDIF()

  INSTALL(TARGETS luarapidjson
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIB_DIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIB_DIR}
  )
ENDIF()