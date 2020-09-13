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

#include <rapidjson/rapidjson.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>

extern "C" {
  #include <lua.h>
  #include <lualib.h>
  #include <lauxlib.h>
}
#include "StringStream.hpp"

/* Registry Table Keys */
#define LUA_RAPIDJSON_REG "lua_rapidjson"
#define LUA_RAPIDJSON_REG_ARRAY "lua_rapidjson_array"
#define LUA_RAPIDJSON_REG_OBJECT "lua_rapidjson_object"
#define LUA_RAPIDJSON_REG_NULL "lua_rapidjson_nullref"

/* Metamethods */
#define LUA_RAPIDJSON_META_TOJSON "__tojson"
#define LUA_RAPIDJSON_META_ORDER "__jsonorder"
#define LUA_RAPIDJSON_META_TYPE "__jsontype"
#define LUA_RAPIDJSON_META_TYPE_ARRAY "array"
#define LUA_RAPIDJSON_META_TYPE_OBJECT "object"

/* Registry Table Index */
#define LUA_RAPIDJSON_REG_FLAGS "flags"
#define LUA_RAPIDJSON_REG_DEPTH "max_depth"
#define LUA_RAPIDJSON_REG_LEVEL "level"
#define LUA_RAPIDJSON_REG_INDENT "indent"
#define LUA_RAPIDJSON_REG_MAXDEC "decimal_count"
#define LUA_RAPIDJSON_REG_PRESET "decoder_preset"

/* DKJson Error Messages */
#define LUA_DKJSON_CYCLE "reference cycle"
#define LUA_DKJSON_FAIL "custom encoder failed"
#define LUA_DKJSON_TYPE "unsupported type"
#define LUA_DKJSON_NUMBER "error encoding number"
#define LUA_DKJSON_DEPTH_LIMIT "maximum table nesting depth exceeded" /* Replaces _CYCLE */

#if !defined(LUA_RAPIDJSON_INLINE)
  #if defined(__GNUC__) || defined(__CLANG__)
    #define LUA_RAPIDJSON_INLINE inline __attribute__((__always_inline__))
  #elif defined(LUA_USE_WINDOWS)
    #define LUA_RAPIDJSON_INLINE __forceinline
  #else
    #define LUA_RAPIDJSON_INLINE inline
  #endif
#endif

/*
** {==================================================================
** Lua Compatibility
** ===================================================================
*/

/* Macro to avoid warnings about unused variables */
#define LUA_JSON_UNUSED(x) ((void)(x))

#if !defined(LUA_MAXINTEGER) /* 51 & 52 */
  #define LUA_MAXINTEGER std::numeric_limits<lua_Integer>::max()
  #define LUA_MININTEGER std::numeric_limits<lua_Integer>::min()
#endif

/* maximum size visible for Lua (must be representable in a lua_Integer) */
#if !defined(MAX_SIZE)
  #if !defined(MAX_SIZET)
    #define MAX_SIZET std::numeric_limits<std::size_t>::max()
  #endif

  #if LUA_VERSION_NUM == 503 || LUA_VERSION_NUM == 504
    #define MAX_SIZE (sizeof(size_t) < sizeof(lua_Integer) ? MAX_SIZET : static_cast<size_t>(LUA_MAXINTEGER))
  #elif LUA_VERSION_NUM == 501 || LUA_VERSION_NUM == 502
    #define MAX_SIZE MAX_SIZET
  #else
    #error unsupported Lua version
  #endif
#endif

/* Utility to help abstract relative stack indices with absolute */
#define lua_json_rel_index(idx, n) (((idx) < 0) ? ((idx) - (n)) : (idx))

/*
** Returns 1 if the value at the given index is an integer (that is, the value
** is a number and is represented as an integer), and 0 otherwise.
*/
#if LUA_VERSION_NUM >= 503
  #define lua_json_isinteger(L, idx) lua_isinteger((L), (idx))
#else
static LUA_RAPIDJSON_INLINE int lua_json_isinteger (lua_State *L, int idx) {
  if (LUA_TNUMBER == lua_type(L, idx)) {
    lua_Number n = lua_tonumber(L, idx);
    return (!isinf(n) && ((lua_Number)((lua_Integer)(n))) == (n));
  }
  return 0;
}
#endif

/* Unsafe macro to disable constant lua_checkstack calls */
#if defined(LUA_RAPIDJSON_UNSAFE)
  #define lua_json_checkstack(L, sz) ((void)0)
#else
  #define lua_json_checkstack(L, sz)          \
    do {                                      \
      if (!lua_checkstack((L), (sz)))         \
        throw rapidjson::LuaStackException(); \
    } while ( 0 )
#endif

/* Macro for geti/seti. Parameters changed from int to lua_Integer in 53 */
#if LUA_VERSION_NUM >= 503
  #define lua_json_ti(v) v
#else
  #define lua_json_ti(v) static_cast<int>(v)
#endif

/* }================================================================== */

/*
** {==================================================================
** Configuration Settings
** ===================================================================
*/

/* @TODO: Support 16-bit Lua */
#if !defined(LUA_RAPIDJSON_BIT32) && UINTPTR_MAX == UINT_MAX
  #define LUA_RAPIDJSON_BIT32
#endif

/*
** Threshold for table_is_json_array: If a table has an integer key greater than
** this value, ensure at least half of the keys within the table have elements
** to be encoded as an array.
*/
#if !defined(LUA_DKJSON_TABLE_CUTOFF)
  #define LUA_DKJSON_TABLE_CUTOFF 11
#endif

/*
** Limit to the encoding of nested tables to prevent infinite looping against
** circular references in tables. The alternative solution, as with DKJson, is
** to use a secondary (weak-)table that is used to store already processed
** references.
*/
#if !defined(JSON_DEFAULT_DEPTH)
  #define JSON_DEFAULT_DEPTH 32
#endif

/* Default context-stack preallocation amount. */
#if !defined(LUA_JSON_STACK_RESERVE)
  #define LUA_JSON_STACK_RESERVE 32
#endif

/*
** rapidjson::Writer<rapidjson::StringBuffer>::kDefaultMaxDecimalPlaces
** replacement, based upon the default string formats for Lua.
*/
#if LUA_VERSION_NUM >= 503
  #if LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT
    #define LUA_NUMBER_FMT_LEN 7
  #elif LUA_FLOAT_TYPE == LUA_FLOAT_LONGDOUBLE  /* }{ long double */
    #define LUA_NUMBER_FMT_LEN 19
  #elif LUA_FLOAT_TYPE == LUA_FLOAT_DOUBLE  /* }{ double */
    #define LUA_NUMBER_FMT_LEN 14
  #else
    #error "numeric float type not defined"
  #endif
#else
  #define LUA_NUMBER_FMT_LEN 14
#endif

#if defined(LUA_RAPIDJSON_LUA_FLOAT)
/*
** Source lobject.c
**
** Maximum length of the conversion of a number to a string. Must be
** enough to accommodate both LUA_INTEGER_FMT and LUA_NUMBER_FMT.
** (For a long long int, this is 19 digits plus a sign and a final '\0',
** adding to 21. For a long double, it can go to a sign, 33 digits,
** the dot, an exponent letter, an exponent sign, 5 exponent digits,
** and a final '\0', adding to 43.)
*/
#define MAXNUMBER2STR 44

/*
** Convert a number object to a string, adding it to a buffer
**
** Based upon: lobject.tostringbuff
*/
template<typename Writer>
static inline bool tostringbuff(lua_State *L, Writer *writer, double value) {
  char buff[MAXNUMBER2STR];
  #if LUA_VERSION_NUM >= 503
  int len = lua_number2str(buff, MAXNUMBER2STR, value);
  #else
  int len = lua_number2str(buff, value);
  #endif
  if (buff[strspn(buff, "-0123456789")] == '\0') {  /* looks like an int? */
    buff[len++] = '.';  /* Forced locale */
    buff[len++] = '0';  /* adds '.0' to result */
  }
  else {
    /* Fix Locale */
    char *begin = buff, *end = buff + len;
    for (; begin != end; ++begin) {
      if (*begin == ',')
        *begin = '.';
    }
  }
  LUA_JSON_UNUSED(L);
  return writer->RawValue(buff, static_cast<size_t>(len), rapidjson::kNumberType);
}
#endif

/* }================================================================== */

/*
** {==================================================================
** API Functions
** ===================================================================
*/

/* Pushes the null-sentinel onto the stack; returning 1. */
LUA_API int json_null (lua_State *L);

/*
** Return true if the object at the specific stack index is, or a reference to,
** json_null.
*/
LUA_API bool is_json_null (lua_State *L, int idx);

/*
** Return true if the table at the specified stack index has a metatable with
** a jsontype (see LUA_RAPIDJSON_META_TYPE) field; storing "true" in is_array
** if that type corresponds to an array (see LUA_RAPIDJSON_REG_ARRAY).
**
** See the note within 'table_is_json_array'.
*/
LUA_API bool has_json_type (lua_State *L, int idx, bool *is_array);

/*
** Return true if the table at the specified stack index can be encoded as an
** array, i.e., a table whose keys are (1) integers; (2) begin at one; (3)
** strictly positive; and (4) form a contiguous sequence.
**
** However, with the flag "JSON_ARRAY_WITH_HOLES" set, condition (4) is
** alleviated and msgpack can encode "null" in the nil array indices.
**
** Note: encode2() doesn't give special treatment/priority to the __jsontype
** metafield besides:
**    local isa, n = isarray (value)
**    if n == 0 and valmeta and valmeta.__jsontype == 'object' then
**      isa = false
**    end*
*/
LUA_API bool table_is_json_array (lua_State *L, int idx, int flags, size_t *array_length);

/*
** A try/catch extension of luaL_error: "raises an error. The error message
** format is given by fmt plus any extra arguments, following the same rules of
** lua_pushfstring".
**
** However, instead of calling LUAI_THROW, which may be a "longjmp", this
** function will throw a rapidjson::LuaException so any privately allocated
** data by rapidjson, e.g., see rapidjson::CrtAllocator, can be properly
** deallocated during stack-unwinding.
*/
LUA_API int json_error (lua_State *L, const char *message);

/* }================================================================== */

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

#define JSON_DEFAULT (JSON_LUA_NILL | JSON_EMPTY_AS_ARRAY | JSON_ARRAY_WITH_HOLES)

namespace LuaSAX {
  struct Key : std::unary_function<Key, bool> {
    bool is_number;
    lua_Number number;
    const char *key;
    size_t len;

    Key() : is_number(false), number(0), key(nullptr), len(0) { }
    Key(const char *k, size_t l) : is_number(false), number(0), key(k), len(l) { }
    Key(lua_Number i, const char *k, size_t l) : is_number(true), number(i), key(k), len(l) { }

    bool operator<(const Key &rhs) const {
      return strcmp(key, rhs.key) < 0;
    }

    bool operator()(Key const &k) const {
      if (is_number == k.is_number)
        return is_number ? (number == k.number) : (strcmp(key, k.key) == 0);
      return false;
    }
  };

  /*
  ** Following JSON in which keys are either strings or numeric. Format and
  ** append all string & numeric keys of the provided array (index) to a key
  ** sink. Returning 0 on success, an error code otherwise.
  */
  static int populate_key_vector (lua_State *L, int idx, std::vector<Key> &sink) {
    size_t i, length;
#if LUA_VERSION_NUM >= 502
    length = static_cast<size_t>(lua_rawlen(L, idx));
#else
    length = lua_objlen(L, idx);
#endif

    for (i = 1; i <= length; ++i) {
      LuaSAX::Key key;
#if LUA_VERSION_NUM >= 503
      lua_rawgeti(L, idx, static_cast<lua_Integer>(i));
#else
      lua_pushinteger(L, static_cast<lua_Integer>(i));
      lua_rawget(L, lua_json_rel_index(idx, 1));
#endif

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
#if defined(LUA_RAPIDJSON_SANITIZE_KEYS)
        throw rapidjson::LuaTypeException(lua_type(L, -1), rapidjson::LuaTypeException::UnsupportedKeyOrder);
#else
        lua_pop(L, 1);
        continue;
#endif
      }
      sink.push_back(key);
      lua_pop(L, 1);
    }
    return 0; /* LUA_OK */
  }

  /** SAX Handler: https://rapidjson.org/classrapidjson_1_1_handler.html */
  struct Reader {
private:
    /// <summary>
    /// </summary>
    struct Ctx {
      typedef void (*ctx_callback) (lua_State *, struct Ctx *);
      static Ctx Object() { return Ctx(&objectFn); }
      static Ctx Array() { return Ctx(&arrayFn); }

      rapidjson::SizeType index_;
      ctx_callback fn_;

      Ctx() : index_(0), fn_(&topFn) { }
      Ctx(const Ctx &rhs) : index_(rhs.index_), fn_(rhs.fn_) { }
      explicit Ctx(void (*f)(lua_State *L, Ctx *ctx)) : index_(0), fn_(f) { }

      const Ctx &operator=(const Ctx &rhs) {
        if (this != &rhs) {
          index_ = rhs.index_;
          fn_ = rhs.fn_;
        }
        return *this;
      }

      void submit(lua_State *Ls) { fn_(Ls, this); }

      static void objectFn(lua_State *L, Ctx *ctx) {
        LUA_JSON_UNUSED(ctx);
        lua_rawset(L, -3);
      }

      static void arrayFn(lua_State *L, Ctx *ctx) {
#if LUA_VERSION_NUM >= 503
        lua_rawseti(L, -2, ++ctx->index_);
#else
        lua_pushinteger(L, ++ctx->index_); /* [..., value, key] */
        lua_pushvalue(L, -2); /* [..., value, key, value] */
        lua_rawset(L, -4); /* [..., value] */
        lua_pop(L, 1); /* [...] */
#endif
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

#if defined(LUA_RAPIDJSON_COMPAT)
    #define LUA_JSON_HANDLE(NAME, ...) bool NAME(__VA_ARGS__, bool mapValue = false)
    #define LUA_JSON_HANDLE_NULL(NAME) bool NAME(bool mapValue = false)
    #define LUA_JSON_SUBMIT() \
      do {                    \
        if (!mapValue)        \
          context_.submit(L); \
      } while ( 0 )

    bool ImplicitArrayInObjectContext(rapidjson::SizeType u) {
#if LUA_VERSION_NUM >= 503
      lua_rawseti(L, -2, static_cast<lua_Integer>(u));
#else
      lua_json_checkstack(L, 3);
      lua_pushinteger(L, static_cast<lua_Integer>(u)); /* [..., value, key] */
      lua_pushvalue(L, -2); /* [..., value, key, value] */
      lua_rawset(L, -4); /* [..., value] */
      lua_pop(L, 1); /* [...] */
#endif
      return true;
    }

    bool ImplicitObjectInContext() {
      lua_rawset(L, -3);
      return true;
    }
#else
    #define LUA_JSON_SUBMIT() context_.submit(L)
    #define LUA_JSON_HANDLE(NAME, ...) bool NAME(__VA_ARGS__)
    #define LUA_JSON_HANDLE_NULL(NAME) bool NAME()
#endif

    LUA_JSON_HANDLE_NULL(Null) {
      if (nullarg >= 0)
        lua_pushvalue(L, nullarg);
      else if ((flags & JSON_LUA_NILL))
        lua_pushnil(L);
      else
        json_null(L);
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Bool, bool b) {
      lua_pushboolean(L, b);
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Int, int i) {
      lua_pushinteger(L, i);
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Uint, unsigned u) {
      if (sizeof(lua_Integer) > sizeof(unsigned int) || u <= static_cast<unsigned>(LUA_MAXINTEGER))
        lua_pushinteger(L, static_cast<lua_Integer>(u));
      else
        lua_pushnumber(L, static_cast<lua_Number>(u));
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Int64, int64_t i) {
      if (sizeof(lua_Integer) >= sizeof(int64_t) || (i <= LUA_MAXINTEGER && i >= LUA_MININTEGER))
        lua_pushinteger(L, static_cast<lua_Integer>(i));
      else
        lua_pushnumber(L, static_cast<lua_Number>(i));
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Uint64, uint64_t u) {
      if (sizeof(lua_Integer) > sizeof(uint64_t) || u <= static_cast<uint64_t>(LUA_MAXINTEGER))
        lua_pushinteger(L, static_cast<lua_Integer>(u));
      else
        lua_pushnumber(L, static_cast<lua_Number>(u));
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(Double, double d) {
      lua_pushnumber(L, static_cast<lua_Number>(d));
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(RawNumber, const char *str, rapidjson::SizeType length, bool copy) {
      LUA_JSON_UNUSED(copy);

      lua_getglobal(L, "tonumber");
      lua_pushlstring(L, str, length);
      lua_call(L, 1, 1);
      LUA_JSON_SUBMIT();
      return true;
    }

    LUA_JSON_HANDLE(String, const char *str, rapidjson::SizeType length, bool copy) {
      LUA_JSON_UNUSED(copy);

      lua_pushlstring(L, str, length);
      LUA_JSON_SUBMIT();
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
          luaL_getmetatable(L, LUA_RAPIDJSON_REG_OBJECT);
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

    LUA_JSON_HANDLE(EndObject, rapidjson::SizeType memberCount) {
      LUA_JSON_UNUSED(memberCount);

      context_ = stack_.back();
      stack_.pop_back();
      LUA_JSON_SUBMIT();
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
          luaL_getmetatable(L, LUA_RAPIDJSON_REG_ARRAY);
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

    LUA_JSON_HANDLE(EndArray, rapidjson::SizeType elementCount) {
#if !defined(LUA_RAPIDJSON_COMPAT)
      lua_assert(elementCount == context_.index_);
#endif
      LUA_JSON_UNUSED(elementCount);

      context_ = stack_.back();
      stack_.pop_back();
      LUA_JSON_SUBMIT();
      return true;
    }
  };

  class Writer {
private:
    int flags;  /* Configuration flags */
    int max_depth;  /* Maximum recursive depth */
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
    void populate_unordered_vector(lua_State *L, int idx, std::vector<Key> &keyorder, std::vector<Key> &sink) {
      int i_idx = lua_json_rel_index(idx, 1); /* Account for key */
      lua_json_checkstack(L, 3);

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
          k.key = lua_tolstring(L, -1, &k.len); /* edge-case: inf/NaN -> "inf"/"NaN" */
          if (std::find_if(keyorder.begin(), keyorder.end(), k) == keyorder.end())
            sink.push_back(k);

          lua_pop(L, 1); /* [key, value] */
        }
#if defined(LUA_RAPIDJSON_SANITIZE_KEYS)
        else {
          throw rapidjson::LuaTypeException(lua_type(L, -2), rapidjson::LuaTypeException::UnsupportedKeyOrder);
        }
#endif
        lua_pop(L, 1); /* [key] */
      }
    }

public:
    Writer()
      : flags(JSON_DEFAULT), max_depth(JSON_DEFAULT_DEPTH), stateidx(-1) {
    }
    Writer(int _flags, int _maxdepth, int _state, std::vector<Key> &_order)
      : flags(_flags), max_depth(_maxdepth), stateidx(_state), order(_order) {
    }

    int GetFlags() { return flags; }
    int GetMaxDepth() { return max_depth; }
    const std::vector<Key> &GetKeyOrder() const { return order; }

    Writer &SetFlags(int f) {
      flags = f;
      return *this;
    }
    Writer &SetMaxDepth(int d) {
      max_depth = d;
      return *this;
    }

    template<typename Writer>
    bool handle_exception(lua_State *L, Writer *writer, int idx, int depth, const char *reason, const char **output) {
      bool result = false;
      if (stateidx >= 0 && lua_istable(L, stateidx)) {
        luaL_checkstack(L, 3, "exception handler");
        lua_getfield(L, stateidx, "exception");  // [function]
        if (lua_isfunction(L, -1)) {
          lua_pushstring(L, reason);  // [function, reason]
          lua_pushvalue(L, lua_json_rel_index(idx, 2));  // [function, reason, value]
          lua_call(L, 2, 2);  // [r_value, r_reason]
          if (lua_isnil(L, -2)) {
            *output = luaL_optstring(L, -1, nullptr);
          }
          else {
            encodeValue(L, writer, -2, depth + 1);
            result = true;
          }
          lua_pop(L, 2);  // []
        }
        else {
          lua_pop(L, 1);  // []
        }
      }
      return result;
    }

#if defined(LUA_RAPIDJSON_ROUND_FLOAT)
    #define _EXP2(a, b) a##b
    #define EXP(digits) _EXP2(1##E, digits)
    static LUA_RAPIDJSON_INLINE double luaRoundDecimal(double d) {
      if ((std::numeric_limits<double>::max() / EXP(LUA_NUMBER_FMT_LEN)) <= d)
        return d;
      return std::round(d * EXP(LUA_NUMBER_FMT_LEN)) / EXP(LUA_NUMBER_FMT_LEN);
    }
#endif

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
              writer->Uint(static_cast<unsigned>(lua_tointeger(L, idx)));
            else
              writer->Int(static_cast<int>(lua_tointeger(L, idx)));
#else
            if (flags & JSON_UNSIGNED_INTEGERS)
              writer->Uint64(static_cast<uint64_t>(lua_tointeger(L, idx)));
            else
              writer->Int64(static_cast<int64_t>(lua_tointeger(L, idx)));
#endif
          }
          else {
            const double d = static_cast<double>(lua_tonumber(L, idx));
            if (rapidjson::internal::Double(d).IsNanOrInf()) {
              writer->Null();
            }
#if defined(LUA_RAPIDJSON_LUA_FLOAT)
            else if (!tostringbuff(L, writer, d)) {
#elif defined(LUA_RAPIDJSON_ROUND_FLOAT)
            else if (!writer->Double(luaRoundDecimal(d))) {
#else
            else if (!writer->Double(d)) {
#endif
              const char *output = nullptr;
              if (!handle_exception(L, writer, idx, depth, LUA_DKJSON_NUMBER, &output)) {
                if (output)
                  json_error(L, output);
                else
                  json_error(L, "error encoding: kWriteNanAndInfFlag");
                return;
              }
            }
          }
          break;
        }
        case LUA_TSTRING: {
          size_t len;
          const char *s = lua_tolstring(L, idx, &len);
          writer->String(s, static_cast<rapidjson::SizeType>(len));
          return;
        }
        case LUA_TTABLE: {
          encodeTable(L, writer, idx, depth + 1);
          return;
        }
        case LUA_TFUNCTION:
          if (is_json_null(L, idx)) {
            writer->Null();
            break;
          }
          RAPIDJSON_DELIBERATE_FALLTHROUGH;
        case LUA_TLIGHTUSERDATA:
        case LUA_TUSERDATA:
        case LUA_TTHREAD:
        case LUA_TNONE:
        default: {
          if (!encodeMetafield(L, writer, idx, depth)) {
            const char *output = nullptr;
            if (!handle_exception(L, writer, idx, depth, LUA_DKJSON_TYPE, &output)) {
              if (output)
                json_error(L, output);
              else
                throw rapidjson::LuaTypeException(lua_type(L, idx), rapidjson::LuaTypeException::UnsupportedType);
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
#if LUA_VERSION_NUM >= 503
      if (luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_TOJSON) == LUA_TNIL) {
#else
      if (luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_TOJSON) == 0) {
#endif
        return false;
      }

      if (lua_type(L, -1) == LUA_TFUNCTION) {
        lua_pushvalue(L, lua_json_rel_index(idx, 1));  /* [metafield, self] */
        lua_call(L, 1, 1); /* [result] */
        if (lua_type(L, -1) == LUA_TSTRING) {
          size_t len;
          const char *str = lua_tolstring(L, -1, &len);
          writer->RawValue(str, len, rapidjson::Type::kObjectType);
        }
        else {
          json_error(L, "Invalid " LUA_RAPIDJSON_META_TOJSON " result");
          return false;
        }
        lua_pop(L, 1); /* [] */
      }
      else {
        json_error(L, "Invalid " LUA_RAPIDJSON_META_TOJSON " function");
        return false;
      }
      return true;
    }

    template<typename Writer>
    void encodeTable(lua_State *L, Writer *writer, int idx, int depth) {
      size_t array_length;
      int top = lua_gettop(L);
      if (depth > max_depth) {
        const char *output = nullptr;
        if (!handle_exception(L, writer, idx, depth, LUA_DKJSON_CYCLE, &output)) {
          if (flags & JSON_ENCODER_NESTING)
            writer->Null();
          else if (output)
            json_error(L, output);
          else
            json_error(L, LUA_DKJSON_DEPTH_LIMIT);
        }
        return;
      }

      if (encodeMetafield(L, writer, idx, depth)) {
        /* Continue */
      }
      else if (table_is_json_array(L, idx, flags, &array_length)) {
        encode_array(L, writer, idx, array_length, depth);
      }
#if LUA_VERSION_NUM >= 503
      else if (luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_ORDER) != LUA_TNIL) {
#else
      else if (luaL_getmetafield(L, idx, LUA_RAPIDJSON_META_ORDER) != 0) {
#endif
        /* __jsonorder returns a function (i.e., order dependent on state) */
        if (lua_type(L, -1) == LUA_TFUNCTION) {
          lua_pushvalue(L, lua_json_rel_index(idx, 1));  /* self */
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
          json_error(L, "Invalid " LUA_RAPIDJSON_META_ORDER " result");
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
    void encode_array(lua_State *L, Writer *writer, int idx, size_t array_length, int depth) {
      writer->StartArray();
      for (size_t i = 1; i <= array_length; ++i) {
#if LUA_VERSION_NUM >= 503
        lua_rawgeti(L, idx, static_cast<lua_Integer>(i));
#else
        lua_pushinteger(L, static_cast<lua_Integer>(i));
        lua_rawget(L, lua_json_rel_index(idx, 1));
#endif
        encodeValue(L, writer, -1, depth);
        lua_pop(L, 1);
      }
      writer->EndArray();
    }

    template<typename Writer>
    void encodeObject(lua_State *L, Writer *writer, int idx, int depth) {
      int i_idx = lua_json_rel_index(idx, 1);
      lua_json_checkstack(L, 3);

      writer->StartObject();
      lua_pushnil(L); /* [table, nil] */
      while (lua_next(L, i_idx)) { /* [table, key, value] */
        size_t len = 0;
        const char *key = nullptr;
        if (lua_type(L, -2) == LUA_TSTRING) {
          key = lua_tolstring(L, -2, &len);
          writer->Key(key, static_cast<rapidjson::SizeType>(len));
          encodeValue(L, writer, -1, depth);
        }
        else if (lua_type(L, -2) == LUA_TNUMBER) {
          lua_pushvalue(L, -2); /* [key, value, key] */
          key = lua_tolstring(L, -1, &len); /* edge-case: inf/NaN -> "inf"/"NaN" */
          writer->Key(key, static_cast<rapidjson::SizeType>(len));
          lua_pop(L, 1); /* [key, value] */

          encodeValue(L, writer, -1, depth);
        }
#if defined(LUA_RAPIDJSON_SANITIZE_KEYS)
        else {
          throw rapidjson::LuaTypeException(lua_type(L, -2), rapidjson::LuaTypeException::UnsupportedKeyOrder);
        }
#endif
        lua_pop(L, 1); /* pop value: [table, key] */
      }
      writer->EndObject();
    }

    template<typename Writer>
    void encodeObject(lua_State *L, Writer *writer, int idx, int depth, std::vector<Key> &keyorder, std::vector<Key> &unordered) {
      int i_idx = lua_json_rel_index(idx, 1);
      lua_json_checkstack(L, 2);

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
