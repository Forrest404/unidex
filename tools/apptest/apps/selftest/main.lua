-- A goes through the tests, B runs one. Each must stop the app with "App error" (or say ok), never crash.
local tests = {
  { "loop forever", function() while true do end end },
  { "use all memory", function() local t = {} for i = 1, 1e9 do t[i] = string.rep("x", 1000) .. i end end },
  { "call nil", function() local f = nil; f() end },
  { "recurse forever", function() local function f(n) return f(n + 1) + 1 end f(1) end },
  { "huge string", function() local s = string.rep("x", 100000000) end },
  { "deep nesting", function()
      local x = ((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((1))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
      return "ok, 100 nested brackets ran"
    end },
  { "data: 70 KB file", function() unidex.data.write("big", string.rep("x", 70000)) end },
  { "data: bad name", function() unidex.data.write("../escape", "x") end },
  { "data: write+read", function()
      unidex.data.write("t.txt", "hello")
      return unidex.data.read("t.txt") == "hello" and "ok, data works" or "data broken"
    end },
  { "http without wifi", function() unidex.http.get("https://example.com/") end },
  { "require missing", function() require("nothere") end },
  { "no io/os/debug", function()
      return (io == nil and os == nil and debug == nil and load == nil and dofile == nil and package == nil)
        and "ok, sandboxed" or "NOT sandboxed"
    end },
}
local at, result = 1, ""

function draw()
  unidex.header("Self test", at .. "/" .. #tests)
  unidex.centered(70, tests[at][1], "bold")
  unidex.wrap(8, 100, 184, result, 3)
  unidex.hints("next", "run", "")
end

function onA()
  at = at % #tests + 1
  result = ""
  return "partial"
end

function onB()
  result = tests[at][2]() or "ok, returned"
  return "partial"
end
