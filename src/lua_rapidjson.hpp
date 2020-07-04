#ifndef __LUA_RAPIDJSON_HPP__
#define __LUA_RAPIDJSON_HPP__

#if defined(__SSE4_2__)
  #define RAPIDJSON_SSE42
#elif defined(__SSE2__)
  #define RAPIDJSON_SSE2
#endif

#include <functional>
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
#if LUA_VERSION_NUM == 504  /* gritLua 5.4 */
  #include <lgrit.h>
#elif LUA_VERSION_NUM == 503  /* cfxLua 5.3 */
  #include <llimits.h>
  #include <lobject.h>
#else
   #error unsupported Lua version
#endif
}

#define LUA_RAPIDJSON_META_TOJSON "__tojson"
#define LUA_RAPIDJSON_META_ORDER "__jsonorder"
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
#define LUA_RAPIDJSON_REG_PRESET "decoder_preset"

#define LUA_DKJSON_CYCLE "reference cycle"
#define LUA_DKJSON_FAIL "custom encoder failed"
#define LUA_DKJSON_TYPE "unsupported type"
#define LUA_DKJSON_NUMBER "error encoding number"
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
#define JSON_TABLE_KEY_ORDER    0x1000 /* Reserved */
#define JSON_DECODER_PRESET     0x2000 /* Preset flags for decoding */
#define JSON_ENCODER_ARRAY_VECTOR 0x4000 /* gritVectors encoded as arrays, otherwise x=x, y=y, objects*/

#define JSON_DEFAULT (JSON_LUA_NILL | JSON_EMPTY_AS_ARRAY)
#define JSON_DEFAULT_DEPTH 128

/* kParseDefaultFlags */
#define JSON_DECODE_DEFAULT 0x0
/*
** kParseFullPrecisionFlag + kParseCommentsFlag + kParseTrailingCommasFlag
** kParseNanAndInfFlag + kParseEscapedApostropheFlag
*/
#define JSON_DECODE_EXTENDED 0x1

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
#if defined(GRIT_POWER_TTYPE)
        switch (lua_tabletype(L, idx)) {
          case LUA_TTEMPTY: *array_length = 0; return 1;
          case LUA_TTARRAY:
            if (!(flags & JSON_ARRAY_WITH_HOLES)) {
              *array_length = lua_rawlen(L, idx);
              return 1;
            }
            break;
          default: break;
        }
#endif

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

    /* Handle gritLua vectors */
    int parseVector (lua_State *L, int idx, lua_Float4 *f) {
      int args = 0;
#if LUA_VERSION_NUM == 504  /* gritLua 5.4 */
      switch (lua_tovector(L, idx, V_PARSETABLE, f)) {
        case LUA_VNUMFLT: args = 1; break;
        case LUA_VVECTOR2: args = 2; break;
        case LUA_VVECTOR3: args = 3; break;
        case LUA_VQUAT: case LUA_VVECTOR4: args = 4; break;
        default:
          luaL_typeerror(L, idx, "number or vector type");
          break;
      }
#elif LUA_VERSION_NUM == 503  /* CfxLua 5.3 */
      switch (lua_type(L, idx)) {
        case LUA_TVECTOR2: args = 2; lua_checkvector2(L, idx, &f->x, &f->y); break;
        case LUA_TVECTOR3: args = 3; lua_checkvector3(L, idx, &f->x, &f->y, &f->z); break;
        case LUA_TVECTOR4: args = 4; lua_checkvector4(L, idx, &f->x, &f->y, &f->z, &f->w); break;
        case LUA_TQUAT: args = 4; lua_checkquat(L, idx, &f->w, &f->x, &f->y, &f->z); break;
        default:
          const char *msg = lua_pushfstring(L, "%s expected, got %s", "vector", luaL_typename(L, idx));
          luaL_argerror(L, idx, msg);
      }
#else
  #error unsupported Lua version
#endif
      return args;
    }
  }

  static bool is_json_null (lua_State *L, int idx) {
    lua_pushvalue(L, idx); /* [value] */

    json_null(L); /* [value, json.null] */
    bool is = lua_rawequal(L, -1, -2) != 0;
    lua_pop(L, 2);
    return is;
  }

  struct Key : std::unary_function<Key, bool> {
    bool is_number;
    lua_Number number;
    const char *key;
    size_t len;

    Key() : is_number(false), number(0), key(NULL), len(0) { }
    Key(const char *k, size_t l) : is_number(false), number(0), key(k), len(l) { }
    Key(lua_Number i, const char *k, size_t l) : is_number(true), number(i), key(k), len(l) { }

    bool operator<(const Key &rhs) const {
      return strcmp(key, rhs.key) < 0;
    }

    bool operator()(Key const& k) const {
      if (is_number == k.is_number)
        return is_number ? (number == k.number) : (strcmp(key, k.key) == 0);
      return false;
    }
  };

  /*
  ** Following JSON in which keys are either strings or numeric. Format and
  ** append all string & numeric keys of the provided table (index) to a key
  ** sink. Returning 0 on success, an error code otherwise.
  */
  static int populate_key_vector (lua_State *L, int idx, std::vector<Key> &sink) {
    size_t i, length;
#if LUA_VERSION_NUM >= 502
    length = lua_rawlen(L, idx);
#else
    length = lua_objlen(L, idx);
#endif

    for (i = 1; i <= length; ++i) {
      LuaSAX::Key key;
      lua_rawgeti(L, -1, (lua_Integer)i);
      if (lua_type(L, -1) == LUA_TSTRING) {
        key.is_number = false;
        key.number = 0;
        key.key = lua_tolstring(L, -1, &key.len);
      }
      else if (lua_type(L, -1) == LUA_TNUMBER) {
        key.is_number = true;
        key.number = lua_tonumber(L, -1);
        key.key = lua_tolstring(L, -1, &key.len); /* value converted to string */
      }
      else {
        lua_pop(L, 1);
        return -1; /* invalid key_order element */
      }
      sink.push_back(key);
      lua_pop(L, 1);
    }
    return 0; /* LUA_OK */
  }

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
    int nullarg;  /* Stack index of "null" arguments */
    int objectarg;  /* Stack index of "object" metatable */
    int arrayarg;  /* Stack index of "array" metatable */
    std::vector<Ctx> stack_;
    Ctx context_;

  public:
    explicit Reader(lua_State *_L, lua_Integer _f = 0, int _n = -1, int _o = -1, int _a = -1)
      : L(_L), flags(_f), nullarg(_n), objectarg(_o), arrayarg(_a) {
      stack_.reserve(LUA_JSON_STACK_RESERVE);
    }

    bool Null() {
      if (nullarg >= 0)
        lua_pushvalue(L, nullarg);
      else if ((flags & JSON_LUA_NILL))
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
        if (objectarg >= 0)
          lua_pushvalue(L, objectarg);
        else {
          luaL_getmetatable(L, LUA_RAPIDJSON_OBJECT);
        }
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
        if (arrayarg >= 0)
          lua_pushvalue(L, arrayarg);
        else {
          luaL_getmetatable(L, LUA_RAPIDJSON_ARRAY);
        }
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
    int stateidx;  /* Stack index of "state" table */
    std::vector<Key> order; /* Key-ordering list */

    /*
    ** Append all string & numeric keys of the given table (index) that are not
    ** contained in the writers ordering list to the provided key sink.
    **
    ** NOTE: It's assumed the "order" table is never of significant size (i.e,
    ** less than thirty elements), where more efficient searching structures are
    ** required.
    */
    void populate_unordered_vector(lua_State *L, int idx,
                           std::vector<Key> &keyorder, std::vector<Key> &sink) {
      int i_idx = JSON_REL_INDEX(idx, 1);  /* Account for key */
      LUA_JSON_CHECKSTACK(L, 3);

      lua_pushnil(L);
      while (lua_next(L, i_idx)) { /* [key, value] */
        if (lua_type(L, -2) == LUA_TSTRING) {
          Key k;
          k.is_number = false;
          k.number = 0;
          k.key = lua_tolstring(L, -2, &k.len);
          if (std::find_if(keyorder.begin(), keyorder.end(), k) == keyorder.end())
            sink.push_back(k);
        }
        else if (lua_type(L, -2) == LUA_TNUMBER) {
          Key k;
          lua_pushvalue(L, -2); /* [key, value, number for formatting] */

          k.is_number = true;
          k.number = lua_tonumber(L, -1);
          k.key = lua_tolstring(L, -1, &k.len);
          if (std::find_if(keyorder.begin(), keyorder.end(), k) == keyorder.end())
            sink.push_back(k);

          lua_pop(L, 1); /* [key, value] */
        }
#if defined(LUA_RAPIDJSON_SANITIZE_KEYS)
        else {
          luaL_error("invalid object key: %s\n", lua_typename(L, lua_type(L, -2)));
          return;
        }
#endif
        lua_pop(L, 1); /* [key] */
      }
    }

  public:
    Writer() : flags(JSON_DEFAULT), max_depth(JSON_DEFAULT_DEPTH), stateidx(-1) { }
    Writer(int _flags, int _maxdepth, int _state, std::vector<Key> &_order)
      : flags(_flags), max_depth(_maxdepth), stateidx(_state), order(_order) { }

    int GetFlags() { return flags; }
    int GetMaxDepth() { return max_depth; }
    const std::vector<Key> &GetKeyOrder() const { return order; }

    Writer &SetFlags(int f) { flags = f; return *this; }
    Writer &SetMaxDepth(int d) { max_depth = d; return *this; }

    template<typename Writer>
    bool handle_exception(lua_State *L, Writer *writer, int idx, int depth,
                                      const char *reason, const char **output) {
      bool result = false;
      if (stateidx >= 0 && lua_istable(L, stateidx)) {
        luaL_checkstack(L, 3, "exception handler");
        lua_getfield(L, stateidx, "exception"); // [function]
        if (lua_isfunction(L, -1)) {
          lua_pushstring(L, reason);  // [function, reason]
          lua_pushvalue(L, JSON_REL_INDEX(idx, 2));  // [function, reason, value]
          lua_call(L, 2, 2);  // [ r_value, r_reason]
          if (lua_isnil(L, -2)) {
            *output = luaL_optstring(L, -1, NULL);
          }
          else {
            encodeValue(L, writer, -2, depth + 1);
            result = true;
          }
          lua_pop(L, 2);  // [function]
        }
        else {
          lua_pop(L, 1);  // []
        }
      }
      return result;
    }

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
            const char *output = NULL;
            if (!handle_exception(L, writer, idx, depth, LUA_DKJSON_NUMBER, &output)) {
              if (output)
                luaL_error(L, "%s", output);
              else
                luaL_error(L, "error while encoding '%s'", lua_typename(L, LUA_TNUMBER));
              return;
            }
          }
          break;
        }
#if LUA_VERSION_NUM == 504
        case LUA_TVECTOR: {
#elif LUA_VERSION_NUM == 503
        case LUA_TVECTOR2:
        case LUA_TVECTOR3:
        case LUA_TVECTOR4:
        case LUA_TQUAT: {
#else
  #error unsupported Lua version
#endif
          lua_Float4 v;
          int args = parseVector(L, idx, &v);
          if (flags & JSON_ENCODER_ARRAY_VECTOR) {
            writer->StartArray();
            if (args) { writer->Double((double)v.x); args--; }
            if (args) { writer->Double((double)v.y); args--; }
            if (args) { writer->Double((double)v.z); args--; }
            if (args) { writer->Double((double)v.w); args--; }
            writer->EndArray();
          }
          else {
            writer->StartObject();
            if (args) { writer->Key("x"); writer->Double((double)v.x); args--; }
            if (args) { writer->Key("y"); writer->Double((double)v.y); args--; }
            if (args) { writer->Key("z"); writer->Double((double)v.z); args--; }
            if (args) { writer->Key("w"); writer->Double((double)v.w); args--; }
            writer->EndObject();
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
          if (!encodeMetafield(L, writer, idx, depth)) {
            const char *output = NULL;
            if (!handle_exception(L, writer, idx, depth, LUA_DKJSON_TYPE, &output)) {
              if (output)
                luaL_error(L, "%s", output);
              else
                luaL_error(L, LUA_DKJSON_TYPE " type '%s' is not supported by JSON.",
                                             lua_typename(L, lua_type(L, idx)));
              return;
            }
          }
          break;
        }
      }
    }

    /*
    ** TODO: The "depth" parameter isn't propagated to the meta-function,
    ** therefore, there's the potential of infinite looping (or stack overflows)
    */
    template<typename Writer>
    bool encodeMetafield(lua_State *L, Writer *writer, int idx, int depth) {
      LUA_JSON_UNUSED(depth);
      if (luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_TOJSON) == 0)
        return false;

      if (lua_type(L, -1) == LUA_TFUNCTION) {
        lua_pushvalue(L, JSON_REL_INDEX(idx, 1));  /* [metafield, self] */
        lua_call(L, 1, 1); /* [result] */
        if (lua_type(L, -1) == LUA_TSTRING) {
          size_t len;
          const char *str = lua_tolstring(L, -1, &len);
          writer->RawValue(str, len, rapidjson::Type::kObjectType);
        }
        else {
          luaL_error(L, "Invalid %s result", LUA_RAPIDJSON_META_TOJSON);
          return false;
        }
        lua_pop(L, 1); /* [] */
      }
      else {
        luaL_error(L, "Invalid %s function", LUA_RAPIDJSON_META_TOJSON);
        return false;
      }
      return true;
    }

    template<typename Writer>
    void encodeTable(lua_State *L, Writer *writer, int idx, int depth) {
      size_t array_length;
      int top = lua_gettop(L);
      if (depth > max_depth) {
        const char *output = NULL;
        if (!handle_exception(L, writer, idx, depth, LUA_DKJSON_CYCLE, &output)) {
          if (flags & JSON_ENCODER_NESTING)
            writer->Null();
          else if (output)
            luaL_error(L, "%s", output);
          else
            luaL_error(L, LUA_DKJSON_DEPTH_LIMIT);
        }
        return;
      }

      if (encodeMetafield(L, writer, idx, depth)) {
        /* Continue */
      }
      else if (table_is_json_array(L, idx, flags, &array_length)
                       && (array_length > 0 || (flags & JSON_EMPTY_AS_ARRAY))) {
        encode_array(L, writer, idx, array_length, depth);
      }
      else if (luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_ORDER) != 0) {
        /* __jsonorder returns a function (i.e., order dependent on state) */
        if (lua_type(L, -1) == LUA_TFUNCTION) {
          lua_pushvalue(L, JSON_REL_INDEX(idx, 1));  /* self */
          lua_call(L, 1, 1);
        }

        /* __jsonorder is a table or a function that returns a table */
        if (lua_type(L, -1) == LUA_TTABLE) {
          std::vector<Key> meta_order, unorder;
          populate_key_vector(L, -1, meta_order);
          lua_settop(L, top); /* & Metafield */

          populate_unordered_vector(L, idx, meta_order, unorder);
          encodeObject(L, writer, idx, depth, meta_order, unorder);
        }
        else {
          luaL_error(L, "Invalid %s result", LUA_RAPIDJSON_META_ORDER);
          return;
        }
      }
      else if ((flags & JSON_SORT_KEYS) == 0 && order.size() == 0) /* Treat table as object */
        encodeObject(L, writer, idx, depth);
      else {
        std::vector<Key> unorder; /* All keys not contained in 'order' */
        populate_unordered_vector(L, idx, order, unorder);
        if (flags & JSON_SORT_KEYS)
          std::sort(unorder.begin(), unorder.end());
        encodeObject(L, writer, idx, depth, order, unorder);
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
      LUA_JSON_CHECKSTACK(L, 3);

      writer->StartObject();
      lua_pushnil(L); /* [table, nil] */
      while (lua_next(L, i_idx)) { /* [table, key, value] */
        size_t len = 0;
        const char *key = NULL;
        if (lua_type(L, -2) == LUA_TSTRING) {
          key = lua_tolstring(L, -2, &len);
          writer->Key(key, static_cast<rapidjson::SizeType>(len));
          encodeValue(L, writer, -1, depth);
        }
        else if (lua_type(L, -2) == LUA_TNUMBER) {
          lua_pushvalue(L, -2); /* [key, value, key] */
          key = lua_tolstring(L, -1, &len);
          writer->Key(key, static_cast<rapidjson::SizeType>(len));
          lua_pop(L, 1); /* [key, value] */

          encodeValue(L, writer, -1, depth);
        }
#if defined(LUA_RAPIDJSON_SANITIZE_KEYS)
        else {
          luaL_error("invalid object key: %s\n", lua_typename(L, lua_type(L, -2)));
          return;
        }
#endif
        lua_pop(L, 1); /* pop value: [table, key] */
      }
      writer->EndObject();
    }

    template<typename Writer>
    void encodeObject(lua_State* L, Writer* writer, int idx, int depth,
                      std::vector<Key> &keyorder, std::vector<Key> &unordered) {
      int i_idx = JSON_REL_INDEX(idx, 1);
      LUA_JSON_CHECKSTACK(L, 2);

      writer->StartObject();

      /* Keys in a predefined order */
      std::vector<Key>::const_iterator oi = keyorder.end();
      for (std::vector<Key>::iterator i = keyorder.begin(); i != oi; ++i) {
        if (i->is_number)
          lua_pushnumber(L, i->number);
        else
          lua_pushlstring(L, i->key, i->len); /* sorted key */
        lua_gettable(L, i_idx);
        if (!lua_isnil(L, -1)) {
          writer->Key(i->key, static_cast<rapidjson::SizeType>(i->len));
          encodeValue(L, writer, -1, depth);
        }
        lua_pop(L, 1);
      }

      /* Keys not in a predefined order */
      std::vector<Key>::const_iterator e = unordered.end();
      for (std::vector<Key>::const_iterator i = unordered.begin(); i != e; ++i) {
        writer->Key(i->key, static_cast<rapidjson::SizeType>(i->len));
        if (i->is_number)
          lua_pushnumber(L, i->number);
        else
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
