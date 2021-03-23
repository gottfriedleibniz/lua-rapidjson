--[[
    Simplified version of describe() but without the elegant dependencies
--]]
function describe(name, descriptor)
  local errors = {}
  local successes = {}
  local setups = {}
  local teardowns = {}

  function setup(f) setups[#setups + 1] = f end
  function teardown(f) teardowns[#teardowns + 1] = f end

  function it(spec_line, spec)
    for i=1,#setups do setups[i]() end
    local status = xpcall(spec, function (err)
      table.insert(errors, string.format("\t%s\n\t\t%s\n%s\n", spec_line, err, debug.traceback()))
    end)

    if status then
      table.insert(successes, string.format("\t%s\n", spec_line))
    end
    for i=1,#teardowns do teardowns[i]() end
  end

  local status = xpcall(descriptor, function (err)
    table.insert(errors, err)
  end, it)

  print(name)
  if #errors > 0 then
    print('Failures:')
    print(table.concat(errors))
  end

  if #successes > 0 then
    print('Successes:')
    print(table.concat(successes))
  end
end

return describe
