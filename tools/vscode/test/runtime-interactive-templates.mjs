import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";

const extensionRoot = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const repositoryRoot = path.resolve(extensionRoot, "..", "..");
const runtimeRoot = process.env.VIS_RUNTIME_ROOT
    ? path.resolve(process.env.VIS_RUNTIME_ROOT)
    : path.join(extensionRoot, "runtime");
const templateRoot = path.join(
    repositoryRoot,
    "tools",
    "skill",
    "vis-game",
    "templates",
);
const audioRoot = path.join(templateRoot, "..", "assets", "audio");
const {createVis} = await import(pathToFileURL(path.join(runtimeRoot, "client.js")));
const wasmBinary = fs.readFileSync(path.join(runtimeRoot, "vis-wasm.wasm"));
const fonts = [
    ["Source Serif 4", "SourceSerif4-Semibold.ttf"],
    ["IBM Plex Sans KR", "IBMPlexSansKR-SemiBold.ttf"],
    ["Pretendard", "Pretendard.ttf"],
].map(([name, file]) => [
    name,
    fs.readFileSync(path.join(extensionRoot, "runtime", file)),
]);
fonts.push(["JetBrainsMono", fs.readFileSync(path.join(repositoryRoot, "src/examples/assets/JetBrainsMono.ttf"))]);

const center = (box) => ({
    x: box.x + box.width / 2,
    y: box.y + box.height / 2,
});

const event = (runtime, type, fields = {}) => runtime.input({
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
    time: 0,
    ...fields,
});

async function loadTemplate(name) {
    const source = fs.readFileSync(path.join(templateRoot, name), "utf8");
    const runtime = await createVis(
        "return vis.scene {width = 8, height = 8}",
        "interactive-template-bootstrap.lua",
        {wasmBinary},
    );
    try {
        for (const [fontName, data] of fonts) runtime.font(fontName, data, "ttf");
        runtime.loadLua(source, name);
        return runtime;
    } catch (error) {
        runtime.destroy();
        throw error;
    }
}

{
    const runtime = await loadTemplate("ui-input-routing.lua");
    try {
        const initial = center(runtime.bounds("routing:input:follower:body", 0.2));
        const down = event(runtime, "pointerdown", {
            x: 140,
            y: 285,
            button: 0,
            buttons: 1,
            pointerId: 12,
            time: 0.2,
        });
        assert.equal(down.capture, true);
        const captured = event(runtime, "pointermove", {
            x: 700,
            y: 250,
            buttons: 1,
            pointerId: 12,
            time: 0.24,
        });
        assert.equal(captured.handled, true);
        const duringCapture = center(runtime.bounds("routing:input:follower:body", 0.4));
        assert.ok(Math.abs(duringCapture.x - initial.x) < 0.1);
        assert.ok(Math.abs(duringCapture.y - initial.y) < 0.1);
        const up = event(runtime, "pointerup", {
            x: 700,
            y: 250,
            button: 0,
            pointerId: 12,
            time: 0.4,
        });
        assert.equal(up.release, true);

        const unhandled = event(runtime, "pointermove", {
            x: 700,
            y: 250,
            pointerId: 12,
            time: 0.5,
        });
        assert.equal(unhandled.handled, true);
        const afterRelease = center(runtime.bounds("routing:input:follower:body", 0.66));
        assert.ok(Math.abs(afterRelease.x - 700) < 0.2);
        assert.ok(Math.abs(afterRelease.y - 250) < 0.2);
    } finally {
        runtime.destroy();
    }
}

{
    const source = fs.readFileSync(path.join(templateRoot, "retained-lua-2d-game.lua"), "utf8");
    const contracts = [
        [/local WIDTH, HEIGHT = 720, 1280/, "the game must retain its portrait playfield"],
        [/local FIELD = \{left = -360, right = 360, top = 500, bottom = -490\}[\s\S]*size = \{720, 990\}[\s\S]*id = "math-strike:top-panel"[\s\S]*size = \{720, 150\}[\s\S]*id = "math-strike:bottom-panel"/,
            "the gameplay and top/bottom HUD bands must use the full 720-pixel canvas width"],
        [/local GRID = \{cell = 60, columns = 12, rows = 18, sheets = 2, highlights = 13\}[\s\S]*fill = "#05080F"[\s\S]*CARD_TYPES\.grid_span = GRID\.cell \* GRID\.rows[\s\S]*CARD_TYPES\.grid_speed = 78[\s\S]*for column = 0, GRID\.columns do[\s\S]*for row = 0, GRID\.rows do[\s\S]*for cell = 1, GRID\.highlights do/,
            "the background must be one dark flat grid with deterministic highlighted cells"],
        [/for sheet = 1, GRID\.sheets do[\s\S]*local phase = \(GRID\.sheets - sheet\) \* CARD_TYPES\.grid_span[\s\S]*run_time \* CARD_TYPES\.grid_speed \+ grid\.phase[\s\S]*% \(CARD_TYPES\.grid_span \* GRID\.sheets\)[\s\S]*grid\.state\.shift\[2\] = y - grid\.base_y/,
            "two flat grid sheets must stream downward and wrap across one continuous span"],
        [/hazard_enemy_speed = 1\.25[\s\S]*TUNING\.enemy_time_scale = function\(time\)[\s\S]*math\.floor\(time \/ ROUND_PERIOD\) % 2 == 1/,
            "alternate hazard rounds must declare an exact 1.25x enemy time scale"],
        [/local action_dt = dt \* TUNING\.enemy_time_scale\(run_time\)[\s\S]*update_enemy_motion\(i, action_dt\)[\s\S]*enemy\.cooldown = enemy\.cooldown - action_dt/,
            "hazard scaling must accelerate enemy locomotion and attack cadence"],
        [/local enemy_dt = dt \* TUNING\.enemy_time_scale\(run_time\)[\s\S]*update_enemy_shot_pool\(enemy_shot_pools\.ring, enemy_dt\)[\s\S]*enemy_shot_pools\.spiral, enemy_dt/,
            "hazard scaling must accelerate hostile projectile travel without speeding player simulation"],
        [/name = "SHORT LASER"[\s\S]*name = "HOMING MISSILE"[\s\S]*name = "RICOCHET BALL"[\s\S]*name = "LONG LASER"[\s\S]*name = "PULSE BULLET"[\s\S]*levels = \{1, 1, 1, 1, 1\}[\s\S]*local function cycle_weapon\(\)[\s\S]*weapon_type = weapon_type % #CARD_TYPES\[6\]\.weapons \+ 1[\s\S]*runtime:bind_key\("SwitchWeapon", "Tab"\)/,
            "all five weapons must begin unlocked and cycle deterministically with Tab"],
        [/local function authored_number[\s\S]*CARD_TYPES\.commit_number\(ctx, life_number, hp\)[\s\S]*score_number, kills[\s\S]*breach_number, breaches[\s\S]*threat_number, unlocked_enemies[\s\S]*power_number, power_level[\s\S]*round_number, math\.floor\(run_time \/ ROUND_PERIOD\) \+ 1/,
            "HP, kill, breach, threat, power, and round HUD values must use numeric displays"],
        [/local START_HP = 100[\s\S]*hp, max_hp = 0, 0, 0, START_HP, START_HP[\s\S]*hp = math\.max\(0, hp - math\.max\(0, amount or 0\)\)[\s\S]*damage_player\(shot\.damage\)/,
            "player HP must start at 100 and subtract exact hostile projectile damage"],
        [/enemy_shot_visual_scale = 0\.62[\s\S]*shot\.scale = \(scale or 1\) \* TUNING\.enemy_shot_visual_scale[\s\S]*enemy_projectile_strength\(shot\.damage, scale or 1\)[\s\S]*local mass = \(scale or 1\)/,
            "hostile shots must use slim visuals without weakening recoil or clash strength"],
        [/local switch_weapon = ctx:action\("SwitchWeapon"\)[\s\S]*elseif switch_weapon\.pressed and not game_is_over then[\s\S]*cycle_weapon\(\)[\s\S]*if not game_is_over then/,
            "Tab weapon switching must use one pressed edge without pausing combat"],
        [/player_clash_multiplier = 1\.60[\s\S]*CARD_TYPES\.player_projectile_strength = function\(damage\)[\s\S]*TUNING\.player_clash_multiplier[\s\S]*shot\.strength = CARD_TYPES\.player_projectile_strength\(damage\)[\s\S]*shot\.max_strength = shot\.strength[\s\S]*CARD_TYPES\.enemy_projectile_strength = function[\s\S]*shot\.strength = CARD_TYPES\.enemy_projectile_strength[\s\S]*remaining_strength \/ math\.max\(shot\.max_strength[\s\S]*shot\.damage = shot\.max_damage \* ratio[\s\S]*local remaining = shot\.strength - hostile\.strength[\s\S]*retain_projectile_power\(shot, remaining\)[\s\S]*retain_projectile_power\(hostile, -remaining\)/,
            "projectile clashes must separate HP damage from strength and preserve the exact remainder"],
        [/if fire\.pressed and not long_laser_active[\s\S]*player_recoil_lock <= 0 then[\s\S]*if fire\.released then/,
            "fire must separate press-to-charge from release-to-shoot and reject recoil-locked presses"],
        [/local MAX_CHARGE_SCALE = 5[\s\S]*size = 1 \+ amount \* \(MAX_CHARGE_SCALE - 1\)[\s\S]*fire_weapon\(size\)/,
            "charged fire must preserve the active weapon pattern and scale it from 1x to 5x"],
        [/local function fire_weapon\(size_multiplier\)[\s\S]*local damage = player_damage \* size_multiplier/,
            "the charged projectile size multiplier must also multiply attack damage"],
        [/weapon_stamina = \{[\s\S]*\{tap = 0\.110[\s\S]*\{tap = 0\.030[\s\S]*\{tap = 0\.180[\s\S]*\{tap = 0\.024[\s\S]*local profile = TUNING\.weapon_stamina\[weapon_type\][\s\S]*cost = profile\.charge \+ amount \* profile\.release[\s\S]*profile\.hold/,
            "each weapon must own tap, charge, release, and hold stamina costs with lasers costlier than round bullets"],
        [/runtime:bind_pointer\("Fire"[\s\S]*runtime:bind_key\("Boost"/,
            "combat fire must be pointer-only"],
        [/target_x = pointer\.position\.x - WIDTH \/ 2[\s\S]*aim_angle = math\.atan/,
            "pointer position must drive the player firing bearing"],
        [/CARD_TYPES\.ray_distance = function\(x, y, angle, bounds\)[\s\S]*math\.min\(tx, ty\)[\s\S]*id = "math-strike:top-panel"[\s\S]*id = "math-strike:bottom-panel"[\s\S]*CARD_TYPES\.aim = \{[\s\S]*id = "math-strike:aim-guide"[\s\S]*CARD_TYPES\.ray_distance\(\s*player_x, player_y, aim_angle/,
            "a thin retained pointer ray must overscan the game while later HUD panels occlude it"],
        [/local function apply_player_recoil[\s\S]*shot_count \* 9[\s\S]*size \^ 1\.15/,
            "player kick must grow with projectile count and projectile scale"],
        [/player_steering_response = 6\.0[\s\S]*player_coast_drag = 3\.0[\s\S]*elseif dx == 0 and dy == 0 then[\s\S]*math\.exp\(-TUNING\.player_coast_drag \* dt\)[\s\S]*math\.exp\(-TUNING\.player_steering_response \* dt\)/,
            "player travel must steer and coast through explicit inertia rather than snap to input"],
        [/player_state\.rotation = aim_angle - math\.pi \/ 2\s*\n\s*player_state\.opacity/,
            "player hull orientation must remain exactly pointer-facing while translation slides independently"],
        [/id = "math-strike:charge:ring-hot"[\s\S]*local charge_heat = smoothstep\(\(charge_amount - 0\.15\) \/ 0\.85\)[\s\S]*player_hot_state\.opacity = player_opacity[\s\S]*overheated and 1[\s\S]*charge_hot_state\.opacity = charging and charge_heat/,
            "the charge ring and player must share a continuous red heat state, forced on by overheat"],
        [/player_recoil_lock[\s\S]*size \* 0\.055\) \* recoil_factor/,
            "the recoil card must shorten the bounded player firing lock with the kick"],
        [/local function emit_enemy_shot[\s\S]*volley\.x = volley\.x \+ math\.cos\(angle\) \* momentum[\s\S]*local function apply_enemy_recoil[\s\S]*impulse_x = -volley\.x \* factor/,
            "enemy recoil must use the vector sum of successfully emitted projectile momentum"],
        [/local function propel_enemy[\s\S]*desired_vx - enemy\.vx[\s\S]*math\.atan\(-direction_y, -direction_x\)[\s\S]*emit_enemy_shot[\s\S]*apply_enemy_recoil\(enemy, volley, false\)/,
            "every enemy guidance mode must move by opposite-vector projectile recoil only"],
        [/enemy\.cooldown <= 0 and enemy\.recoil_lock <= 0[\s\S]*emit_pattern\(enemy\)[\s\S]*enemy\.cooldown = math\.max/,
            "enemies must not emit another volley while directional recoil is active"],
        [/local function trigger_screen_shake[\s\S]*shake_factor[\s\S]*update_screen_shake/,
            "tap and charged shots must drive bounded deterministic battlefield wiggle"],
        [/local function boid_steering[\s\S]*center_x[\s\S]*velocity_x[\s\S]*separation_x/,
            "flocking enemies must combine cohesion, alignment, and separation"],
        [/local function enemy_archetype[\s\S]*"ring", "boid", "danger"[\s\S]*"aim", "dance", "warning"[\s\S]*"spiral", "sentinel", "secondary"[\s\S]*"direct", "recoil", "info"[\s\S]*"heavy", "sentinel", "result"/,
            "enemy color must be the authoritative mapping for movement and firing traits"],
        [/enemy\.behavior == "dance"[\s\S]*math\.sin\(1\.72 \* t[\s\S]*enemy\.anchor_y - 62 \* t/,
            "dance enemies must advance while following a multi-frequency path"],
        [/Sentinel enemies never transition to exit/,
            "sentinel enemies must remain until destroyed"],
        [/enemy\.phase == "death"[\s\S]*enemy\.phase_time >= 0\.24[\s\S]*spawn_impact\(enemy\.x, enemy\.y, "death"\)[\s\S]*trigger_screen_shake/,
            "enemy death must shake before resolving through a pooled cross flash"],
        [/enemy_explosion_radius = 132[\s\S]*CARD_TYPES\.apply_enemy_explosion = function\(source_index\)[\s\S]*enemy\.hp = enemy\.hp - base_damage \* falloff[\s\S]*CARD_TYPES\.mark_enemy_destroyed\(enemy\)[\s\S]*CARD_TYPES\.explode_enemy_shot_pool[\s\S]*CARD_TYPES\.apply_enemy_explosion\(i\)/,
            "enemy death must damage nearby enemies and clear hostile bullets through delayed chain explosions"],
        [/impact\.kind == "death"[\s\S]*death_state\.scale\[1\] = 0\.28 \+ flare \* 1\.32[\s\S]*death_state\.scale\[2\] = 0\.28 \+ flare \* 1\.72/,
            "the death flash must expand into a vertically dominant cross silhouette"],
        [/enemy\.pattern == "direct"[\s\S]*bearing[\s\S]*volley, "aim", enemy\.x, enemy\.y, bearing[\s\S]*damage \* 1\.35, scale \* 1\.25/,
            "former graph and sigil emitters must use simple direct and heavy aimed shots"],
        [/weapon_kind = i <= 10 and 1[\s\S]*i <= 43 and 4 or 5[\s\S]*:laser[\s\S]*:missile[\s\S]*:ball[\s\S]*:long-laser[\s\S]*:bullet/,
            "the 64-slot player pool must pre-author all five weapon silhouettes"],
        [/weapon_damage = \{[\s\S]*short_laser = 1\.80[\s\S]*homing = 0\.44[\s\S]*ricochet = 6\.00[\s\S]*long_laser = 1\.15[\s\S]*pulse = 1\.44/,
            "non-Long-Laser player weapons must use the requested doubled base damage"],
        [/weapon_type == 1[\s\S]*local thickness[\s\S]*power_level \* 0\.24[\s\S]*size_multiplier - 1\) \* 0\.75[\s\S]*TUNING\.weapon_damage\.short_laser[\s\S]*power_level \+ math\.floor/,
            "the doubled starting Short Laser must retain level-scaled penetration and thickness"],
        [/local burst_count = 1 \+ math\.floor\(\(power_level - 1\) \/ 2\)[\s\S]*burst\.remaining = burst_count - 1[\s\S]*CARD_TYPES\[6\]\.update_burst\(dt\)/,
            "high-level Short Laser must add a bounded delayed burst rather than simultaneous copies"],
        [/shot\.charge_heat = charge_heat or 0[\s\S]*local charge_heat = smoothstep[\s\S]*spawn_player_shot[\s\S]*shot\.state\.fill = shot\.charge_heat > 0 and "danger" or shot\.base_fill/,
            "charged projectiles must retain the danger heat color after release"],
        [/if weapon_kind == 1 then\s*\n\s*base_fill = "accent"[\s\S]*size = \{7, 48\}[\s\S]*fill = base_fill[\s\S]*id = id \.\. ":laser-glow"[\s\S]*id = id \.\. ":laser-heat"[\s\S]*fill = "danger", opacity = 0\.88[\s\S]*shot\.heat_state\.opacity = shot\.active and shot\.charge_heat or 0/,
            "Short Laser core and glow must gain a charge-weighted red heat overlay"],
        [/weapon_type == 2[\s\S]*TUNING\.weapon_damage\.homing[\s\S]*shot\.orbit_angle[\s\S]*local nearest = math\.huge[\s\S]*desired - shot\.angle/,
            "minimum-damage missiles must compensate with a large orbiting homing flock"],
        [/weapon_type == 3[\s\S]*TUNING\.weapon_damage\.ricochet[\s\S]*shot\.weapon_kind == 3[\s\S]*shot\.vx = -shot\.vx[\s\S]*shot\.vy = -shot\.vy[\s\S]*shot\.damage = shot\.damage \* 0\.90[\s\S]*shot\.damage = shot\.damage \* 0\.76/,
            "the highest-damage ball must reflect while shrinking in residual power and size"],
        [/long_laser_extend = 0\.08[\s\S]*long_laser_hold = 0\.26[\s\S]*long_laser_retract = 0\.14[\s\S]*CARD_TYPES\.beam_bounds = \{[\s\S]*WIDTH \/ 2 \+ 64[\s\S]*length_ratio = ease_out_cubic[\s\S]*length_ratio = 1 - retract[\s\S]*CARD_TYPES\[6\]\.long_active = false/,
            "tap and charged Long Lasers must overscan above ordinary UI, hold, retract, and release their channel lock"],
        [/local long_laser_active = CARD_TYPES\[6\]\.long_active[\s\S]*if long_laser_active then dx, dy = 0, 0 end[\s\S]*local recoil_drag = math\.exp\(-TUNING\.long_laser_recoil_drag \* dt\)/,
            "long laser channeling must reject steering and preserve recoil-only motion"],
        [/if pointer then[\s\S]*if shot\.weapon_kind == 4 then[\s\S]*shot\.angle = follow_angle\([\s\S]*shot\.angle, aim_angle, TUNING\.long_laser_aim_response[\s\S]*CARD_TYPES\.ray_distance\([\s\S]*shot\.angle/,
            "an active long laser must turn toward the live pointer through bounded angular tension"],
        [/beam_line = beam:line \{[\s\S]*to = \{STAGE_X \+ 1, STAGE_Y\}[\s\S]*:long-laser:beam[\s\S]*local function beam_collision[\s\S]*target_x - shot\.beam_origin_x[\s\S]*0, shot\.visible_length[\s\S]*shot\.beam_origin_x \+ axis_x \* projection[\s\S]*shot\.beam_state\.scale\[1\] = math\.max\(shot\.visible_length[\s\S]*ctx:update\(shot\.beam_line/,
            "long laser rendering and collision must share one exact straight segment"],
        [/local count = 1 \+ \(power_level - 1\) \* 2[\s\S]*local spread_step = 0\.11 \+ power_level \* 0\.035[\s\S]*aim_angle \+ spread[\s\S]*TUNING\.weapon_damage\.pulse/,
            "pulse bullets must upgrade from one shot into a wider 3/5-shot volley"],
        [/local function enemy_health\(enemy\)[\s\S]*level - 1[\s\S]*local function enemy_damage\(\)/,
            "enemy health and damage must scale with level and survival time"],
        [/boost_multiplier = 1\.25/, "held Shift boost must be exactly 1.25x"],
        [/overheated = true[\s\S]*overheated and stamina >= stamina_capacity/,
            "overheat must lock actions until full recovery"],
        [/dash_invulnerable = TUNING\.dash_immunity \+ dash_immunity_bonus/,
            "dash must open an upgradeable post-dash invulnerability window"],
        [/enemy\.phase = "attack"[\s\S]*enemy\.phase = "exit"[\s\S]*breaches = breaches \+ 1/,
            "non-sentinel motion must progress through attack, exit, and breach states"],
        [/CARD_TYPES\.sound_assets = \{[\s\S]*orbital-warden-loop\.wav[\s\S]*short-laser\.wav[\s\S]*long-laser\.wav[\s\S]*explosion\.wav[\s\S]*CARD_TYPES\.flush_sounds = function\(ctx\)[\s\S]*ctx:sound[\s\S]*vis\.audio\.cue/,
            "the retained game must declare optional loop music and bounded runtime sound events"],
    ];
    for (const [pattern, message] of contracts) assert.match(source, pattern, message);
    const damagePlayer = source.match(
        /local function damage_player\(amount\)[\s\S]*?\nlocal function spend_stamina/,
    )?.[0] ?? "";
    assert.doesNotMatch(damagePlayer,
        /PLAYER_HOME|player_vx, player_vy = 0, 0|clear_enemy_projectiles\(\)/,
        "taking damage must preserve player motion and unrelated hostile projectiles");
    assert.doesNotMatch(source, /nearby_attackers|burst_remaining|burst_timer/,
        "enemy attack cadence must not depend on nearby allies");
    assert.doesNotMatch(source,
        /runtime:bind_pointer\("Choose"|runtime:bind_key\("Card[123]"|if kills >= next_level_kills/,
        "random card selection inputs and triggers must remain removed");
    assert.doesNotMatch(source,
        /beam_curve|beam_hot_curve|long_laser_point|beam_bend|tail_angle|long_laser_collision_samples|long_laser_tail_response|:long-laser:curve/,
        "Long Laser must not retain curved-beam or sampled-collision state");
    assert.doesNotMatch(source, /pattern == "sine"|"sigil"|math\.sin\(u \* TAU/,
        "enemy fire must not draw sine graphs or sigil shapes with bullets");
    assert.doesNotMatch(source,
        /CARD_TYPES\.earth|earth-|surface-flow|math-strike:sun|math-strike:moon|star_|stars:|nebula|ROUND_PALETTES/,
        "the flat grid background must not retain Earth, space, parallax, or palette layers");
}

for (const file of [
    "orbital-warden-loop.wav", "short-laser.wav", "homing-missile.wav",
    "ricochet-ball.wav", "long-laser.wav", "pulse-bullet.wav", "explosion.wav",
    "level-up.wav", "player-hit.wav", "card-select.wav",
]) {
    const wav = fs.readFileSync(path.join(audioRoot, file));
    assert(wav.length > 44, `${file} must contain PCM samples`);
    assert.equal(wav.toString("ascii", 0, 4), "RIFF", `${file} must be a RIFF file`);
    assert.equal(wav.toString("ascii", 8, 12), "WAVE", `${file} must be a WAVE file`);
    assert.equal(wav.readUInt16LE(20), 1, `${file} must use uncompressed PCM`);
    assert.equal(wav.readUInt16LE(22), 1, `${file} must remain mono`);
    assert.equal(wav.readUInt32LE(24), 22050, `${file} must use the bounded sample rate`);
    assert.equal(wav.readUInt16LE(34), 16, `${file} must use 16-bit samples`);
}
assert.match(
    fs.readFileSync(path.join(audioRoot, "generate-game-audio.mjs"), "utf8"),
    /const SAMPLE_RATE = 22050[\s\S]*function wav\([\s\S]*music\(\)/,
    "game audio must remain reproducible from the dependency-free repository generator",
);
assert.match(
    fs.readFileSync(path.join(audioRoot, "generate-game-audio.mjs"), "utf8"),
    /noiseGenerator\(0x4c415345\)[\s\S]*long-laser\.wav", 0\.62[\s\S]*const ignition[\s\S]*const sustain[\s\S]*const snap[\s\S]*const tail/,
    "Long Laser must use a dedicated ignition transient and sustained tail",
);

console.log("interactive skill templates passed");
