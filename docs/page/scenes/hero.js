// Home hero: each API call lights up, then shows what it builds.
export const HERO = `local scene = vis.scene {
    width = 960, height = 540, theme = "pro_black",
    camera = {mode = "fixed", view = "2d", height = 6},
}
local ACC, DIM = "#ff7a45", "#3a4152"
local calls = {"scene:circle", "scene:route", "axes:plot", "scene:tex", "scene:morph"}
local tags, list = {}, scene:group {}
for i, name in ipairs(calls) do
    tags[i] = list:text {text = name, point = {-4.7, 1.8 - (i - 1) * 0.9}, align = {0, 0.5},
                          size = 22, font = "JetBrainsMono", color = DIM}
end
local cursor = scene:rectangle {center = {-4.95, 1.8}, size = {0.08, 0.5}, fill = ACC, stroke = ACC, width = 0}
local fast = {duration = 0.35, curve = "smooth"}
local function pick(i)
    if i > 1 then scene:fill(tags[i - 1], DIM, 0.3) end
    scene:play({target = cursor, shift = {0, i > 1 and -0.9 or 0}}, 0.3, "smooth")
    scene:fill(tags[i], "#e6e8ee", 0.3)
end
-- The loop ends with the list shown and dim, so it starts that way too.
scene:wait(0.5)

-- circle
pick(1)
local c = scene:circle {center = {2, 0}, radius = 1.4, fill = "#ff7a4522", stroke = ACC, width = 3}
scene:grow_from_center(c, {duration = 0.6})
scene:wait(0.4)
scene:fade_out(c, {duration = 0.3})

-- route
pick(2)
local r = scene:route {points = {{-0.2, -1.4}, {1.2, 1.2}, {2.8, -0.6}, {4.3, 1.4}}, stroke = ACC, width = 3, tip = 12}
scene:create(r, {duration = 0.8, curve = "smooth"})
scene:wait(0.4)
scene:fade_out(r, {duration = 0.3})

-- plot
pick(3)
local stage = scene:group {matrix = {1,0,0,2, 0,1,0,0, 0,0,1,0, 0,0,0,1}}
local axes = stage:space {x = {-2.6, 2.6, 1}, y = {-1.6, 1.6, 1}, stroke = "#00000000"}
local wave = axes:plot {fn = function(x) return math.sin(2 * x) * 1.2 end, x_range = {-2.6, 2.6, 0.02}, stroke = ACC, width = 3}
scene:create(wave, {duration = 0.9, curve = "linear"})
scene:wait(0.4)
scene:fade_out(stage, {duration = 0.3})

-- tex
pick(4)
local eq = scene:tex {tex = "e^{i\\\\pi} + 1 = 0", point = {2, 0}, size = 44, color = "#e6e8ee"}
scene:write(eq, {duration = 0.8})
scene:indicate(eq, {duration = 0.6})
scene:fade_out(eq, {duration = 0.3})

-- morph
pick(5)
local a = scene:circle {center = {2, 0}, radius = 1.3, fill = "#ff7a4522", stroke = ACC, width = 3}
scene:fade_in(a, {duration = 0.3})
local b = scene:polygon {points = {{2, 1.5}, {3.5, -1.1}, {0.5, -1.1}}, fill = "#ff7a4522", stroke = ACC, width = 3}
scene:morph(a, b, {duration = 0.9, curve = "smooth"})
scene:wait(0.5)
scene:fade_out(b, {duration = 0.3})
scene:fill(tags[5], DIM, 0.3)
scene:play({target = cursor, shift = {0, 3.6}}, 0.4, "smooth")
return scene`;

export const HERO_JS = `const scene = vis.scene({
    width: 960, height: 540, theme: "pro_black",
    camera: {mode: "fixed", view: "2d", height: 6},
});
const ACC = "#ff7a45", DIM = "#3a4152";
const calls = ["scene:circle", "scene:route", "axes:plot", "scene:tex", "scene:morph"];
const list = scene.group({});
const tags = calls.map((name, i) => list.text({text: name, point: [-4.7, 1.8 - i * 0.9], align: [0, 0.5],
                                              size: 22, font: "JetBrainsMono", color: DIM}));
const cursor = scene.rectangle({center: [-4.95, 1.8], size: [0.08, 0.5], fill: ACC, stroke: ACC, width: 0});
const pick = (i) => {
    if (i > 0) scene.fill(tags[i - 1], DIM, 0.3);
    scene.play({target: cursor, shift: [0, i > 0 ? -0.9 : 0]}, 0.3, "smooth");
    scene.fill(tags[i], "#e6e8ee", 0.3);
};
// The loop ends with the list shown and dim, so it starts that way too.
scene.wait(0.5);

// circle
pick(0);
const c = scene.circle({center: [2, 0], radius: 1.4, fill: "#ff7a4522", stroke: ACC, width: 3});
scene.growFromCenter(c, {duration: 0.6});
scene.wait(0.4);
scene.fadeOut(c, {duration: 0.3});

// route
pick(1);
const r = scene.route({points: [[-0.2, -1.4], [1.2, 1.2], [2.8, -0.6], [4.3, 1.4]], stroke: ACC, width: 3, tip: 12});
scene.create(r, {duration: 0.8, curve: "smooth"});
scene.wait(0.4);
scene.fadeOut(r, {duration: 0.3});

// plot
pick(2);
const stage = scene.group({matrix: [1,0,0,2, 0,1,0,0, 0,0,1,0, 0,0,0,1]});
const axes = stage.space({x: [-2.6, 2.6, 1], y: [-1.6, 1.6, 1], stroke: "#00000000"});
const wave = axes.plot({fn: (x) => Math.sin(2 * x) * 1.2, x_range: [-2.6, 2.6, 0.02], stroke: ACC, width: 3});
scene.create(wave, {duration: 0.9, curve: "linear"});
scene.wait(0.4);
scene.fadeOut(stage, {duration: 0.3});

// tex
pick(3);
const eq = scene.tex({tex: "e^{i\\\\pi} + 1 = 0", point: [2, 0], size: 44, color: "#e6e8ee"});
scene.write(eq, {duration: 0.8});
scene.indicate(eq, {duration: 0.6});
scene.fadeOut(eq, {duration: 0.3});

// morph
pick(4);
const a = scene.circle({center: [2, 0], radius: 1.3, fill: "#ff7a4522", stroke: ACC, width: 3});
scene.fadeIn(a, {duration: 0.3});
const b = scene.polygon({points: [[2, 1.5], [3.5, -1.1], [0.5, -1.1]], fill: "#ff7a4522", stroke: ACC, width: 3});
scene.morph(a, b, {duration: 0.9, curve: "smooth"});
scene.wait(0.5);
scene.fadeOut(b, {duration: 0.3});
scene.fill(tags[4], DIM, 0.3);
scene.play({target: cursor, shift: [0, 3.6]}, 0.4, "smooth");
return scene;`;
