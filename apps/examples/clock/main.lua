-- A big clock: tick() redraws it when the minute changes.
local DAYS = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" }
local MONTHS = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" }
local twelve = unidex.data.read("12h") == "1"
local shown = -1

local function clockText(t)
  if not twelve then return string.format("%02d:%02d", t.hour, t.min) end
  local h = t.hour % 12
  if h == 0 then h = 12 end
  return string.format("%d:%02d %s", h, t.min, t.hour < 12 and "am" or "pm")
end

function draw()
  local t = unidex.time()
  shown = t.min
  unidex.wake_in((60 - t.sec) * 1000)  -- the device may nap: be back for the next minute
  unidex.header("Clock", twelve and "12h" or "24h")
  if not t.valid then
    unidex.centered(80, "Clock not set", "bold")
    unidex.centered(104, "Set it in Settings")
  else
    unidex.centered(88, clockText(t), "large")
    unidex.centered(122, DAYS[t.wday] .. " " .. t.day .. " " .. MONTHS[t.month])
  end
  unidex.hints("", twelve and "24h" or "12h", "")
end

function tick()
  if unidex.time().min ~= shown then return "partial" end
end

function onB()
  twelve = not twelve
  unidex.data.write("12h", twelve and "1" or "0")
  return "partial"
end
