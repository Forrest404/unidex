-- The example from docs/APPS.md: a counter that remembers its count.
local count = tonumber(unidex.data.read("count") or "0")

function draw()
  unidex.header("Hello")
  unidex.centered(80, tostring(count), "large")
  unidex.centered(120, count == 1 and "press" or "presses")
  unidex.hints("", "add", "reset")
end

function onB()
  count = count + 1
  unidex.data.write("count", tostring(count))
  print("count is now", count)  -- shows in the emulator's log (and on USB with the test firmware)
  return "partial"
end

function onBHold()
  count = 0
  unidex.data.write("count", "0")
  return "partial"
end

function status()
  return count .. " presses"
end
