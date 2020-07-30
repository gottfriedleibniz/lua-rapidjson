local lua_assert = assert  -- elegant!
local deepcompare = nil
local elegant = nil
local table_unpack = table.unpack or unpack

elegant = {
    jsonnil = nil,

    equal = function(a, b)
        if type(a) == "nil" or type(b) == "nil" then
            return (a == nil or a == elegant.jsonnil)
                and (b == nil or b == elegant.jsonnil)
        end
        return a == b
    end,

    has_error = function(f)
        local has = false
        local status = xpcall(f, function(err)
            has = true
        end)
        return has
    end,

    has = {
        errors = function(f)
            local has = false
            local status = xpcall(f, function(err)
                has = true
            end)
            return has
        end,
    },

    are = {
        equal = function(a, b)
            lua_assert(elegant.equal(a, b), "elegant!")
        end,

        same = function(a, b)
            if type(a) == 'table' and type(b) == 'table' then
                return deepcompare(a, b, true)
            end
            return elegant.equal(a, b)
        end,

        has_error = function(f)
            return elegant.has_error(f)
        end
    },

    are_not = setmetatable({ }, {
        __index = function(self, key)
            local f = elegant[key]
            return function(...)
                local args = { ... }
                local result = f(table_unpack(args))
                return not result
            end
        end,
    }),
}

--[[ Source: https://github.com/Olivine-Labs/luassert --]]
deepcompare = function(t1,t2,ignore_mt,cycles,thresh1,thresh2)
    local ty1 = type(t1)
    local ty2 = type(t2)
    -- non-table types can be directly compared
    if ty1 ~= 'table' or ty2 ~= 'table' then
        return elegant.equal(t1, t2)
    end

    local mt1 = debug.getmetatable(t1)
    local mt2 = debug.getmetatable(t2)
    if mt1 and mt1 == mt2 and mt1.__eq then -- would equality be determined by metatable __eq?
        -- then use that unless asked not to
        if not ignore_mt then
            return elegant.equal(t1, t2)
        end
    else -- we can skip the deep comparison below if t1 and t2 share identity
        if rawequal(t1, t2) then
            return true
        end
    end

    -- handle recursive tables
    cycles = cycles or {{},{}}
    thresh1, thresh2 = (thresh1 or 1), (thresh2 or 1)
    cycles[1][t1] = (cycles[1][t1] or 0)
    cycles[2][t2] = (cycles[2][t2] or 0)
    if cycles[1][t1] == 1 or cycles[2][t2] == 1 then
        thresh1 = cycles[1][t1] + 1
        thresh2 = cycles[2][t2] + 1
    end

    if cycles[1][t1] > thresh1 and cycles[2][t2] > thresh2 then
        return true
    end

    cycles[1][t1] = cycles[1][t1] + 1
    cycles[2][t2] = cycles[2][t2] + 1

    for k1,v1 in next, t1 do
        local v2 = t2[k1]
        if elegant.equal(v1, v2) then -- JSON Nil
            return true
        elseif v2 == nil then
            return false
        end

        local same, crumbs = deepcompare(v1,v2,nil,cycles,thresh1,thresh2)
        if not same then
            crumbs = crumbs or {}
            table.insert(crumbs, k1)
            return false, crumbs
        end
    end

    for k2,_ in next, t2 do
        -- only check whether each element has a t1 counterpart, actual
        -- comparison has been done in first loop above
        if t1[k2] == nil then
            return false, {k2}
        end
    end

    cycles[1][t1] = cycles[1][t1] - 1
    cycles[2][t2] = cycles[2][t2] - 1
    return true
end

return elegant