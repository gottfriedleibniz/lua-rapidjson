--luacheck: ignore describe it
describe('rapidjson.null', function()
  local rapidjson = require('rapidjson')

  --[[
      Lua51/LuaJIT requires invoking the function; other Lua versions treat
      sentinel as a 'light' C function, where rapidjson.null == rapidjson.null()
  --]]
  local rapidjson_null = rapidjson.null()

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
