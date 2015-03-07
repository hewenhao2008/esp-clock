-- ***************************************************************************
-- User script - performs user - implemented actions
-- ***************************************************************************

local modname = ...
local M = {}
_G[modname] = M

t = nil
lpd = nil
sensor = nil
d = nil

local function scroll_finish()
  if sensor ~= nil then
    t:set_text("CO2=", 32, 128, 16)
    t:add_text(sensor.getCO2() .. " ", 128, 16, 32)
    
    if d ~= nil then
      local temp, humid = d:read()
      if temp ~= nil then
        t:add_text("t=", 32, 128, 16)
        t:add_text(string.format("%.1f", temp) .. " ", 128, 16, 32)
        t:add_text("h=", 32, 128, 16)
        t:add_text(string.format("%.1f", humid), 128, 16, 32)
      end
    end
  else
    t:set_text("XXXX", 266, 128, 16)
  end
end

function M.run()
  lpd = lpd8806.setup(6, 5, 60)
  t = ticker.setup(lpd, 150, 60, scroll_finish)
  ip = wifi.sta.getip()
  t:add_text(ip:sub(#ip - 2), 128, 32, 255)
  ip = nil
  
  sensor = require("K30")
  d = dht.setup(4, dht.DHT22)
end

return M
