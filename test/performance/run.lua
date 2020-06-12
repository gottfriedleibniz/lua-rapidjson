



local function time(f, times)
	collectgarbage()
	local gettime = os.clock

	local ok, socket = pcall(require, 'socket')
	if ok then
		gettime = socket.gettime
	end

	local start = gettime()

	for _=0,times do f() end

	local stop = gettime()

	return stop - start
end


local function readfile(file)
	local f = io.open(file)
	if not f then return nil end
	local d = f:read('*a')
	f:close()
	return d
end


local function profile(jsonfile, times)
	times = times or 10000

	print(jsonfile..': (x'..times..')')
	print('              module  decoding      encoding')
	local d = readfile(jsonfile)

	local rapidjson = require('rapidjson')
	local cjson = require('cjson')
	local dkjson = require('dkjson')

	local function docParse(s)
		local d = rapidjson.Document()
		d:parse(s)
		return d
	end

	local function docStringify(d)
		return d:stringify()
	end

	local modules = {
		{'            dkjson', dkjson.decode, dkjson.encode},
		{'             cjson', cjson.decode, cjson.encode},
		{'         rapidjson', rapidjson.decode, rapidjson.encode},
		{'rapidjson.Document', docParse, docStringify},
	}

	for _, m in ipairs(modules) do
		local name, dec, enc = m[1], m[2], m[3]
		local td = time(function() dec(d) end, times)
		local t = dec(d)
		local te = time(function() enc(t) end, times)
		print(string.format('% 20s % 13.10f % 13.10f', name, td, te))
	end
end

local function main()
	profile('rapidjson/src/rapidjson/bin/types/nulls.json')
	profile('rapidjson/src/rapidjson/bin/types/booleans.json')
	profile('rapidjson/src/rapidjson/bin/types/guids.json')
	profile('rapidjson/src/rapidjson/bin/types/paragraphs.json')
	profile('rapidjson/src/rapidjson/bin/types/floats.json')
	profile('rapidjson/src/rapidjson/bin/types/integers.json')
	profile('rapidjson/src/rapidjson/bin/types/mixed.json')
end

local r, m = pcall(main)

if not r then
	print(m)
end

return 0
