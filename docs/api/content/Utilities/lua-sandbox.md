---
title: Lua Formatting & Sandbox
id: lua-sandbox
order: 6
parent: utilities
---

# Lua Formatting & Sandbox

The Lua environment exposes the base, `table`, `string`, `math` and `utf8` libraries, with the restrictions below. JavaScript uses its standard language libraries.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `tostring(value)` | — | Convert nil, booleans, numbers or strings; handle/table/function conversion is rejected. |
| `string.format(format, ...)` | — | Format primitive values; `%p` and non-primitive arguments are rejected. |

Use `ipairs` for arrays and `vis.keys` for other tables. `pairs`, `next`, `print`, `warn`, `require`, `dofile`, `loadfile`, `load`, `pcall`, `xpcall`, `collectgarbage`, `getmetatable`, `setmetatable`, `io`, `os`, `package` and `debug` are unavailable. Pattern functions `string.find`, `string.match`, `string.gmatch` and `string.gsub` are also unavailable.

## Examples

### tostring

<!-- api-example: tostring -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local value = true
scene:text {id = "value", text = "tostring(true) = " .. tostring(value), point = {0, 0}, size = 24, fill = "accent"}

return scene
```

### string.format

<!-- api-example: string.format -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

local value = 3.14159
scene:text {id = "value", text = string.format("pi = %.2f", value), point = {0, 0}, size = 24, fill = "accent"}

return scene
```
