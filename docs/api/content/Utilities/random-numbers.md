---
title: Deterministic Randomness
id: random-numbers
order: 4
parent: utilities
---

# Deterministic Randomness

Lua starts from seed 0 for every script load; scenes created in that script share its generator. JavaScript generators are independent; equal seeds produce the same sequence as Lua.

## Functions

| Lua | JavaScript | Purpose |
| --- | --- | --- |
| `math.randomseed(seed, seed2?)` | `vis.random(seed, seed2?)` | Seed Lua's generator, or return a new JavaScript generator; a seed is required and seed2 defaults to 0. |
| `math.random()` | `rng()` | Draw a number from 0 inclusive to 1 exclusive. |
| `math.random(upper)` | `rng(upper)` | Draw an integer from 1 through upper, inclusive. |
| `math.random(lower, upper)` | `rng(lower, upper)` | Draw an integer from lower through upper, inclusive. |
| `math.random(0)` | — | Draw a full-width Lua integer; JavaScript rejects this form. |

`rng` is the function returned by `vis.random(...)`. JavaScript bounds and seeds must be safe integers; empty ranges are rejected.

## Examples

### math.randomseed

<!-- api-example: math.randomseed -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

math.randomseed(42)
for i = 1, 6 do
    local value = 0.5 + math.random() * 2
    scene:rectangle {id = "bar-" .. i, center = {i - 3.5, value / 2 - 1.5}, size = {0.7, value}, fill = "accent"}
end

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const rng = vis.random(42);
for (let i = 1; i <= 6; i++) {
    const value = 0.5 + rng() * 2;
    scene.rectangle({id: "bar-" + i, center: [i - 3.5, value / 2 - 1.5], size: [0.7, value], fill: "accent"});
}

return scene;
```

### math.random

<!-- api-example: math.random -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

math.randomseed(42)
for i = 1, 6 do
    local value = 0.5 + math.random() * 2
    scene:rectangle {id = "bar-" .. i, center = {i - 3.5, value / 2 - 1.5}, size = {0.7, value}, fill = "accent"}
end

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const rng = vis.random(42);
for (let i = 1; i <= 6; i++) {
    const value = 0.5 + rng() * 2;
    scene.rectangle({id: "bar-" + i, center: [i - 3.5, value / 2 - 1.5], size: [0.7, value], fill: "accent"});
}

return scene;
```

### math.random.upper

<!-- api-example: math.random.upper -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

math.randomseed(42)
for i = 1, 6 do
    local value = math.random(3)
    scene:rectangle {id = "bar-" .. i, center = {i - 3.5, value / 2 - 1.5}, size = {0.7, value}, fill = "accent"}
end

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const rng = vis.random(42);
for (let i = 1; i <= 6; i++) {
    const value = rng(3);
    scene.rectangle({id: "bar-" + i, center: [i - 3.5, value / 2 - 1.5], size: [0.7, value], fill: "accent"});
}

return scene;
```

### math.random.range

<!-- api-example: math.random.range -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

math.randomseed(42)
for i = 1, 6 do
    local value = math.random(1, 3)
    scene:rectangle {id = "bar-" .. i, center = {i - 3.5, value / 2 - 1.5}, size = {0.7, value}, fill = "accent"}
end

return scene
```

```viscript-js
const scene = vis.scene({
    width: 960,
    height: 540,
});

const rng = vis.random(42);
for (let i = 1; i <= 6; i++) {
    const value = rng(1, 3);
    scene.rectangle({id: "bar-" + i, center: [i - 3.5, value / 2 - 1.5], size: [0.7, value], fill: "accent"});
}

return scene;
```

### math.random.0

<!-- api-example: math.random.0 -->

```viscript
local scene = vis.scene {
    width = 960,
    height = 540,
}

math.randomseed(42)
scene:text {id = "value", text = tostring(math.random(0)), point = {0, 0}, size = 24, fill = "accent"}

return scene
```
