#pragma once
#include <string>
#if defined(LUA_INCLUDE_HPP)
  #include <lua.hpp>
#else
extern LUA_RAPIDJSON_LINKAGE {
  #include <lua.h>
}
#endif

/*
** StringStream
**
** Support Constructs StringStream with the first count characters of the
** character array starting with the element pointed by src
**
** ! \note implements Stream concept
*/
namespace rapidjson {
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

      Ch *PutBegin() { RAPIDJSON_ASSERT(false); return nullptr; }
      void Put(Ch) { RAPIDJSON_ASSERT(false); }
      void Flush() { RAPIDJSON_ASSERT(false); }
      size_t PutEnd(Ch *) { RAPIDJSON_ASSERT(false); return 0; }

      const Ch *src_; //!< Current read position.
      const Ch *head_; //!< Original head of the string.
      size_t count_; //!< Original head of the string.
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
    static thread_local lua_State *L;
    static thread_local void *l_ud; /* Cache allocator state */
    static thread_local lua_Alloc l_alloc;
    bool _singleton = false;

public:
    static const bool kNeedFree = true;

    LuaAllocator() = default;

    /// <summary>
    /// Update the singleton fields with the allocator fields of the given Lua
    /// state.
    /// </summary>
    LuaAllocator(lua_State *_L)
      : _singleton(true) {
      L = _L;
      l_alloc = lua_getallocf(L, &l_ud);
    }

    ~LuaAllocator() {
      if (_singleton) {
        l_ud = nullptr;
        l_alloc = nullptr;
      }
    }

    void *Malloc(size_t size) {
      if (l_alloc != nullptr && size)  //  behavior of malloc(0) is implementation defined.
        return l_alloc(l_ud, nullptr, 0, size);
      return nullptr;  // standardize to returning NULL.
    }

    void *Realloc(void *originalPtr, size_t originalSize, size_t newSize) {
      if (l_alloc != nullptr)
        return l_alloc(l_ud, originalPtr, originalSize, newSize);
      return nullptr;
    }

    /* When nsize is zero, the allocator must behave like free and then return NULL. */
    static void Free(void *ptr) {
      if (l_alloc != nullptr)
        l_alloc(l_ud, ptr, 0, 0);
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
  ///
  /// TODO: Consider placing the exception message on top of the Lua stack,
  /// ensuring that fields (i.e., strings) in the exception do not leak.
  /// </summary>
  class LuaException : public std::exception {
private:
    std::string _msg;
    LuaException &operator=(const LuaException &other);  // prevent

public:
    LuaException(const char* s) : _msg(s) { }
    LuaException(const std::string& s) : _msg(s) { }
    LuaException(const LuaException &other) : _msg(other._msg) { }

    const char *what() const noexcept override {
      return _msg.c_str();
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

    int pushError(lua_State *L) const {
      switch (_errorcode) {
        case UnsupportedType:
          lua_pushfstring(L, "type '%s' is not supported by JSON\n", lua_typename(L, _luatype));
          break;
        case UnsupportedKeyOrder:
          lua_pushfstring(L, "type '%s' is not supported as a keyorder by JSON\n", lua_typename(L, _luatype));
          break;
        default:
          lua_pushstring(L, "LuaTypeException");
          break;
      }
      return 1;
    }

    const char *what() const noexcept override {
      return "LuaTypeException";
    }
  };

  thread_local lua_State *LuaAllocator::L = nullptr;
  thread_local lua_Alloc LuaAllocator::l_alloc = nullptr;
  thread_local void *LuaAllocator::l_ud = nullptr;
}
