#pragma once
extern "C" {
  #include <lua.h>
  #include <lualib.h>
  #include <lauxlib.h>
}

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

      Ch *PutBegin() { RAPIDJSON_ASSERT(false); return 0; }
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
    static lua_State *L;
    static void *l_ud; /* Cache allocator state */
    static lua_Alloc l_alloc;
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
        return l_alloc(l_ud, NULL, 0, size);
      return NULL;  // standardize to returning NULL.
    }

    void *Realloc(void *originalPtr, size_t originalSize, size_t newSize) {
      if (l_alloc != nullptr)
        return l_alloc(l_ud, originalPtr, originalSize, newSize);
      return NULL;
    }

    /* When nsize is zero, the allocator must behave like free and then return NULL. */
    static void Free(void *ptr) {
      if (l_alloc != nullptr)
        l_alloc(l_ud, ptr, 0, 0);
    }
  };

  lua_State *LuaAllocator::L = nullptr;
  lua_Alloc LuaAllocator::l_alloc = nullptr;
  void *LuaAllocator::l_ud = nullptr;
}
