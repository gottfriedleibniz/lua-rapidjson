/*
** $Id: lua_rapidjson.cpp $
** rapidjson binding library
** See Copyright Notice in LICENSE
*/
#define lua_rapidjson_c
#define LUA_LIB

#include <vector>
#include <algorithm>

#include <rapidjson/rapidjson.h>
#include <rapidjson/encodedstream.h>
#include <rapidjson/error/en.h>
#include <rapidjson/error/error.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>

#include "lua_rapidjson.hpp"
#include "StringStream.hpp"

#include "lua_rapidjsonlib.h"

/* Registry Table */
#define LUA_RAPIDJSON_REG "lua_rapidjson"

#if LUA_VERSION_NUM <= 502
typedef int json_regType;  // Registry Table KeyType
#else
typedef lua_Integer json_regType;  // Registry Table KeyType
#endif

/* Registry Subtable Keys */
#define LUA_RAPIDJSON_REG_FLAGS 1
#define LUA_RAPIDJSON_REG_DEPTH 2
#define LUA_RAPIDJSON_REG_INDENT 3
#define LUA_RAPIDJSON_REG_INDENT_AMT 4
#define LUA_RAPIDJSON_REG_MAXDEC 5
#define LUA_RAPIDJSON_REG_PRESET 6

#define json_conf_getfield(L, I, K) lua_rawgeti((L), (I), (K))
#define json_conf_setfield(L, I, K) lua_rawseti((L), (I), (K))

#define lua_absindex(L, i) ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : lua_gettop(L) + (i) + 1)
static int lua_rapidjson_getsubtable (lua_State *L, int idx, const char *key) {
  if (json_getfield(L, idx, key) == LUA_TTABLE)
    return 1;  // table already there
  else {
    lua_pop(L, 1);  // remove previous result
    idx = lua_absindex(L, idx);
    lua_createtable(L, LUA_RAPIDJSON_REG_PRESET + 1, 0);
    lua_pushvalue(L, -1);  // copy to be left at top
    lua_setfield(L, idx, key);  // assign new table to field
    return 0;  // false, because did not find table there
  }
}

static lua_Integer geti (lua_State *L, int idx, json_regType key, lua_Integer opt) {
  json_conf_getfield(L, idx, key);

  const lua_Integer result = luaL_optinteger(L, -1, opt);
  lua_pop(L, 1);
  return result;
}

/* Push a integer into the registry table at the specified key */
static void seti (lua_State *L, int idx, json_regType key, lua_Integer value) {
  lua_pushinteger(L, value);  // [..., value]
  json_conf_setfield(L, json_rel_index(idx, 1), key);  // [...]
}

/*
** If the stack argument is a convertible to a size_t from an lua_Integer,
** returns the size_t. If the argument is absent or is nil, returns def.
** Otherwise, throw an error.
*/
static size_t luaL_optsizet (lua_State *L, int arg, size_t def) {
  if (lua_isnoneornil(L, arg))
    return def;
  else if (json_isinteger(L, arg)) {  // 5.1/5.2: Number not an integer
    const lua_Integer i = lua_tointeger(L, arg);
    if (i >= 0 && static_cast<size_t>(i) <= JSON_MAX_LUAINDEX)
      return static_cast<size_t>(i);

    luaL_argerror(L, arg, "invalid integer argument");
    return 0;
  }
  luaL_argerror(L, arg, lua_pushfstring(L, "integer expected"));
  return 0;
}

/* A luaL_checkoption that doesn't throw an error. */
static int luaL_optcheckoption (lua_State *L, int arg, const char *def, const char *const lst[], int ldef) {
  const char *name = (def) ? luaL_optstring(L, arg, def) : luaL_checkstring(L, arg);
  for (int i = 0; lst[i]; i++) {
    if (strcmp(lst[i], name) == 0)
      return i;
  }
  return ldef;
}

/*
** {==================================================================
** API Functions
** ===================================================================
*/

// Default exception implementation.
int json_error (lua_State *L, const char *message) {
  JSON_UNUSED(L);
  throw rapidjson::LuaException(message);
}

int json_null (lua_State *L) {
  lua_pushcfunction(L, json_null);
  return 1;
}

bool is_json_null (lua_State *L, int idx) {
  return lua_tocfunction(L, idx) == json_null;
}

bool has_json_type (lua_State *L, int idx, bool *is_array) {
  bool result = false;

  const int type = luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_TYPE);
  if (type != LUA_METAFIELD_FAIL) {
#if LUA_VERSION_NUM > 502
    if (type == LUA_TSTRING) {
#else
    if (lua_type(L, -1) == LUA_TSTRING) {
#endif
      *is_array = strcmp(lua_tostring(L, -1), LUA_RAPIDJSON_META_TYPE_ARRAY) == 0;
      result = true;
    }
    lua_pop(L, 1);
  }

  return result;
}

bool table_is_json_array (lua_State *L, int idx, lua_Integer flags, size_t *array_length) {
  const int stacktop = lua_gettop(L);
  const int i_idx = json_rel_index(idx, 1);

  bool has_type = false;  // Has __jsontype field;
  bool is_array = false;  // __jsontype field corresponds to an "array" value.

  size_t count = 0;  // Number of valid array elements
  size_t max = 0;  // Maximum (parsed) integer key
  size_t arraylen = 0;  // Supplied table.pack 'n' value

  json_checkstack(L, 3);
  has_type = has_json_type(L, idx, &is_array);

  lua_pushnil(L);  // [..., key]
  while (lua_next(L, i_idx)) {  // [..., key, value]
    lua_Integer n;
    size_t strlen = 0;
    const char *key = nullptr;

    /* && within range of size_t */
    if (json_isinteger(L, -2) && (n = lua_tointeger(L, -2), (n >= 1 && static_cast<size_t>(n) <= JSON_MAX_LUAINDEX))) {
      const size_t nst = static_cast<size_t>(n);
      max = nst > max ? nst : max;
      count++;
    }
    /* Similar to dkjson; support the common table.pack / { n = select("#", ...), ... } idiom */
    else if (lua_type(L, -2) == LUA_TSTRING
             && json_isinteger(L, -1)
             && (n = lua_tointeger(L, -1), (n >= 1 && static_cast<size_t>(n) <= JSON_MAX_LUAINDEX))
             && (key = lua_tolstring(L, -2, &strlen)) != nullptr
             && strlen == 1 && key[0] == 'n') {
      arraylen = static_cast<size_t>(n);
      max = arraylen > max ? arraylen : max;
    }
    else {
      lua_settop(L, stacktop);
      return false;
    }
    lua_pop(L, 1);  // [..., key]
  }

  *array_length = max;
  lua_settop(L, stacktop);

  /*
  ** encode2: encode an empty Lua table as an object iff its given an object
  ** jsontype. (Library addition:) Otherwise, only encode an empty table as an
  ** object if the JSON_ARRAY_EMPTY is not set.
  **/
  if (max == 0 && has_type && !is_array)
    return false;
  else if (max == count)  // all keys are positive integers from [1, count] or the table is empty;
    return max > 0 || (flags & JSON_ARRAY_EMPTY);
  else if (flags & JSON_ARRAY_WITH_HOLES)  // do not create an array with too many holes (inserted nils);
    return (max < LUA_RAPIDJSON_TABLE_CUTOFF) || max <= arraylen || (count >= (max >> 1));

  return false;
}

/* }================================================================== */

/*
** {==================================================================
** CoreAPI
** ===================================================================
*/

/* kParseDefaultFlags */
#define JSON_DECODE_DEFAULT 0x0

/*
** kParseFullPrecisionFlag + kParseCommentsFlag + kParseTrailingCommasFlag
** kParseNanAndInfFlag + kParseEscapedApostropheFlag
*/
#define JSON_DECODE_EXTENDED 0x1

/* PrettyWriter indentation characters */
static const char pretty_indent[] = { ' ', '\t', '\n', '\r' };

/* Configuration options */
static const char *const option_keys[] = {
  "",
  "indent", "pretty",
  "sort_keys",
  "null",
  "nesting",
  "unsigned",
  "single_line",
  "empty_table_as_array",
  "with_hole",
  "decoder_preset",
  "max_depth",
  "indent_char",
  "indent_count", "level",  /* state.level in dkjson */
  "decimal_count",
  "keyorder",
  nullptr
};

/* option_keys -> integer codes */
static const lua_Integer option_keys_num[] = {
  0x0, /* RESERVED */
  JSON_PRETTY_PRINT, JSON_PRETTY_PRINT,
  JSON_SORT_KEYS,
  JSON_LUA_NULL,
  JSON_NESTING_NULL,
  JSON_UNSIGNED_INTEGERS,
  JSON_ARRAY_SINGLE_LINE,
  JSON_ARRAY_EMPTY,
  JSON_ARRAY_WITH_HOLES,
  JSON_DECODER_PRESET,
  JSON_ENCODER_MAX_DEPTH,
  JSON_ENCODER_INDENT,
  JSON_ENCODER_INDENT_AMT, JSON_ENCODER_INDENT_AMT,
  JSON_ENCODER_DECIMALS,
  JSON_TABLE_KEY_ORDER,
};

/* Decoder PrettyWriter/Writer preset configurations */
static const char *const decode_presets[] = {
  "default", "extended", nullptr
};

/* decode_presets -> integer codes */
static const lua_Integer decode_presets_num[] = {
  JSON_DECODE_DEFAULT,
  JSON_DECODE_EXTENDED,
};

static inline void create_shared_meta (lua_State *L, const char *meta, const char *type) {
  luaL_newmetatable(L, meta);
  lua_pushstring(L, type);
  lua_setfield(L, -2, LUA_RAPIDJSON_META_TYPE);
  lua_pop(L, 1);
}

static int make_table_type (lua_State *L, int idx, const char *meta, const char *type) {
  if (lua_isnoneornil(L, idx))
    lua_createtable(L, 0, 0);
  else if (lua_istable(L, idx)) {
    lua_pushvalue(L, idx);  // [..., table]
    if (lua_getmetatable(L, -1)) {  // already has metatable, set __jsontype field
      lua_pushstring(L, type);  // [..., table, meta_table, type]
      lua_setfield(L, -2, LUA_RAPIDJSON_META_TYPE);
      lua_pop(L, 1);  // [..., table]
      return 1;
    }
  }
  else {
    return luaL_argerror(L, idx, "optional table excepted");
  }

  luaL_getmetatable(L, meta);  // [..., table, meta_table]
  lua_setmetatable(L, -2);  // [..., table]
  return 1;
}

extern "C" {

LUALIB_API int rapidjson_encode (lua_State *L) {
  const int top = lua_gettop(L);
  int state_idx = -1;  // Stack index of function-supplied state/configuration table.
  int depth = LUA_RAPIDJSON_DEFAULT_DEPTH;  // Maximum nested-table/recursive depth
  int decimals = LUA_NUMBER_FMT_LEN;  // rapidjson::Writer<rapidjson::StringBuffer>::kDefaultMaxDecimalPlaces;

  lua_Integer flags = JSON_DEFAULT;  // Encoding flags
  lua_Integer indent = 0;  // Indentation character index
  lua_Integer indent_amt = 0;  // Indentation character count
  lua_Integer parsemode = JSON_DECODE_DEFAULT;  // Parsing PrettyWriter/Writer mode (preset configuration)

  /* Parse default options */
  lua_rapidjson_getsubtable(L, LUA_REGISTRYINDEX, LUA_RAPIDJSON_REG);
  flags = geti(L, -1, LUA_RAPIDJSON_REG_FLAGS, flags);
  indent = geti(L, -1, LUA_RAPIDJSON_REG_INDENT, indent);
  parsemode = geti(L, -1, LUA_RAPIDJSON_REG_PRESET, parsemode);
  indent_amt = geti(L, -1, LUA_RAPIDJSON_REG_INDENT_AMT, (indent == 0) ? 4 : 0);
  depth = static_cast<int>(geti(L, -1, LUA_RAPIDJSON_REG_DEPTH, depth));
  decimals = static_cast<int>(geti(L, -1, LUA_RAPIDJSON_REG_MAXDEC, decimals));
  lua_pop(L, 1);

  try {
    std::vector<LuaSAX::Key> order;
    rapidjson::StringBuffer s;
#if defined(LUA_RAPIDJSON_ALLOCATOR)
    rapidjson::LuaAllocator singleton(L);  // Update singleton
#endif

    if (lua_istable(L, 2)) {  // Parse all options from the argument table
      state_idx = 2;

      lua_pushnil(L);
      while (lua_next(L, 2)) {  // [..., key, value]
        const lua_Integer opt = option_keys_num[luaL_optcheckoption(L, -2, nullptr, option_keys, 0)];
        switch (opt) {
          case JSON_PRETTY_PRINT:
          case JSON_SORT_KEYS:
          case JSON_LUA_NULL:
          case JSON_UNSIGNED_INTEGERS:
          case JSON_ARRAY_SINGLE_LINE:
          case JSON_ARRAY_EMPTY:
          case JSON_ARRAY_WITH_HOLES:
            flags = lua_toboolean(L, -1) ? (flags | opt) : (flags & ~opt);
            break;
          case JSON_ENCODER_MAX_DEPTH:
            if ((depth = static_cast<int>(lua_tointeger(L, -1))) <= 0)
              return json_error(L, "invalid encoder depth");
            break;
          case JSON_ENCODER_DECIMALS:
            if ((decimals = static_cast<int>(lua_tointeger(L, -1))) <= 0)
              return json_error(L, "invalid decimal count");
            break;
          case JSON_ENCODER_INDENT:
            indent = lua_tointeger(L, -1);
            if (indent < 0 || indent >= 4)
              return json_error(L, "invalid indentation index");
            break;
          case JSON_ENCODER_INDENT_AMT:
            if ((indent_amt = lua_tointeger(L, -1)) < 0)
              return json_error(L, "invalid indentation amount");
            break;
          case JSON_TABLE_KEY_ORDER: {
            if (lua_istable(L, -1)) {
              if (LuaSAX::populate_key_vector(L, -1, order) != 0)
                return json_error(L, "invalid key_order element");
            }
            break;
          }
          default:
            break;
        }
        lua_pop(L, 1);  // [..., key]
      }
    }
    else if (!lua_isnoneornil(L, 2)) {
      return json_error(L, "Argument 2: table or nothing expected");
    }

    /* Sanitize pretty_print parameters even when not using them. */
    if (indent < 0 || indent >= 4 || depth < 0)
      return json_error(L, "invalid encoder parameters");

    /* Temporary fix for propagating runtime "NanAndInf" checking */
    if (parsemode == JSON_DECODE_EXTENDED)
      flags |= JSON_NAN_AND_INF;

    LuaSAX::Writer encode(flags, depth, state_idx, order);
    if (flags & JSON_PRETTY_PRINT) {
      rapidjson::PrettyFormatOptions option = rapidjson::PrettyFormatOptions::kFormatDefault;
      if (flags & JSON_ARRAY_SINGLE_LINE)
        option = rapidjson::PrettyFormatOptions::kFormatSingleLineArray;

      auto dowriting = [&](auto &writer) {
        writer.SetMaxDecimalPlaces(decimals);
        writer.SetIndent(pretty_indent[indent], static_cast<unsigned>(indent_amt));
        writer.SetFormatOptions(option);
        encode.encodeValue(L, writer, 1);
      };

      switch (parsemode) {
        case JSON_DECODE_EXTENDED: {
          rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, RAPIDJSON_ALLOCATOR, rapidjson::WriteFlag::kWriteNanAndInfFlag> writer(s);
          dowriting(writer);
          break;
        }
        case JSON_DECODE_DEFAULT:
        default: {
          rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, RAPIDJSON_ALLOCATOR> writer(s);
          dowriting(writer);
          break;
        }
      }
    }
    else {
      auto dowriting = [&](auto &writer) {
        writer.SetMaxDecimalPlaces(decimals);
        encode.encodeValue(L, writer, 1);
      };

      switch (parsemode) {
        case JSON_DECODE_EXTENDED: {
          rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, RAPIDJSON_ALLOCATOR, rapidjson::WriteFlag::kWriteNanAndInfFlag> writer(s);
          dowriting(writer);
          break;
        }
        case JSON_DECODE_DEFAULT:
        default: {
          rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, RAPIDJSON_ALLOCATOR> writer(s);
          dowriting(writer);
          break;
        }
      }
    }
    lua_pushlstring(L, s.GetString(), s.GetSize());
    return 1;
  }
  catch (const rapidjson::LuaTypeException &e) {
    lua_settop(L, top);
    e.pushError(L);
  }
  catch (const std::exception &e) {
    lua_settop(L, top);
    lua_pushstring(L, e.what());
  }
  catch (...) {
    lua_settop(L, top);
    lua_pushstring(L, "Unexpected exception");
  }

  return lua_error(L);
}

LUALIB_API int rapidjson_decode (lua_State *L) {
  const int top = lua_gettop(L);

  int trailer = 0;  // First argument after the input string/length
  int nullarg = -1;  // Stack index of object that represents "null"
  int objectarg = -1;  // Stack index of "object" metatable
  int arrayarg = -1;  // Stack index of "array" metatable
  lua_Integer parsemode = 0;
  lua_Integer flags = 0;

  lua_rapidjson_getsubtable(L, LUA_REGISTRYINDEX, LUA_RAPIDJSON_REG);
  parsemode = geti(L, -1, LUA_RAPIDJSON_REG_PRESET, JSON_DECODE_DEFAULT);
  flags = geti(L, -1, LUA_RAPIDJSON_REG_FLAGS, JSON_DEFAULT);
  lua_pop(L, 1);

  size_t len = 0, position = 0;
  const char *contents = nullptr;

  /*
  ** Similar to dkjson, attempt to coerce non-string types to strings.
  ** luaL_checklstring should throw an error when not possible.
  */
  trailer = 2;
  switch (lua_type(L, 1)) {
    case LUA_TNIL: break;
    case LUA_TLIGHTUSERDATA: {
      luaL_checktype(L, 2, LUA_TNUMBER);

      trailer = 3;
      contents = reinterpret_cast<const char *>(lua_touserdata(L, 1));
      len = luaL_optsizet(L, 2, 0);  // before offset for rapidjson compat
      break;
    }
    default: {
      contents = luaL_checklstring(L, 1, &len);
      break;
    }
  }

  /* Explicitly handle empty string */
  if (len == 0) {
    lua_pushnil(L);
    lua_pushinteger(L, 0);
    lua_pushfstring(L, "%s (%d)", rapidjson::GetParseError_En(rapidjson::ParseErrorCode::kParseErrorDocumentEmpty), 0);
    return 3;
  }

  /* Common suffix arguments */
  position = luaL_optsizet(L, trailer, 1);
  if (position == 0 || position > len)
    return luaL_error(L, "invalid position");

  nullarg = (top >= (trailer + 1)) ? (trailer + 1) : nullarg;
  if (lua_isnil(L, trailer + 2)) {
    objectarg = arrayarg = (trailer + 2);
  }
  else {
    objectarg = lua_istable(L, trailer + 2) ? (trailer + 2) : objectarg;
    arrayarg = lua_istable(L, trailer + 3) ? (trailer + 3) : arrayarg;
  }

  try {
    /* Temporary fix for propagating runtime "NanAndInf" checking */
    if (parsemode == JSON_DECODE_EXTENDED)
      flags |= JSON_NAN_AND_INF;

    LuaSAX::Reader handler(L, flags, nullarg, objectarg, arrayarg);
    rapidjson::extend::StringStream s(contents + (position - 1), len - (position - 1));

    rapidjson::Reader reader;
    rapidjson::ParseResult r;
    switch (parsemode) {
      case JSON_DECODE_EXTENDED: {
        r = reader.Parse<rapidjson::ParseFlag::kParseFullPrecisionFlag
          | rapidjson::ParseFlag::kParseCommentsFlag
          | rapidjson::ParseFlag::kParseTrailingCommasFlag
          | rapidjson::ParseFlag::kParseNanAndInfFlag
          | rapidjson::ParseFlag::kParseStopWhenDoneFlag
          | rapidjson::ParseFlag::kParseEscapedApostropheFlag>(s, handler);
        break;
      }
      case JSON_DECODE_DEFAULT:
      default: {
        r = reader.Parse<rapidjson::ParseFlag::kParseDefaultFlags
          | rapidjson::ParseFlag::kParseStopWhenDoneFlag
          | rapidjson::ParseFlag::kParseTrailingCommasFlag>(s, handler);
        break;
      }
    }

    if (r.IsError()) {
      lua_settop(L, top);
#if defined(LUA_RAPIDJSON_EXPLICIT)
      lua_pushfstring(L, "%s (%d)", rapidjson::GetParseError_En(r.Code()), r.Offset());
      /* fall outside of try/catch */
#else
      lua_pushnil(L);
      lua_pushinteger(L, static_cast<lua_Integer>(r.Offset()));
      lua_pushfstring(L, "%s (%d)", rapidjson::GetParseError_En(r.Code()), r.Offset());
      return 3;
#endif
    }
    else {
      lua_pushinteger(L, 1 + static_cast<lua_Integer>(s.Tell()));
      return 2;
    }
  }
  catch (const rapidjson::LuaTypeException &e) {
    lua_settop(L, top);
    e.pushError(L);
  }
  catch (const std::exception &e) {
    lua_settop(L, top);
#if defined(LUA_RAPIDJSON_EXPLICIT)
    lua_pushstring(L, e.what());
#else
    lua_pushnil(L);
    lua_pushinteger(L, -1);
    lua_pushstring(L, e.what());
    return 3;
#endif
  }
  catch (...) {
    lua_settop(L, top);
    lua_pushstring(L, "Unexpected exception");
  }

  return lua_error(L);
}

LUALIB_API int rapidjson_setoption (lua_State *L) {
  lua_Integer v = 0;
  const lua_Integer opt = option_keys_num[luaL_checkoption(L, 1, nullptr, option_keys)];

  lua_settop(L, 2);  // create a 2nd argument if there isn't one
  lua_rapidjson_getsubtable(L, LUA_REGISTRYINDEX, LUA_RAPIDJSON_REG);
  switch (opt) {
    case JSON_PRETTY_PRINT:
    case JSON_SORT_KEYS:
    case JSON_LUA_NULL:
    case JSON_UNSIGNED_INTEGERS:
    case JSON_ARRAY_SINGLE_LINE:
    case JSON_ARRAY_EMPTY:
    case JSON_ARRAY_WITH_HOLES: {
      v = geti(L, -1, LUA_RAPIDJSON_REG_FLAGS, JSON_DEFAULT);
      luaL_checktype(L, 2, LUA_TBOOLEAN);
      seti(L, -1, LUA_RAPIDJSON_REG_FLAGS, lua_toboolean(L, 2) ? (v | opt) : (v & ~opt));
      break;
    }
    case JSON_ENCODER_MAX_DEPTH:
      if ((v = luaL_checkinteger(L, 2)) > 0)
        seti(L, -1, LUA_RAPIDJSON_REG_DEPTH, v);
      break;
    case JSON_ENCODER_INDENT:
      if ((v = luaL_checkinteger(L, 2)) >= 0 || v < 4)
        seti(L, -1, LUA_RAPIDJSON_REG_INDENT, v);
      break;
    case JSON_ENCODER_INDENT_AMT:
      if ((v = luaL_checkinteger(L, 2)) >= 0)
        seti(L, -1, LUA_RAPIDJSON_REG_INDENT_AMT, v);
      break;
    case JSON_ENCODER_DECIMALS:
      if ((v = luaL_checkinteger(L, 2)) >= 0)
        seti(L, -1, LUA_RAPIDJSON_REG_MAXDEC, v);
      break;
    case JSON_DECODER_PRESET:
      v = decode_presets_num[luaL_optcheckoption(L, 2, nullptr, decode_presets, 0)];
      seti(L, -1, LUA_RAPIDJSON_REG_PRESET, v);
      break;
    default:
      break;
  }
  lua_pop(L, 1);
  return 0;
}

LUALIB_API int rapidjson_getoption (lua_State *L) {
  lua_Integer v = 0;
  const lua_Integer opt = option_keys_num[luaL_checkoption(L, 1, nullptr, option_keys)];

  lua_rapidjson_getsubtable(L, LUA_REGISTRYINDEX, LUA_RAPIDJSON_REG);  // [..., reg]
  switch (opt) {
    case JSON_PRETTY_PRINT:
    case JSON_SORT_KEYS:
    case JSON_LUA_NULL:
    case JSON_UNSIGNED_INTEGERS:
    case JSON_ARRAY_SINGLE_LINE:
    case JSON_ARRAY_EMPTY:
    case JSON_ARRAY_WITH_HOLES:
      v = geti(L, -1, LUA_RAPIDJSON_REG_FLAGS, JSON_DEFAULT);
      lua_pushboolean(L, (v & opt) != 0);  // [..., reg, flag]
      break;
    case JSON_ENCODER_MAX_DEPTH:
      lua_pushinteger(L, geti(L, -1, LUA_RAPIDJSON_REG_DEPTH, LUA_RAPIDJSON_DEFAULT_DEPTH));  // [..., reg, depth]
      break;
    case JSON_ENCODER_INDENT:
      lua_pushinteger(L, geti(L, -1, LUA_RAPIDJSON_REG_INDENT, 0));  // [..., reg, indent]
      break;
    case JSON_ENCODER_INDENT_AMT:
      lua_pushinteger(L, geti(L, -1, LUA_RAPIDJSON_REG_INDENT_AMT, 0));  // [..., reg, amount]
      break;
    case JSON_ENCODER_DECIMALS:
      v = geti(L, -1, LUA_RAPIDJSON_REG_MAXDEC, rapidjson::Writer<rapidjson::StringBuffer>::kDefaultMaxDecimalPlaces);
      lua_pushinteger(L, v);  // [..., reg, decimals]
      break;
    case JSON_DECODER_PRESET: {
      v = geti(L, -1, LUA_RAPIDJSON_REG_PRESET, JSON_DECODE_DEFAULT);
      if (JSON_DECODE_DEFAULT <= v && v <= JSON_DECODE_EXTENDED)
        lua_pushstring(L, decode_presets[v]);  // [..., reg, preset]
      else
        lua_pushnil(L);
      break;
    }
    default: {
      lua_pop(L, 1);
      return 0;
    }
  }
  return 1;
}

LUALIB_API int rapidjson_object (lua_State *L) {
  //luaL_getmetatable(L, LUA_RAPIDJSON_REG_OBJECT);
  make_table_type(L, 1, LUA_RAPIDJSON_REG_OBJECT, LUA_RAPIDJSON_META_TYPE_OBJECT);
  return 1;
}

LUALIB_API int rapidjson_array (lua_State *L) {
  //luaL_getmetatable(L, LUA_RAPIDJSON_REG_ARRAY);
  make_table_type(L, 1, LUA_RAPIDJSON_REG_ARRAY, LUA_RAPIDJSON_META_TYPE_ARRAY);
  return 1;
}

LUALIB_API int rapidjson_isobject (lua_State *L) {
  bool is_array = 0;
  lua_pushboolean(L, has_json_type(L, 1, &is_array) && !is_array);
  return 1;
}

LUALIB_API int rapidjson_isarray (lua_State *L) {
  bool is_array = 0;
  lua_pushboolean(L, has_json_type(L, 1, &is_array) && is_array);
  return 1;
}

static int rapidjson_use_lpeg (lua_State *L) {
  return luaL_error(L, "use_lpeg has been deprecated!");
}

LUAMOD_API int luaopen_rapidjson (lua_State *L) {
  static const luaL_Reg luajson_lib[] = {
    { "decode", rapidjson_decode },
    { "encode", rapidjson_encode },
    { "setoption", rapidjson_setoption },
    { "getoption", rapidjson_getoption },
    /* special tags and functions */
    { "null", json_null },
    { "object", rapidjson_object },
    { "array", rapidjson_array },
    { "isobject", rapidjson_isobject },
    { "isarray", rapidjson_isarray },
    { "use_lpeg", rapidjson_use_lpeg },
    { "lua_round", nullptr },
    /* library details */
    { "_NAME", nullptr },
    { "_VERSION", nullptr },
    { "_COPYRIGHT", nullptr },
    { "_DESCRIPTION", nullptr },
    { nullptr, nullptr }
  };

  create_shared_meta(L, LUA_RAPIDJSON_REG_ARRAY, LUA_RAPIDJSON_META_TYPE_ARRAY);
  create_shared_meta(L, LUA_RAPIDJSON_REG_OBJECT, LUA_RAPIDJSON_META_TYPE_OBJECT);

#if LUA_VERSION_NUM == 501
  luaL_register(L, LUA_RAPIDJSON_JSON_LIBNAME, luajson_lib);
#else
  luaL_newlib(L, luajson_lib);
#endif

  lua_pushboolean(L, 0); lua_setfield(L, -2, "using_lpeg");
  lua_pushliteral(L, LUA_RAPIDJSON_NAME); lua_setfield(L, -2, "_NAME");
  lua_pushliteral(L, LUA_RAPIDJSON_VERSION); lua_setfield(L, -2, "_VERSION");
  lua_pushliteral(L, LUA_RAPIDJSON_COPYRIGHT); lua_setfield(L, -2, "_COPYRIGHT");
  lua_pushliteral(L, LUA_RAPIDJSON_DESCRIPTION); lua_setfield(L, -2, "_DESCRIPTION");
#if defined(LUA_RAPIDJSON_LUA_FLOAT)
  lua_pushboolean(L, 1); lua_setfield(L, -2, "lua_round");
#else
  lua_pushboolean(L, 0); lua_setfield(L, -2, "lua_round");
#endif

  /* Register name globally for 5.1 */
#if LUA_VERSION_NUM == 501
  lua_pushvalue(L, -1);
  lua_setglobal(L, LUA_RAPIDJSON_JSON_LIBNAME);
#endif
  return 1;
}

}
