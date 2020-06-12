#ifndef __LUA_RAPIDJSON_HPP__
#define __LUA_RAPIDJSON_HPP__

#if defined(__SSE4_2__)
  #define RAPIDJSON_SSE42
#elif defined(__SSE2__)
  #define RAPIDJSON_SSE2
#endif

#include <cstring>
#include <vector>
#include <math.h>

#include <rapidjson/encodedstream.h>
#include <rapidjson/error/en.h>
#include <rapidjson/error/error.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/reader.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

extern "C" {
  #include <lua.h>
  #include <lualib.h>
  #include <lauxlib.h>
}

#define LUA_RAPIDJSON_META_TYPE "__jsontype"
#define LUA_RAPIDJSON_TYPE_ARRAY "array"
#define LUA_RAPIDJSON_TYPE_OBJECT "object"

#define LUA_RAPIDJSON_ARRAY "json.array"
#define LUA_RAPIDJSON_OBJECT "json.object"

#define LUA_RAPIDJSON_REG "lua_rapidjson"
#define LUA_RAPIDJSON_REG_FLAGS "flags"
#define LUA_RAPIDJSON_REG_DEPTH "max_depth"
#define LUA_RAPIDJSON_REG_LEVEL "level"  /* PrettyWriter */
#define LUA_RAPIDJSON_REG_INDENT "indent"
#define LUA_RAPIDJSON_REG_MAXDEC "decimal_count" /* Writer */

#define LUA_DKJSON_CYCLE "reference cycle"
#define LUA_DKJSON_FAIL "custom encoder failed"
#define LUA_DKJSON_TYPE "unsupported type"
#define LUA_DKJSON_DEPTH_LIMIT "maximum table nesting depth exceeded" /* Replaces _CYCLE */

#define LUA_JSON_UNUSED(x) ((void)(x))

#if defined(LUA_RAPIDJSON_UNSAFE) /* Unsafe macro to disable checkstack */
  #define LUA_JSON_CHECKSTACK(L, sz) ((void)0)
#else
  #define LUA_JSON_CHECKSTACK(L, sz) luaL_checkstack((L), (sz), "too many (nested) values in encoded json")
#endif

/* @TODO: Support 16-bit Lua */
#if !defined(LUA_RAPIDJSON_BIT32) && UINTPTR_MAX == UINT_MAX
  #define LUA_RAPIDJSON_BIT32
#endif

/* maximum size visible for Lua (must be representable in a lua_Integer) */
#if !defined(MAX_SIZE)
  #if !defined (MAX_SIZET)
    #define MAX_SIZET ((size_t)(~(size_t)0))
  #endif

  #if LUA_VERSION_NUM == 503 || LUA_VERSION_NUM == 504
    #define MAX_SIZE (sizeof(size_t) < sizeof(lua_Integer) ? MAX_SIZET : (size_t)(LUA_MAXINTEGER))
  #elif LUA_VERSION_NUM == 501 || LUA_VERSION_NUM == 502
    #define MAX_SIZE MAX_SIZET
  #else
    #error unsupported Lua version
  #endif
#endif

/*
** {==================================================================
** LuaSAX
** ===================================================================
*/
/** SAX Handler: https://rapidjson.org/classrapidjson_1_1_handler.html */

#define JSON_LUA_NILL           0x01 /* If enabled use lua_pushnil, otherwise json.null */
#define JSON_PRETTY_PRINT       0x02 /* Created string will contain newlines and indentations */
#define JSON_SORT_KEYS          0x04 /* Sort keys of a table */
#define JSON_UNSIGNED_INTEGERS  0x08 /* Encode integers as signed/unsigned values */
#define JSON_EMPTY_AS_ARRAY     0x10 /* Empty table encoded as an array. */
#define JSON_ARRAY_SINGLE_LINE  0x20 /* kFormatSingleLineArray */
#define JSON_ARRAY_WITH_HOLES   0x40 /* Encode all tables with positive integer keys as arrays. */
#define JSON_ENCODER_MAX_DEPTH  0x80 /* Maximum depth of a table. */
#define JSON_ENCODER_LEVEL      0x100 /* Reserved */
#define JSON_ENCODER_INDENT     0x200 /* Reserved */
#define JSON_ENCODER_DECIMALS   0x400
#define JSON_ENCODER_NESTING    0x800 /* Push json.nill() instead of throwing a LUA_DKJSON_DEPTH_LIMIT error */

#define JSON_DEFAULT (JSON_LUA_NILL)
#define JSON_DEFAULT_DEPTH 128

#if !defined(LUA_JSON_STACK_RESERVE)
  #define LUA_JSON_STACK_RESERVE 32
#endif

#if !defined(LUA_MAXINTEGER) /* 5.1 & 5.2 */
  #define LUA_MAXINTEGER std::numeric_limits<lua_Integer>::max()
  #define LUA_MININTEGER std::numeric_limits<lua_Integer>::min()
#endif

#define JSON_REL_INDEX(idx, n) (((idx) < 0) ? ((idx) - (n)) : (idx))

#if LUA_VERSION_NUM >= 503
  #define lua_json_isinteger(L, idx) lua_isinteger((L), (idx))
#else
static int lua_json_isinteger (lua_State *L, int idx) {
  if (LUA_TNUMBER == lua_type(L, idx)) {
    lua_Number n = lua_tonumber(L, idx);
    return (!isinf(n) && (lua_Integer)(n) == (n));
  }
  return 0;
}
#endif

namespace LuaSAX {
  /* Returns rapidjson.null */
  static int json_null (lua_State *L);

  extern "C" {
    static int has_json_type (lua_State *L, int idx, int *is_array) {
      int result = 0;
#if LUA_VERSION_NUM >= 503
      if (luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_TYPE) != LUA_TNIL) {
#else
      if (luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_TYPE) != 0) {
#endif
        if ((result = (lua_type(L, -1) == LUA_TSTRING))) {
          size_t len;
          const char *s = lua_tolstring(L, -1, &len);
          *is_array = strncmp(s, LUA_RAPIDJSON_TYPE_ARRAY, sizeof(LUA_RAPIDJSON_TYPE_ARRAY)) == 0;
        }
        lua_pop(L, 1);
      }
      return result;
    }

    static bool table_is_json_array (lua_State *L, int idx, int flags, size_t *array_length) {
      int is_array = 0;
      LUA_JSON_CHECKSTACK(L, 2);

      if (has_json_type(L, idx, &is_array)) {
#if LUA_VERSION_NUM >= 502
        *array_length = lua_rawlen(L, idx);
#else
        *array_length = lua_objlen(L, idx);
#endif
        return is_array;
      }
      else {
        lua_Integer n;
        size_t count = 0, max = 0;
        int stacktop = lua_gettop(L), i_idx = JSON_REL_INDEX(idx, 1);

        lua_pushnil(L);
        while (lua_next(L, i_idx)) { /* [key, value] */
          lua_pop(L, 1); /* [key] */
          if (lua_json_isinteger(L, -1) /* && within range of size_t */
              && ((n = lua_tointeger(L, -1)) >= 1 && ((size_t)n) <= MAX_SIZE)) {
            count++;
            max = ((size_t)n) > max ? ((size_t)n) : max;
          }
          else {
            lua_settop(L, stacktop);
            return 0;
          }
        }
        *array_length = max;
        lua_settop(L, stacktop);

        if ((flags & JSON_ARRAY_WITH_HOLES) == JSON_ARRAY_WITH_HOLES)
          return 1; /* All integer keys, insert nils. */
        return max == count;
      }
    }
  }

  static bool is_json_null (lua_State *L, int idx) {
    lua_pushvalue(L, idx); /* [value] */

    json_null(L); /* [value, json.null] */
    bool is = lua_rawequal(L, -1, -2) != 0;
    lua_pop(L, 2);
    return is;
  }

  struct Key {
    const char *key;
    size_t len;

    Key() : key(NULL), len(0) { }
    Key(const char *k, size_t l) : key(k), len(l) { }

    bool operator<(const Key &rhs) const {
      return strcmp(key, rhs.key) < 0;
    }
  };

  /** SAX Handler: https://rapidjson.org/classrapidjson_1_1_handler.html */
  struct Reader {
  private:
    struct Ctx {
      Ctx() : index_(0), fn_(&topFn) { }
      Ctx(const Ctx &rhs) : index_(rhs.index_), fn_(rhs.fn_) { }

      const Ctx &operator=(const Ctx &rhs) {
        if (this != &rhs) {
          index_ = rhs.index_;
          fn_ = rhs.fn_;
        }
        return *this;
      }

      static Ctx Object() {
        return Ctx(&objectFn);
      }

      static Ctx Array() {
        return Ctx(&arrayFn);
      }

      void submit(lua_State *L) {
        fn_(L, this);
      }

      rapidjson::SizeType index_;
      void (*fn_)(lua_State *L, Ctx *ctx);

    private:
      explicit Ctx(void (*f)(lua_State *L, Ctx *ctx)) : index_(0), fn_(f) { }

      static void objectFn(lua_State *L, Ctx *ctx) {
        LUA_JSON_UNUSED(ctx);
        lua_rawset(L, -3);
      }

      static void arrayFn(lua_State *L, Ctx *ctx) {
        LUA_JSON_UNUSED(ctx);
        lua_rawseti(L, -2, ++ctx->index_);
      }

      static void topFn(lua_State *L, Ctx *ctx) {
        LUA_JSON_UNUSED(L);
        LUA_JSON_UNUSED(ctx);
      }
    };

    lua_State *L;
    lua_Integer flags;
    std::vector<Ctx> stack_;
    Ctx context_;

  public:
    explicit Reader(lua_State *_L, lua_Integer _f = 0) : L(_L), flags(_f) {
      stack_.reserve(LUA_JSON_STACK_RESERVE);
    }

    bool Null() {
      if ((flags & JSON_LUA_NILL))
        lua_pushnil(L);
      else
        json_null(L);
      context_.submit(L);
      return true;
    }

    bool Bool(bool b) {
      lua_pushboolean(L, b);
      context_.submit(L);
      return true;
    }

    bool Int(int i) {
      lua_pushinteger(L, i);
      context_.submit(L);
      return true;
    }

    bool Uint(unsigned u) {
      if (sizeof(lua_Integer) > sizeof(unsigned int) || u <= static_cast<unsigned>(LUA_MAXINTEGER))
        lua_pushinteger(L, static_cast<lua_Integer>(u));
      else
        lua_pushnumber(L, static_cast<lua_Number>(u));
      context_.submit(L);
      return true;
    }

    bool Int64(int64_t i) {
      if (sizeof(lua_Integer) >= sizeof(int64_t) || (i <= LUA_MAXINTEGER && i >= LUA_MININTEGER))
        lua_pushinteger(L, static_cast<lua_Integer>(i));
      else
        lua_pushnumber(L, static_cast<lua_Number>(i));
      context_.submit(L);
      return true;
    }

    bool Uint64(uint64_t u) {
      if (sizeof(lua_Integer) > sizeof(uint64_t) || u <= static_cast<uint64_t>(LUA_MAXINTEGER))
        lua_pushinteger(L, static_cast<lua_Integer>(u));
      else
        lua_pushnumber(L, static_cast<lua_Number>(u));
      context_.submit(L);
      return true;
    }

    bool Double(double d) {
      lua_pushnumber(L, static_cast<lua_Number>(d));
      context_.submit(L);
      return true;
    }

    bool RawNumber(const char *str, rapidjson::SizeType length, bool copy) {
      LUA_JSON_UNUSED(copy);

      lua_getglobal(L, "tonumber");
      lua_pushlstring(L, str, length);
      lua_call(L, 1, 1);
      context_.submit(L);
      return true;
    }

    bool String(const char *str, rapidjson::SizeType length, bool copy) {
      LUA_JSON_UNUSED(copy);

      lua_pushlstring(L, str, length);
      context_.submit(L);
      return true;
    }

    bool StartObject() {
#if !defined(LUA_RAPIDJSON_UNSAFE)
      if (lua_checkstack(L, 2)) { /* ensure room on the stack */
#endif
        lua_createtable(L, 0, 0); /* mark as object */
        luaL_getmetatable(L, LUA_RAPIDJSON_OBJECT);
        lua_setmetatable(L, -2);

        stack_.push_back(context_);
        context_ = Ctx::Object();
        return true;
#if !defined(LUA_RAPIDJSON_UNSAFE)
      }
      return false;
#endif
    }

    bool Key(const char *str, rapidjson::SizeType length, bool copy) const {
      LUA_JSON_UNUSED(copy);
      lua_pushlstring(L, str, length);
      return true;
    }

    bool EndObject(rapidjson::SizeType memberCount) {
      LUA_JSON_UNUSED(memberCount);

      context_ = stack_.back();
      stack_.pop_back();
      context_.submit(L);
      return true;
    }

    bool StartArray() {
#if !defined(LUA_RAPIDJSON_UNSAFE)
      if (lua_checkstack(L, 2)) { /* ensure room on the stack */
#endif
        lua_createtable(L, 0, 0); /* mark as array */
        luaL_getmetatable(L, LUA_RAPIDJSON_ARRAY);
        lua_setmetatable(L, -2);

        stack_.push_back(context_);
        context_ = Ctx::Array();
        return true;
#if !defined(LUA_RAPIDJSON_UNSAFE)
      }
      return false;
#endif
    }

    bool EndArray(rapidjson::SizeType elementCount) {
      lua_assert(elementCount == context_.index_);
      LUA_JSON_UNUSED(elementCount);

      context_ = stack_.back();
      stack_.pop_back();
      context_.submit(L);
      return true;
    }
  };

  class Writer {
  private:
    int flags; /* Configuration flags */
    int max_depth; /* Maximum recursive depth */

  public:
    Writer() : flags(JSON_DEFAULT), max_depth(JSON_DEFAULT_DEPTH) { }
    Writer(int _flags, int _maxdepth) : flags(_flags), max_depth(_maxdepth) { }

    int GetFlags() { return flags; }
    int GetMaxDepth() { return max_depth; }

    Writer &SetFlags(int f) { flags = f; return *this; }
    Writer &SetMaxDepth(int d) { max_depth = d; return *this; }

    template<typename Writer>
    void encodeValue(lua_State *L, Writer *writer, int idx, int depth = 0) {
      switch (lua_type(L, idx)) {
        case LUA_TNIL:
          writer->Null();
          break;
        case LUA_TBOOLEAN:
          writer->Bool(lua_toboolean(L, idx) != 0);
          break;
        case LUA_TNUMBER: {
          if (lua_json_isinteger(L, idx)) {
#if defined(LUA_RAPIDJSON_BIT32)
            if (flags & JSON_UNSIGNED_INTEGERS)
              writer->Uint((unsigned)lua_tointeger(L, idx));
            else
              writer->Int((int)lua_tointeger(L, idx));
#else
            if (flags & JSON_UNSIGNED_INTEGERS)
              writer->Uint64((uint64_t)lua_tointeger(L, idx));
            else
              writer->Int64((int64_t)lua_tointeger(L, idx));
#endif
          }
          else if (!writer->Double((double)lua_tonumber(L, idx))) {
            luaL_error(L, "error while encoding '%s'", lua_typename(L, LUA_TNUMBER));
            return;
          }
          break;
        }
        case LUA_TSTRING: {
          size_t len;
          const char *s = lua_tolstring(L, idx, &len);
          writer->String(s, static_cast<rapidjson::SizeType>(len));
          return;
        }
        case LUA_TTABLE:
          return encodeTable(L, writer, idx, depth + 1);
        case LUA_TFUNCTION:
          if (LuaSAX::is_json_null(L, idx)) {
            writer->Null();
            break;
          }
          /* FALLTHROUGH */
        case LUA_TLIGHTUSERDATA:
        case LUA_TUSERDATA:
        case LUA_TTHREAD:
        case LUA_TNONE:
        default: {
          luaL_error(L, LUA_DKJSON_TYPE " type '%s' is not supported by JSON.",
                                             lua_typename(L, lua_type(L, idx)));
          break;
        }
      }
    }

    template<typename Writer>
    void encodeTable(lua_State *L, Writer *writer, int idx, int depth) {
      size_t array_length;
      if (depth > max_depth) {
        if (flags & JSON_ENCODER_NESTING)
          writer->Null();
        else
          luaL_error(L, LUA_DKJSON_DEPTH_LIMIT);
        return;
      }

      if (table_is_json_array(L, idx, flags, &array_length)
                       && (array_length > 0 || (flags & JSON_EMPTY_AS_ARRAY))) {
        encode_array(L, writer, idx, array_length, depth);
      }
      else if ((flags & JSON_SORT_KEYS) == 0) /* Treat table as object */
        encodeObject(L, writer, idx, depth);
      else {
        int i_idx = JSON_REL_INDEX(idx, 1);  /* Account for key */
        std::vector<Key> keys;  /* Collect all table keys  */

        lua_pushnil(L);
        while (lua_next(L, i_idx)) { /* [key][value] */
          if (lua_type(L, -2) == LUA_TSTRING) {
            Key k;
            k.key = lua_tolstring(L, -2, &k.len);
            keys.push_back(k);
          }
          lua_pop(L, 1); /* [key] */
        }
        std::sort(keys.begin(), keys.end());
        encodeObject(L, writer, idx, depth, keys);
      }
    }

    template<typename Writer>
	  void encode_array(lua_State* L, Writer* writer, int idx, size_t array_length, int depth) {
      writer->StartArray();
      for (size_t i = 1; i <= array_length; ++i) {
        lua_rawgeti(L, idx, (lua_Integer)i);
        encodeValue(L, writer, -1, depth);
        lua_pop(L, 1);
      }
      writer->EndArray();
    }

    template<typename Writer>
    void encodeObject(lua_State* L, Writer* writer, int idx, int depth) {
      int i_idx = JSON_REL_INDEX(idx, 1);
      LUA_JSON_CHECKSTACK(L, 2);

      writer->StartObject();
      lua_pushnil(L); /* [table, nil] */
      while (lua_next(L, i_idx)) { /* [table, key, value] */
        if (lua_type(L, -2) == LUA_TSTRING) {
          size_t len = 0;
          const char *k = lua_tolstring(L, -2, &len);
          writer->Key(k, static_cast<rapidjson::SizeType>(len));
          encodeValue(L, writer, -1, depth);
        }
        lua_pop(L, 1); /* pop value: [table, key] */
      }
      writer->EndObject();
    }

    template<typename Writer>
    void encodeObject(lua_State* L, Writer* writer, int idx, int depth, std::vector<Key> &keys) {
      int i_idx = JSON_REL_INDEX(idx, 1);
      LUA_JSON_CHECKSTACK(L, 2);

      writer->StartObject();
      std::vector<Key>::const_iterator e = keys.end();
      for (std::vector<Key>::const_iterator i = keys.begin(); i != e; ++i) {
        writer->Key(i->key, static_cast<rapidjson::SizeType>(i->len));
        lua_pushlstring(L, i->key, i->len); /* sorted key */
        lua_gettable(L, i_idx);
        encodeValue(L, writer, -1, depth);
        lua_pop(L, 1);
      }
      writer->EndObject();
    }
  };
}

/* }================================================================== */

#endif
