lua_assert = assert
rapidjson = require('rapidjson')
describe = require('test/describe')
assert = require('test/elegantassert')

assert.jsonnil = rapidjson.null

get_file_content = function(file)
    local f = lua_assert(io.open(file, "rb"))
    local content = f:read("*all")
    f:close()
    return content
end

--[[ Compatibility Load --]]
rapidjson.load = function(file)
    return rapidjson.decode(get_file_content(file))
end

--[[ Compatibility Dump --]]
rapidjson.dump = function(json, output, ...)
    local f = io.open(output, "w")
    if f then
        f:write(rapidjson.encode(json, ...))
        f:close()
    else
        error("Invalid file for writing")
    end
end

for i=1,#arg do
    local f = loadfile(arg[i])
    if f then
        f()
    end
end