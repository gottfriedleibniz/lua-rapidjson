-- @TODO: Rewrite
dir = arg[1] or "build/rapidjson/src/rapidjson/"
times = tonumber(arg[2]) or 10000

local function time(f, times)
    collectgarbage()
    collectgarbage()
    local gettime = os.clock

    local ok, socket = pcall(require, 'socket')
    if ok then
        gettime = socket.gettime
    end

    local start = gettime()
    for _=0,times do f() end
    return gettime() - start
end


local function readfile(file)
    local f = io.open(file)
    if not f then return nil end
    local d = f:read('*a')
    f:close()
    return d
end

local function run_module(d, m, times)
    local name, dec, enc = m[1], m[2], m[3]
    local td = time(function() dec(d) end, times)
    local t = dec(d)
    local te = time(function() enc(t) end, times)
    print(string.format('% 20s % 13.10f % 13.10f', name, td, te))
end

local function profile(jsonfile, times)
    times = times or 10000

    print(jsonfile..': (x'..times..')')
    print('              module  decoding      encoding')

    local d = readfile(jsonfile)
    local modules = { }

    local dk_ok, dkjson = pcall(require, 'dkjson')
    local cjson_ok, cjson = pcall(require, 'cjson')
    local rapid_ok, rapidjson = pcall(require, 'rapidjson')

    if dk_ok then modules[#modules + 1] = {'dkjson', dkjson.decode, dkjson.encode} end
    if cjson_ok then modules[#modules + 1] = {'cjson', cjson.decode, cjson.encode} end
    if rapid_ok then modules[#modules + 1] = {'rapidjson', rapidjson.decode, rapidjson.encode} end

    for _, m in ipairs(modules) do
        run_module(d, m, times)
    end

    if rapid_ok then
        local modes = { "extended", }
        for i=1,#modes do
            local module = { modes[i], rapidjson.decode, rapidjson.encode }
            rapidjson.setoption('decoder_preset', modes[i])
            run_module(d, module, times)
        end
    end
end

local function main()
    profile(dir .. 'bin/types/nulls.json', times)
    profile(dir .. 'bin/types/booleans.json', times)
    profile(dir .. 'bin/types/guids.json', times)
    profile(dir .. 'bin/types/paragraphs.json', times / 100)
    profile(dir .. 'bin/types/floats.json', times)
    profile(dir .. 'bin/types/integers.json', times)
    profile(dir .. 'bin/types/mixed.json', times / 100)
end

local r, m = pcall(main)

if not r then
    print(m)
end

return 0
