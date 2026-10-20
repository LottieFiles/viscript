# Scenes

A code block with Korean, which the bundled fonts have and the code font lacks:

```viscript loop
local scene = vis.scene {
    width = 640, height = 360, theme = "adaptive",
    camera = {view = "2d", target = {0, 0}, height = 7},
}
local ball = scene:circle {center = {-3, 0}, radius = 0.5, fill = "accent"}
scene:text {text = "안녕하세요, viscript", point = {0, 2.2}, size = 26}
scene:text {text = "local x = 42 -- 코드", point = {0, -2.2}, size = 16, role = "code"}
scene:grow_from_center(ball, 0.6)
scene:shift(ball, {6, 0}, 1.5)
scene:wait(0.5)
return scene
```

An embedded scene file:

![[orbit.lua|480|Two circles]]

A JavaScript scene:

```viscript-js center 520
const scene = vis.scene({width: 640, height: 240, theme: "adaptive", camera: {view: "2d", target: [0, 0], height: 4}});
const dot = scene.circle({center: [-2, 0], radius: 0.4, fill: "accent"});
scene.text({text: "JavaScript", point: [0, 1.3], size: 18});
scene.growFromCenter(dot, 0.5).shift(dot, [4, 0], 1).wait(0.3);
return scene;
```

Fonts the scene names, one in the font folder and one nowhere, and an image beside the note:

```viscript
local scene = vis.scene {
    width = 640, height = 300, theme = "adaptive",
    camera = {view = "2d", target = {0, 0}, height = 6},
}
scene:text {text = "Pretendard Std from the folder", point = {0, 2.2}, size = 20, font = "Pretendard Std"}
scene:text {text = "No Such Sans, drawn in the text font", point = {0, 1.3}, size = 16, font = "No Such Sans"}
local earth = scene:picture {asset = "earth.png", center = {0, -1}, width = 2.6}
scene:fade_in(earth, {duration = 0.8})
return scene
```

Not a scene:

```viscript
print("hello")
```
