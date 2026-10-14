-- A badge plugin: Badge calls badge() each time this badge is shown, so it is always today's date.
local DAYS = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" }
local MONTHS = { "January", "February", "March", "April", "May", "June", "July", "August", "September",
                 "October", "November", "December" }

function badge()
  unidex.rect(6, 6, 188, 188)
  unidex.rect(8, 8, 184, 184)
  local t = unidex.time()
  if not t.valid then
    unidex.centered(96, "Clock not set", "bold")
    unidex.centered(120, "Settings > Date & time")
    return
  end
  unidex.centered(56, DAYS[t.wday], "medium")
  unidex.fill(40, 70, 120, 64)
  unidex.centered(116, tostring(t.day), "large", "white")
  unidex.centered(164, MONTHS[t.month] .. " " .. t.year, "medium")
end
