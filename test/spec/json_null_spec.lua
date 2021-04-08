--luacheck: ignore describe it
describe('rapidjson.null', function()
  local rapidjson = require('rapidjson')

  --[[
      rapidjson.null is implemented as a 'light' C function for Lua 52, Lua 53,
      and Lua 54, allowing rapidjson.null == rapidjson.null(). For previous
      versions of Lua (Lua51/LuaJIT) the field is a light userdata.
  --]]
  local rapidjson_null = rapidjson.null

  it('should encode as null', function()
    assert.are.equal('null', rapidjson.encode(rapidjson_null))
    assert.are.equal('[null]', rapidjson.encode({rapidjson_null}))
    assert.are.equal('{"a":null}', rapidjson.encode({a=rapidjson_null}))
  end)

  it('should be same as all decoded null', function()
      local default = rapidjson.getoption('null')

      rapidjson.setoption('null', true)
      assert.are.equal(nil, rapidjson.decode('null'))
      assert.are.same({nil}, rapidjson.decode('[null]'))
      assert.are.equal(nil, rapidjson.decode('[null]')[1])
      assert.are.same({a=nil}, rapidjson.decode('{"a":null}'))
      assert.are.equal(nil, rapidjson.decode('{"a":null}').a)

      rapidjson.setoption('null', false)
      assert.are.equal(rapidjson_null, rapidjson.decode('null'))
      assert.are.same({rapidjson_null}, rapidjson.decode('[null]'))
      assert.are.equal(rapidjson_null, rapidjson.decode('[null]')[1])
      assert.are.same({a=rapidjson_null}, rapidjson.decode('{"a":null}'))
      assert.are.equal(rapidjson_null, rapidjson.decode('{"a":null}').a)

      -- Reset default state
      rapidjson.setoption('null', default)
  end)
end)
