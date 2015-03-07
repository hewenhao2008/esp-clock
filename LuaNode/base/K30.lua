-- ***************************************************************************
-- K-30 ASCII CO2 Sensor module 
-- Measurements are coded in the wollowing pattern: [N], where N is a CO2 value
-- ***************************************************************************

local modname = ...
local M = {}
_G[modname] = M

local co2 = 0

uart.on("data", "]", 
  function(data)
    if data:sub(1,1) == "[" then 
      data = data:sub(2) 
    end

    local index = data:find("]")
    if index ~= nil then 
      co2 = tonumber(data:sub(1, index - 1)) 
    end
  end,
0)

function M.getCO2()
  return co2
end

return M
