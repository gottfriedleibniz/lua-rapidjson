# lua-rapidjson
A [rapidjson](https://github.com/Tencent/rapidjson) binding for Lua 5.1, Lua 5.2, Lua 5.3, Lua 5.4, and [LuaJIT](https://github.com/LuaJIT/LuaJIT) with the intention of being an API compatible replacement of [dkjson](https://github.com/LuaDist/dkjson).

## Documentation
The exported API is broken down into three categories: **Configuration**, **Encoding**, and **Decoding**. Deviations from the [dkjson API](http://dkolf.de/src/dkjson-lua.fsl/wiki?name=Documentation) will be noted in the function definition. See **Developer Notes** for implementation details/caveats.

##### Configuration
```lua
-- Return the current value of the global encoding/decoding option.
--
-- Default Flags:
-- 'null' + 'empty_table_as_array' + 'with_hole + 'nan'
--
-- Options:
--  ENCODING_OPTS: [BOOL]
--   'indent' - Encoded string will contain newlines and indentations.
--   'pretty' - Alias of 'indent'.
--   'single_line' - Format arrays on a single line during pretty printing.
--   'sort_keys' - Sort keys of a table prior to encoding.
--   'nesting' - Write null instead of throwing ERROR_DEPTH_LIMIT when the
--      recursive encoding depth exceeds a max_depth.
--
--  ENCODING_OPTS: [NUMBERS]
--   'indent_char' - Index for indentation (0 = ' ', 1 = '\t', 2 = '\n', 3 = '\r').
--   'indent_count' - Number of indent characters for each indentation level.
--   'level' - Alias of "indent_count".
--   'max_depth' - Maximum table recursion depth
--   'decimal_count' - the maximum number of decimal places for double output.
--
--  DECODING_OPTS: [STRING]
--   'decoder_preset' - ["default", "extended"] - Preset decoding configuration.
--      "extended" enables all fields (see rapidjson::ParseFlag).
--
--  NUMBER_OPTS: [BOOL]
--   'nan' - Allow writing of Infinity, -Infinity and NaN.
--   'inf' - Alias of "nan".
--   'unsigned' - Encode integers as unsigned integers/values.
--   'bit32' - Encode integers as 32-bit values.
--   'lua_format_float' - Use sprintf instead of rapidjson's native Grisu2.
--   'lua_round_float' - Massage Grisu2 by rounding at maxDecimalsPlaces.
--
--  TABLE_OPTS [BOOL]:
--   'with_hole' - Allow tables to be encoded as arrays iff all keys are
--      positive integers and satisfies the LUA_RAPIDJSON_TABLE_CUTOFF
--      limitation. Inserting nil's when encoding to satisfy the array type.
--   'empty_table_as_array' - empty tables packed as arrays. Beware, when
--      'always_as_map' is enabled, this flag is forced to disabled.
--   'sentinel' - Replace 'nil' values with a 'sentinel' value during decoding.
--      The encoder will always replace sentinel's with null during encoding.
--   'null' - Alias of 'sentinel'.
value = json.getoption(option)

-- Set a global encoding/decoding option; see json.getoption.
json.setoption(option, value)

-- Returns a sentinel value used to represent "null". Lua 5.1 and LuaJIT require
-- invoking the function while other Lua versions treat sentinel as a 'light' C
-- function, where json.sentinel == json.sentinel().
null = json.null() -- or json.sentinel()

-- [dkjson:REMOVED]
-- Will throw an error.
json.use_lpeg()
```

##### Encoding
```lua
-- Create a string representing an object.
--
-- Supported metamethods for encoding tables/userdata:
--  '__tojson' - A function: 'encoding = F(self)' to allow tables to provide
--      their own customized JSON encoding. If 'encoding' is not a string an
--      exception will be thrown.
--
--  '__jsonorder' - A function: 'keyorder = F(self)' to allow tables to
--      overwrite its keyorder for a specific table. See the 'keyorder'
--      description below.
--
-- @PARAM object: a table, a string, a number, a boolean, "nil", "json.null" or
--  any value with a "__tojson" function in its metatable. A table can only have
--  strings and numbers as keys and its values must all be valid JSON objects
--  as well.
--
-- @PARAM state: an optional table with the following fields:
--   indent: When indent (a boolean) is set, the created string will contain
--     newlines and indentations, i.e., the 'indent' option explicitly set for
--     this encoding. Otherwise it will be one long line.
--
--   indent_amt: This is the initial level of indentation used when indent is
--      set. For each level two spaces are added; when absent it is set to the
--      global 'indent_count'.
--
--   keyorder: an array to specify the ordering of keys in the encoded output.
--      If an object has keys which are not in this array they are written after
--      the sorted keys.
--
--   [dkjson PARTIAL COMPATBILITY]
--   exception: An exception handler: "newValue,newReason = F(reason, value)" where:
--          reason - is "reference cycle", "custom encoder failed", "unsupported type", or "error encoding number".
--          value - the original value that caused the exception.
--
--      The exception handler can either return (1) a modified value that can
--      be; or (2) nil and an expanded error message to be supplied to the Lua
--      exception handling
--
--   [dkjson:REMOVED]
--   buffer: an array to store the strings for the result so they can be
--      concatenated at once. When it isn't given, the encode function will
--      create it temporary and will return the concatenated result.
--
--   [dkjson:REMOVED]
--   bufferlen: the index of the last element of `buffer`.
--
--   [dkjson:REMOVED]
--   tables: a set to detect reference cycles. It is created temporary when
--      absent. Every table that is currently processed is used as key, the
--      value is true.
encodedString = json.encode(object [, state])
```

##### Decoding
```lua
-- Decode a JSON encoded string.
--
-- @PARAM "position": starting position within the string, defaulting to 1.
--
-- @PARAM "null": an optional value to be returned for null values. The default
--  "null" functionality is defined by json.getoption('null'). However, any
--   value may be used.
--
-- @PARAM "objectmeta, arraymeta": Every decoded table is given a metatable that
--  contains __jsontype field. This is used by dkjson to distinguish how an
--  empty table has been created. Consequently, json.encode will prioritize
--  this metafield when encoding empty tables to ensure a JSON string and its
--  re-encoding are identical. The default metatable(s) may be overridden.
--
-- The return values are the object or, in case of errors, nil, the position of
-- the next character that doesn't belong to the object, and an error message.
object[, errPos [, errMessage]] = json.decode(string [, position [, null [, objectmeta [, arraymeta]]]])

-- Return a metatable with an 'object' __jsontype field. See the 'objectmeta'
-- parameter in json.decode
metatable = json.object()

-- Return a metatable with an 'array' __jsontype field. See the 'objectmeta'
-- parameter in json.decode
metatable = json.array()

-- Return true if the provided table has metatable with an 'object' __jsontype field
json.isobject(value)

-- Return true if the provided table has metatable with an 'array' __jsontype field\
json.isarray(value)
```

## Building
A CMake project that builds the shared library is included. See `cmake -LAH` or [cmake-gui](https://cmake.org/runningcmake/) for the complete list of build options.

```bash
# Create build directory:
└> mkdir -p build ; cd build
└> cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release ..

# Using a custom Lua build (Unix). When using Windows, -DLUA_LIBRARIES= must also
# be defined for custom Lua paths. Otherwise, CMake will default to 'FindLua'
└> cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DLUA_INCLUDE_DIR=${LUA_DIR} ..

# Build
└> make
```

### Compile Options
- **LUA\_COMPILED\_AS\_HPP**: Library compiled for Lua with C++ linkage.
- **LUA\_INCLUDE\_TEST**: Build with `LUA_USER_H="ltests.h"`.
- **LUA\_RAPIDJSON\_ALLOCATOR**: Use a `lua_getallocf` binding for the rapidjson allocator class.
- **LUA\_RAPIDJSON\_BIT32**: i386 compilation.
- **LUA\_RAPIDJSON\_COMPAT**: Strict compatibility requirements with dkjson.
- **LUA\_RAPIDJSON\_EXPLICIT**: Throw a lua_Error when handling a non-zero rapidjson::ParseErrorCode instead of returning a `<nil, offset, error message>` tuple when decoding.
- **LUA\_RAPIDJSON\_SANITIZE\_KEYS**: Throw an error if a `__jsonorder` key is neither a string or numeric. Otherwise, ignore the key.
- **LUA\_RAPIDJSON\_LUA\_FLOAT**: Use lua_number2str instead of `internal::dtoa/Grisu2` for formatting numbers.
- **LUA\_RAPIDJSON\_ROUND\_FLOAT**: Round decimals (to a decimal point that coincides `LUA_NUMBER_FMT`) prior to `using internal::dtoa/Grisu2`. Note, this feature is very much a 64-bit hack.
- **LUA\_RAPIDJSON\_TABLE\_CUTOFF**: Threshold for table_is_json_array. If a table of only integer keys has a key greater than this value: ensure at least half of the keys within the table have non-nil objects to be encoded as an array.

## Developer Notes
The [test](test/) directory maintains a collection of scripts carried over from the previous [lua-rapidjson](https://github.com/xpol/lua-rapidjson) implementation. The original [busted](https://olivinelabs.com/busted/) dependency has been replaced with a minimal set of functions to emulate the required unit testing.

### TODO
1. An actual C API.
1. Allow `LUA_RAPIDJSON_TABLE_CUTOFF` to be a runtime option.
1. Replace uses of `std::vector` for key-ordering with temporarily anchored userdata.
1. Document "test/" and possibly reintroduce github/workflows support.
1. Given that this library ignores the schema/document bits of rapidjson, its possible to introduce a CrtAllocator implementation that throws exceptions on failure.

## Sources & Acknowledgments:
1. [rapidjson](https://github.com/Tencent/rapidjson): json spec implementation.
1. [lua-rapidjson](https://github.com/xpol/lua-rapidjson): original implementation.
1. [dkjson](https://github.com/LuaDist/dkjson) and its [documentation](http://dkolf.de/src/dkjson-lua.fsl/wiki?name=Documentation): API compatibility reference.

## License
lua-rapidjson is distributed under the terms of the [MIT license](https://opensource.org/licenses/mit-license.html); see [lua_rapidjsonlib.h](src/lua_rapidjsonlib.h)
