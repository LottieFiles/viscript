// README Skills for AI agents animation, with theme-aware cell fills and contrast.
// Source: docs/assets/skills.lua.
export const SKILLS = String.raw`-- README "Skills for AI agents": the example prompt, and the scene an agent makes from it.
local scene = vis.scene {width = 1000, height = 360, fps = 20}

local PROMPT = "Use vis-skills to animate binary search on a sorted array of 16 numbers."
local DATA = {2, 5, 8, 12, 16, 23, 38, 41, 45, 56, 61, 67, 72, 80, 88, 95}
local TARGET = 23

-- Deterministic trace of lo/mid/hi.
local trace, lo, hi = {}, 1, #DATA
while lo <= hi do
  local mid = (lo + hi) // 2
  trace[#trace + 1] = {lo = lo, hi = hi, mid = mid}
  if DATA[mid] == TARGET then break elseif DATA[mid] < TARGET then lo = mid + 1 else hi = mid - 1 end
end

-- Prompt bubble.
local bubble = scene:rectangle {center = {0, 2.0}, size = {15.2, 0.9}, corner = 0.45, fill = "surface", stroke = "border", width = 2, layer = 0}
local prompt = scene:text {text = PROMPT, point = {0, 2.0}, size = 14, role = "code", fill = "foreground", layer = 1, id = "prompt"}

-- Array cells.
local CW, Y = 0.92, -0.2
local x0 = -(#DATA - 1) * CW / 2
local cells, nums, idxs = {}, {}, {}
for i, v in ipairs(DATA) do
  local x = x0 + (i - 1) * CW
  cells[i] = scene:rectangle {center = {x, Y}, size = {CW - 0.1, 0.82}, corner = 0.08, fill = "background", stroke = "border", width = 2, layer = 2, id = "cell" .. i}
  nums[i] = scene:text {text = tostring(v), point = {x, Y}, size = 17, fill = "foreground", layer = 3, id = "num" .. i}
  idxs[i] = scene:text {text = tostring(i - 1), point = {x, Y - 0.7}, size = 11, fill = "muted", layer = 3, id = "idx" .. i}
end
local function cx(i) return x0 + (i - 1) * CW end
local goal = scene:text {text = "target = " .. TARGET, point = {0, -1.75}, size = 16, role = "code", fill = "muted", layer = 3, id = "goal"}
local mid = scene:arrow {from = {cx(trace[1].mid), Y + 1.15}, to = {cx(trace[1].mid), Y + 0.5}, stroke = "warning", tip = 9, width = 3, layer = 4, opacity = 0}
local range = scene:rectangle {center = {(cx(1) + cx(#DATA)) / 2, Y}, size = {cx(#DATA) - cx(1) + CW + 0.08, 1.04}, corner = 0.12,
  stroke = "accent", width = 3, layer = 4, opacity = 0, id = "range"}
local all = {}
for i = 1, #DATA do all[#all + 1] = cells[i]; all[#all + 1] = nums[i]; all[#all + 1] = idxs[i] end
for _, h in ipairs(all) do scene:set({target = h, opacity = 0}) end
scene:set({target = goal, opacity = 0})

-- Timeline: type the prompt, then the agent's scene appears and runs.
scene:wait(0.2)
scene:write(prompt, {duration = 1.6})
scene:wait(0.3)
scene:tween({{target = goal, opacity = 1}, {target = range, opacity = 1}}, {duration = 0.3})
local show = {}
for i, h in ipairs(all) do show[i] = {target = h, opacity = 1} end
scene:tween(show, {duration = 0.5})
scene:tween({target = mid, opacity = 1}, {duration = 0.2})
local plo, phi = 1, #DATA
for k, s in ipairs(trace) do
  if k > 1 then
    local w = cx(s.hi) - cx(s.lo) + CW + 0.08
    local dims = {}
    for i = plo, phi do if i < s.lo or i > s.hi then dims[#dims + 1] = {target = cells[i], opacity = 0.25}; dims[#dims + 1] = {target = nums[i], opacity = 0.25} end end
    dims[#dims + 1] = {target = range, size = {w, 1.04}, position = {(cx(s.lo) + cx(s.hi)) / 2, Y}}
    dims[#dims + 1] = {target = mid, shift = {cx(s.mid) - cx(trace[k - 1].mid), 0}}
    scene:tween(dims, {duration = 0.6, curve = "ease_in_out"})
  end
  scene:tween({{target = cells[s.mid], fill = "warning", stroke = "warning"},
              {target = nums[s.mid], fill = "background"}}, {duration = 0.25})
  scene:wait(0.35)
  if DATA[s.mid] == TARGET then
    scene:tween({{target = cells[s.mid], fill = "success", stroke = "success"}, {target = range, stroke = "success"}}, {duration = 0.3})
  else
    scene:tween({{target = cells[s.mid], fill = "background", stroke = "border"}, {target = nums[s.mid], fill = "foreground"}}, {duration = 0.2})
  end
  plo, phi = s.lo, s.hi
end
scene:wait(1.4)
return scene`;
