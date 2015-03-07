local function setup()
  config = require("config")
  retries = config.ap_connect_retries
  
  wifi.setmode(wifi.STATION)
  wifi.sta.config(config.SSID, config.password)

  -- Setup startup timer
  tmr.alarm(0, 1000, 1, function()
    if wifi.sta.getip() == nil then
      local status = "UNKNOWN"
      local st = wifi.sta.status()
      
      if st == 0 then status = "IDLE"           end
      if st == 1 then status = "CONNECTING"     end
      if st == 2 then status = "WRONG_PASSWORD" end
      if st == 3 then status = "NO_AP_FOUND"    end
      if st == 4 then status = "CONNECT_FAIL"   end
      if st == 5 then status = "GOT_IP"         end
      
      print("Connecting to AP: " .. config.SSID .. " : " .. status .. " | " .. retries)
      
      retries = retries - 1
      if retries == 0 then
        print("Restarting in AP mode!")
        
        wifi.setmode(wifi.SOFTAP)
      
        tmr.stop(0)
        fs = file_server.start(config.file_server_port, config.client_timeout)
          
        user_scripts = require("user_script")
        if user_scripts ~= nil and user_scripts ~= false then
          user_scripts.run()
        end 
      end
   else
      print('Connected: ', wifi.sta.getip())
      tmr.stop(0)

      fs = file_server.start(config.file_server_port, config.client_timeout)

      user_scripts = require("user_script")
      if user_scripts ~= nil and user_scripts ~= false then
        user_scripts.run()
      end 
   end
  end)
end

setup()
