#include <vector>
#include <algorithm>

#include "lua_rapidjson.hpp"
#include "StringStream.hpp"

extern "C" {
  #include "lua_rapidjsonlib.h"
}

/* PrettyWriter indentation characters */
static const char pretty_indent[] = { ' ', '\t', '\n', '\r' };

/*
** Registry (sub-)table
*/

#if LUA_VERSION_NUM < 502
  #define lua_absindex(L, i) ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : lua_gettop(L) + (i) + 1)
static int luaL_getsubtable(lua_State *L, int idx, const char *fname) {
  lua_getfield(L, idx, fname);
  if (lua_istable(L, -1))
    return 1; /* table already there */
  else {
    lua_pop(L, 1); /* remove previous result */
    idx = lua_absindex(L, idx);
    lua_newtable(L);
    lua_pushvalue(L, -1); /* copy to be left at top */
    lua_setfield(L, idx, fname); /* assign new table to field */
    return 0; /* false, because did not find table there */
  }
}
#endif

static lua_Integer geti (lua_State *L, int idx, const char *key, lua_Integer opt) {
  lua_Integer result;

  lua_getfield(L, idx, key);
  result = luaL_optinteger(L, -1, opt);
  lua_pop(L, 1); /* key */
  return result;
}

/* Fetch a lua_Int from the registry table */
static lua_Integer getregi (lua_State *L, const char *key, lua_Integer opt) {
  lua_Integer result;

  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_RAPIDJSON_REG);
  result = geti(L, -1, key, opt);
  lua_pop(L, 1); /* registry */
  return result;
}

/* Push a integer into the registry table at the specified key */
static void setregi (lua_State *L, const char *key, lua_Integer value) {
  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_RAPIDJSON_REG);
  lua_pushinteger(L, value);
  lua_setfield(L, -2, key); /* pops value */
  lua_pop(L, 1); /* getregtable */
}

/*
** If the stack argument is a convertible to a size_t from an lua_Integer,
** returns the size_t. If the argument is absent or is nil, returns def.
** Otherwise, throw an error.
*/
static inline size_t luaL_optsizet (lua_State *L, int arg, size_t def) {
  lua_Integer i;
  if (lua_isnoneornil(L, arg))
    return def;
  else if (!lua_json_isinteger(L, arg)) { /* 5.1/5.2: Number not an integer */
    luaL_argerror(L, arg, lua_pushfstring(L, "integer expected"));
    return 0;
  }
  else if ((i = lua_tointeger(L, arg)) < 0) {
    luaL_argerror(L, arg, "negative integer argument");
    return 0;
  }
  else if (((size_t)i) > MAX_SIZE) {
    luaL_argerror(L, arg, "invalid integer argument");
    return 0;
  }
  return (size_t)i;
}

/* A luaL_checkoption that doesn't throw an error. */
static int luaL_optcheckoption (lua_State *L, int arg, const char *def,
                                            const char *const lst[], int ldef) {
  const char *name = (def) ? luaL_optstring(L, arg, def) :
                             luaL_checkstring(L, arg);
  int i;
  for (i = 0; lst[i]; i++) {
    if (strcmp(lst[i], name) == 0)
      return i;
  }
  return ldef;
}

/*
** {==================================================================
** CoreAPI
** ===================================================================
*/

static const char *const opts[] = {
  "",
  "null",
  "indent", "pretty",
  "sort_keys",
  "single_line",
  "unsigned",
  "empty_table_as_array",
  "with_hole",
  LUA_RAPIDJSON_REG_DEPTH,
  LUA_RAPIDJSON_REG_LEVEL,
  LUA_RAPIDJSON_REG_INDENT,
  LUA_RAPIDJSON_REG_MAXDEC,
  "nesting",
  "keyorder",
  "decoder_preset",
  NULL
};

static const int optsnum[] = {
  0x0, /* RESERVED */
  JSON_LUA_NILL,
  JSON_PRETTY_PRINT, JSON_PRETTY_PRINT,
  JSON_SORT_KEYS,
  JSON_ARRAY_SINGLE_LINE,
  JSON_UNSIGNED_INTEGERS,
  JSON_EMPTY_AS_ARRAY,
  JSON_ARRAY_WITH_HOLES,
  JSON_ENCODER_MAX_DEPTH,
  JSON_ENCODER_LEVEL,
  JSON_ENCODER_INDENT,
  JSON_ENCODER_DECIMALS,
  JSON_ENCODER_NESTING,
  JSON_TABLE_KEY_ORDER,
  JSON_DECODER_PRESET,
};

/* */
static const char *const d_opts[] = {
  "default", "extended", NULL
};

static const int d_optsnums[] = {
  JSON_DECODE_DEFAULT,
  JSON_DECODE_EXTENDED,
};

namespace LuaSAX {
  static int nullref = LUA_NOREF;

  static int json_null(lua_State *L) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, nullref);
    return 1;
  }
}

static void create_shared_meta (lua_State *L, const char *meta, const char *type) {
  luaL_newmetatable(L, meta);
  lua_pushstring(L, type);
  lua_setfield(L, -2, LUA_RAPIDJSON_META_TYPE);
  lua_pop(L, 1);
}

static int make_table_type (lua_State *L, int idx, const char *meta, const char *type) {
  if (lua_isnoneornil(L, idx))
    lua_createtable(L, 0, 0);
  else if (lua_istable(L, idx)) {
    lua_pushvalue(L, idx); /* [table] */
    if (lua_getmetatable(L, -1)) { /* already has metatable, set __jsontype field */
      lua_pushstring(L, type); /* [table, meta, type] */
      lua_setfield(L, -2, LUA_RAPIDJSON_META_TYPE);
      lua_pop(L, 1); /* [table] */
      return 1;
    }
  }
  else {
    return luaL_argerror(L, idx, "optional table excepted");
  }

  luaL_getmetatable(L, meta); /* [table, meta] */
  lua_setmetatable(L, -2); /* [table] */
  return 1;
}

LUALIB_API int rapidjson_encode (lua_State *L) {
  LUA_JSON_CHECKSTACK(L, 4);

  int level = 0, indent = 0, state_idx = -1;
  int flags = JSON_DEFAULT, depth = JSON_DEFAULT_DEPTH;
  int parsemode = JSON_DECODE_DEFAULT;
  int decimals = rapidjson::Writer<rapidjson::StringBuffer>::kDefaultMaxDecimalPlaces;
  std::vector<LuaSAX::Key> order;

  /* Parse default options */
  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_RAPIDJSON_REG);
  flags = (int)geti(L, -1, LUA_RAPIDJSON_REG_FLAGS, flags);
  depth = (int)geti(L, -1, LUA_RAPIDJSON_REG_DEPTH, depth);
  indent = (int)geti(L, -1, LUA_RAPIDJSON_REG_INDENT, indent);
  level = (int)geti(L, -1, LUA_RAPIDJSON_REG_LEVEL, (indent == 0) ? 4 : 0);
  decimals = (int)geti(L, -1, LUA_RAPIDJSON_REG_MAXDEC, decimals);
  parsemode = (int)getregi(L, LUA_RAPIDJSON_REG_PRESET, parsemode);
  lua_pop(L, 1);

  if (lua_istable(L, 2)) { /* Parse all options from the argument table */
    LUA_JSON_CHECKSTACK(L, 3);
    state_idx = 2;

    lua_pushnil(L);
    while (lua_next(L, 2)) { /* [key, value] */
      int opt;
      switch ((opt = optsnum[luaL_optcheckoption(L, -2, NULL, opts, 0x0)])) {
        case JSON_LUA_NILL:
        case JSON_PRETTY_PRINT:
        case JSON_SORT_KEYS:
        case JSON_UNSIGNED_INTEGERS:
        case JSON_EMPTY_AS_ARRAY:
        case JSON_ARRAY_WITH_HOLES:
        case JSON_ARRAY_SINGLE_LINE:
          flags = lua_toboolean(L, -1) ? (flags | opt) : (flags & ~opt);
          break;
        case JSON_ENCODER_MAX_DEPTH:
          if ((depth = (int)lua_tointeger(L, -1)) <= 0)
            return luaL_error(L, "invalid encoder depth");
          break;
        case JSON_ENCODER_LEVEL:
          if ((level = (int)lua_tointeger(L, -1)) < 0)
            return luaL_error(L, "invalid indentation level");
          break;
        case JSON_ENCODER_DECIMALS:
          if ((decimals = (int)lua_tointeger(L, -1)) < 0)
            return luaL_error(L, "invalid decimal count");
          break;
        case JSON_ENCODER_INDENT:
          indent = (int)lua_tointeger(L, -1);
          if (indent < 0 || indent >= 4)
            return luaL_error(L, "invalid indentation index");
          break;
        case JSON_TABLE_KEY_ORDER: {
          if (lua_istable(L, -1)) {
            if (LuaSAX::populate_key_vector(L, -1, order) != 0)
              return luaL_error(L, "invalid key_order element");
          }
        }
        default:
          break;
      }
      lua_pop(L, 1); /* [key] */
    }
  }
  else if (!lua_isnoneornil(L, 2)) {
    return luaL_argerror(L, 2, "optional table excepted");
  }

  LuaSAX::Writer encode(flags, depth, state_idx, order);
  rapidjson::StringBuffer s;
  try {
    if (flags & JSON_PRETTY_PRINT) {
      rapidjson::PrettyFormatOptions option = rapidjson::PrettyFormatOptions::kFormatDefault;
      if (indent < 0 || indent >= 4 || depth < 0)
        return luaL_error(L, "invalid encoder parameters");
      if (flags & JSON_ARRAY_SINGLE_LINE)
        option = rapidjson::PrettyFormatOptions::kFormatSingleLineArray;

      auto dowriting = [&](auto &writer) {
        writer.SetMaxDecimalPlaces(decimals);
        writer.SetIndent(pretty_indent[indent], (unsigned int)level);
        writer.SetFormatOptions(option);
        encode.encodeValue(L, &writer, 1);
      };

      switch (parsemode) {
        case JSON_DECODE_EXTENDED: {
          rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>,
            rapidjson::CrtAllocator, rapidjson::WriteFlag::kWriteNanAndInfFlag> writer(s);
          dowriting(writer);
          break;
        }
        case JSON_DECODE_DEFAULT:
        default: {
          rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(s);
          dowriting(writer);
          break;
        }
      }
    }
    else {
      auto dowriting = [&](auto &writer) {
        writer.SetMaxDecimalPlaces(decimals);
        encode.encodeValue(L, &writer, 1);
      };

      switch (parsemode) {
        case JSON_DECODE_EXTENDED: {
          rapidjson::Writer<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>,
            rapidjson::CrtAllocator, rapidjson::WriteFlag::kWriteNanAndInfFlag> writer(s);
          dowriting(writer);
          break;
        }
        case JSON_DECODE_DEFAULT:
        default: {
          rapidjson::Writer<rapidjson::StringBuffer> writer(s);
          dowriting(writer);
          break;
        }
      }
    }
  }
  catch (...) {
    return luaL_error(L, LUA_DKJSON_FAIL);
  }

  lua_pushlstring(L, s.GetString(), s.GetSize());
  return 1;
}

LUALIB_API int rapidjson_decode (lua_State *L) {
  int trailer = 0;
  int nullarg = -1;
  int objectarg = -1;
  int arrayarg = -1;
  int top = lua_gettop(L);
  int parsemode = (int)getregi(L, LUA_RAPIDJSON_REG_PRESET, JSON_DECODE_DEFAULT);
  lua_Integer flags = getregi(L, LUA_RAPIDJSON_REG_FLAGS, JSON_DEFAULT);

  size_t len = 0, position = 0;
  const char *contents = nullptr;
  switch (lua_type(L, 1)) {
    case LUA_TSTRING:
      trailer = 2;
      contents = luaL_checklstring(L, 1, &len);
      break;
    case LUA_TLIGHTUSERDATA: {
      luaL_checktype(L, 2, LUA_TNUMBER);

      trailer = 3;
      contents = reinterpret_cast<const char *>(lua_touserdata(L, 1));
      len = luaL_optsizet(L, 2, 0); /* Before offset for rapidjson compat */
      break;
    }
    default:
      return luaL_argerror(L, 1, "required string or lightuserdata (points to a memory of a string)");
  }

  if (len == 0) {  /* Explicitly handle empty string ... */
    lua_pushnil(L);
    lua_pushinteger(L, 0);
    lua_pushfstring(L, "%s (%d)", rapidjson::GetParseError_En(
                       rapidjson::ParseErrorCode::kParseErrorDocumentEmpty), 0);
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

  LuaSAX::Reader handler(L, flags, nullarg, objectarg, arrayarg);
  rapidjson::Reader reader;
  rapidjson::extend::StringStream s(contents + (position - 1), len - (position - 1));
  try {
    rapidjson::ParseResult r;
    switch (parsemode) {
      case JSON_DECODE_EXTENDED:
        r = reader.Parse<rapidjson::ParseFlag::kParseFullPrecisionFlag
          | rapidjson::ParseFlag::kParseCommentsFlag
          | rapidjson::ParseFlag::kParseTrailingCommasFlag
          | rapidjson::ParseFlag::kParseNanAndInfFlag
          | rapidjson::ParseFlag::kParseEscapedApostropheFlag>(s, handler);
        break;
      case JSON_DECODE_DEFAULT:
      default:
        r = reader.Parse<rapidjson::ParseFlag::kParseDefaultFlags
          | rapidjson::ParseFlag::kParseTrailingCommasFlag>(s, handler);
        break;
    }

    if (r.IsError()) {
#if defined(LUA_RAPIDJSON_EXPLICIT)
      return luaL_error(L, "error while decoding: %s (%d)",
                             rapidjson::GetParseError_En(r.Code()), r.Offset());
#else
      lua_settop(L, top);
      lua_pushnil(L);
      lua_pushinteger(L, (lua_Integer)r.Offset());
      lua_pushfstring(L, "%s (%d)", rapidjson::GetParseError_En(r.Code()), r.Offset());
#endif
      return 3;
    }
  }
  catch (...) {
#if defined(LUA_RAPIDJSON_EXPLICIT)
    return luaL_error(L, LUA_DKJSON_FAIL);
#else
    lua_settop(L, top);
    lua_pushnil(L);
    lua_pushinteger(L, -1);
    lua_pushstring(L, LUA_DKJSON_FAIL);
    return 3;
#endif
  }

  return 1;
}

LUALIB_API int rapidjson_setoption (lua_State *L) {
  int opt;
  lua_Integer v;
  switch ((opt = optsnum[luaL_checkoption(L, 1, NULL, opts)])) {
    case JSON_LUA_NILL:
    case JSON_PRETTY_PRINT:
    case JSON_SORT_KEYS:
    case JSON_UNSIGNED_INTEGERS:
    case JSON_EMPTY_AS_ARRAY:
    case JSON_ARRAY_SINGLE_LINE:
    case JSON_ARRAY_WITH_HOLES:
      v = getregi(L, LUA_RAPIDJSON_REG_FLAGS, JSON_DEFAULT);
      luaL_checktype(L, 2, LUA_TBOOLEAN);
      setregi(L, LUA_RAPIDJSON_REG_FLAGS, lua_toboolean(L, 2) ? (v | opt) : (v & ~opt));
      break;
    case JSON_ENCODER_MAX_DEPTH:
      if ((v = luaL_checkinteger(L, 2)) > 0)
        setregi(L, LUA_RAPIDJSON_REG_DEPTH, v);
      break;
    case JSON_ENCODER_LEVEL:
      if ((v = luaL_checkinteger(L, 2)) >= 0)
        setregi(L, LUA_RAPIDJSON_REG_LEVEL, v);
      break;
    case JSON_ENCODER_INDENT:
      if ((v = luaL_checkinteger(L, 2)) >= 0 || v < 4)
        setregi(L, LUA_RAPIDJSON_REG_INDENT, v);
      break;
    case JSON_ENCODER_DECIMALS:
      if ((v = luaL_checkinteger(L, 2)) >= 0)
        setregi(L, LUA_RAPIDJSON_REG_MAXDEC, v);
      break;
    case JSON_DECODER_PRESET:
      v = luaL_optcheckoption(L, 2, NULL, d_opts, JSON_DECODE_DEFAULT);
      setregi(L, LUA_RAPIDJSON_REG_PRESET, d_optsnums[v]);
      break;
    default:
      break;
  }
  return 0;
}

LUALIB_API int rapidjson_getoption (lua_State *L) {
  int opt;
  lua_Integer flags = 0;
  switch ((opt = optsnum[luaL_checkoption(L, 1, NULL, opts)])) {
    case JSON_LUA_NILL:
    case JSON_PRETTY_PRINT:
    case JSON_SORT_KEYS:
    case JSON_UNSIGNED_INTEGERS:
    case JSON_EMPTY_AS_ARRAY:
    case JSON_ARRAY_SINGLE_LINE:
    case JSON_ARRAY_WITH_HOLES:
      flags = getregi(L, LUA_RAPIDJSON_REG_FLAGS, JSON_DEFAULT);
      lua_pushboolean(L, (flags & opt) != 0);
      break;
    case JSON_ENCODER_MAX_DEPTH:
      lua_pushinteger(L, getregi(L, LUA_RAPIDJSON_REG_DEPTH, JSON_DEFAULT_DEPTH));
      break;
    case JSON_ENCODER_LEVEL:
      lua_pushinteger(L, getregi(L, LUA_RAPIDJSON_REG_LEVEL, 0));
      break;
    case JSON_ENCODER_INDENT:
      lua_pushinteger(L, getregi(L, LUA_RAPIDJSON_REG_INDENT, 0));
      break;
    case JSON_ENCODER_DECIMALS:
      flags = getregi(L, LUA_RAPIDJSON_REG_MAXDEC,
          rapidjson::Writer<rapidjson::StringBuffer>::kDefaultMaxDecimalPlaces);
      lua_pushinteger(L, flags);
      break;
    case JSON_DECODER_PRESET:
      flags = getregi(L, LUA_RAPIDJSON_REG_PRESET, JSON_DECODE_DEFAULT);
      if (JSON_DECODE_DEFAULT <= flags && flags <= JSON_DECODE_EXTENDED)
        lua_pushstring(L, d_opts[flags]);
      else
        lua_pushnil(L);
      break;
    default:
      return 0;
  }
  return 1;
}

LUALIB_API int rapidjson_object (lua_State *L) {
  //luaL_getmetatable(L, LUA_RAPIDJSON_OBJECT);
  make_table_type(L, 1, LUA_RAPIDJSON_OBJECT, LUA_RAPIDJSON_TYPE_OBJECT);
  return 1;
}

LUALIB_API int rapidjson_array (lua_State *L) {
  //luaL_getmetatable(L, LUA_RAPIDJSON_ARRAY);
  make_table_type(L, 1, LUA_RAPIDJSON_ARRAY, LUA_RAPIDJSON_TYPE_ARRAY);
  return 1;
}

LUALIB_API int rapidjson_isobject (lua_State *L) {
  int isarray = 0;
  lua_pushboolean(L, LuaSAX::has_json_type(L, 1, &isarray) && isarray == 0);
  return 1;
}

LUALIB_API int rapidjson_isarray (lua_State *L) {
  int isarray = 0;
  lua_pushboolean(L, LuaSAX::has_json_type(L, 1, &isarray) && isarray != 0);
  return 1;
}

static int rapidjson_use_lpeg (lua_State *L) {
  return luaL_error(L, "use_lpeg has been deprecated!");
}

LUAMOD_API int luaopen_rapidjson (lua_State *L) {
  static const luaL_Reg luajson_lib[] = {
    { "decode", rapidjson_decode }, /* */
    { "encode", rapidjson_encode }, /* Create a string representing the object */
    { "setoption", rapidjson_setoption },
    { "getoption", rapidjson_getoption },
    /* special tags and functions */
    { "object", rapidjson_object },
    { "array", rapidjson_array },
    { "isobject", rapidjson_isobject },
    { "isarray", rapidjson_isarray },
    { "use_lpeg", rapidjson_use_lpeg },
    { "null", LuaSAX::json_null },
    { NULL, NULL }
  };

  create_shared_meta(L, LUA_RAPIDJSON_ARRAY, LUA_RAPIDJSON_TYPE_ARRAY);
  create_shared_meta(L, LUA_RAPIDJSON_OBJECT, LUA_RAPIDJSON_TYPE_OBJECT);

#if LUA_VERSION_NUM == 501
  luaL_register(L, "LUACMSGPACK_NAME", luajson_lib);
#else
  luaL_newlib(L, luajson_lib);
#endif

  lua_pushboolean(L, 0); lua_setfield(L, -2, "using_lpeg");
  lua_pushliteral(L, LUA_RAPIDJSON_NAME); lua_setfield(L, -2, "_NAME");
  lua_pushliteral(L, LUA_RAPIDJSON_VERSION); lua_setfield(L, -2, "_VERSION");
  lua_pushliteral(L, LUA_RAPIDJSON_COPYRIGHT); lua_setfield(L, -2, "_COPYRIGHT");
  lua_pushliteral(L, LUA_RAPIDJSON_DESCRIPTION); lua_setfield(L, -2, "_DESCRIPTION");

  /* Create json.null reference */
  lua_getfield(L, -1, "null");
  LuaSAX::nullref = luaL_ref(L, LUA_REGISTRYINDEX);

  /* Register name globally for 5.1 */
#if LUA_VERSION_NUM == 501
  lua_pushvalue(L, -1);
  lua_setglobal(L, "rapidjson");
#endif
  return 1;
}
