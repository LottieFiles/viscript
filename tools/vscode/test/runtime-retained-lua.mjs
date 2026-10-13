import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const codeFont = fs.readFileSync(path.resolve(extensionRoot, "../../src/examples/assets/JetBrainsMono.ttf"));
const runtimeRoot = process.env.VIS_RUNTIME_ROOT
    ? path.resolve(process.env.VIS_RUNTIME_ROOT)
    : path.join(extensionRoot, "runtime");
const {createVis} = await import(pathToFileURL(path.join(runtimeRoot, "client.js")));
const wasmBinary = fs.readFileSync(path.join(runtimeRoot, "vis-wasm.wasm"));
const sourceSerif = fs.readFileSync(path.join(
    extensionRoot, "runtime", "SourceSerif4-Semibold.ttf",
));
const pretendard = fs.readFileSync(path.join(extensionRoot, "runtime", "Pretendard.ttf"));
const koreanSans = fs.readFileSync(path.join(
    extensionRoot, "runtime", "IBMPlexSansKR-SemiBold.ttf",
));

const source = `
local scene = vis.scene {
    width = 320, height = 180,
    camera = {view = "2d", target = {0, 0}, height = 18},
}
local subject = scene:rectangle {
    center = {-2, 0}, size = {2, 2}, fill = "accent", id = "runtime-subject",
}
local marker = scene:circle {
    center = {2, 0}, radius = 0.5, fill = "result", id = "runtime-marker",
}
local x, presses, pointer_x = 0, 0, 0
local runtime = vis.runtime(scene, {
    fixed_step = 0.1,
    max_steps = 4,
    update = function(ctx, dt, elapsed, tick)
        local move = ctx:action("Move")
        local key = ctx:key("D")
        local place = ctx:action("Place")
        if move.pressed and key.pressed then presses = presses + 1 end
        if place.pressed then
            local pointer = ctx:pointer(1)
            if pointer then pointer_x = pointer.position.x / 100 end
        end
        if tick == 1 then
            assert(ctx:sound("test-shot.wav", {gain = 0.5, rate = 1.25}))
        end
        x = x + move.value.x * dt
        ctx:update(subject, {shift = {x, presses, 0}})
        ctx:update(marker, {shift = {pointer_x, 0, 0}})
    end,
})
runtime:bind_key("Move", "D", {1, 0})
runtime:bind_pointer("Place", 0, {1, 0}, 1)
return scene
`;

const center = (box) => ({
    x: box.x + box.width / 2,
    y: box.y + box.height / 2,
});

const pixelAt = (pixels, width, point) => {
    const x = Math.max(0, Math.min(width - 1, Math.floor(point.x)));
    const height = pixels.length / (width * 4);
    const y = Math.max(0, Math.min(height - 1, Math.floor(point.y)));
    const offset = (y * width + x) * 4;
    return pixels.slice(offset, offset + 4);
};

const redPixelCount = (pixels, width, box) => {
    const height = pixels.length / (width * 4);
    const left = Math.max(0, Math.floor(box.x));
    const right = Math.min(width - 1, Math.ceil(box.x + box.width));
    const top = Math.max(0, Math.floor(box.y));
    const bottom = Math.min(height - 1, Math.ceil(box.y + box.height));
    let count = 0;
    for (let y = top; y <= bottom; y += 1) {
        for (let x = left; x <= right; x += 1) {
            const offset = (y * width + x) * 4;
            const red = pixels[offset];
            if (red > pixels[offset + 1] * 1.25
                && red > pixels[offset + 2] * 1.25) count += 1;
        }
    }
    return count;
};

const visibleCenters = (runtime, prefix, count) => {
    const result = [];
    for (let index = 1; index <= count; ++index) {
        try {
            result.push(center(runtime.bounds(`${prefix}${index}`, 0)));
        } catch {
            // Inactive retained pool entries intentionally have no painted bounds.
        }
    }
    return result;
};

const runtime = await createVis(source, "runtime-retained-lua.lua", {wasmBinary});
const event = (type, fields = {}) => runtime.input({
    type,
    x: 0,
    y: 0,
    dx: 0,
    dy: 0,
    wheelX: 0,
    wheelY: 0,
    button: -1,
    buttons: 0,
    modifiers: 0,
    pointerId: 1,
    time: runtime.runtimeTime,
    ...fields,
});

try {
    runtime.font("Source Serif 4", sourceSerif, "ttf");
    runtime.font("Pretendard", pretendard, "ttf");
    runtime.font("JetBrainsMono", codeFont, "ttf");
    runtime.font("IBM Plex Sans KR", koreanSans, "ttf");
    assert.equal(typeof runtime.advanceRuntime, "function");
    assert.equal(runtime.retainedLua, true);
    assert.equal(runtime.runtimeTime, 0);

    const initial = center(runtime.bounds("runtime-subject", 0));
    runtime.beginInputFrame(0);
    assert.equal(event("keydown", {key: "D"}).handled, true);
    const first = runtime.advanceRuntime(0.2);
    assert.equal(first.steps, 2);
    assert.equal(first.tick, 2);
    assert(Math.abs(first.time - 0.2) < 1e-5);
    assert.deepEqual(first.sounds, [{
        asset: "test-shot.wav", bus: "effect", gain: 0.5, rate: 1.25, loop: false,
    }]);
    const moved = center(runtime.bounds("runtime-subject", 0));
    assert(moved.x > initial.x);
    assert(moved.y < initial.y, "Pressed must be visible to the first catch-up step only");

    runtime.beginInputFrame(first.time);
    const held = runtime.advanceRuntime(0.1);
    assert.equal(held.steps, 1);
    assert.deepEqual(held.sounds, []);
    const heldCenter = center(runtime.bounds("runtime-subject", 0));
    assert(heldCenter.x > moved.x);
    assert(Math.abs(heldCenter.y - moved.y) < 0.1);

    runtime.beginInputFrame(held.time);
    assert.equal(event("keyup", {key: "D"}).handled, true);
    runtime.advanceRuntime(0.1);
    const stopped = center(runtime.bounds("runtime-subject", 0));
    assert(Math.abs(stopped.x - heldCenter.x) < 0.1);

    runtime.beginInputFrame(runtime.runtimeTime);
    const pointerDown = event("pointerdown", {
        x: 150, y: 70, button: 0, buttons: 1,
    });
    assert.equal(pointerDown.handled, true);
    assert.equal(pointerDown.capture, true);
    const markerInitial = center(runtime.bounds("runtime-marker", 0));
    const pointerStep = runtime.advanceRuntime(0.1);
    const markerMoved = center(runtime.bounds("runtime-marker", 0));
    assert.equal(pointerStep.steps, 1);
    assert(markerMoved.x > markerInitial.x);
    runtime.beginInputFrame(pointerStep.time);
    const pointerUp = event("pointerup", {x: 150, y: 70, button: 0, buttons: 0});
    assert.equal(pointerUp.handled, true);
    assert.equal(pointerUp.release, true);

    runtime.loadLua("return vis.scene {width = 32, height = 18}", "ordinary.lua");
    assert.equal(runtime.retainedLua, false);
    assert.equal(runtime.runtimeTime, 0);
    assert.equal(runtime.advanceRuntime(0.1), null);

    runtime.loadLua(`
local scene = vis.scene {
    width = 320, height = 180,
    camera = {view = "2d", target = {0, 0}, height = 18},
}
local subject = scene:rectangle {
    center = {0, 0}, size = {2, 2}, id = "atomic-subject",
}
vis.runtime(scene, {
    fixed_step = 0.1,
    update = function(ctx, dt, time, tick)
        ctx:update(subject, {shift = {tick == 2 and 40 or tick, 0, 0}})
        if tick == 2 then scene:circle {center = {0, 0}, radius = 1} end
    end,
})
return scene
`, "runtime-authoring-lock.lua");
    runtime.advanceRuntime(0.1);
    const committed = runtime.bounds("atomic-subject", 0);
    assert.throws(
        () => runtime.advanceRuntime(0.1),
        /scene authoring is unavailable after loading/,
    );
    assert.deepEqual(runtime.bounds("atomic-subject", 0), committed);
    assert.throws(() => runtime.advanceRuntime(0.1), /scene authoring is unavailable after loading/);

    runtime.loadLua(`
local scene = vis.scene {width = 64, height = 36}
local diagram = vis.diagram(scene, {id = "runtime-lock"})
vis.runtime(scene, {
    fixed_step = 0.1,
    update = function() diagram:node {id = "late-node"} end,
})
return scene
`, "runtime-diagram-lock.lua");
    assert.throws(
        () => runtime.advanceRuntime(0.1),
        /diagram authoring is unavailable after loading/,
    );

    const gameSource = fs.readFileSync(path.join(
        extensionRoot, "..", "skill", "vis-game", "templates",
        "retained-lua-2d-game.lua",
    ), "utf8");
    runtime.loadLua(gameSource, "retained-lua-tab-weapons.lua");
    runtime.beginInputFrame(0);
    runtime.advanceRuntime(1 / 60);
    const visibleWeapon = () => {
        const visible = [];
        for (let weapon = 1; weapon <= 5; ++weapon) {
            const icon = center(runtime.bounds(`math-strike:weapon-icon:${weapon}`, 0));
            if (icon.x >= 0 && icon.x <= 720 && icon.y >= 0 && icon.y <= 1280) {
                visible.push(weapon);
            }
        }
        return visible;
    };
    assert.deepEqual(visibleWeapon(), [1]);
    assert.throws(
        () => runtime.bounds("math-strike:card:1:weapon:body", 0),
        /InsufficientCondition/,
        "Random upgrade cards must remain hidden",
    );
    for (const expected of [2, 3, 4, 5, 1]) {
        runtime.beginInputFrame(runtime.runtimeTime);
        assert.equal(event("keydown", {key: "Tab"}).handled, true);
        runtime.advanceRuntime(1 / 60);
        assert.deepEqual(visibleWeapon(), [expected]);
        runtime.beginInputFrame(runtime.runtimeTime);
        assert.equal(event("keyup", {key: "Tab"}).handled, true);
        runtime.advanceRuntime(1 / 60);
    }
    const enemyProjectileRetentionSource = gameSource.replace(
        "local function destroy_enemy_bullet_with_shot(shot, pool)",
        `retain_projectile_power(enemy_shot_pools.ring[1], 0.5)

local function destroy_enemy_bullet_with_shot(shot, pool)`,
    );
    assert.notEqual(enemyProjectileRetentionSource, gameSource,
        "The retention fixture must exercise an actual enemy projectile pool entry");
    runtime.loadLua(enemyProjectileRetentionSource, "retained-lua-enemy-retention.lua");

    const slimEnemyShotSource = gameSource.replace(
        "local runtime = vis.runtime(scene, {",
        `spawn_enemy_shot("ring", -60, 0, 0, 0, 0, 8, 1.84)
spawn_enemy_shot("aim", -20, 0, 0, 0, 0, 8, 1.84)
spawn_enemy_shot("spiral", 20, 0, 0, 0, 0, 8, 1.84)
spawn_enemy_shot("ring", 60, 0, 0, 0, 0, 10.8, 2.30)

local runtime = vis.runtime(scene, {`,
    );
    assert.notEqual(slimEnemyShotSource, gameSource,
        "The hostile-shot fixture must enter the retained game before runtime lock");
    runtime.loadLua(slimEnemyShotSource, "retained-lua-slim-enemy-shot.lua");
    runtime.beginInputFrame(0);
    runtime.advanceRuntime(1 / 60);
    const slimEnemyShot = runtime.bounds("math-strike:enemy-shot:ring:1", 0);
    assert(slimEnemyShot.width >= 12 && slimEnemyShot.width <= 17,
        "A default hostile ring shot must remain a slim 12-17 pixel silhouette");
    const slimAimShot = runtime.bounds("math-strike:enemy-shot:aim:1", 0);
    assert(Math.max(slimAimShot.width, slimAimShot.height) <= 19,
        "An aimed hostile shot must remain below 19 pixels on its longest axis");
    const slimSpiralShot = runtime.bounds("math-strike:enemy-shot:spiral:1", 0);
    assert(Math.max(slimSpiralShot.width, slimSpiralShot.height) <= 18,
        "A spiral hostile shot and tail must remain below 18 pixels");
    const slimHeavyShot = runtime.bounds("math-strike:enemy-shot:ring:2", 0);
    assert(slimHeavyShot.width <= 20,
        "Even a heavy hostile ring shot must remain at most 20 pixels wide");

    const playerDamageSource = gameSource.replace(
        "local runtime = vis.runtime(scene, {",
        `CARD_TYPES.damage_probe = enemy_shot_pools.ring[1]
CARD_TYPES.damage_probe.active = true
player_x, player_y = 91, -321
player_vx, player_vy = 37, -29
charging, charge_time = true, 0.44
hp, hit_invulnerable = 100, 0
damage_player(8)
CARD_TYPES.damage_again = damage_player(8)
scene:circle {
    center = {player_x, player_y}, radius = 1,
    fill = "accent", id = "math-strike:damage:position",
}
scene:circle {
    center = {player_vx, player_vy}, radius = 1,
    fill = "accent", id = "math-strike:damage:velocity",
}
scene:circle {
    center = {hp, -600}, radius = 1,
    fill = "accent", id = "math-strike:damage:hp",
}
scene:circle {
    center = {hit_invulnerable * 100, -620}, radius = 1,
    fill = "accent", id = "math-strike:damage:invulnerability",
}
scene:circle {
    center = {CARD_TYPES.damage_probe.active and 40 or -40, -640}, radius = 1,
    fill = "accent", id = "math-strike:damage:hostile-shot",
}
scene:circle {
    center = {charging and charge_time * 100 or -40, -660}, radius = 1,
    fill = "accent", id = "math-strike:damage:charge",
}
scene:circle {
    center = {CARD_TYPES.damage_again and 40 or -40, -680}, radius = 1,
    fill = "accent", id = "math-strike:damage:repeat",
}

local runtime = vis.runtime(scene, {`,
    );
    assert.notEqual(playerDamageSource, gameSource,
        "The damage fixture must enter the retained game before runtime lock");
    runtime.loadLua(playerDamageSource, "retained-lua-player-damage.lua");
    const damagedPosition = center(runtime.bounds("math-strike:damage:position", 0));
    const damagedVelocity = center(runtime.bounds("math-strike:damage:velocity", 0));
    assert(Math.abs(damagedPosition.x - 451) < 0.1
        && Math.abs(damagedPosition.y - 961) < 0.1,
    "Taking damage must preserve the player's current position");
    assert(Math.abs(damagedVelocity.x - 397) < 0.1
        && Math.abs(damagedVelocity.y - 669) < 0.1,
    "Taking damage must preserve the player's inertial velocity");
    assert(Math.abs(center(runtime.bounds("math-strike:damage:hp", 0)).x - 452) < 0.1,
        "Taking 8 damage at 100 HP must leave exactly 92 HP");
    assert(Math.abs(
        center(runtime.bounds("math-strike:damage:invulnerability", 0)).x - 505,
    ) < 0.1, "Taking damage must start the 1.45-second hit invulnerability window");
    assert(center(runtime.bounds("math-strike:damage:hostile-shot", 0)).x > 360,
        "Taking damage must not clear unrelated hostile projectiles");
    assert(Math.abs(center(runtime.bounds("math-strike:damage:charge", 0)).x - 404) < 0.1,
        "Taking damage must not reset the player's active charge state");
    assert(center(runtime.bounds("math-strike:damage:repeat", 0)).x < 360,
        "The hit-invulnerability window must reject immediate repeated contact damage");

    const projectileBalanceSource = gameSource.replace(
        "local runtime = vis.runtime(scene, {",
        `CARD_TYPES.balance_regular = CARD_TYPES.enemy_projectile_strength(8, 1.84)
CARD_TYPES.balance_heavy = CARD_TYPES.enemy_projectile_strength(10.8, 2.30)
CARD_TYPES.balance_strength = CARD_TYPES.player_projectile_strength or function(value)
    return value
end
CARD_TYPES.balance_short = CARD_TYPES.balance_strength(TUNING.weapon_damage.short_laser)
CARD_TYPES.balance_homing = CARD_TYPES.balance_strength(TUNING.weapon_damage.homing)
CARD_TYPES.balance_pulse = CARD_TYPES.balance_strength(TUNING.weapon_damage.pulse)
scene:circle {
    center = {CARD_TYPES.balance_short > CARD_TYPES.balance_regular and 40 or -40, -600},
    radius = 1, fill = "accent", id = "math-strike:balance:short-regular",
}
scene:circle {
    center = {CARD_TYPES.balance_pulse > CARD_TYPES.balance_regular and 40 or -40, -620},
    radius = 1, fill = "accent", id = "math-strike:balance:pulse-regular",
}
scene:circle {
    center = {CARD_TYPES.balance_short > CARD_TYPES.balance_heavy and 40 or -40, -640},
    radius = 1, fill = "accent", id = "math-strike:balance:short-heavy",
}
scene:circle {
    center = {CARD_TYPES.balance_homing < CARD_TYPES.balance_regular
        and CARD_TYPES.balance_homing * 2 > CARD_TYPES.balance_regular and 40 or -40,
        -660},
    radius = 1, fill = "accent", id = "math-strike:balance:homing-regular",
}

local runtime = vis.runtime(scene, {`,
    );
    assert.notEqual(projectileBalanceSource, gameSource,
        "The projectile-balance fixture must enter before runtime lock");
    runtime.loadLua(projectileBalanceSource, "retained-lua-projectile-balance.lua");
    for (const id of ["short-regular", "pulse-regular", "short-heavy", "homing-regular"]) {
        assert(center(runtime.bounds(`math-strike:balance:${id}`, 0)).x > 360,
            `${id} must satisfy the declared projectile-cancellation threshold`);
    }

    const projectileClashSource = gameSource.replace(
        "local runtime = vis.runtime(scene, {",
        `CARD_TYPES.clash_fixture = function(shot, strength, damage)
    shot.active = true
    shot.x, shot.y = 0, 0
    shot.weapon_kind = 5
    shot.damage, shot.max_damage = damage, damage
    shot.strength, shot.max_strength = strength, strength
    shot.radius, shot.spawn_radius = 5, 5
    shot.scale, shot.scale_x, shot.scale_y = 1, 1, 1
    shot.spawn_scale, shot.spawn_scale_x, shot.spawn_scale_y = 1, 1, 1
end

CARD_TYPES.clash_hostile = enemy_shot_pools.ring[1]
CARD_TYPES.clash_player = player_shots[44]
CARD_TYPES.clash_fixture(
    CARD_TYPES.clash_hostile, CARD_TYPES.enemy_projectile_strength(8.0, 1.0), 8.0
)
CARD_TYPES.clash_fixture(CARD_TYPES.clash_player, 0.4, 0.4)
destroy_enemy_bullet_with_shot(CARD_TYPES.clash_player, enemy_shot_pools.ring)
CARD_TYPES.clash_damage_after_one = CARD_TYPES.clash_hostile.damage
CARD_TYPES.clash_fixture(CARD_TYPES.clash_player, 0.4, 0.4)
destroy_enemy_bullet_with_shot(CARD_TYPES.clash_player, enemy_shot_pools.ring)
CARD_TYPES.clash_fixture(CARD_TYPES.clash_player, 0.4, 0.4)
destroy_enemy_bullet_with_shot(CARD_TYPES.clash_player, enemy_shot_pools.ring)
scene:circle {
    center = {CARD_TYPES.clash_hostile.active and 40 or -40, -600}, radius = 1,
    fill = "accent", id = "math-strike:clash:hostile-active",
}
scene:circle {
    center = {CARD_TYPES.clash_player.active and 40 or -40, -620}, radius = 1,
    fill = "accent", id = "math-strike:clash:player-active",
}
scene:circle {
    center = {(CARD_TYPES.clash_player.strength or -1) * 100, -640}, radius = 1,
    fill = "accent", id = "math-strike:clash:player-strength",
}
scene:circle {
    center = {CARD_TYPES.clash_damage_after_one * 10, -580}, radius = 1,
    fill = "accent", id = "math-strike:clash:hostile-damage",
}

local runtime = vis.runtime(scene, {`,
    );
    assert.notEqual(projectileClashSource, gameSource,
        "The projectile clash fixture must enter the retained game before runtime lock");
    runtime.loadLua(projectileClashSource, "retained-lua-projectile-clash.lua");
    assert(center(runtime.bounds("math-strike:clash:hostile-active", 0)).x < 360,
        "Three weak player bullets must cumulatively destroy one stronger hostile bullet");
    assert(center(runtime.bounds("math-strike:clash:player-active", 0)).x > 360,
        "The final weak player bullet must survive with the exact residual strength");
    assert(Math.abs(
        center(runtime.bounds("math-strike:clash:player-strength", 0)).x - 380,
    ) < 0.1, "The surviving player bullet must retain strength 0.2");
    assert(Math.abs(
        center(runtime.bounds("math-strike:clash:hostile-damage", 0)).x - 408,
    ) < 0.1, "A hostile bullet at 60% residual strength must retain 60% of 8 HP damage");

    runtime.loadLua(gameSource, "retained-lua-2d-game.lua");
    assert.equal(runtime.retainedLua, true);
    assert.deepEqual(runtime.size(), [720, 1280]);
    const playerInitial = center(runtime.bounds("math-strike:player", 0));
    const enemyInitial = center(runtime.bounds("math-strike:enemy:1", 0));
    runtime.beginInputFrame(0);
    assert.equal(event("keydown", {key: "D", time: 0}).handled, true);
    let gameStep = runtime.advanceRuntime(1 / 60);
    assert.equal(gameStep.steps, 2);
    for (let frame = 1; frame < 12; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        gameStep = runtime.advanceRuntime(1 / 60);
        assert.equal(gameStep.steps, 2);
    }
    const playerWarm = center(runtime.bounds("math-strike:player", 0));
    const gridBefore = center(runtime.bounds("math-strike:grid:1", 0));
    const highlightedCell = runtime.bounds("math-strike:grid:1:cell:1", 0);
    assert.equal(Math.round(highlightedCell.width), 55,
        "A 54-pixel highlighted cell must retain only its Pro White stroke fringe");
    assert.equal(Math.round(highlightedCell.height), 55,
        "A 54-pixel highlighted cell must retain only its Pro White stroke fringe");
    assert(playerWarm.x > playerInitial.x);
    for (const [labelId, indicatorId] of [
        ["math-strike:threat-label", "math-strike:threat:2"],
        ["math-strike:weapon-label", "math-strike:weapon:1"],
        ["math-strike:power-label", "math-strike:power:1"],
        ["math-strike:round-label", "math-strike:round:2"],
    ]) {
        const label = runtime.bounds(labelId, 0);
        const indicator = runtime.bounds(indicatorId, 0);
        assert(label.x + label.width + 4 <= indicator.x,
            `${labelId} must keep a four-pixel gap from its first status indicator`);
    }
    for (const id of [
        "math-strike:life:1:digit:1",
        "math-strike:score:3:digit:0",
        "math-strike:breach:2:digit:0",
        "math-strike:threat:2:digit:3",
        "math-strike:power:1:digit:1",
        "math-strike:round:2:digit:1",
    ]) {
        const value = center(runtime.bounds(id, 0));
        assert(value.y > 0 && value.y < 1280,
            `${id} must expose the current HUD value as a readable number`);
    }
    for (let frame = 0; frame < 12; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    const playerBase = center(runtime.bounds("math-strike:player", 0));
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("keydown", {key: "Shift"}).handled, true);
    assert.equal(runtime.keyState("Shift").down, true);
    runtime.advanceRuntime(1 / 60);
    for (let frame = 1; frame < 12; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    const playerBoosted = center(runtime.bounds("math-strike:player", 0));
    const gridAfter = center(runtime.bounds("math-strike:grid:1", 0));
    const baseDistance = playerBase.x - playerWarm.x;
    const boostedDistance = playerBoosted.x - playerBase.x;
    assert(boostedDistance > baseDistance * 1.12,
        "Held Shift must raise movement speed above the settled base-speed segment");
    assert(gridAfter.y > gridBefore.y + 20
        && Math.abs(gridAfter.x - gridBefore.x) < 0.1,
    "The flat grid must stream straight downward without lateral drift");

    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("keyup", {key: "D"}).handled, true);
    assert.equal(event("keyup", {key: "Shift"}).handled, true);
    runtime.advanceRuntime(1 / 60);
    for (let frame = 0; frame < 12; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("keydown", {key: "X"}).handled, false,
        "Keyboard fire must remain unbound; combat fire is pointer-only");
    assert.equal(event("pointermove", {x: 700, y: 250, pointerId: 1}).handled, false);
    assert.equal(event("pointerdown", {
        x: 700, y: 250, button: 0, buttons: 1, pointerId: 1,
    }).handled, true);
    runtime.advanceRuntime(1 / 60);
    const aimGuide = runtime.bounds("math-strike:aim-guide", 0);
    const aimingPlayer = center(runtime.bounds("math-strike:player", 0));
    assert(aimGuide.x <= aimingPlayer.x + 6
        && aimGuide.x + aimGuide.width > 670
        && aimGuide.y < aimingPlayer.y - 250,
    "The pointer aim guide must begin at the player and extend to the field edge");
    assert.throws(
        () => runtime.bounds("math-strike:player-shot:1", 0),
        /InsufficientCondition/,
        "Pointer press must begin charge without firing before release",
    );
    const tapCharge = runtime.bounds("math-strike:charge", 0);
    const fieldBeforeTap = center(runtime.bounds("math-strike:field", 0));
    const playerBeforeTapRecoil = center(runtime.bounds("math-strike:player", 0));
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("pointerup", {
        x: 700, y: 250, button: 0, buttons: 0, pointerId: 1,
    }).handled, true);
    const tapFireStep = runtime.advanceRuntime(1 / 60);
    assert(tapFireStep.sounds.some(
        (sound) => sound.asset === "../assets/audio/short-laser.wav"
            && sound.bus === "effect",
    ), "Short Laser release must emit its retained Audio-independent sound event");
    const firedShot = runtime.bounds("math-strike:player-shot:1", 0);
    const firingPlayer = center(runtime.bounds("math-strike:player", 0));
    assert(center(firedShot).x > firingPlayer.x && center(firedShot).y < firingPlayer.y,
        "A completed click must fire toward the current pointer bearing");
    assert(Math.hypot(firedShot.width, firedShot.height) > 35,
        "Laser weapon one must emit one visibly elongated beam");
    const fieldAfterTap = center(runtime.bounds("math-strike:field", 0));
    assert(Math.hypot(
        fieldAfterTap.x - fieldBeforeTap.x,
        fieldAfterTap.y - fieldBeforeTap.y,
    ) > 0.45, "Every shot must trigger the tuned 3x battlefield recoil wiggle");
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("pointerdown", {
        x: 700, y: 250, button: 0, buttons: 1, pointerId: 1,
    }).handled, true);
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("pointerup", {
        x: 700, y: 250, button: 0, buttons: 0, pointerId: 1,
    }).handled, true);
    runtime.advanceRuntime(1 / 60);
    assert.throws(
        () => runtime.bounds("math-strike:player-shot:2", 0),
        /InsufficientCondition/,
        "A press made during recoil recovery must not start or release another shot",
    );
    for (let frame = 0; frame < 10; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    const playerAfterTapRecoil = center(runtime.bounds("math-strike:player", 0));
    const aimScreenX = 700 - playerBeforeTapRecoil.x;
    const aimScreenY = 250 - playerBeforeTapRecoil.y;
    const aimScreenLength = Math.hypot(aimScreenX, aimScreenY);
    const recoilProjection = (
        (playerAfterTapRecoil.x - playerBeforeTapRecoil.x) * aimScreenX
        + (playerAfterTapRecoil.y - playerBeforeTapRecoil.y) * aimScreenY
    ) / aimScreenLength;
    assert(recoilProjection < -3,
        "Directional fire must push the player with tuned 3x recoil opposite the pointer bearing");

    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("pointerdown", {
        x: 700, y: 250, button: 0, buttons: 1, pointerId: 1,
    }).handled, true);
    runtime.advanceRuntime(1 / 60);
    for (let frame = 1; frame < 70; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    assert.throws(
        () => runtime.bounds("math-strike:player-shot:2", 0),
        /InsufficientCondition/,
        "Held fire must charge without auto-repeat shots",
    );
    const heldCharge = runtime.bounds("math-strike:charge", 0);
    assert(heldCharge.width > tapCharge.width * 4,
        "Held pointer fire must grow the visible one-to-five-times charge telegraph");
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("pointerup", {
        x: 700, y: 250, button: 0, buttons: 0, pointerId: 1,
    }).handled, true);
    runtime.advanceRuntime(1 / 60);
    const chargedShot = runtime.bounds("math-strike:player-shot:2", 0);
    assert(chargedShot.width * chargedShot.height
        > firedShot.width * firedShot.height * 1.20,
        "Charge release must preserve the laser while materially increasing its beam thickness");
    const chargedCore = runtime.bounds("math-strike:player-shot:2:laser-heat:core", 0);
    const chargedPixels = runtime.render(0, true);
    assert(redPixelCount(chargedPixels, 720, chargedCore) >= 6,
        "A charged projectile must retain a visibly red heat state after release");
    const chargedGlow = runtime.bounds("math-strike:player-shot:2:laser-glow", 0);
    const chargedCenter = center(chargedShot);
    const chargedPlayer = center(runtime.bounds("math-strike:player", 0));
    const chargedDirectionLength = Math.hypot(
        chargedCenter.x - chargedPlayer.x,
        chargedCenter.y - chargedPlayer.y,
    );
    const chargedDirection = {
        x: (chargedCenter.x - chargedPlayer.x) / chargedDirectionLength,
        y: (chargedCenter.y - chargedPlayer.y) / chargedDirectionLength,
    };
    const glowRadius = Math.hypot(chargedGlow.width, chargedGlow.height) * 0.42;
    const glowColor = pixelAt(chargedPixels, 720, {
        x: chargedCenter.x + chargedDirection.x * glowRadius,
        y: chargedCenter.y + chargedDirection.y * glowRadius,
    });
    assert(glowColor[0] > glowColor[1] * 1.15
        && glowColor[0] > glowColor[2] * 1.05,
    "Charged Short Laser glow must turn red together with its core");

    const beforeDash = center(runtime.bounds("math-strike:player", 0));
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("keydown", {key: "Space"}).handled, true);
    runtime.advanceRuntime(1 / 60);
    for (let frame = 1; frame < 6; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    const afterDash = center(runtime.bounds("math-strike:player", 0));
    assert(afterDash.y < beforeDash.y - 45,
        "Space must perform a forward invulnerability dash, not a passive speed modifier");
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("keyup", {key: "Space"}).handled, true);
    runtime.advanceRuntime(1 / 60);

    const [ringA, ringB] = visibleCenters(
        runtime, "math-strike:enemy-shot:ring:", 36,
    );
    assert(ringA && ringB, "The ring pool must expose at least two active samples");
    assert(Math.hypot(ringA.x - ringB.x, ringA.y - ringB.y) > 20,
        "The radial pattern must place pooled shots at distinct angular samples");
    const [aimA, aimB] = visibleCenters(
        runtime, "math-strike:enemy-shot:aim:", 30,
    );
    assert(aimA && aimB, "The aimed pool must expose at least two active samples");
    assert(Math.hypot(aimA.x - aimB.x, aimA.y - aimB.y) > 1,
        "The aimed fan must sample distinct offsets around the player bearing");
    const [spiralA, spiralB] = visibleCenters(
        runtime, "math-strike:enemy-shot:spiral:", 30,
    );
    assert(spiralA && spiralB, "The spiral pool must expose at least two active samples");
    assert(Math.hypot(spiralA.x - spiralB.x, spiralA.y - spiralB.y) > 12,
        "The golden-angle emitter must produce separate rotating arms");
    const enemyMoved = center(runtime.bounds("math-strike:enemy:1", 0));
    assert(Math.hypot(enemyMoved.x - enemyInitial.x, enemyMoved.y - enemyInitial.y) > 10,
        "Enemy attack states must follow time-varying mathematical paths");

    for (let frame = 0; frame < 180; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    assert.throws(
        () => runtime.bounds("math-strike:player-hot", 0),
        /InsufficientCondition/,
        "Laser overheat must recover before the next charged weapon input is accepted",
    );
    let sentinelTarget = center(runtime.bounds("math-strike:enemy:3", 0));
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("pointerdown", {
        x: sentinelTarget.x, y: sentinelTarget.y,
        button: 0, buttons: 1, pointerId: 1,
    }).handled, true);
    runtime.advanceRuntime(1 / 60);
    for (let frame = 1; frame < 70; ++frame) {
        sentinelTarget = center(runtime.bounds("math-strike:enemy:3", 0));
        runtime.beginInputFrame(runtime.runtimeTime);
        event("pointermove", {
            x: sentinelTarget.x, y: sentinelTarget.y,
            buttons: 1, pointerId: 1,
        });
        runtime.advanceRuntime(1 / 60);
    }
    assert(runtime.bounds("math-strike:charge:ring-hot", 0).width > 160,
        "A complete charge must visibly settle on the full-red ring layer");
    assert(runtime.bounds("math-strike:player-hot", 0).width > 30,
        "A complete charge must apply the same red heat state to the player craft");
    sentinelTarget = center(runtime.bounds("math-strike:enemy:3", 0));
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("pointerup", {
        x: sentinelTarget.x, y: sentinelTarget.y,
        button: 0, buttons: 0, pointerId: 1,
    }).handled, true);
    runtime.advanceRuntime(1 / 60);
    assert.throws(
        () => runtime.bounds("math-strike:charge:ring-hot", 0),
        /InsufficientCondition/,
        "Releasing a charged shot must clear the red charge layer",
    );
    assert.throws(
        () => runtime.bounds("math-strike:player-hot", 0),
        /InsufficientCondition/,
        "A full reactor must restore the player's normal color after charged release",
    );
    const sentinelShot = runtime.bounds("math-strike:player-shot:3", 0);
    const sentinelHeat = runtime.bounds("math-strike:player-shot:3:laser-heat:core", 0);
    assert(Math.hypot(sentinelShot.width, sentinelShot.height) > 35
        && sentinelHeat.width > 4 && sentinelHeat.height > 4,
    "The sentinel-directed release must emit an elongated red charged projectile");

    const enemyDeathSource = gameSource.replace(/\nreturn scene\s*$/, `
CARD_TYPES.mark_enemy_destroyed(enemies[3])
return scene
`);
    assert.notEqual(enemyDeathSource, gameSource,
        "The enemy-death fixture must begin from a deterministic transition");
    runtime.loadLua(enemyDeathSource, "retained-lua-2d-game-enemy-death.lua");
    runtime.beginInputFrame(0);
    runtime.advanceRuntime(1 / 60);

    let deathCrossPeak = 0;
    const transitionSounds = new Set();
    for (let frame = 0; frame < 180; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        const transitionStep = runtime.advanceRuntime(1 / 60);
        for (const sound of transitionStep.sounds) transitionSounds.add(sound.asset);
        for (let impact = 1; impact <= 8; ++impact) {
            try {
                const impactBounds = runtime.bounds(`math-strike:death-flash:${impact}`, 0);
                deathCrossPeak = Math.max(
                    deathCrossPeak,
                    impactBounds.width,
                );
            } catch {
                // Inactive pooled effects intentionally have no visible bounds.
            }
        }
    }
    assert(deathCrossPeak > 100,
        "Enemy death must still resolve through a large pooled cross-flash");
    assert(transitionSounds.has("../assets/audio/explosion.wav"),
        "Enemy death must retain its explosion sound event");
    assert.equal(transitionSounds.has("../assets/audio/level-up.wav"), false,
        "Enemy death must not trigger the removed random-card flow");
    assert.throws(
        () => runtime.bounds("math-strike:level-up", 0),
        /InsufficientCondition/,
        "The removed level-up overlay must remain hidden after enemy deaths",
    );

    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("keydown", {key: "R"}).handled, true);
    runtime.advanceRuntime(1 / 60);
    assert.throws(
        () => runtime.bounds("math-strike:player-shot:1", 0),
        /InsufficientCondition/,
        "R must clear and return the bounded projectile pool to its initial state",
    );
    runtime.beginInputFrame(runtime.runtimeTime);
    assert.equal(event("keyup", {key: "R"}).handled, true);
    runtime.advanceRuntime(1 / 60);
    for (let tap = 0; tap < 18; ++tap) {
        runtime.beginInputFrame(runtime.runtimeTime);
        assert.equal(event("pointerdown", {
            x: 700, y: 1200, button: 0, buttons: 1, pointerId: 1,
        }).handled, true);
        runtime.advanceRuntime(1 / 60);
        runtime.beginInputFrame(runtime.runtimeTime);
        assert.equal(event("pointerup", {
            x: 700, y: 1200, button: 0, buttons: 0, pointerId: 1,
        }).handled, true);
        runtime.advanceRuntime(1 / 60);
        for (let frame = 0; frame < 9; ++frame) {
            runtime.beginInputFrame(runtime.runtimeTime);
            runtime.advanceRuntime(1 / 60);
        }
    }
    const overheat = runtime.bounds("math-strike:stamina:overheat", 0);
    assert(overheat.width > 40 && overheat.height > 8,
        "Exhausted stamina must expose a visible OVERHEAT lock beside the player");
    assert(runtime.bounds("math-strike:player-hot", 0).width > 30,
        "Overheat must force the player craft into its red heat state");
    assert.throws(
        () => runtime.bounds("math-strike:player-shot:29", 0),
        /InsufficientCondition/,
        "Overheat must lock further tap fire instead of draining into another shot",
    );
    const pixels = runtime.render(0, true);
    assert.equal(pixels.length, 720 * 1280 * 4);

    runtime.loadLua(gameSource, "retained-lua-2d-game-inertial-steering.lua");
    runtime.beginInputFrame(runtime.runtimeTime);
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    event("keydown", {key: "D"});
    for (let frame = 0; frame < 30; ++frame) {
        runtime.advanceRuntime(1 / 60);
        runtime.beginInputFrame(runtime.runtimeTime);
    }
    const reversalStart = center(runtime.bounds("math-strike:player", 0));
    event("keyup", {key: "D"});
    event("keydown", {key: "A"});
    for (let frame = 0; frame < 10; ++frame) {
        runtime.advanceRuntime(1 / 60);
        runtime.beginInputFrame(runtime.runtimeTime);
    }
    const slidingThroughTurn = center(runtime.bounds("math-strike:player", 0));
    assert(slidingThroughTurn.x > reversalStart.x + 4,
        "Opposite input must steer gradually while prior velocity keeps sliding forward");
    for (let frame = 0; frame < 20; ++frame) {
        runtime.advanceRuntime(1 / 60);
        runtime.beginInputFrame(runtime.runtimeTime);
    }
    const completedTurn = center(runtime.bounds("math-strike:player", 0));
    assert(completedTurn.x < reversalStart.x - 10,
        "Sustained opposite input must eventually overcome inertia and reverse travel");
    event("keyup", {key: "A"});

    const quietGameSource = gameSource.replace(
        /local function spawn_enemy_shot\(kind, x, y, angle, speed, turn, damage, scale\)[\s\S]*?\nend\n\nlocal function emit_enemy_shot/,
        `local function spawn_enemy_shot()
    return 0
end

local function emit_enemy_shot`,
    );
    const homingSource = quietGameSource.replace(
        "local power_level, weapon_type = 1, 1",
        "local power_level, weapon_type = 3, 2",
    );
    runtime.loadLua(homingSource, "retained-lua-2d-game-homing.lua");
    runtime.beginInputFrame(runtime.runtimeTime);
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    event("pointerdown", {x: 360, y: 120, button: 0, buttons: 1, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    for (let frame = 1; frame < 70; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    runtime.beginInputFrame(runtime.runtimeTime);
    event("pointerup", {x: 360, y: 120, button: 0, buttons: 0, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    for (let frame = 0; frame < 10; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    const orbitingMissiles = visibleCenters(runtime, "math-strike:player-shot:", 64);
    assert(orbitingMissiles.length >= 14,
        "A maximum charge must release a large low-damage homing-missile flock");
    const orbitSpanX = Math.max(...orbitingMissiles.map((point) => point.x))
        - Math.min(...orbitingMissiles.map((point) => point.x));
    const orbitSpanY = Math.max(...orbitingMissiles.map((point) => point.y))
        - Math.min(...orbitingMissiles.map((point) => point.y));
    assert(orbitSpanX > 35 && orbitSpanY > 35,
        "Homing missiles must visibly orbit around the player before launch");
    const homingPlayer = center(runtime.bounds("math-strike:player", 0));
    for (let frame = 0; frame < 32; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    const launchedMissiles = visibleCenters(runtime, "math-strike:player-shot:", 64);
    assert(launchedMissiles.some((point) => Math.hypot(
        point.x - homingPlayer.x, point.y - homingPlayer.y,
    ) > 90), "Orbiting missiles must launch and home after their staggered dwell");

    const ballSource = quietGameSource.replace(
        "local power_level, weapon_type = 1, 1",
        "local power_level, weapon_type = 1, 3",
    );
    runtime.loadLua(ballSource, "retained-lua-2d-game-ball.lua");
    runtime.beginInputFrame(runtime.runtimeTime);
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    event("pointerdown", {x: 710, y: 1048, button: 0, buttons: 1, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    event("pointerup", {x: 710, y: 1048, button: 0, buttons: 0, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    const ballInitial = runtime.bounds("math-strike:player-shot:31", 0);
    let ballMaxX = center(ballInitial).x;
    let ballFinal = ballInitial;
    for (let frame = 0; frame < 55; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
        ballFinal = runtime.bounds("math-strike:player-shot:31", 0);
        ballMaxX = Math.max(ballMaxX, center(ballFinal).x);
    }
    assert(center(ballFinal).x < ballMaxX - 30,
        "The ball projectile must reflect from the field boundary");
    assert(ballFinal.width * ballFinal.height < ballInitial.width * ballInitial.height * 0.95,
        "Every ball reflection must visibly reduce projectile size and power");

    const shortBurstSource = quietGameSource.replace(
        "local power_level, weapon_type = 1, 1",
        "local power_level, weapon_type = 5, 1",
    );
    runtime.loadLua(shortBurstSource, "retained-lua-2d-game-short-burst.lua");
    runtime.beginInputFrame(runtime.runtimeTime);
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    event("pointerdown", {x: 710, y: 1048, button: 0, buttons: 1, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    event("pointerup", {x: 710, y: 1048, button: 0, buttons: 0, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    assert.equal(visibleCenters(runtime, "math-strike:player-shot:", 64).length, 1,
        "A high-level Short Laser must begin with one projectile");
    for (let frame = 0; frame < 12; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    assert.equal(visibleCenters(runtime, "math-strike:player-shot:", 64).length, 3,
        "Level-five Short Laser must resolve into a three-shot timed burst");

    const longLaserSource = quietGameSource.replace(
        "local power_level, weapon_type = 1, 1",
        "local power_level, weapon_type = 1, 4",
    );
    runtime.loadLua(longLaserSource, "retained-lua-2d-game-long-laser.lua");
    runtime.beginInputFrame(runtime.runtimeTime);
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    event("pointerdown", {x: 360, y: 120, button: 0, buttons: 1, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    event("pointerup", {x: 360, y: 120, button: 0, buttons: 0, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    const longLaserStart = runtime.bounds("math-strike:player-shot:43:long-laser", 0);
    const longLaserPlayerStart = center(runtime.bounds("math-strike:player", 0));
    runtime.beginInputFrame(runtime.runtimeTime);
    event("keydown", {key: "D"});
    for (let frame = 0; frame < 4; ++frame) {
        runtime.advanceRuntime(1 / 60);
        runtime.beginInputFrame(runtime.runtimeTime);
    }
    const longLaserFull = runtime.bounds("math-strike:player-shot:43:long-laser", 0);
    const longLaserBeamFull = runtime.bounds(
        "math-strike:player-shot:43:long-laser:beam", 0,
    );
    const longLaserPlayerLocked = center(runtime.bounds("math-strike:player", 0));
    assert(longLaserStart.height > 400
        && longLaserFull.y < 5 && longLaserFull.height > 1000,
    `An uncharged Long Laser must rapidly overscan from the player beyond the canvas edge: ${JSON.stringify({start: longLaserStart, full: longLaserFull})}`);
    assert(Math.abs(longLaserPlayerLocked.x - longLaserPlayerStart.x) < 4
        && longLaserPlayerLocked.y > longLaserPlayerStart.y,
    "Long Laser channeling must reject movement input and preserve recoil-only travel");
    event("pointermove", {x: 680, y: 1030, buttons: 0, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    const longLaserBeamTurn = runtime.bounds(
        "math-strike:player-shot:43:long-laser:beam", 0,
    );
    const longLaserPlayerTurned = center(runtime.bounds("math-strike:player", 0));
    assert(longLaserBeamTurn.width > longLaserBeamFull.width * 2
        && longLaserBeamTurn.height > 800,
    "The rigid Long Laser must begin turning as one straight beam without snapping");
    assert(Math.abs(longLaserPlayerTurned.x - longLaserPlayerLocked.x) < 4,
        "Turning an active Long Laser must not restore movement input");
    for (let frame = 0; frame < 5; ++frame) {
        runtime.advanceRuntime(1 / 60);
        runtime.beginInputFrame(runtime.runtimeTime);
    }
    const longLaserBeamFollow = runtime.bounds(
        "math-strike:player-shot:43:long-laser:beam", 0,
    );
    assert(longLaserBeamFollow.width > longLaserBeamTurn.width * 1.5,
        "The complete straight beam must follow the tensioned aim direction");
    for (let frame = 0; frame < 15; ++frame) {
        runtime.advanceRuntime(1 / 60);
        runtime.beginInputFrame(runtime.runtimeTime);
    }
    const longLaserBeamRetract = runtime.bounds(
        "math-strike:player-shot:43:long-laser:beam", 0,
    );
    assert(Math.hypot(longLaserBeamRetract.width, longLaserBeamRetract.height)
        < Math.hypot(longLaserBeamFollow.width, longLaserBeamFollow.height) * 0.82,
    "The straight Long Laser must retract from the battlefield edge after its hold");
    for (let frame = 0; frame < 10; ++frame) {
        runtime.advanceRuntime(1 / 60);
        runtime.beginInputFrame(runtime.runtimeTime);
    }
    assert.throws(
        () => runtime.bounds("math-strike:player-shot:43:long-laser", 0),
        /InsufficientCondition/,
        "Long Laser must fade completely after its retract phase",
    );
    event("keyup", {key: "D"});

    const pulseBulletSource = quietGameSource.replace(
        "local power_level, weapon_type = 1, 1",
        "local power_level, weapon_type = 5, 5",
    );
    runtime.loadLua(pulseBulletSource, "retained-lua-2d-game-pulse-bullet.lua");
    runtime.beginInputFrame(runtime.runtimeTime);
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    event("pointerdown", {x: 360, y: 120, button: 0, buttons: 1, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    runtime.beginInputFrame(runtime.runtimeTime);
    event("pointerup", {x: 360, y: 120, button: 0, buttons: 0, pointerId: 1});
    runtime.advanceRuntime(1 / 60);
    const pulseBullet = runtime.bounds("math-strike:player-shot:49:bullet", 0);
    assert(pulseBullet.height < 20,
        "Pulse Bullet must remain shorter than the compact Short Laser silhouette");
    let pulseVolley = visibleCenters(runtime, "math-strike:player-shot:", 64);
    assert.equal(pulseVolley.length, 9,
        "Level-five Pulse Bullet must emit a nine-projectile volley");
    for (let frame = 0; frame < 10; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    pulseVolley = visibleCenters(runtime, "math-strike:player-shot:", 64);
    const pulseSpan = Math.max(...pulseVolley.map((point) => point.x))
        - Math.min(...pulseVolley.map((point) => point.x));
    assert(pulseSpan > 45,
        "Upgraded Pulse Bullet must visibly fan multiple projectiles across a wide angle");

    const progressionSource = gameSource.replace(/\nreturn scene\s*$/, `
scene:circle {center = {experience_to_next_level(1) * 10 - 200, -600}, radius = 1,
    fill = "accent", id = "math-strike:xp-probe:1"}
scene:circle {center = {experience_to_next_level(2) * 10 - 200, -600}, radius = 1,
    fill = "accent", id = "math-strike:xp-probe:2"}
scene:circle {center = {experience_to_next_level(3) * 10 - 200, -600}, radius = 1,
    fill = "accent", id = "math-strike:xp-probe:3"}
scene:circle {center = {experience_to_next_level(4) * 10 - 200, -600}, radius = 1,
    fill = "accent", id = "math-strike:xp-probe:4"}
scene:circle {center = {experience_to_next_level(5) * 10 - 200, -600}, radius = 1,
    fill = "accent", id = "math-strike:xp-probe:5"}
scene:circle {center = {experience_to_next_level(6) * 10 - 200, -600}, radius = 1,
    fill = "accent", id = "math-strike:xp-probe:6"}
scene:circle {center = {experience_to_next_level(7) * 10 - 200, -600}, radius = 1,
    fill = "accent", id = "math-strike:xp-probe:7"}
scene:circle {center = {TUNING.recoil_multiplier * 10 - 200, -610}, radius = 1,
    fill = "accent", id = "math-strike:recoil-multiplier-probe"}
return scene
`);
    assert.notEqual(progressionSource, gameSource,
        "The progression fixture must probe the template's real experience function");
    runtime.loadLua(progressionSource, "retained-lua-2d-game-progression.lua");
    const requirements = [];
    for (let level = 1; level <= 7; ++level) {
        const probe = center(runtime.bounds(`math-strike:xp-probe:${level}`, 0));
        requirements.push(Math.round((probe.x - 160) / 10));
    }
    assert.deepEqual(requirements, [1, 2, 3, 4, 6, 9, 14],
        "Experience required per level must follow the declared 1.55x exponential curve");
    const recoilMultiplierProbe = center(runtime.bounds(
        "math-strike:recoil-multiplier-probe", 0,
    ));
    assert.equal(Math.round((recoilMultiplierProbe.x - 160) / 10), 3,
        "Player movement, battlefield shake, and directional enemy recoil must use 3x tuning");

    const recoilOnlySource = gameSource.replace(
        /local function spawn_enemy_shot\(kind, x, y, angle, speed, turn, damage, scale\)[\s\S]*?\nend\n\nlocal function emit_enemy_shot/,
        `local function spawn_enemy_shot()
    return 0
end

local function emit_enemy_shot`,
    );
    assert.notEqual(recoilOnlySource, gameSource,
        "The recoil-only fixture must disable every enemy projectile spawn");
    runtime.loadLua(recoilOnlySource, "retained-lua-2d-game-recoil-only.lua");
    const stalledEnemyBefore = center(runtime.bounds("math-strike:enemy:1", 0));
    for (let frame = 0; frame < 120; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
    }
    const stalledEnemyAfter = center(runtime.bounds("math-strike:enemy:1", 0));
    assert(Math.hypot(
        stalledEnemyAfter.x - stalledEnemyBefore.x,
        stalledEnemyAfter.y - stalledEnemyBefore.y,
    ) < 0.1, "An enemy without emitted projectiles must have no movement source");

    const saturationSource = gameSource.replace(
        "local ENEMY_SHOT_COUNT = {ring = 128, aim = 128, spiral = 64}",
        "local ENEMY_SHOT_COUNT = {ring = 12, aim = 128, spiral = 64}",
    ).replace(
        /local function damage_player\(amount\)[\s\S]*?\nlocal function spend_stamina/,
        `local function damage_player()
    return false
end

local function spend_stamina`,
    );
    assert.notEqual(saturationSource, gameSource,
        "The saturation fixture must force a small pool and disable player damage");
    runtime.loadLua(saturationSource, "retained-lua-2d-game-saturation.lua");
    const previousProjectiles = new Map();
    let overwrittenProjectile = null;
    const projectilePools = [["ring", 12]];
    for (let frame = 0; frame < 12 * 60 && !overwrittenProjectile; ++frame) {
        runtime.beginInputFrame(runtime.runtimeTime);
        runtime.advanceRuntime(1 / 60);
        if (frame % 4 !== 0) continue;
        for (const [kind, count] of projectilePools) {
            for (let index = 1; index <= count; ++index) {
                const id = `math-strike:enemy-shot:${kind}:${index}`;
                try {
                    const current = center(runtime.bounds(id, 0));
                    const previous = previousProjectiles.get(id);
                    if (previous
                        && previous.x > 80 && previous.x < 640
                        && previous.y > 170 && previous.y < 1110
                        && Math.hypot(current.x - previous.x, current.y - previous.y) > 48) {
                        overwrittenProjectile = id;
                        break;
                    }
                    previousProjectiles.set(id, current);
                } catch {
                    previousProjectiles.delete(id);
                }
            }
            if (overwrittenProjectile) break;
        }
    }
    assert.equal(overwrittenProjectile, null,
        "An active projectile slot must never be reassigned while its bullet is in flight");
} finally {
    runtime.destroy();
}

console.log("retained Lua fixed-step, input, rollback, authoring lock, and game template passed");
