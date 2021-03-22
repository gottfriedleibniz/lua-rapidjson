#pragma once
#include <string>
#if defined(LUA_INCLUDE_HPP)
  #include <lua.hpp>
#else
extern LUA_RAPIDJSON_LINKAGE {
  #include <lua.h>
}
#endif

/* LUA_OK introduced in Lua 5.2 */
#if !defined(LUA_OK)
  #define LUA_OK 0
#endif

/* @TODO C98 compatibility (likely never) */
#define RAPIDJSON_NULLPTR nullptr

/*
** StringStream
**
** Support Constructs StringStream with the first count characters of the
** character array starting with the element pointed by src
**
** ! \note implements Stream concept
*/
RAPIDJSON_NAMESPACE_BEGIN
namespace extend {
  template<typename Encoding>
  struct GenericStringStream {
    typedef typename Encoding::Ch Ch;

    GenericStringStream(const Ch *src, const size_t count)
      : src_(src), head_(src), count_(count) {
    }

    Ch Peek() const { return Tell() < count_ ? *src_ : '\0'; }
    Ch Take() { return *src_++; }
    size_t Tell() const { return static_cast<size_t>(src_ - head_); }

    Ch *PutBegin() { RAPIDJSON_ASSERT(false); return RAPIDJSON_NULLPTR; }
    void Put(Ch) { RAPIDJSON_ASSERT(false); }
    void Flush() { RAPIDJSON_ASSERT(false); }
    size_t PutEnd(Ch *) { RAPIDJSON_ASSERT(false); return 0; }

    const Ch *src_;  //!< Current read position.
    const Ch *head_;  //!< Original head of the string.
    size_t count_;  //!< Original head of the string.
  };

  //! String stream with UTF8 encoding.
  typedef GenericStringStream<UTF8<>> StringStream;
}

template<typename Encoding>
struct StreamTraits<extend::GenericStringStream<Encoding>> {
  enum { copyOptimization = 1 };
};

/// <summary>
/// Allocators.h requites "Free" to be a static function and gives no
/// reference to the allocator object maintained by the writer. Thus, to
/// safely piggyback of the Lua allocator, the allocator itself must be a
/// static.
///
/// Thus, unusable for any multi-threaded Lua environment.
/// </summary>
class LuaAllocator {
private:
  /// <summary>
  /// Memory block contains a header that stores the active Lua allocator state
  /// </summary>
  struct lua_AllocHeader {
    lua_Alloc alloc;  // Allocation function;
    void *ud;  // Allocator userdata;
    size_t size;  // Size of segment

    lua_AllocHeader()
      : alloc(RAPIDJSON_NULLPTR), ud(RAPIDJSON_NULLPTR), size(0) {
    }

    lua_AllocHeader(lua_State *L) {
      alloc = lua_getallocf(L, &ud);
      size = 0;
    }
  };

  lua_State *L;
  lua_AllocHeader cache;

public:
  static const bool kNeedFree = true;

  LuaAllocator() : L(RAPIDJSON_NULLPTR) { }
  LuaAllocator(lua_State *L_) : L(L_), cache(L_) { }
  ~LuaAllocator() {
  }

  /// <summary>
  /// Have the Lua allocator create a block of "size + N" bytes with the
  /// lua_AllocHeader padded to the front of the block.
  /// </summary>
  void *Realloc(void *originalPtr, size_t originalSize, size_t newSize) {
    lua_AllocHeader header;  // Active allocation header
    void *result = RAPIDJSON_NULLPTR;  // Result

    void *origSegment = RAPIDJSON_NULLPTR;  // If the original pointer is non-null, assume it has a packed header
    if (originalPtr != RAPIDJSON_NULLPTR) {
      origSegment = reinterpret_cast<void *>(reinterpret_cast<char *>(originalPtr) - sizeof(lua_AllocHeader));
      header = *reinterpret_cast<lua_AllocHeader *>(origSegment);
    }
    else {
      if (L == RAPIDJSON_NULLPTR)  // @TODO: Throw an exception; invalid Lua state and no allocator exists.
        return RAPIDJSON_NULLPTR;

      header.size = 0;
      header.ud = cache.ud;
      header.alloc = cache.alloc;
    }

    // Per Lua: When nsize is zero, the allocator must behave like free and then return NULL.
    const size_t newSegSize = newSize > 0 ? (newSize + sizeof(lua_AllocHeader)) : 0;
    if (origSegment != RAPIDJSON_NULLPTR || newSegSize != 0) {
      void *newSegment = header.alloc(header.ud, origSegment, header.size, newSegSize);
      if (newSegment != RAPIDJSON_NULLPTR) {
        header.size = newSegSize;

        // Append header to beginning of allocated block
        *(reinterpret_cast<lua_AllocHeader *>(newSegment)) = header;

        // Move to the beginning of the writable/readable memory segments.
        result = reinterpret_cast<void *>(reinterpret_cast<char *>(newSegment) + sizeof(lua_AllocHeader));
      }
    }

    ((void)(originalSize));  // Padded into header
    return result;
  }

  void RAPIDJSON_FORCEINLINE *Malloc(size_t size) {
    return Realloc(RAPIDJSON_NULLPTR, 0, size);
  }

  static RAPIDJSON_FORCEINLINE void Free(void *ptr) {
    if (ptr != RAPIDJSON_NULLPTR) {
      LuaAllocator allocator;  // So long as "originalPtr" is non-null
      allocator.Realloc(ptr, 0, 0);
    }
  }
};

/// <summary>
/// lua_checkstack returned false.
/// </summary>
class LuaStackException : public std::exception {
public:
  LuaStackException() { }
  const char *what() const noexcept override {
    return "Lua Stack Overflow";
  }
};

/// <summary>
/// A JSON encoding/decoding error.
/// </summary>
class LuaException : public std::exception {
private:
  const char *_msg;  // Error message string literal
  LuaException &operator=(const LuaException &other);  // prevent

public:
  LuaException(const char *s) : _msg(s) { }
  LuaException(const LuaException &other) : _msg(other._msg) { }
  const char *what() const noexcept override {
    return _msg;
  }
};

class LuaCallException : public std::exception {
private:
  int top;  // lua_pcall error message index

public:
  LuaCallException(int _top) : top(_top) { }
  LuaCallException(const LuaCallException &other) : top(other.top) { }
  const char *what() const noexcept override {
    return "lua_pcall error";
  }

  /// <summary>
  /// Rotates the error message
  /// </summary>
  bool pushError(lua_State *L, const int call_top) const {
    if (top <= call_top || !lua_checkstack(L, 2)) {  // Invalid state, should never happen
      lua_settop(L, call_top);
      return false;
    }
    else if (top > (call_top + 1)) {  // Move "error message" to call_top + 1
#if LUA_VERSION_NUM < 503
      static auto compat_absindex = [](lua_State *L, int i) -> int {
        if (i < 0 && i > LUA_REGISTRYINDEX)
          i += lua_gettop(L) + 1;
        return i;
      };

      static auto compat_reverse = [](lua_State *L, int a, int b) {
        for (; a < b; ++a, --b) {
          lua_pushvalue(L, a);
          lua_pushvalue(L, b);
          lua_replace(L, a);
          lua_replace(L, b);
        }
      };

      static auto compat_rotate = [](lua_State *L_, int idx, int n) {
        idx = compat_absindex(L_, idx);

        const int n_elems = lua_gettop(L_) - idx + 1;
        if (n < 0)
          n += n_elems;

        if (n > 0 && n < n_elems) {
          n = n_elems - n;
          compat_reverse(L_, idx, idx + n - 1);
          compat_reverse(L_, idx + n, idx + n_elems - 1);
          compat_reverse(L_, idx, idx + n_elems - 1);
        }
      };

      compat_rotate(L, call_top + 1, 1);
#else
      lua_rotate(L, call_top + 1, 1);
#endif
      lua_settop(L, call_top + 1);
      return true;
    }

    return true;
  }
};

/// <summary>
/// lua_types exception, e.g., Expected vs. Actual; or Invalid Type.
/// </summary>
class LuaTypeException : public std::exception {
private:
  int _luatype;
  int _errorcode;

public:
  static const int UnsupportedType = 0x0;
  static const int UnsupportedKeyOrder = 0x1;

  LuaTypeException(int type, int code)
    : _luatype(type), _errorcode(code) {
  }

  LuaTypeException(const LuaTypeException &other)
    : _luatype(other._luatype), _errorcode(other._errorcode) {
  }

  bool pushError(lua_State *L, const int call_top) const {
    lua_settop(L, call_top);
    switch (_errorcode) {
      case UnsupportedType:
        return LuaTypeException::_lua_typestring(L, "type '%s' is not supported by JSON\n", _luatype);
      case UnsupportedKeyOrder:
        return LuaTypeException::_lua_typestring(L, "type '%s' is not supported as a keyorder by JSON\n", _luatype);
      default:
        return LuaTypeException::_lua_pushstring(L, "LuaTypeException");
    }
  }

  const char *what() const noexcept override {
    return "LuaTypeException";
  }

  /// <summary>
  /// Safely push strings onto the Lua stack. This function should generally
  /// only be called during exception handling.
  /// </summary>
  static bool _lua_pushstring(lua_State *L_, char const *str) {
    auto push = [](lua_State *L) {
      auto ptr = lua_touserdata(L, 1);
      if (ptr) {
        const char *sptr = *reinterpret_cast<const char **>(ptr);
        lua_pushstring(L, sptr);
        return 1;
      }
      return 0;
    };

    // Use Lua API functions that cannot raise errors (especially LUA_ERRMEM)
    lua_pushcfunction(L_, push);
    lua_pushlightuserdata(L_, &str);
    return lua_pcall(L_, 1, 1, 0) == LUA_OK;
  }

  /// <summary>
  /// lua_pushfstring helper where "_fmt" is a string with only one string
  /// conversion specifier.
  /// </summary>
  static bool _lua_typestring(lua_State *L_, const char *_fmt, int _type) {
    auto push = [](lua_State *L) {
      auto ptr_fmt = lua_touserdata(L, 1);
      int ptr_type = static_cast<int>(lua_tointeger(L, 2));
      if (ptr_fmt) {
        const char *fmt = *reinterpret_cast<char **>(ptr_fmt);
        const char *type = lua_typename(L, ptr_type);
        lua_pushfstring(L, fmt, type);
        return 1;
      }

      return 0;  // raise an error
    };

    lua_pushcfunction(L_, push);
    lua_pushlightuserdata(L_, &_fmt);
    lua_pushinteger(L_, _type);
    return lua_pcall(L_, 2, 1, 0) == LUA_OK;
  }
};
RAPIDJSON_NAMESPACE_END
