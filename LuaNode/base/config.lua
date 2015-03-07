-- ***************************************************************************
-- Contains node configuration (SSID and password)
-- ***************************************************************************

local modname = ...
local M = {}
_G[modname] = M

M.SSID = "SSID"
M.password = "PASSWORD"
M.client_timeout = 10
M.file_server_port = 20123
M.ap_connect_retries = 30

return M
