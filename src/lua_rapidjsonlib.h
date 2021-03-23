/*
** dkjson compatibility:
**
**  <metatable>.__jsonorder
**    __jsonorder can overwrite the keyorder for a specific table.
**
**  <metatable>.__tojson (self, state)
**    You can provide your own __tojson function in a metatable. In this
**    function you can either add directly to the buffer and return true, or you
**    can return a string. On errors nil and a message should be returned.
**
**  [compat.lua]
**  json.quotestring (string): Quote a UTF-8 string and escape critical
**    characters using JSON escape sequences. This function is only necessary
**    when you build your own __tojson functions.
**
**  [compat.lua]
**  json.encodeexception (reason, value, state, defaultmessage): This function
**    can be used as value to the exception option. Instead of raising an error
**    this function encodes the error message as a string. This can help to
**    debug malformed input data.
**
** See Copyright Notice at the end of this file
*/
#ifndef lrapidjsonlib_h
#define lrapidjsonlib_h

#include <lua.h>

#define LUA_RAPIDJSON_NAME "lua-rapidjson"
#define LUA_RAPIDJSON_VERSION "lua-rapidjson 1.2.1"
#define LUA_RAPIDJSON_COPYRIGHT "Copyright (C) 2015 Xpol Wan; 2020, Gottfried Leibniz"
#define LUA_RAPIDJSON_DESCRIPTION "rapidjson bindings for Lua"

#if defined(__cplusplus)
extern "C" {
#endif

/*
** {==================================================================
** Library
** ===================================================================
*/

#if !defined(LUAMOD_API)  /* LUA_VERSION_NUM == 501 */
  #define LUAMOD_API LUALIB_API
#endif

#define LUA_RAPIDJSON_JSON_LIBNAME "json"
LUAMOD_API int (luaopen_rapidjson) (lua_State *L);

/* }================================================================== */

/*
** {==================================================================
** C/C++ API
** ===================================================================
*/

/*
** json.encode(object [, state])
**
** Create a string representing an object.
**
**  @PARAM object: a table, a string, a number, a boolean, "nil", "json.null" or
**   any value with a "__tojson" function in its metatable. A table can only
**   have strings and numbers as keys and its values must all be valid JSON
**   objects as well.
**
**  @PARAM state: an optional table with the following fields:
**
**    indent: When indent (a boolean) is set, the created string will contain
**      newlines and indentations. Otherwise it will be one long line.
**
**    keyorder: an array to specify the ordering of keys in the encoded output.
**      If an object has keys which are not in this array they are written after
**      the sorted keys.
**
**    indent_amt: This is the initial level of indentation used when indent is
**       set. For each level two spaces are added; when absent it is set to 0.
**
**    [dkjson PARTIAL COMPATBILITY]
**    exception: An exception handler: "newValue,newReason = F(reason, value)" where:
**           reason - is "reference cycle", "custom encoder failed", "unsupported type", or "error encoding number".
**           value - the original value that caused the exception.
**
**       The exception handler can either return (1) a modified value that can
**       be; or (2) nil and an expanded error message to be supplied to the Lua
**       exception handling
**
**    [dkjson:REMOVED]
**    buffer: an array to store the strings for the result so they can be
**    concatenated at once. When it isn't given, the encode function will create
**    it temporary and will return the concatenated result.
**
**    [dkjson:REMOVED]
**    bufferlen: the index of the last element of `buffer`.
**
**    [dkjson:REMOVED]
**    tables: a set to detect reference cycles. It is created temporary when
**	  absent. Every table that is currently processed is used as key, the value
**	  is true.
*/
LUALIB_API int rapidjson_encode(lua_State *L);

/*
** json.decode(string [, position [, null [, objectmeta [, arraymeta]]]])
**
** Decode a JSON encoded string.
**
**  @PARAM "position": starting position within the string, defaulting to 1 if
**   if position was omitted.
**
**  @PARAM "null": an optional value to be returned for null values. The default
**   "null" functionality is defined by json.getoption('null'). However,
**   json.null or any other value may be used.
**
**  @PARAM "objectmeta, arraymeta": Every decoded table is given a metatable
**	 that contains __jsontype field. This is used in dkjson to distinguish how
**	 an empty table has been created. Consequently, json.encode will prioritize
**   this metafield when encoding empty tables ensuring the string and its
**   re-encoding are identical. These default metatables can be overridden.
**
** The return values are the object or, in case of errors, nil, the position of
** the next character that doesn't belong to the object, and an error message.
*/
LUALIB_API int rapidjson_decode(lua_State *L);

/*
** Return the current value of the global encoding/decoding option.
**
** Options:
**  ENCODING_OPTS: [BOOL]
**   'indent' - Encoded string will contain newlines and indentations.
**   'pretty' - Alias of 'indent'.
**   'single_line' - Format arrays on a single line during pretty printing.
**   'sort_keys' - Sort keys of a table prior to encoding.
**   'nesting' - Write null instead of throwing ERROR_DEPTH_LIMIT when the
**      recursive encoding depth exceeds a max_depth.
**
**  ENCODING_OPTS: [NUMBERS]
**   'indent_char' - Index for indentation (0 = ' ', 1 = '\t', 2 = '\n', 3 = '\r').
**   'indent_count' - Number of indent characters for each indentation level.
**   'level' - Alias of "indent_count".
**   'max_depth' - Maximum table recursion depth
**   'decimal_count' - the maximum number of decimal places for double output.
**
**  DECODING_OPTS: [NUMBERS]
**   'decoder_preset' - ["default", "extended"] - Preset parsing configuration.
**      "extended" enables all fields (see rapidjson::ParseFlag).
**
**  NUMBER_OPTS: [BOOL]
**   'nan' - Allow writing of Infinity, -Infinity and NaN.
**   'inf' - Alias of "nan".
**   'unsigned' - Encode integers as unsigned integers/values.
**   'bit32' - Encode integers as 32-bit values.
**   'lua_format_float' - Use sprintf instead of rapidjson's native Grisu2.
**   'lua_round_float' - Massage Grisu2 by rounding at maxDecimalsPlaces.
**
**  TABLE_OPTS:
**   'with_hole' - Allow tables to be encoded as arrays iff all keys are
**      positive integers and satisfies the MP_TABLE_CUTOFF limitation.
**      Inserting nil's when encoding to satisfy the array type.
**   'empty_table_as_array' - empty tables packed as arrays. Beware, when
**      'always_as_map' is enabled, this flag is forced to disabled.
**   'sentinel' - Replace 'nil' values with a 'sentinel' value during decoding.
**      The encoder will always replace sentinel's with null during encoding.
**   'null' - Alias of 'sentinel'.
*/
LUALIB_API int rapidjson_setoption (lua_State *L);
LUALIB_API int rapidjson_getoption (lua_State *L);

/* Pushes the null-sentinel onto the stack; returning 1. */
LUALIB_API int rapidjson_null (lua_State *L);

LUALIB_API int rapidjson_object (lua_State *L);
LUALIB_API int rapidjson_isobject (lua_State *L);

LUALIB_API int rapidjson_array (lua_State *L);
LUALIB_API int rapidjson_isarray (lua_State *L);

/* }================================================================== */

#if defined(__cplusplus)
}
#endif

/******************************************************************************
* luamsgpack-c
* Copyright (C) 2021 - gottfriedleibniz
* Copyright (C) 2015 - Xpol Wan (https://github.com/xpol/lua-rapidjson)
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

#endif
