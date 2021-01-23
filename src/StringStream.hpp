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
      lua_Alloc alloc = nullptr;  // Allocation function;
      void *ud = nullptr;  // Allocator userdata;
      size_t size = 0;  // Size of segment

      lua_AllocHeader() = default;
      lua_AllocHeader(lua_State *L) {
        alloc = lua_getallocf(L, &ud);
      }
    };

    lua_State *L;
    lua_AllocHeader cache;

public:
    static const bool kNeedFree = true;

    LuaAllocator() : L(nullptr) { }
    LuaAllocator(lua_State *_L) : L(_L), cache(L) { }
    ~LuaAllocator() {
    }

    /// <summary>
    /// Have the Lua allocator create a block of "size + N" bytes with the
    /// lua_AllocHeader padded to the front of the block.
    /// </summary>
    void *Realloc(void *originalPtr, size_t originalSize, size_t newSize) {
      lua_AllocHeader header;  // Active allocation header
      void *result = nullptr;  // Result

      void *origSegment = nullptr;  // If the original pointer is non-null, assume it has a packed header
      if (originalPtr != nullptr) {
        origSegment = reinterpret_cast<void *>(reinterpret_cast<char *>(originalPtr) - sizeof(lua_AllocHeader));
        header = *reinterpret_cast<lua_AllocHeader *>(origSegment);
      }
      else {
        if (L == nullptr)  // @TODO: Throw an exception; invalid Lua state and no allocator exists.
          return nullptr;

        header.size = 0;
        header.ud = cache.ud;
        header.alloc = cache.alloc;
      }

      // Per Lua: When nsize is zero, the allocator must behave like free and then return NULL.
      const size_t newSegSize = newSize > 0 ? (newSize + sizeof(lua_AllocHeader)) : 0;
      if (origSegment != nullptr || newSegSize != 0) {
        void *newSegment = header.alloc(header.ud, origSegment, header.size, newSegSize);
        if (newSegment != nullptr) {
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
      return Realloc(nullptr, 0, size);
    }

    static RAPIDJSON_FORCEINLINE void Free(void *ptr) {
      if (ptr != nullptr) {
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
  ///
  /// @TODO: Consider placing the exception message on top of the Lua stack,
  /// ensuring that fields (i.e., strings) in the exception do not leak.
  /// </summary>
  class LuaException : public std::exception {
private:
    std::string _msg;
    LuaException &operator=(const LuaException &other);  // prevent

public:
    LuaException(const char *s) : _msg(s) { }
    LuaException(const std::string &s) : _msg(s) { }
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
}
