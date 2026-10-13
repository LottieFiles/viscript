-- Experimental retained-Lua portrait defense game.
-- Requires -Dmodules=runtime,input. It has no UI dependency.
--
-- The Scene stays immutable after loading. Background grid sheets, enemies, shots,
-- effects, and HUD indicators are pre-authored and reused through
-- bounded Object pools. All scenery and combat variation is deterministic.
if not vis.runtime then
    error("retained-lua-2d-game.lua requires -Dmodules=runtime")
end
if not vis.input then
    error("retained-lua-2d-game.lua requires -Dmodules=input")
end

local WIDTH, HEIGHT = 720, 1280
local PRIMARY_POINTER = 1
local TAU = math.pi * 2
local GOLDEN_ANGLE = math.pi * (3 - math.sqrt(5))
local STAGE_X, STAGE_Y = 0, 760
local CARD_STAGE_Y = HEIGHT * 2
local FIELD = {left = -360, right = 360, top = 500, bottom = -490}
local PLAYER_HOME = {x = 0, y = -408}
local HUD_Y = -514
local MAX_HP = 250
local START_HP = 100
local MAX_POWER = 5
local ROUND_PERIOD = 18

local LAYER = {
    sky = 0,
    grid = 3,
    structure = 12,
    enemy_shot = 30,
    player_shot = 35,
    actor = 40,
    effect = 48,
    text = 56,
    overlay = 70,
}

local MAX_ENEMIES = 10
local PLAYER_SHOT_COUNT = 64
local ENEMY_SHOT_COUNT = {ring = 128, aim = 128, spiral = 64}
local IMPACT_COUNT = 8
local GRID = {cell = 60, columns = 12, rows = 18, sheets = 2, highlights = 13}
-- 495 retained Object identities are updated, below Runtime::ObjectLimit 512.

local TUNING = {
    enemy_unlock_period = 7,
    player_speed = 270,
    player_steering_response = 6.0,
    player_coast_drag = 3.0,
    boost_multiplier = 1.25,
    boost_stamina_per_second = 0.22,
    player_shot_speed = 640,
    weapon_stamina = {
        {tap = 0.110, charge = 0.170, release = 0.300, hold = 0.085},
        {tap = 0.070, charge = 0.110, release = 0.190, hold = 0.045},
        {tap = 0.030, charge = 0.060, release = 0.110, hold = 0.022},
        {tap = 0.180, charge = 0.260, release = 0.420, hold = 0.120},
        {tap = 0.024, charge = 0.050, release = 0.090, hold = 0.018},
    },
    charge_threshold = 0.24,
    dash_speed = 760,
    dash_duration = 0.16,
    dash_recovery = 0.30,
    dash_immunity = 0.72,
    initial_recovery = 1.0,
    hit_recovery = 1.45,
    stamina_delay = 0.46,
    stamina_recovery = 0.30,
    overheat_recovery = 0.44,
    enemy_speed_base = 112,
    enemy_speed_step = 6,
    enemy_fire_base = 1.65,
    enemy_fire_min = 0.62,
    aimed_speed_bonus = 18,
    recoil_multiplier = 3,
    enemy_recoil_multiplier = 2,
    enemy_recoil_per_momentum = 0.055,
    enemy_recoil_impulse_cap = 190,
    enemy_recoil_drag = 0.72,
    enemy_propulsion_period = 0.34,
    enemy_propulsion_speed = 178,
    enemy_propulsion_scale = 0.72,
    enemy_shot_visual_scale = 0.62,
    enemy_explosion_radius = 132,
    enemy_explosion_bullet_radius = 154,
    boid_neighbor_radius = 152,
    boid_separation_radius = 54,
    boid_speed = 54,
    hazard_enemy_speed = 1.25,
    player_clash_multiplier = 1.60,
    weapon_damage = {
        short_laser = 1.80,
        homing = 0.44,
        ricochet = 6.00,
        long_laser = 1.15,
        pulse = 1.44,
    },
    long_laser_extend = 0.08,
    long_laser_hold = 0.26,
    long_laser_retract = 0.14,
    long_laser_recoil_drag = 1.8,
    long_laser_aim_response = 5.5,
}

TUNING.enemy_time_scale = function(time)
    return math.floor(time / ROUND_PERIOD) % 2 == 1
        and TUNING.hazard_enemy_speed or 1
end

local CHARGE_DURATION = 1.10
local MAX_CHARGE_SCALE = 5
local LEVEL_XP_GROWTH = 1.55
local CARD_INPUT_DELAY = 1.20
local CARD_REVEAL_START = 0.20
local CARD_REVEAL_DURATION = 0.58
local CARD_REVEAL_STAGGER = 0.10
local CARD_HOVER_DELAY = 0.72
local CARD_HOVER_RESPONSE = 15

local CARD_TYPES = {
    {
        key = "health", category = "HEALTH", title = "HULL PLATING",
        line1 = "MAX HP +20", line2 = "RECOVER 30 HP", color = "success",
    },
    {
        key = "stamina", category = "STAMINA", title = "REACTOR CELL",
        line1 = "CAPACITY +25%", line2 = "FULL RECHARGE", color = "warning",
    },
    {
        key = "item", category = "ITEM", title = "REPAIR DRONE",
        line1 = "RESTORE ALL HP", line2 = "3.0 s BARRIER", color = "info",
    },
    {
        key = "skill", category = "SKILL", title = "PHASE DRIVE",
        line1 = "DASH RECOVERY +", line2 = "IMMUNITY +120 ms", color = "secondary",
    },
    {
        key = "power", category = "ATTACK", title = "AMPLIFIER",
        line1 = "DAMAGE +25%", line2 = "ALL WEAPONS", color = "accent",
    },
    {
        key = "weapon", category = "WEAPON", title = "WEAPON CYCLER",
        line1 = "ALL UNLOCKED", line2 = "PRESS TAB TO SWITCH", color = "result",
        weapons = {
            {name = "SHORT LASER", detail = "FAST · COMPACT"},
            {name = "HOMING MISSILE", detail = "ORBIT · HOMING"},
            {name = "RICOCHET BALL", detail = "BOUNCE · SHRINK"},
            {name = "LONG LASER", detail = "LONG · SLOW FIRE"},
            {name = "PULSE BULLET", detail = "WIDE · 1 / 3 / 5"},
        },
        levels = {1, 1, 1, 1, 1},
    },
    {
        key = "recoil", category = "CONTROL", title = "RECOIL DAMPER",
        line1 = "KICK × 0.75", line2 = "LOCK × 0.75", color = "focus",
    },
}

CARD_TYPES[6].offer = 2
CARD_TYPES[6].offer_level = 1
CARD_TYPES[6].random_state = 104729
CARD_TYPES[6].last_offer = 0
CARD_TYPES[6].burst = {remaining = 0, timer = 0}
CARD_TYPES[6].long_active = false
CARD_TYPES[6].random_unit = function()
    CARD_TYPES[6].random_state = (CARD_TYPES[6].random_state * 16807) % 2147483647
    return CARD_TYPES[6].random_state / 2147483647
end
CARD_TYPES[6].roll = function()
    local eligible = {}
    for pick = 1, #CARD_TYPES[6].weapons do
        if CARD_TYPES[6].levels[pick] < MAX_POWER
            and pick ~= CARD_TYPES[6].last_offer then
            eligible[#eligible + 1] = pick
        end
    end
    if #eligible == 0 then
        for pick = 1, #CARD_TYPES[6].weapons do
            if CARD_TYPES[6].levels[pick] < MAX_POWER then
                eligible[#eligible + 1] = pick
            end
        end
    end
    if #eligible == 0 then return 0 end
    local index = math.floor(CARD_TYPES[6].random_unit() * #eligible) + 1
    local offer = eligible[math.min(index, #eligible)]
    CARD_TYPES[6].last_offer = offer
    return offer
end

CARD_TYPES.beam_bounds = {
    left = -WIDTH / 2 - 64, right = WIDTH / 2 + 64,
    top = HEIGHT / 2 + 64, bottom = -HEIGHT / 2 - 64,
}

CARD_TYPES.ray_distance = function(x, y, angle, bounds)
    local limit = bounds or FIELD
    local dx, dy = math.cos(angle), math.sin(angle)
    local tx, ty = math.huge, math.huge
    if dx > 0.0001 then
        tx = (limit.right - x) / dx
    elseif dx < -0.0001 then
        tx = (limit.left - x) / dx
    end
    if dy > 0.0001 then
        ty = (limit.top - y) / dy
    elseif dy < -0.0001 then
        ty = (limit.bottom - y) / dy
    end
    return math.max(0, math.min(tx, ty))
end

CARD_TYPES.set_number = function(displays, value)
    local maximum = 10 ^ #displays - 1
    local number = math.floor(math.max(0, math.min(maximum, value)))
    local started = false
    for place = 1, #displays do
        local divisor = 10 ^ (#displays - place)
        local digit = math.floor(number / divisor) % 10
        local visible = started or digit > 0 or place == #displays
        started = started or digit > 0
        local display = displays[place]
        display.state.shift[2] = display.target_y - display.base_y
            + (4.5 - digit) * HEIGHT * 2
        display.state.opacity = visible and 1 or 0
    end
end

CARD_TYPES.commit_number = function(ctx, displays, value)
    CARD_TYPES.set_number(displays, value)
    for place = 1, #displays do
        ctx:update(displays[place].object, displays[place].state)
    end
end

local function clamp(value, minimum, maximum)
    return math.max(minimum, math.min(maximum, value))
end

local function lerp(a, b, amount)
    return a + (b - a) * amount
end

local function angle_delta(current, target)
    return (target - current + math.pi) % TAU - math.pi
end

local function follow_angle(current, target, response, dt)
    return current + angle_delta(current, target)
        * (1 - math.exp(-response * dt))
end

local function smoothstep(value)
    local t = clamp(value, 0, 1)
    return t * t * (3 - 2 * t)
end

local function ease_out_cubic(value)
    local t = clamp(value, 0, 1)
    return 1 - (1 - t) * (1 - t) * (1 - t)
end

local function experience_to_next_level(current_level)
    return math.max(1, math.ceil(LEVEL_XP_GROWTH ^ math.max(0, current_level - 1)))
end

local function distance_squared(ax, ay, bx, by)
    local dx, dy = ax - bx, ay - by
    return dx * dx + dy * dy
end

local function normalized(dx, dy, fallback_x, fallback_y)
    local length = math.sqrt(dx * dx + dy * dy)
    if length <= 0.0001 then return fallback_x, fallback_y, 0 end
    return dx / length, dy / length, length
end

local scene = vis.scene {
    width = WIDTH,
    height = HEIGHT,
    fps = 60,
    loop = false,
    theme = "adaptive_vscode",
    camera = {mode = "fixed", view = "2d", target = {0, 0}, height = HEIGHT},
}

CARD_TYPES.sound_assets = {
    music = {
        asset = "../assets/audio/orbital-warden-loop.wav", bus = "music", gain = 0.32,
    },
    weapons = {
        {asset = "../assets/audio/short-laser.wav", bus = "effect", gain = 0.48},
        {asset = "../assets/audio/homing-missile.wav", bus = "effect", gain = 0.42},
        {asset = "../assets/audio/ricochet-ball.wav", bus = "effect", gain = 0.50},
        {asset = "../assets/audio/long-laser.wav", bus = "effect", gain = 0.62},
        {asset = "../assets/audio/pulse-bullet.wav", bus = "effect", gain = 0.34},
    },
    explosion = {asset = "../assets/audio/explosion.wav", bus = "effect", gain = 0.58},
    level_up = {asset = "../assets/audio/level-up.wav", bus = "ui", gain = 0.72},
    player_hit = {asset = "../assets/audio/player-hit.wav", bus = "effect", gain = 0.64},
    weapon_switch = {asset = "../assets/audio/card-select.wav", bus = "ui", gain = 0.66},
}

CARD_TYPES.sound_events = {}
CARD_TYPES.sound_event_count = 0
for i = 1, 16 do
    CARD_TYPES.sound_events[i] = {
        asset = "",
        options = {bus = "effect", gain = 1, rate = 1, loop = false},
    }
end

CARD_TYPES.queue_sound = function(sound, gain, rate)
    if not sound or CARD_TYPES.sound_event_count >= #CARD_TYPES.sound_events then return end
    CARD_TYPES.sound_event_count = CARD_TYPES.sound_event_count + 1
    local event = CARD_TYPES.sound_events[CARD_TYPES.sound_event_count]
    event.asset = sound.asset
    event.options.bus = sound.bus
    event.options.gain = gain or sound.gain
    event.options.rate = rate or 1
end

CARD_TYPES.flush_sounds = function(ctx)
    for i = 1, CARD_TYPES.sound_event_count do
        local event = CARD_TYPES.sound_events[i]
        ctx:sound(event.asset, event.options)
    end
    CARD_TYPES.sound_event_count = 0
end

if vis.audio then
    vis.audio.cue(scene, {
        asset = "../assets/audio/orbital-warden-loop.wav",
        bus = "music", begin = 0, gain = CARD_TYPES.sound_assets.music.gain,
        loop = true,
    })
end

scene:text {
    text = "ORBITAL WARDEN",
    point = {-336, 608},
    align = {0, 0.5},
    role = "h2",
    layer = LAYER.text,
    id = "math-strike:title",
}
scene:text {
    text = "streaming grid defense · retained Lua · fixed 120 Hz",
    point = {-336, 570},
    align = {0, 0.5},
    role = "text",
    fill = "muted",
    layer = LAYER.text,
    id = "math-strike:subtitle",
}
scene:text {
    text = "MOVE WASD/ARROWS · BOOST SHIFT · WEAPON TAB · DASH SPACE",
    point = {-336, 540},
    align = {0, 0.5},
    role = "code",
    fill = "foreground",
    layer = LAYER.text,
    id = "math-strike:controls-move",
}
scene:line {
    from = {-360, 510},
    to = {360, 510},
    color = "border",
    width = 1.5,
    layer = LAYER.structure,
    id = "math-strike:header-rule",
}

local battlefield = scene:group {id = "math-strike:battlefield"}

battlefield:rectangle {
    center = {0, 5},
    size = {720, 990},
    fill = "#05080F",
    layer = LAYER.sky,
    id = "math-strike:field",
}

CARD_TYPES.grid_span = GRID.cell * GRID.rows
CARD_TYPES.grid_speed = 78
CARD_TYPES.grid_cell_colors = {"#12334A", "#183E51", "#292947"}
local grid_sheets = {}
for sheet = 1, GRID.sheets do
    local phase = (GRID.sheets - sheet) * CARD_TYPES.grid_span
    local initial_y = (sheet - 1) * CARD_TYPES.grid_span
    local grid = battlefield:group {
        id = "math-strike:grid:" .. tostring(sheet),
    }
    for column = 0, GRID.columns do
        local x = FIELD.left + column * GRID.cell
        grid:line {
            from = {x, -CARD_TYPES.grid_span / 2},
            to = {x, CARD_TYPES.grid_span / 2},
            color = "#263449", width = 1, opacity = 0.52,
            layer = LAYER.grid,
            id = "math-strike:grid:" .. tostring(sheet)
                .. ":column:" .. tostring(column),
        }
    end
    for row = 0, GRID.rows do
        local y = -CARD_TYPES.grid_span / 2 + row * GRID.cell
        grid:line {
            from = {FIELD.left, y}, to = {FIELD.right, y},
            color = "#263449", width = 1, opacity = 0.52,
            layer = LAYER.grid,
            id = "math-strike:grid:" .. tostring(sheet)
                .. ":row:" .. tostring(row),
        }
    end
    for cell = 1, GRID.highlights do
        local column = (cell * 5 + sheet * 3) % GRID.columns
        local row = (cell * 7 + sheet * 5) % GRID.rows
        grid:rectangle {
            center = {
                FIELD.left + (column + 0.5) * GRID.cell,
                -CARD_TYPES.grid_span / 2 + (row + 0.5) * GRID.cell,
            },
            size = {GRID.cell - 6, GRID.cell - 6},
            fill = CARD_TYPES.grid_cell_colors[(cell + sheet - 2)
                % #CARD_TYPES.grid_cell_colors + 1],
            opacity = 0.30 + (cell % 3) * 0.07,
            layer = LAYER.grid + 1,
            id = "math-strike:grid:" .. tostring(sheet)
                .. ":cell:" .. tostring(cell),
        }
    end
    grid:move_to {0, initial_y}
    grid_sheets[sheet] = {
        object = grid,
        phase = phase,
        base_y = initial_y,
        state = {
            origin = {0, initial_y}, shift = {0, 0, 0}, opacity = 1,
        },
    }
end

scene:rectangle {
    center = {0, 575}, size = {720, 130},
    fill = "#071426F4", layer = LAYER.text - 1,
    id = "math-strike:top-panel",
}

scene:rectangle {
    center = {0, -565}, size = {720, 150},
    fill = "#071426E8", layer = LAYER.text - 1,
    id = "math-strike:bottom-panel",
}

scene:line {
    from = {-360, -490},
    to = {360, -490},
    color = "#FFFFFF22",
    width = 1,
    layer = LAYER.structure,
    id = "math-strike:hud-rule",
}
CARD_TYPES.hud_label = function(text, x, id)
    scene:text {
        text = text, point = {x, HUD_Y}, align = {0, 0.5}, role = "code",
        fill = "muted", layer = LAYER.text, id = "math-strike:" .. id .. "-label",
    }
end
CARD_TYPES.hud_label("HP", -298, "hp")
CARD_TYPES.hud_label("KILL", -165, "score")
CARD_TYPES.hud_label("BREACH", 8, "breach")
CARD_TYPES.hud_label("THREAT", 181, "threat")

local function authored_number(prefix, x, y, digits, color, initial)
    local displays = {}
    for place = 1, digits do
        local target_x = x + (place - 1) * 18
        local display = scene:group {id = prefix .. ":" .. tostring(place)}
        for digit = 0, 9 do
            display:text {
                text = tostring(digit),
                point = {0, (digit - 4.5) * HEIGHT * 2},
                align = {0.5, 0.5}, role = "code", fill = color,
                layer = LAYER.text, id = prefix .. ":" .. tostring(place)
                    .. ":digit:" .. tostring(digit),
            }
        end
        display:move_to {target_x, HEIGHT * 12}
        displays[place] = {
            object = display, base_y = HEIGHT * 12, target_y = y,
            state = {
                origin = {target_x, HEIGHT * 12}, shift = {0, 0, 0}, opacity = 0,
            },
        }
    end
    CARD_TYPES.set_number(displays, initial)
    return displays
end

local life_number = authored_number("math-strike:life", -248, HUD_Y, 3, "success", START_HP)
local score_number = authored_number("math-strike:score", -104, HUD_Y, 3, "result", 0)
local breach_number = authored_number("math-strike:breach", 104, HUD_Y, 2, "danger", 0)
local threat_number = authored_number("math-strike:threat", 274, HUD_Y, 2, "warning", 3)

scene:text {
    text = "WEAPON", point = {-336, -550}, align = {0, 0.5}, role = "code",
    fill = "muted", layer = LAYER.text, id = "math-strike:weapon-label",
}

CARD_TYPES.weapon_silhouette = function(parent, weapon_kind, prefix, scale, layer)
    local size = scale or 1
    local paint_layer = layer or LAYER.overlay + 4
    local indicator = parent:group {id = prefix}
    if weapon_kind == 1 then
        indicator:rectangle {
            center = {0, 0}, size = {7 * size, 20 * size}, corner = 2,
            fill = "accent", layer = paint_layer,
            id = prefix .. ":short-laser",
        }
    elseif weapon_kind == 2 then
        for ray = -1, 1 do
            indicator:line {
                from = {0, -7 * size}, to = {ray * 8 * size, 9 * size},
                color = "result", width = 2 * size, layer = paint_layer,
                id = prefix .. ":missile:" .. tostring(ray),
            }
        end
    elseif weapon_kind == 3 then
        indicator:circle {
            center = {0, 0}, radius = 9 * size, fill = "#00000000",
            stroke = "secondary", width = 2 * size, layer = paint_layer,
            id = prefix .. ":ball-ring",
        }
        indicator:circle {
            center = {0, 0}, radius = 3 * size, fill = "secondary",
            layer = paint_layer + 1, id = prefix .. ":ball-core",
        }
    elseif weapon_kind == 4 then
        indicator:rectangle {
            center = {0, 0}, size = {6 * size, 29 * size}, corner = 1,
            fill = "foreground", stroke = "danger", width = 1.5 * size,
            layer = paint_layer, id = prefix .. ":long-laser",
        }
    else
        for bullet = -1, 1 do
            indicator:circle {
                center = {bullet * 8 * size, math.abs(bullet) * -3 * size},
                radius = 3.5 * size, fill = "info", layer = paint_layer,
                id = prefix .. ":pulse:" .. tostring(bullet),
            }
        end
    end
    return indicator
end

local weapon_indicators = {}
do
    local display = scene:group {id = "math-strike:weapon:1"}
    for i = 1, #CARD_TYPES[6].weapons do
        local indicator = CARD_TYPES.weapon_silhouette(
            display, i, "math-strike:weapon-icon:" .. tostring(i), 1, LAYER.effect
        )
        indicator:move_to {0, (i - 1) * HEIGHT * 2}
    end
    display:move_to {-222, -550}
    weapon_indicators[1] = {
        object = display,
        state = {
            origin = {-222, -550}, shift = {0, HEIGHT * 4, 0}, opacity = 1,
        },
    }
end

scene:text {
    text = "POWER", point = {-112, -550}, align = {0, 0.5}, role = "code",
    fill = "muted", layer = LAYER.text, id = "math-strike:power-label",
}
local power_number = authored_number("math-strike:power", -24, -550, 1, "accent", 1)

scene:text {
    text = "ROUND", point = {42, -550}, align = {0, 0.5}, role = "code",
    fill = "muted", layer = LAYER.text, id = "math-strike:round-label",
}
local round_number = authored_number("math-strike:round", 132, -550, 2, "info", 1)

CARD_TYPES.aim = {
    object = battlefield:line {
        from = {PLAYER_HOME.x, PLAYER_HOME.y},
        to = {PLAYER_HOME.x + 1, PLAYER_HOME.y},
        color = "#C8F3FF99", width = 1.2,
        layer = LAYER.enemy_shot - 1, id = "math-strike:aim-guide",
    },
    state = {
        origin = {PLAYER_HOME.x, PLAYER_HOME.y}, shift = {0, 0, 0},
        rotation = math.pi / 2, scale = {1, 1, 1}, opacity = 1,
    },
}

scene:text {
    text = "MOUSE  AIM · CLICK FIRE · HOLD + RELEASE  1–5× SHOT",
    point = {336, -584}, align = {1, 0.5}, role = "code",
    fill = "foreground", layer = LAYER.text, id = "math-strike:controls-fire",
}
scene:text {
    text = "FINAL = KILLS × 100 − BREACH × 75     R  RESTART",
    point = {336, -614}, align = {1, 0.5}, role = "code",
    fill = "muted", layer = LAYER.text, id = "math-strike:score-formula",
}

CARD_TYPES.player_craft = function(id, color, core_stroke, layer, core_width)
    local craft = battlefield:group {id = id}
    craft:polygon {
        points = {{0, 27}, {-18, -18}, {0, -10}, {18, -18}},
        fill = "background", stroke = color, width = 3,
        layer = layer, id = id .. ":hull",
    }
    craft:polygon {
        points = {{0, 15}, {-7, -8}, {0, -3}, {7, -8}},
        fill = color, stroke = color, width = 1,
        layer = layer + 1, id = id .. ":vector",
    }
    craft:circle {
        center = {0, 0}, radius = 4, fill = "foreground", stroke = core_stroke,
        width = core_width, layer = layer + 2, id = id .. ":core",
    }
    return craft
end

local player = CARD_TYPES.player_craft(
    "math-strike:player", "accent", "background", LAYER.actor, 1
)
local player_hot = CARD_TYPES.player_craft(
    "math-strike:player-hot", "danger", "danger", LAYER.actor + 3, 1.5
)
player_hot:move_to {PLAYER_HOME.x, PLAYER_HOME.y}
local stamina_curves = {}
local stamina_bases = {}
local stamina_colors = {"success", "warning", "danger"}
for i = 1, 3 do
    local base_x = i == 1 and PLAYER_HOME.x or STAGE_X
    local base_y = i == 1 and PLAYER_HOME.y or STAGE_Y
    stamina_bases[i] = {x = base_x, y = base_y}
    stamina_curves[i] = battlefield:curve {
        from = {base_x + 25, base_y - 20},
        control1 = {base_x + 39, base_y - 7},
        control2 = {base_x + 39, base_y + 20},
        to = {base_x + 25, base_y + 34},
        samples = 18, color = stamina_colors[i], width = 4,
        progress = 1,
        layer = LAYER.effect + i,
        id = "math-strike:stamina:" .. tostring(i),
    }
end
local overheat_label = battlefield:text {
    text = "OVERHEAT", point = {STAGE_X, STAGE_Y}, align = {0, 0.5}, role = "code",
    fill = "danger", layer = LAYER.effect + 4,
    id = "math-strike:stamina:overheat",
}
player:move_to {PLAYER_HOME.x, PLAYER_HOME.y}

local charge_visuals = {}
charge_visuals.base = battlefield:polygon {
    points = {
        {STAGE_X, STAGE_Y + 35},
        {STAGE_X - 5, STAGE_Y + 23},
        {STAGE_X + 5, STAGE_Y + 23},
    },
    fill = "result",
    layer = LAYER.effect + 1, id = "math-strike:charge",
}
charge_visuals.cool = battlefield:group {id = "math-strike:charge:ring"}
charge_visuals.cool:circle {
    center = {0, 0}, radius = 25, fill = "#00000000",
    stroke = "focus", width = 2.5, layer = LAYER.effect,
    id = "math-strike:charge:ring:body",
}
charge_visuals.cool:move_to {STAGE_X, STAGE_Y}
charge_visuals.hot = battlefield:group {id = "math-strike:charge:ring-hot"}
charge_visuals.hot:circle {
    center = {0, 0}, radius = 25, fill = "#00000000",
    stroke = "danger", width = 3, layer = LAYER.effect + 2,
    id = "math-strike:charge:ring-hot:body",
}
charge_visuals.hot:move_to {STAGE_X, STAGE_Y}

local function pattern_role(pattern)
    if pattern == "ring" then return "danger" end
    if pattern == "aim" then return "warning" end
    if pattern == "spiral" then return "secondary" end
    return "result"
end

local function enemy_archetype(index)
    local choice = (index - 1) % 5
    if choice == 0 then return "ring", "boid", "danger" end
    if choice == 1 then return "aim", "dance", "warning" end
    if choice == 2 then return "spiral", "sentinel", "secondary" end
    if choice == 3 then return "direct", "recoil", "info" end
    return "heavy", "sentinel", "result"
end

local enemies = {}
for i = 1, MAX_ENEMIES do
    local pattern, behavior, color = enemy_archetype(i)
    local enemy = battlefield:group {id = "math-strike:enemy:" .. tostring(i)}
    enemy:polygon {
        points = {{0, -18}, {17, -7}, {13, 15}, {0, 9}, {-13, 15}, {-17, -7}},
        fill = "background", stroke = color, width = 2.5,
        layer = LAYER.actor, id = "math-strike:enemy:" .. tostring(i) .. ":hull",
    }
    enemy:circle {
        center = {0, -1}, radius = 5, fill = color, stroke = "background",
        width = 1, layer = LAYER.actor + 1,
        id = "math-strike:enemy:" .. tostring(i) .. ":core",
    }
    enemy:line {
        from = {-10, 21}, to = {10, 21}, color = "border", width = 3,
        layer = LAYER.actor, id = "math-strike:enemy:" .. tostring(i) .. ":health-track",
    }
    local health = enemy:line {
        from = {-10, 21}, to = {10, 21}, color = color, width = 2,
        layer = LAYER.actor + 1, id = "math-strike:enemy:" .. tostring(i) .. ":health",
    }
    enemy:move_to {STAGE_X, STAGE_Y}
    enemies[i] = {
        object = enemy, health = health, base_x = STAGE_X, base_y = STAGE_Y,
        x = STAGE_X, y = STAGE_Y, anchor_x = 0, anchor_y = 260, start_x = 0,
        path_x = STAGE_X, path_y = STAGE_Y, vx = 0, vy = 0,
        recoil_lock = 0, propulsion_timer = 0, propulsion_shots = 0,
        phase_seed = (i - 1) * TAU / MAX_ENEMIES,
        trait = color, pattern = pattern, behavior = behavior,
        damage = 1, projectile_scale = 1,
        scenario = 1, phase = "inactive", phase_time = 0, active = false,
        hit_time = 0, death_burst = false, death_respawn = 0,
        hp = 1, max_hp = 1, respawn = i * 0.45,
        cooldown = 0.6, volley = 0,
        state = {
            origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0}, rotation = 0,
            scale = {1, 1, 1}, opacity = 0,
        },
        health_state = {progress = 1, opacity = 0},
    }
end

local player_shots = {}
for i = 1, PLAYER_SHOT_COUNT do
    local weapon_kind = i <= 10 and 1
        or (i <= 30 and 2 or (i <= 42 and 3 or (i <= 43 and 4 or 5)))
    local id = "math-strike:player-shot:" .. tostring(i)
    local shot = battlefield:group {id = id}
    local base_fill, beam_line, beam_hot_line, heat_object, heat_state
    if weapon_kind == 1 then
        base_fill = "accent"
        shot:rectangle {
            center = {0, 0}, size = {4, 34}, corner = 1,
            fill = base_fill, stroke = "foreground", width = 1.25,
            layer = LAYER.player_shot,
            id = id .. ":laser",
        }
        shot:rectangle {
            center = {0, 0}, size = {7, 48}, corner = 2,
            fill = base_fill,
            opacity = 0.42, layer = LAYER.player_shot - 1,
            id = id .. ":laser-glow",
        }
        heat_object = shot:group {id = id .. ":laser-heat"}
        heat_object:rectangle {
            center = {0, 0}, size = {7, 48}, corner = 2,
            fill = "danger", opacity = 0.88,
            layer = LAYER.player_shot + 1, id = id .. ":laser-heat:glow",
        }
        heat_object:rectangle {
            center = {0, 0}, size = {4, 34}, corner = 1,
            fill = "danger", stroke = "foreground", width = 1.25,
            layer = LAYER.player_shot + 2, id = id .. ":laser-heat:core",
        }
        heat_state = {opacity = 0}
    elseif weapon_kind == 2 then
        base_fill = "result"
        shot:polygon {
            points = {{0, 11}, {6, 1}, {3, -7}, {-3, -7}, {-6, 1}},
            fill = base_fill, stroke = "foreground", width = 1,
            layer = LAYER.player_shot,
            id = id .. ":missile",
        }
        shot:circle {
            center = {0, -8}, radius = 3, fill = "accent",
            layer = LAYER.player_shot - 1,
            id = id .. ":missile-drive",
        }
    elseif weapon_kind == 3 then
        base_fill = "background"
        shot:circle {
            center = {0, 0}, radius = 7, fill = "background",
            stroke = "secondary", width = 3, layer = LAYER.player_shot,
            id = id .. ":ball",
        }
        shot:circle {
            center = {-2, 2}, radius = 2.5, fill = "foreground",
            layer = LAYER.player_shot + 1,
            id = id .. ":ball-core",
        }
    elseif weapon_kind == 4 then
        base_fill = "foreground"
        local beam = shot:group {id = id .. ":long-laser"}
        beam_line = beam:line {
            from = {STAGE_X, STAGE_Y},
            to = {STAGE_X + 1, STAGE_Y},
            color = base_fill, width = 5.5,
            layer = LAYER.text + 2, id = id .. ":long-laser:beam",
        }
        beam_hot_line = beam:line {
            from = {STAGE_X, STAGE_Y},
            to = {STAGE_X + 1, STAGE_Y},
            color = "danger", width = 7.5,
            layer = LAYER.text + 3, id = id .. ":long-laser:beam-hot",
        }
    else
        base_fill = "info"
        shot:circle {
            center = {0, 0}, radius = 5, fill = base_fill,
            stroke = "foreground", width = 1.5,
            layer = LAYER.player_shot, id = id .. ":bullet",
        }
        shot:circle {
            center = {-1.5, 1.5}, radius = 1.5, fill = "foreground",
            layer = LAYER.player_shot + 1, id = id .. ":bullet-core",
        }
    end
    if weapon_kind ~= 4 then shot:move_to {STAGE_X, STAGE_Y} end
    player_shots[i] = {
        object = shot, weapon_kind = weapon_kind, base_fill = base_fill,
        beam_line = beam_line, beam_hot_line = beam_hot_line,
        heat_object = heat_object, heat_state = heat_state,
        base_x = STAGE_X, base_y = STAGE_Y,
        x = STAGE_X, y = STAGE_Y, vx = 0, vy = 0, turn = 0,
        angle = math.pi / 2, radius = 6, spawn_radius = 6,
        beam_half = weapon_kind == 1 and 17 or (weapon_kind == 4 and 95 or 0),
        damage = 1, max_damage = 1, strength = 1, max_strength = 1, pierce = 0,
        scale = 1, scale_x = 1, scale_y = 1, spawn_scale = 1,
        spawn_scale_x = 1, spawn_scale_y = 1,
        age = 0, orbit_time = 0, orbit_angle = 0, orbit_radius = 0, charge_heat = 0,
        beam_origin_x = 0, beam_origin_y = 0, beam_length = 0,
        visible_length = 0, visual_opacity = 1,
        bounces = 0, hit_lock = 0, last_enemy = 0, active = false,
        state = {
            origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0}, rotation = 0,
            scale = {1, 1, 1}, opacity = 0, fill = base_fill,
        },
        beam_state = {
            origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0}, rotation = 0,
            scale = {1, 1, 1}, opacity = 0,
        },
        beam_hot_state = {
            origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0}, rotation = 0,
            scale = {1, 1, 1}, opacity = 0,
        },
    }
end

local enemy_shot_pools = {ring = {}, aim = {}, spiral = {}}
local function create_enemy_shot_pool(kind, count)
    local color = pattern_role(kind)
    local pool = enemy_shot_pools[kind]
    for i = 1, count do
        local shot = battlefield:group {id = "math-strike:enemy-shot:" .. kind .. ":" .. tostring(i)}
        if kind == "ring" then
            shot:circle {
                center = {0, 0}, radius = 5.5, fill = "background",
                stroke = color, width = 2, layer = LAYER.enemy_shot,
                id = "math-strike:enemy-shot:ring:" .. tostring(i) .. ":ring",
            }
            shot:circle {
                center = {0, 0}, radius = 2, fill = color,
                layer = LAYER.enemy_shot + 1,
                id = "math-strike:enemy-shot:ring:" .. tostring(i) .. ":core",
            }
        elseif kind == "aim" then
            shot:polygon {
                points = {{0, 7}, {5, 0}, {0, -7}, {-5, 0}},
                fill = color, stroke = "background", width = 1,
                layer = LAYER.enemy_shot,
                id = "math-strike:enemy-shot:aim:" .. tostring(i) .. ":body",
            }
        else
            shot:circle {
                center = {0, 0}, radius = 4.5, fill = color,
                stroke = "background", width = 1, layer = LAYER.enemy_shot,
                id = "math-strike:enemy-shot:spiral:" .. tostring(i) .. ":core",
            }
            shot:line {
                from = {-8, 0}, to = {-4, 0}, color = color, width = 2,
                layer = LAYER.enemy_shot,
                id = "math-strike:enemy-shot:spiral:" .. tostring(i) .. ":tail",
            }
        end
        shot:move_to {STAGE_X, STAGE_Y}
        pool[i] = {
            object = shot, base_x = STAGE_X, base_y = STAGE_Y,
            x = STAGE_X, y = STAGE_Y, vx = 0, vy = 0, turn = 0, angle = 0,
            base_radius = kind == "aim" and 6 or 5.5,
            radius = kind == "aim" and 6 or 5.5,
            spawn_radius = kind == "aim" and 6 or 5.5,
            damage = 1, max_damage = 1, strength = 1, max_strength = 1,
            scale = 1, scale_x = 1, scale_y = 1, spawn_scale = 1,
            spawn_scale_x = 1, spawn_scale_y = 1,
            active = false,
            state = {
                origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0},
                rotation = 0, scale = {1, 1, 1}, opacity = 0,
            },
        }
    end
end
create_enemy_shot_pool("ring", ENEMY_SHOT_COUNT.ring)
create_enemy_shot_pool("aim", ENEMY_SHOT_COUNT.aim)
create_enemy_shot_pool("spiral", ENEMY_SHOT_COUNT.spiral)

local impacts = {}
for i = 1, IMPACT_COUNT do
    local impact = battlefield:group {id = "math-strike:impact:" .. tostring(i)}
    impact:circle {
        center = {0, 0}, radius = 13, fill = "background",
        stroke = "result", width = 2, layer = LAYER.effect,
        id = "math-strike:impact:" .. tostring(i) .. ":ring",
    }
    impact:line {
        from = {-18, 0}, to = {18, 0}, color = "result", width = 1.5,
        layer = LAYER.effect, id = "math-strike:impact:" .. tostring(i) .. ":x",
    }
    impact:line {
        from = {0, -18}, to = {0, 18}, color = "result", width = 1.5,
        layer = LAYER.effect, id = "math-strike:impact:" .. tostring(i) .. ":y",
    }
    impact:move_to {STAGE_X, STAGE_Y}
    local death_flash = battlefield:group {
        id = "math-strike:death-flash:" .. tostring(i),
    }
    death_flash:line {
        from = {0, -76}, to = {0, 76}, color = "foreground", width = 5.5,
        layer = LAYER.effect + 3,
        id = "math-strike:death-flash:" .. tostring(i) .. ":vertical",
    }
    death_flash:line {
        from = {-42, 0}, to = {42, 0}, color = "result", width = 4.5,
        layer = LAYER.effect + 4,
        id = "math-strike:death-flash:" .. tostring(i) .. ":horizontal",
    }
    death_flash:circle {
        center = {0, 0}, radius = 24, fill = "#00000000",
        stroke = "accent", width = 2.5, layer = LAYER.effect + 2,
        id = "math-strike:death-flash:" .. tostring(i) .. ":halo",
    }
    death_flash:circle {
        center = {0, 0}, radius = 7, fill = "foreground", stroke = "result",
        width = 2, layer = LAYER.effect + 5,
        id = "math-strike:death-flash:" .. tostring(i) .. ":core",
    }
    death_flash:move_to {STAGE_X, STAGE_Y}
    impacts[i] = {
        object = impact, death_object = death_flash,
        base_x = STAGE_X, base_y = STAGE_Y,
        x = STAGE_X, y = STAGE_Y, age = 0, duration = 0.28,
        kind = "hit", active = false,
        state = {
            origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0}, rotation = 0,
            scale = {1, 1, 1}, opacity = 0,
        },
        death_state = {
            origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0}, rotation = 0,
            scale = {1, 1, 1}, opacity = 0,
        },
    }
end

scene:line {
    from = {238, -550}, to = {336, -550}, color = "border", width = 7,
    opacity = 0.35, layer = LAYER.structure, id = "math-strike:dash-track",
}
scene:text {
    text = "DASH", point = {190, -550}, align = {0, 0.5}, role = "code",
    fill = "muted", layer = LAYER.text, id = "math-strike:dash-label",
}
local dash_gauge = scene:line {
    from = {238, -550}, to = {336, -550}, color = "accent", width = 4,
    layer = LAYER.effect, id = "math-strike:dash-gauge",
}

local CARD_POSITIONS = {
    {x = -204, y = -16},
    {x = 0, y = -16},
    {x = 204, y = -16},
}

local level_overlay = scene:group {id = "math-strike:level-up", opacity = 0}
level_overlay:rectangle {
    center = {0, -10}, size = {632, 1016}, corner = 1,
    fill = "background", stroke = "border", width = 1.5,
    opacity = 0.94, layer = LAYER.overlay,
    id = "math-strike:level-up:backdrop",
}
local level_up_burst = level_overlay:group {id = "math-strike:level-up:burst"}
level_up_burst:circle {
    center = {0, 260}, radius = 54, fill = "#00000000",
    stroke = "result", width = 2.5, opacity = 0.72,
    layer = LAYER.overlay + 1, id = "math-strike:level-up:burst:inner",
}
level_up_burst:circle {
    center = {0, 260}, radius = 82, fill = "#00000000",
    stroke = "accent", width = 1.5, opacity = 0.42,
    layer = LAYER.overlay + 1, id = "math-strike:level-up:burst:outer",
}
for ray = 0, 11 do
    local angle = ray * TAU / 12
    level_up_burst:line {
        from = {math.cos(angle) * 91, 260 + math.sin(angle) * 91},
        to = {math.cos(angle) * 112, 260 + math.sin(angle) * 112},
        color = ray % 2 == 0 and "result" or "accent", width = 2,
        opacity = 0.60, layer = LAYER.overlay + 2,
        id = "math-strike:level-up:burst:ray:" .. tostring(ray),
    }
end
level_overlay:text {
    text = "LEVEL UP // CHOOSE ONE",
    point = {0, 260}, align = {0.5, 0.5}, role = "h2",
    fill = "foreground", layer = LAYER.overlay + 4,
    id = "math-strike:level-up:title",
}
level_overlay:text {
    text = "COMBAT PAUSED · REVIEW, THEN CLICK OR PRESS 1 / 2 / 3",
    point = {0, 220}, align = {0.5, 0.5}, role = "code",
    fill = "muted", layer = LAYER.overlay + 4,
    id = "math-strike:level-up:hint",
}
level_overlay:line {
    from = {-278, 190}, to = {278, 190}, color = "border", width = 1.5,
    layer = LAYER.overlay + 1, id = "math-strike:level-up:rule",
}
local level_ready_line = level_overlay:line {
    from = {-278, 190}, to = {278, 190}, color = "result", width = 2.5,
    layer = LAYER.overlay + 2, id = "math-strike:level-up:ready",
}
level_overlay:move_to {0, CARD_STAGE_Y}
local level_overlay_state = {
    origin = {0, CARD_STAGE_Y}, shift = {0, -CARD_STAGE_Y, 0},
    scale = {1, 1, 1}, opacity = 0,
}
local level_up_burst_state = {
    origin = {0, 260}, scale = {1, 1, 1}, rotation = 0, opacity = 0,
}
local level_ready_state = {progress = 0, opacity = 0}

local function card_icon(card, type_index, color, prefix)
    if type_index == 1 then
        card:line {
            from = {-13, 68}, to = {13, 68}, color = color, width = 5,
            layer = LAYER.overlay + 4, id = prefix .. ":icon-x",
        }
        card:line {
            from = {0, 55}, to = {0, 81}, color = color, width = 5,
            layer = LAYER.overlay + 4, id = prefix .. ":icon-y",
        }
    elseif type_index == 2 then
        card:polygon {
            points = {{-6, 86}, {14, 72}, {3, 68}, {9, 51}, {-14, 67}, {-3, 71}},
            fill = color, layer = LAYER.overlay + 4, id = prefix .. ":icon",
        }
    elseif type_index == 3 then
        card:circle {
            center = {0, 68}, radius = 18, fill = "background",
            stroke = color, width = 2.5, layer = LAYER.overlay + 4,
            id = prefix .. ":icon-shell",
        }
        card:circle {
            center = {0, 68}, radius = 5, fill = color,
            layer = LAYER.overlay + 5, id = prefix .. ":icon-core",
        }
        for ray = 0, 2 do
            local angle = ray * TAU / 3 + math.pi / 2
            card:line {
                from = {math.cos(angle) * 8, 68 + math.sin(angle) * 8},
                to = {math.cos(angle) * 17, 68 + math.sin(angle) * 17},
                color = color, width = 2, layer = LAYER.overlay + 5,
                id = prefix .. ":icon-ray:" .. tostring(ray),
            }
        end
    elseif type_index == 4 then
        card:polygon {
            points = {{0, 88}, {18, 68}, {7, 68}, {18, 48}, {0, 59}, {-18, 48}, {-7, 68}, {-18, 68}},
            fill = "background", stroke = color, width = 2.5,
            layer = LAYER.overlay + 4, id = prefix .. ":icon",
        }
    elseif type_index == 5 then
        card:rectangle {
            center = {0, 67}, size = {12, 35}, corner = 2, fill = color,
            layer = LAYER.overlay + 4, id = prefix .. ":icon-body",
        }
        card:polygon {
            points = {{0, 92}, {8, 78}, {-8, 78}}, fill = "result",
            layer = LAYER.overlay + 5, id = prefix .. ":icon-tip",
        }
    elseif type_index == 6 then
        for ray = -1, 1 do
            card:line {
                from = {0, 52}, to = {ray * 17, 84}, color = color, width = 3,
                layer = LAYER.overlay + 4,
                id = prefix .. ":icon-ray:" .. tostring(ray),
            }
        end
    else
        card:line {
            from = {-25, 68}, to = {-5, 68}, color = color, width = 3,
            layer = LAYER.overlay + 4, id = prefix .. ":icon-left",
        }
        card:line {
            from = {5, 68}, to = {25, 68}, color = color, width = 3,
            layer = LAYER.overlay + 4, id = prefix .. ":icon-right",
        }
        card:line {
            from = {-5, 58}, to = {-5, 78}, color = color, width = 2,
            layer = LAYER.overlay + 4, id = prefix .. ":icon-stop-left",
        }
        card:line {
            from = {5, 58}, to = {5, 78}, color = color, width = 2,
            layer = LAYER.overlay + 4, id = prefix .. ":icon-stop-right",
        }
    end
end

CARD_TYPES.build_face = function(
    card, slot, type_index, definition, prefix, title, line1, offset_y
)
    local face = card:group {id = prefix .. ":face"}
    face:rectangle {
        center = {0, 0}, size = {184, 300}, corner = 3,
        fill = "surface", stroke = definition.color, width = 2,
        layer = LAYER.overlay + 2, id = prefix .. ":body",
    }
    face:line {
        from = {-76, 136}, to = {76, 136}, color = definition.color, width = 3,
        layer = LAYER.overlay + 3, id = prefix .. ":accent",
    }
    face:text {
        text = tostring(slot), point = {-69, 111}, align = {0.5, 0.5},
        role = "code", fill = definition.color, layer = LAYER.overlay + 4,
        id = prefix .. ":slot",
    }
    face:text {
        text = definition.category, point = {65, 111}, align = {1, 0.5},
        role = "code", fill = definition.color, layer = LAYER.overlay + 4,
        id = prefix .. ":category",
    }
    if type_index ~= 6 then card_icon(face, type_index, definition.color, prefix) end
    if title then
        face:text {
            text = title, point = {0, 22}, align = {0.5, 0.5},
            role = "code", fill = "foreground", layer = LAYER.overlay + 4,
            id = prefix .. ":title",
        }
    end
    if line1 then
        face:text {
            text = line1, point = {0, -30}, align = {0.5, 0.5},
            role = "code", fill = "foreground", layer = LAYER.overlay + 4,
            id = prefix .. ":line-1",
        }
    end
    face:text {
        text = definition.line2, point = {0, -58}, align = {0.5, 0.5},
        role = "code", fill = "muted", layer = LAYER.overlay + 4,
        id = prefix .. ":line-2",
    }
    face:text {
        text = "SELECT", point = {0, -116}, align = {0.5, 0.5},
        role = "code", fill = definition.color, layer = LAYER.overlay + 4,
        id = prefix .. ":select",
    }
    face:move_to {0, offset_y or 0}
end

local card_faces = {}
for slot = 1, 3 do
    card_faces[slot] = {}
    for type_index = 1, #CARD_TYPES do
        local definition = CARD_TYPES[type_index]
        local prefix = "math-strike:card:" .. tostring(slot) .. ":" .. definition.key
        local card = scene:group {id = prefix, opacity = 0}
        local title_wheel, level_wheel
        if type_index == 6 then
            CARD_TYPES.build_face(
                card, slot, type_index, definition, prefix, nil, nil, 0
            )
            local title_object = card:group {id = prefix .. ":title-wheel"}
            for weapon_kind, weapon in ipairs(definition.weapons) do
                local offset = (weapon_kind - 3) * HEIGHT * 2
                local silhouette = CARD_TYPES.weapon_silhouette(
                    title_object, weapon_kind,
                    prefix .. ":target:" .. tostring(weapon_kind) .. ":bullet-image",
                    1.12, LAYER.overlay + 4
                )
                silhouette:move_to {0, offset + 68}
                title_object:text {
                    text = "EQUIP\n" .. weapon.name,
                    point = {0, offset},
                    align = {0.5, 0.5}, role = "code", fill = "foreground",
                    layer = LAYER.overlay + 4,
                    id = prefix .. ":target:" .. tostring(weapon_kind) .. ":title",
                }
            end
            title_object:move_to {0, 22}
            title_wheel = {
                object = title_object,
                state = {origin = {0, 22}, shift = {0, 0, 0}, opacity = 1},
            }
            local level_object = card:group {id = prefix .. ":level-wheel"}
            for weapon_level = 1, MAX_POWER do
                local detail = weapon_level == 1 and "ACQUIRE · LEVEL 1"
                    or "LEVEL " .. tostring(weapon_level - 1)
                        .. " → " .. tostring(weapon_level)
                level_object:text {
                    text = detail,
                    point = {0, (weapon_level - 3) * HEIGHT * 2},
                    align = {0.5, 0.5}, role = "code", fill = "foreground",
                    layer = LAYER.overlay + 4,
                    id = prefix .. ":level:" .. tostring(weapon_level) .. ":line-1",
                }
            end
            level_object:move_to {0, -30}
            level_wheel = {
                object = level_object,
                state = {origin = {0, -30}, shift = {0, 0, 0}, opacity = 1},
            }
        else
            CARD_TYPES.build_face(
                card, slot, type_index, definition, prefix,
                definition.title, definition.line1, 0
            )
        end
        card:move_to {0, CARD_STAGE_Y}
        card_faces[slot][type_index] = {
            object = card,
            title_wheel = title_wheel,
            level_wheel = level_wheel,
            state = {
                origin = {0, CARD_STAGE_Y},
                shift = {CARD_POSITIONS[slot].x, CARD_POSITIONS[slot].y - CARD_STAGE_Y, 0},
                scale = {1, 1, 1}, opacity = 0,
            },
        }
    end
end

local card_focuses = {}
for slot = 1, 3 do
    local prefix = "math-strike:card-focus:" .. tostring(slot)
    local focus = scene:group {id = prefix, opacity = 0}
    focus:rectangle {
        center = {0, 0}, size = {196, 312}, corner = 4,
        fill = "#00000000", stroke = "focus", width = 2,
        layer = LAYER.overlay + 6, id = prefix .. ":frame",
    }
    focus:line {
        from = {-38, 158}, to = {38, 158}, color = "result", width = 3,
        layer = LAYER.overlay + 7, id = prefix .. ":top",
    }
    focus:line {
        from = {-38, -158}, to = {38, -158}, color = "result", width = 3,
        layer = LAYER.overlay + 7, id = prefix .. ":bottom",
    }
    focus:move_to {0, CARD_STAGE_Y}
    card_focuses[slot] = {
        object = focus,
        state = {
            origin = {0, CARD_STAGE_Y},
            shift = {CARD_POSITIONS[slot].x, CARD_POSITIONS[slot].y - CARD_STAGE_Y, 0},
            scale = {1, 1, 1}, opacity = 0,
        },
    }
end

local game_over = scene:group {id = "math-strike:game-over"}
local game_over_outer = game_over:rectangle {
    center = {0, 0}, size = {520, 150}, corner = 2,
    fill = "#ffffff01", stroke = "#00000000", width = 1,
    layer = LAYER.overlay, id = "math-strike:game-over:outer",
}
local game_over_inner = game_over:rectangle {
    center = {0, 0}, size = {516, 146}, corner = 1,
    fill = "#ffffff01", stroke = "#00000000", width = 1,
    layer = LAYER.overlay + 1, id = "math-strike:game-over:inner",
}
local game_over_title = game_over:text {
    text = "SECTOR DEFENSE FAILED", point = {0, 28}, align = {0.5, 0.5},
    role = "h3", fill = "#ffffff01", layer = LAYER.overlay + 2,
    id = "math-strike:game-over:title",
}
local game_over_hint = game_over:text {
    text = "FINAL = KILLS × 100 − BREACH × 75     R  RESTART",
    point = {0, -27}, align = {0.5, 0.5}, role = "code",
    fill = "#ffffff01", layer = LAYER.overlay + 2,
    id = "math-strike:game-over:hint",
}
game_over:move_to {STAGE_X, STAGE_Y}

vis.input.controller(scene)

local player_x, player_y = PLAYER_HOME.x, PLAYER_HOME.y
local player_vx, player_vy = 0, 0
local aim_angle = math.pi / 2
local dash_x, dash_y = 0, 1
local dash_charge, dash_time, dash_invulnerable = 1, 0, 0
local hit_invulnerable = TUNING.initial_recovery
local stamina, stamina_capacity, stamina_delay, overheated = 1, 1, 0, false
local charge_time, charge_flash, player_recoil_lock = 0, 0, 0
local charging = false
local run_time, kills, breaches, hp, max_hp = 0, 0, 0, START_HP, START_HP
local power_level, weapon_type = 1, 1
local player_damage, recoil_factor, shake_factor = 1, 1, 1
local dash_recovery_bonus, dash_immunity_bonus = 0, 0
local level, next_level_kills = 1, experience_to_next_level(1)
local choosing_card, card_time, card_hover = false, 0, 0
local card_pointer_slot = 0
local card_hover_amount = {0, 0, 0}
local card_choices = {5, 1, 2}
local unlocked_enemies = 3
local game_is_over = false
local spawn_serial = 0
local cancelled_bullets = 0
local player_shot_cursor = 1
local enemy_shot_cursors = {ring = 1, aim = 1, spiral = 1}
local impact_cursor = 1
local shake_time, shake_duration, shake_strength, shake_phase = 0, 0, 0, 0
local shake_x, shake_y, shake_rotation = 0, 0, 0

local battlefield_state = {
    origin = {0, 0}, shift = {0, 0, 0}, rotation = 0, opacity = 1,
}

local player_state = {
    origin = {PLAYER_HOME.x, PLAYER_HOME.y}, shift = {0, 0, 0},
    rotation = 0, opacity = 1,
}
local player_hot_state = {
    origin = {PLAYER_HOME.x, PLAYER_HOME.y}, shift = {0, 0, 0},
    rotation = 0, opacity = 0,
}
local charge_state = {
    origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0},
    rotation = 0, scale = {1, 1, 1}, opacity = 0,
}
local charge_cool_state = {
    origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0},
    rotation = 0, scale = {1, 1, 1}, opacity = 0,
}
local charge_hot_state = {
    origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0},
    rotation = 0, scale = {1, 1, 1}, opacity = 0,
}
local stamina_states = {
    {
        origin = {stamina_bases[1].x, stamina_bases[1].y},
        shift = {0, 0, 0}, progress = 1, opacity = 1,
    },
    {
        origin = {stamina_bases[2].x, stamina_bases[2].y},
        shift = {PLAYER_HOME.x - stamina_bases[2].x, PLAYER_HOME.y - stamina_bases[2].y, 0},
        progress = 1, opacity = 0,
    },
    {
        origin = {stamina_bases[3].x, stamina_bases[3].y},
        shift = {PLAYER_HOME.x - stamina_bases[3].x, PLAYER_HOME.y - stamina_bases[3].y, 0},
        progress = 1, opacity = 0,
    },
}
local overheat_state = {
    origin = {STAGE_X, STAGE_Y}, shift = {0, 0, 0}, opacity = 0,
}
local dash_state = {progress = 1}
local game_over_state = {
    origin = {STAGE_X, STAGE_Y}, shift = {-STAGE_X, -20 - STAGE_Y, 0},
    scale = {1, 1, 1}, opacity = 0,
}
local game_over_outer_state = {fill = "danger"}
local game_over_inner_state = {fill = "background"}
local game_over_title_state = {fill = "danger"}
local game_over_hint_state = {fill = "foreground"}

local function take_slot(pool, cursor)
    local slot = pool[cursor]
    return slot, cursor % #pool + 1
end

local function take_inactive_slot(pool, cursor, kind)
    for offset = 0, #pool - 1 do
        local index = (cursor + offset - 1) % #pool + 1
        local slot = pool[index]
        if not slot.active and (not kind or slot.weapon_kind == kind) then
            return slot, index % #pool + 1
        end
    end
    return nil, cursor
end

local function spawn_impact(x, y, kind)
    local impact
    impact, impact_cursor = take_slot(impacts, impact_cursor)
    impact.x, impact.y = x, y
    impact.age = 0
    impact.kind = kind or "hit"
    impact.duration = impact.kind == "death" and 0.44 or 0.28
    impact.active = true
end

CARD_TYPES.player_projectile_strength = function(damage)
    return math.max(0, damage or 0) * TUNING.player_clash_multiplier
end

local function spawn_player_shot(
    x, y, angle, speed, damage, pierce, radius, scale, turn, charge_heat, stretch
)
    local shot
    shot, player_shot_cursor = take_inactive_slot(
        player_shots, player_shot_cursor, weapon_type
    )
    if not shot then return 0 end
    shot.x, shot.y = x, y
    shot.vx = math.cos(angle) * speed
    shot.vy = math.sin(angle) * speed
    shot.turn = weapon_type == 2 and 0 or (turn or 0)
    shot.angle = angle
    shot.damage = damage
    shot.max_damage = damage
    shot.strength = CARD_TYPES.player_projectile_strength(damage)
    shot.max_strength = shot.strength
    shot.pierce = pierce
    shot.radius = radius
    shot.spawn_radius = radius
    shot.scale = scale
    shot.scale_x = scale
    shot.scale_y = stretch or scale
    if weapon_type == 1 then
        shot.scale_y = 3.0 + power_level * 0.28 + math.max(0, scale - 1) * 0.18
    end
    shot.spawn_scale = scale
    shot.spawn_scale_x = shot.scale_x
    shot.spawn_scale_y = shot.scale_y
    shot.age = 0
    shot.orbit_angle = turn or 0
    shot.orbit_radius = weapon_type == 2 and (24 + ((player_shot_cursor - 1) % 4) * 6) or 0
    shot.orbit_time = weapon_type == 2 and (0.24 + ((player_shot_cursor - 1) % 5) * 0.045) or 0
    shot.bounces = weapon_type == 3 and (5 + power_level * 2) or 0
    shot.hit_lock = 0
    shot.last_enemy = 0
    shot.charge_heat = charge_heat or 0
    shot.visual_opacity = 1
    if weapon_type == 4 then
        shot.vx, shot.vy = 0, 0
        shot.beam_origin_x, shot.beam_origin_y = x, y
        shot.beam_length = CARD_TYPES.ray_distance(
            x, y, angle, CARD_TYPES.beam_bounds
        )
        shot.beam_target_scale_y = shot.beam_length / (shot.beam_half * 2)
        shot.scale_y = 0.01
        shot.visible_length = 0
        CARD_TYPES[6].long_active = true
    end
    shot.active = true
    return 1
end

CARD_TYPES[6].update_burst = function(dt)
    local burst = CARD_TYPES[6].burst
    if burst.remaining <= 0 or weapon_type ~= 1 then return end
    burst.timer = burst.timer - dt
    if burst.timer > 0 then return end
    local muzzle_x = player_x + math.cos(burst.angle) * 29
    local muzzle_y = player_y + math.sin(burst.angle) * 29
    local emitted = spawn_player_shot(
        muzzle_x, muzzle_y, burst.angle, TUNING.player_shot_speed * 1.35,
        burst.damage, burst.pierce, burst.radius, burst.scale, 0, burst.charge_heat
    )
    if emitted > 0 then
        burst.remaining = burst.remaining - 1
        burst.timer = 0.075
    else
        burst.remaining = 0
    end
end

CARD_TYPES.enemy_projectile_strength = function(damage, scale)
    local damage_growth = math.max(0, (damage or 8) - 8) * 0.08
    local size_growth = math.max(0, (scale or 1) - 1) * 0.15
    return 1 + damage_growth + size_growth
end

local function spawn_enemy_shot(kind, x, y, angle, speed, turn, damage, scale)
    local pool = enemy_shot_pools[kind]
    local shot
    shot, enemy_shot_cursors[kind] = take_inactive_slot(
        pool, enemy_shot_cursors[kind]
    )
    if not shot then return 0 end
    shot.x, shot.y = x, y
    shot.vx = math.cos(angle) * speed
    shot.vy = math.sin(angle) * speed
    shot.turn = turn or 0
    shot.angle = angle
    shot.damage = damage or 1
    shot.max_damage = shot.damage
    shot.scale = (scale or 1) * TUNING.enemy_shot_visual_scale
    shot.strength = CARD_TYPES.enemy_projectile_strength(shot.damage, scale or 1)
    shot.max_strength = shot.strength
    shot.scale_x = shot.scale
    shot.scale_y = shot.scale
    shot.radius = shot.base_radius * shot.scale
    shot.spawn_scale = shot.scale
    shot.spawn_scale_x = shot.scale_x
    shot.spawn_scale_y = shot.scale_y
    shot.spawn_radius = shot.radius
    shot.active = true
    return 1
end

local function emit_enemy_shot(volley, kind, x, y, angle, speed, turn, damage, scale)
    local emitted = spawn_enemy_shot(kind, x, y, angle, speed, turn, damage, scale)
    if emitted == 0 then return 0 end
    local mass = (scale or 1) * (1 + ((damage or 1) - 1) * 0.16)
    local momentum = speed * mass
    volley.x = volley.x + math.cos(angle) * momentum
    volley.y = volley.y + math.sin(angle) * momentum
    volley.total = volley.total + momentum
    volley.count = volley.count + 1
    return 1
end

local function deactivate_enemy(enemy, delay)
    enemy.active = false
    enemy.phase = "inactive"
    enemy.respawn = delay
end

local function enemy_health(enemy)
    local behavior_bonus = enemy.behavior == "sentinel" and 1
        or (enemy.behavior == "boid" and 1 or 0)
    return 2 + behavior_bonus + math.floor((level - 1) * 0.8)
        + math.floor(run_time / 42)
end

local function enemy_damage()
    return 8 + math.floor((level - 1) * 1.5) + math.floor(run_time / 28)
end

local function activate_enemy(index, initial)
    local enemy = enemies[index]
    spawn_serial = spawn_serial + 1
    enemy.active = true
    enemy.scenario = (spawn_serial + index - 2) % 3 + 1
    enemy.phase = initial and "attack" or "entry"
    enemy.phase_time = initial and (index - 1) * 0.18 or 0
    enemy.hit_time = 0
    enemy.death_burst = false
    enemy.death_respawn = 0
    enemy.max_hp = enemy_health(enemy)
    enemy.hp = enemy.max_hp
    enemy.damage = enemy_damage()
    enemy.projectile_scale = 1 + (enemy.damage - 1) * 0.12
    enemy.cooldown = initial and 0.18 + index * 0.08 or 0.55
    enemy.volley = 0
    enemy.recoil_lock = 0
    enemy.propulsion_timer = initial and index * 0.035 or 0
    enemy.propulsion_shots = 0
    local lane = (index - 1) % 5
    enemy.anchor_x = -220 + lane * 110
    enemy.anchor_y = 305 - math.floor((index - 1) / 5) * 90
    local side = (spawn_serial + index) % 2 == 0 and -1 or 1
    enemy.start_x = enemy.scenario == 2 and side * 285
        or clamp(enemy.anchor_x + side * 80, FIELD.left + 28, FIELD.right - 28)
    if initial then
        enemy.path_x = enemy.anchor_x
        enemy.path_y = enemy.behavior == "sentinel" and enemy.anchor_y - 36 or enemy.anchor_y
    else
        enemy.path_x = enemy.start_x
        enemy.path_y = FIELD.top + 54
    end
    enemy.x, enemy.y = enemy.path_x, enemy.path_y
    enemy.vx, enemy.vy = 0, 0
end

for i = 1, 3 do
    activate_enemy(i, true)
    local enemy = enemies[i]
    enemy.object:move_to {enemy.x, enemy.y}
    enemy.base_x, enemy.base_y = enemy.x, enemy.y
    enemy.state.origin[1], enemy.state.origin[2] = enemy.x, enemy.y
end

local function clear_enemy_pool(pool)
    for i = 1, #pool do pool[i].active = false end
end

local function clear_enemy_projectiles()
    clear_enemy_pool(enemy_shot_pools.ring)
    clear_enemy_pool(enemy_shot_pools.aim)
    clear_enemy_pool(enemy_shot_pools.spiral)
end

local function clear_projectiles_and_effects()
    for i = 1, #player_shots do player_shots[i].active = false end
    CARD_TYPES[6].long_active = false
    clear_enemy_projectiles()
    for i = 1, #impacts do impacts[i].active = false end
end

local function reset_game()
    player_x, player_y = PLAYER_HOME.x, PLAYER_HOME.y
    player_vx, player_vy = 0, 0
    aim_angle = math.pi / 2
    dash_x, dash_y = 0, 1
    dash_charge, dash_time, dash_invulnerable = 1, 0, 0
    hit_invulnerable = TUNING.initial_recovery
    stamina, stamina_capacity, stamina_delay, overheated = 1, 1, 0, false
    charge_time, charge_flash, player_recoil_lock, charging = 0, 0, 0, false
    run_time, kills, breaches, hp, max_hp = 0, 0, 0, START_HP, START_HP
    power_level, weapon_type = 1, 1
    player_damage, recoil_factor, shake_factor = 1, 1, 1
    dash_recovery_bonus, dash_immunity_bonus = 0, 0
    level, next_level_kills = 1, experience_to_next_level(1)
    choosing_card, card_time, card_hover = false, 0, 0
    card_pointer_slot = 0
    for slot = 1, #card_hover_amount do card_hover_amount[slot] = 0 end
    card_choices[1], card_choices[2], card_choices[3] = 5, 1, 2
    for i = 1, #CARD_TYPES[6].levels do CARD_TYPES[6].levels[i] = 1 end
    CARD_TYPES[6].offer = 2
    CARD_TYPES[6].offer_level = 1
    CARD_TYPES[6].random_state = 104729
    CARD_TYPES[6].last_offer = 0
    CARD_TYPES[6].burst.remaining, CARD_TYPES[6].burst.timer = 0, 0
    CARD_TYPES[6].long_active = false
    unlocked_enemies = 3
    game_is_over = false
    spawn_serial = 0
    cancelled_bullets = 0
    player_shot_cursor = 1
    enemy_shot_cursors.ring = 1
    enemy_shot_cursors.aim = 1
    enemy_shot_cursors.spiral = 1
    impact_cursor = 1
    shake_time, shake_duration, shake_strength, shake_phase = 0, 0, 0, 0
    shake_x, shake_y, shake_rotation = 0, 0, 0
    clear_projectiles_and_effects()
    for i = 1, #enemies do
        local enemy = enemies[i]
        enemy.active = false
        enemy.phase = "inactive"
        enemy.respawn = i * 0.42
        enemy.x, enemy.y = STAGE_X, STAGE_Y
        enemy.path_x, enemy.path_y = STAGE_X, STAGE_Y
        enemy.vx, enemy.vy = 0, 0
        enemy.recoil_lock = 0
        enemy.propulsion_timer = 0
        enemy.propulsion_shots = 0
        enemy.death_burst = false
        enemy.death_respawn = 0
    end
    for i = 1, 3 do activate_enemy(i, true) end
end

local function trigger_screen_shake(strength, duration)
    local scaled = strength * shake_factor
    if scaled >= shake_strength or shake_time <= 0 then
        shake_strength = scaled
        shake_duration = duration
        shake_time = duration
    else
        shake_time = math.max(shake_time, duration * 0.65)
    end
    shake_phase = shake_phase + 1.73
end

CARD_TYPES.mark_enemy_destroyed = function(enemy)
    if not enemy.active or enemy.phase == "death" then return false end
    kills = kills + 1
    enemy.phase = "death"
    enemy.phase_time = 0
    enemy.death_burst = false
    enemy.death_respawn = math.max(0.65, 1.55 - unlocked_enemies * 0.055)
    enemy.vx, enemy.vy = 0, 0
    return true
end

CARD_TYPES.explode_enemy_shot_pool = function(pool, x, y, radius_squared)
    for i = 1, #pool do
        local shot = pool[i]
        if shot.active and distance_squared(shot.x, shot.y, x, y) <= radius_squared then
            shot.active = false
            cancelled_bullets = cancelled_bullets + 1
            spawn_impact(shot.x, shot.y)
        end
    end
end

CARD_TYPES.apply_enemy_explosion = function(source_index)
    local source = enemies[source_index]
    local radius = TUNING.enemy_explosion_radius
    local radius_squared = radius * radius
    local base_damage = 2.2 + source.max_hp * 0.65
    for i = 1, unlocked_enemies do
        local enemy = enemies[i]
        if i ~= source_index and enemy.active and enemy.phase ~= "death" then
            local distance2 = distance_squared(enemy.x, enemy.y, source.x, source.y)
            if distance2 <= radius_squared then
                local distance = math.sqrt(distance2)
                local falloff = 1 - 0.55 * distance / radius
                enemy.hp = enemy.hp - base_damage * falloff
                enemy.hit_time = 0.18
                if enemy.hp <= 0 then CARD_TYPES.mark_enemy_destroyed(enemy) end
            end
        end
    end
    local bullet_radius = TUNING.enemy_explosion_bullet_radius
    local bullet_radius_squared = bullet_radius * bullet_radius
    CARD_TYPES.explode_enemy_shot_pool(
        enemy_shot_pools.ring, source.x, source.y, bullet_radius_squared
    )
    CARD_TYPES.explode_enemy_shot_pool(
        enemy_shot_pools.aim, source.x, source.y, bullet_radius_squared
    )
    CARD_TYPES.explode_enemy_shot_pool(
        enemy_shot_pools.spiral, source.x, source.y, bullet_radius_squared
    )
end

local function update_screen_shake(dt)
    shake_time = math.max(0, shake_time - dt)
    if shake_time <= 0 or shake_duration <= 0 then
        shake_x, shake_y, shake_rotation = 0, 0, 0
        return
    end
    local p = shake_time / shake_duration
    local amplitude = shake_strength * p * p
    local clock = run_time + card_time * 0.35
    shake_x = amplitude * (0.72 * math.sin(clock * 91 + shake_phase)
        + 0.28 * math.sin(clock * 151 + shake_phase * 0.7))
    shake_y = amplitude * (0.66 * math.cos(clock * 117 + shake_phase * 1.2)
        + 0.24 * math.sin(clock * 173 + shake_phase))
    shake_rotation = amplitude * 0.00055 * math.sin(clock * 83 + shake_phase * 0.5)
end

local function apply_player_recoil(angle, shot_count, size)
    local impulse = (38 + shot_count * 9) * (size ^ 1.15) * recoil_factor
        * TUNING.recoil_multiplier
    player_vx = player_vx - math.cos(angle) * impulse
    player_vy = player_vy - math.sin(angle) * impulse
    player_recoil_lock = math.max(
        player_recoil_lock,
        clamp((0.07 + shot_count * 0.012 + size * 0.055) * recoil_factor, 0.08, 0.48)
    )
    local shake = (2.4 + shot_count * 0.55) * (size ^ 1.18)
        * TUNING.recoil_multiplier
    trigger_screen_shake(shake, size > 1.05 and 0.28 or 0.12)
end

local function apply_enemy_recoil(enemy, volley, locks_attack)
    if volley.count == 0 then return false end
    local net = math.sqrt(volley.x * volley.x + volley.y * volley.y)
    if net <= math.max(0.001, volley.total * 0.01) then return false end
    local factor = TUNING.enemy_recoil_per_momentum * TUNING.recoil_multiplier
        * TUNING.enemy_recoil_multiplier
    local impulse_x = -volley.x * factor
    local impulse_y = -volley.y * factor
    local impulse = math.sqrt(impulse_x * impulse_x + impulse_y * impulse_y)
    if impulse > TUNING.enemy_recoil_impulse_cap then
        local cap = TUNING.enemy_recoil_impulse_cap / impulse
        impulse_x, impulse_y = impulse_x * cap, impulse_y * cap
        impulse = TUNING.enemy_recoil_impulse_cap
    end
    enemy.vx = enemy.vx + impulse_x
    enemy.vy = enemy.vy + impulse_y
    if locks_attack then
        enemy.recoil_lock = math.max(
            enemy.recoil_lock,
            clamp(0.11 + volley.count * 0.012 + impulse * 0.0008, 0.14, 0.42)
        )
    end
    return true
end

local function begin_level_up()
    level = level + 1
    next_level_kills = kills + experience_to_next_level(level)
    local attack_types = {5, 6}
    local utility_types = {2, 3, 4, 7}
    local attack_pick = math.floor(
        math.abs(math.sin(level * 91.17 + kills * 17.31)) * 100000
    ) % #attack_types + 1
    CARD_TYPES[6].offer = CARD_TYPES[6].roll()
    if CARD_TYPES[6].offer == 0 then attack_pick = 1 end
    if CARD_TYPES[6].offer > 0 then
        CARD_TYPES[6].offer_level = math.min(
            MAX_POWER, CARD_TYPES[6].levels[CARD_TYPES[6].offer] + 1
        )
    end
    card_choices[1] = attack_types[attack_pick]
    card_choices[2] = 1
    card_choices[3] = utility_types[(level - 2) % #utility_types + 1]
    choosing_card = true
    CARD_TYPES[6].burst.remaining = 0
    card_time, card_hover = 0, 0
    card_pointer_slot = 0
    for slot = 1, #card_hover_amount do card_hover_amount[slot] = 0 end
    charging, charge_time = false, 0
    player_vx, player_vy = 0, 0
    CARD_TYPES.queue_sound(CARD_TYPES.sound_assets.level_up)
end

local function apply_card(type_index)
    if type_index == 1 then
        max_hp = math.min(MAX_HP, max_hp + 20)
        hp = math.min(max_hp, hp + 30)
    elseif type_index == 2 then
        stamina_capacity = math.min(2.0, stamina_capacity + 0.25)
        stamina = stamina_capacity
        overheated = false
    elseif type_index == 3 then
        hp = max_hp
        hit_invulnerable = math.max(hit_invulnerable, 3.0)
    elseif type_index == 4 then
        dash_recovery_bonus = math.min(0.42, dash_recovery_bonus + 0.08)
        dash_immunity_bonus = math.min(0.48, dash_immunity_bonus + 0.12)
        dash_charge = 1
    elseif type_index == 5 then
        player_damage = player_damage + 0.25
    elseif type_index == 6 then
        weapon_type = CARD_TYPES[6].offer
        CARD_TYPES[6].levels[weapon_type] = CARD_TYPES[6].offer_level
        power_level = CARD_TYPES[6].levels[weapon_type]
    else
        recoil_factor = math.max(0.35, recoil_factor * 0.75)
        shake_factor = math.max(0.45, shake_factor * 0.80)
    end
    choosing_card = false
    card_time, card_hover = 0, 0
    card_pointer_slot = 0
    for slot = 1, #card_hover_amount do card_hover_amount[slot] = 0 end
    hit_invulnerable = math.max(hit_invulnerable, 0.75)
    CARD_TYPES.queue_sound(CARD_TYPES.sound_assets.weapon_switch)
end

local function emit_pattern(enemy)
    enemy.volley = enemy.volley + 1
    local speed = TUNING.enemy_speed_base + unlocked_enemies * TUNING.enemy_speed_step
    local damage = enemy.damage
    local scale = enemy.projectile_scale
    local volley = {x = 0, y = 0, total = 0, count = 0}
    if enemy.pattern == "ring" then
        local count = 5 + math.floor((unlocked_enemies + 1) / 3)
        local base = 0.42 * run_time + enemy.phase_seed
        for k = 0, count - 1 do
            emit_enemy_shot(
                volley, "ring", enemy.x, enemy.y, base + TAU * k / count,
                speed, 0, damage, scale
            )
        end
    elseif enemy.pattern == "aim" then
        local count = unlocked_enemies >= 7 and 5 or 3
        local half = math.floor(count / 2)
        local bearing = math.atan(player_y - enemy.y, player_x - enemy.x)
        for j = -half, half do
            emit_enemy_shot(
                volley, "aim", enemy.x, enemy.y, bearing + j * 0.13,
                speed + TUNING.aimed_speed_bonus, 0, damage, scale
            )
        end
    elseif enemy.pattern == "spiral" then
        local angle = enemy.volley * GOLDEN_ANGLE + enemy.phase_seed
        local arms = unlocked_enemies >= 8 and 3 or 2
        for arm = 0, arms - 1 do
            local direction = angle + TAU * arm / arms
            local turn = arm % 2 == 0 and 0.24 or -0.24
            emit_enemy_shot(
                volley, "spiral", enemy.x, enemy.y, direction, speed - 10,
                turn, damage, scale
            )
        end
    elseif enemy.pattern == "direct" then
        local bearing = math.atan(player_y - enemy.y, player_x - enemy.x)
        emit_enemy_shot(
            volley, "aim", enemy.x, enemy.y, bearing,
            speed + TUNING.aimed_speed_bonus, 0, damage, scale
        )
    else
        local bearing = math.atan(player_y - enemy.y, player_x - enemy.x)
        emit_enemy_shot(
            volley, "ring", enemy.x, enemy.y, bearing,
            speed - 24, 0, damage * 1.35, scale * 1.25
        )
    end
    apply_enemy_recoil(enemy, volley, true)
end

local function boid_steering(index)
    local enemy = enemies[index]
    local neighbors = 0
    local center_x, center_y = 0, 0
    local velocity_x, velocity_y = 0, 0
    local separation_x, separation_y = 0, 0
    for peer_index = 1, unlocked_enemies do
        local peer = enemies[peer_index]
        if peer_index ~= index and peer.active and peer.behavior == "boid" then
            local dx = enemy.x - peer.x
            local dy = enemy.y - peer.y
            local distance2 = dx * dx + dy * dy
            if distance2 < TUNING.boid_neighbor_radius * TUNING.boid_neighbor_radius then
                neighbors = neighbors + 1
                center_x = center_x + peer.x
                center_y = center_y + peer.y
                velocity_x = velocity_x + peer.vx
                velocity_y = velocity_y + peer.vy
                if distance2 < TUNING.boid_separation_radius * TUNING.boid_separation_radius then
                    local safe = math.max(distance2, 64)
                    separation_x = separation_x + dx * 760 / safe
                    separation_y = separation_y + dy * 760 / safe
                end
            end
        end
    end
    local target_x = math.sin(run_time * 0.34 + enemy.phase_seed) * 185
    local target_y = 190 + 42 * math.sin(run_time * 0.27 + enemy.phase_seed)
    local steer_x = (target_x - enemy.x) * 0.16 + separation_x
    local steer_y = (target_y - enemy.y) * 0.10 - 8 + separation_y
    if neighbors > 0 then
        center_x, center_y = center_x / neighbors, center_y / neighbors
        velocity_x, velocity_y = velocity_x / neighbors, velocity_y / neighbors
        steer_x = steer_x + (center_x - enemy.x) * 0.28
            + (velocity_x - enemy.vx) * 0.34
        steer_y = steer_y + (center_y - enemy.y) * 0.22
            + (velocity_y - enemy.vy) * 0.34
    end
    return steer_x, steer_y
end

-- Enemy guidance never writes position or injects velocity. A craft can only
-- accelerate by emitting real projectiles opposite the requested delta-v.
local function propel_enemy(enemy, desired_vx, desired_vy)
    if enemy.propulsion_timer > 0 then return end
    local error_x = desired_vx - enemy.vx
    local error_y = desired_vy - enemy.vy
    local direction_x, direction_y, error = normalized(error_x, error_y, 0, 0)
    if error < 10 then return end

    local count = 1 + math.floor(clamp(error / 55, 0, 2))
    local exhaust_angle = math.atan(-direction_y, -direction_x)
    local speed = TUNING.enemy_propulsion_speed + math.min(error, 80) * 0.25
    local damage = math.max(0.35, enemy.damage * 0.45)
    local scale = enemy.projectile_scale * TUNING.enemy_propulsion_scale
    local volley = {x = 0, y = 0, total = 0, count = 0}
    for shot = 1, count do
        local offset = 0
        if count == 2 then
            offset = shot == 1 and -0.11 or 0.11
        elseif count == 3 then
            offset = (shot - 2) * 0.14
        end
        emit_enemy_shot(
            volley, "aim", enemy.x, enemy.y, exhaust_angle + offset,
            speed, 0, damage, scale
        )
    end
    if apply_enemy_recoil(enemy, volley, false) then
        enemy.propulsion_shots = enemy.propulsion_shots + volley.count
        enemy.propulsion_timer = TUNING.enemy_propulsion_period
            + 0.035 * ((enemy.propulsion_shots + enemy.scenario) % 3)
    else
        -- Pool pressure is allowed to stall movement; hidden thrust is not.
        enemy.propulsion_timer = 0.08
    end
end

local function update_enemy_motion(index, dt)
    local enemy = enemies[index]
    enemy.phase_time = enemy.phase_time + dt
    enemy.propulsion_timer = math.max(0, enemy.propulsion_timer - dt)
    local desired_vx, desired_vy = 0, 0
    if enemy.phase == "entry" then
        local duration = 1.45 + enemy.scenario * 0.20
        local p = clamp(enemy.phase_time / duration, 0, 1)
        local eased = smoothstep(p)
        if enemy.scenario == 1 then
            enemy.path_x = lerp(enemy.start_x, enemy.anchor_x, eased)
                + 24 * math.sin(p * math.pi * 2 + enemy.phase_seed) * (1 - eased)
            enemy.path_y = lerp(FIELD.top + 54, enemy.anchor_y, eased)
        elseif enemy.scenario == 2 then
            local bend = math.sin(p * math.pi)
            enemy.path_x = lerp(enemy.start_x, enemy.anchor_x, eased)
                - enemy.start_x * 0.42 * bend
            enemy.path_y = lerp(FIELD.top + 54, enemy.anchor_y, eased)
                - 72 * bend
        else
            enemy.path_x = lerp(enemy.start_x, enemy.anchor_x, eased)
                + 36 * math.sin(p * math.pi)
            enemy.path_y = lerp(FIELD.top + 54, enemy.anchor_y, eased)
        end
        local guide_x, guide_y, distance = normalized(
            enemy.path_x - enemy.x, enemy.path_y - enemy.y, 0, -1
        )
        local guide_speed = clamp(distance * 2.4, 42, 150)
        desired_vx, desired_vy = guide_x * guide_speed, guide_y * guide_speed
        if p >= 1 and distance < 32 then
            enemy.phase = "attack"
            enemy.phase_time = 0
            enemy.cooldown = 0.30 + (enemy.scenario - 1) * 0.16
        end
    elseif enemy.phase == "attack" then
        local t = enemy.phase_time
        if enemy.behavior == "boid" then
            local steer_x, steer_y = boid_steering(index)
            local direction_x, direction_y, speed = normalized(
                enemy.vx + steer_x, enemy.vy + steer_y, 0, -1
            )
            local bounded_speed = clamp(speed, 32, TUNING.boid_speed + 24)
            desired_vx, desired_vy = direction_x * bounded_speed, direction_y * bounded_speed
            enemy.path_x, enemy.path_y = enemy.x, enemy.y
            if t >= 7.2 then
                enemy.phase = "exit"
                enemy.phase_time = 0
                enemy.anchor_x, enemy.anchor_y = enemy.x, enemy.y
            end
        elseif enemy.behavior == "dance" then
            enemy.path_x = enemy.anchor_x
                + 76 * math.sin(1.72 * t + enemy.phase_seed)
                + 24 * math.sin(3.44 * t + enemy.phase_seed * 0.5)
            enemy.path_y = enemy.anchor_y - 62 * t
                + 18 * math.cos(2.58 * t + enemy.phase_seed)
            desired_vx = 76 * 1.72 * math.cos(1.72 * t + enemy.phase_seed)
                + 24 * 3.44 * math.cos(3.44 * t + enemy.phase_seed * 0.5)
                + (enemy.path_x - enemy.x) * 1.1
            desired_vy = -62 - 18 * 2.58 * math.sin(2.58 * t + enemy.phase_seed)
                + (enemy.path_y - enemy.y) * 1.1
            local dx, dy, speed = normalized(desired_vx, desired_vy, 0, -1)
            if speed > 145 then desired_vx, desired_vy = dx * 145, dy * 145 end
            if t >= 7.6 then
                enemy.phase = "exit"
                enemy.phase_time = 0
                enemy.anchor_x, enemy.anchor_y = enemy.x, enemy.y
            end
        elseif enemy.behavior == "sentinel" then
            -- Sentinel enemies never transition to exit; only damage can remove them.
            enemy.path_x = enemy.anchor_x + 42 * math.sin(0.74 * t + enemy.phase_seed)
            enemy.path_y = enemy.anchor_y
                + 24 * math.sin(1.48 * t + enemy.phase_seed * 0.6)
            desired_vx = 42 * 0.74 * math.cos(0.74 * t + enemy.phase_seed)
                + (enemy.path_x - enemy.x) * 1.25
            desired_vy = 24 * 1.48 * math.cos(1.48 * t + enemy.phase_seed * 0.6)
                + (enemy.path_y - enemy.y) * 1.25
            local dx, dy, speed = normalized(desired_vx, desired_vy, 0, 0)
            if speed > 95 then desired_vx, desired_vy = dx * 95, dy * 95 end
        else
            desired_vx = 72 * math.sin(0.90 * t + enemy.phase_seed)
            desired_vy = -38 + 18 * math.cos(1.35 * t + enemy.phase_seed)
            enemy.path_x, enemy.path_y = enemy.x, enemy.y
            if t >= 9.2 then
                enemy.phase = "exit"
                enemy.phase_time = 0
                enemy.anchor_x, enemy.anchor_y = enemy.x, enemy.y
            end
        end
    else
        local side = enemy.scenario == 2 and (enemy.anchor_x < 0 and 1 or -1) or 0
        enemy.path_x = clamp(enemy.anchor_x + side * 170,
            FIELD.left + 30, FIELD.right - 30)
        enemy.path_y = FIELD.bottom - 76
        local exit_x, exit_y, distance = normalized(
            enemy.path_x - enemy.x, enemy.path_y - enemy.y, 0, -1
        )
        desired_vx, desired_vy = exit_x * 112, exit_y * 112
        if enemy.y <= FIELD.bottom - 60 or distance < 18 then
            breaches = breaches + 1
            deactivate_enemy(enemy, 0.72 + (enemy.scenario - 1) * 0.24)
            return
        end
    end

    propel_enemy(enemy, desired_vx, desired_vy)
    local damping = math.exp(-TUNING.enemy_recoil_drag * dt)
    enemy.vx, enemy.vy = enemy.vx * damping, enemy.vy * damping
    enemy.x = enemy.x + enemy.vx * dt
    enemy.y = enemy.y + enemy.vy * dt

    local left, right = FIELD.left + 24, FIELD.right - 24
    if enemy.x < left then
        enemy.x = left
        if enemy.vx < 0 then enemy.vx = 0 end
    elseif enemy.x > right then
        enemy.x = right
        if enemy.vx > 0 then enemy.vx = 0 end
    end
    if enemy.phase ~= "exit" then
        local bottom = FIELD.bottom + 42
        local top = enemy.phase == "entry" and FIELD.top + 62 or FIELD.top - 24
        if enemy.y < bottom then
            enemy.y = bottom
            if enemy.vy < 0 then enemy.vy = 0 end
        elseif enemy.y > top then
            enemy.y = top
            if enemy.vy > 0 then enemy.vy = 0 end
        end
    end
end

local function update_enemies(dt)
    local action_dt = dt * TUNING.enemy_time_scale(run_time)
    local desired = math.min(MAX_ENEMIES, 3 + math.floor(run_time / TUNING.enemy_unlock_period))
    if desired > unlocked_enemies then unlocked_enemies = desired end
    local death_in_progress = false
    for i = 1, MAX_ENEMIES do
        local enemy = enemies[i]
        enemy.hit_time = math.max(0, enemy.hit_time - dt)
        if i <= unlocked_enemies then
            if enemy.active then
                if enemy.phase == "death" then
                    death_in_progress = true
                    enemy.phase_time = enemy.phase_time + dt
                    if enemy.phase_time >= 0.24 and not enemy.death_burst then
                        enemy.death_burst = true
                        spawn_impact(enemy.x, enemy.y, "death")
                        CARD_TYPES.queue_sound(
                            CARD_TYPES.sound_assets.explosion,
                            CARD_TYPES.sound_assets.explosion.gain,
                            0.92 + (kills % 5) * 0.03
                        )
                        CARD_TYPES.apply_enemy_explosion(i)
                        trigger_screen_shake(11, 0.30)
                    end
                    if enemy.phase_time >= 0.68 then
                        deactivate_enemy(enemy, enemy.death_respawn)
                    end
                else
                    enemy.recoil_lock = math.max(0, enemy.recoil_lock - action_dt)
                    local scaled_health = enemy_health(enemy)
                    if scaled_health > enemy.max_hp then
                        enemy.hp = enemy.hp + scaled_health - enemy.max_hp
                        enemy.max_hp = scaled_health
                    end
                    enemy.damage = enemy_damage()
                    enemy.projectile_scale = 1 + (enemy.damage - 1) * 0.12
                    update_enemy_motion(i, action_dt)
                end
            else
                enemy.respawn = math.max(0, enemy.respawn - action_dt)
                if enemy.respawn <= 0 then activate_enemy(i, false) end
            end
        else
            enemy.active = false
            enemy.phase = "inactive"
        end
    end
    for i = 1, unlocked_enemies do
        local enemy = enemies[i]
        if enemy.active and enemy.phase == "attack" then
            enemy.cooldown = enemy.cooldown - action_dt
            if enemy.cooldown <= 0 and enemy.recoil_lock <= 0 then
                emit_pattern(enemy)
                enemy.cooldown = math.max(
                    TUNING.enemy_fire_min,
                    TUNING.enemy_fire_base - unlocked_enemies * 0.07
                ) + (i % 3) * 0.10
            end
        end
    end
end

local function damage_player(amount)
    if game_is_over or hit_invulnerable > 0 or dash_invulnerable > 0 or dash_time > 0 then
        return false
    end
    hp = math.max(0, hp - math.max(0, amount or 0))
    hit_invulnerable = TUNING.hit_recovery
    spawn_impact(player_x, player_y)
    CARD_TYPES.queue_sound(CARD_TYPES.sound_assets.player_hit)
    if hp <= 0 then game_is_over = true end
    return true
end

local function spend_stamina(amount)
    if amount <= 0 or overheated then return false end
    stamina = math.max(0, stamina - amount)
    stamina_delay = TUNING.stamina_delay
    if stamina <= 0.0001 then
        stamina = 0
        overheated = true
        charging = false
    end
    return true
end

local function fire_weapon(size_multiplier)
    local muzzle_x = player_x + math.cos(aim_angle) * 29
    local muzzle_y = player_y + math.sin(aim_angle) * 29
    local damage = player_damage * size_multiplier
    local charge_heat = smoothstep((size_multiplier - 1) / (MAX_CHARGE_SCALE - 1))
    local emitted = 0
    if weapon_type == 1 then
        local thickness = (0.72 + power_level * 0.24)
            * (1 + (size_multiplier - 1) * 0.75)
        local burst_count = 1 + math.floor((power_level - 1) / 2)
        emitted = spawn_player_shot(
            muzzle_x, muzzle_y, aim_angle, TUNING.player_shot_speed * 1.35,
            damage * TUNING.weapon_damage.short_laser,
            power_level + math.floor(size_multiplier * 0.5),
            5 * thickness, thickness, 0, charge_heat
        )
        if emitted > 0 and burst_count > 1 then
            local burst = CARD_TYPES[6].burst
            burst.remaining = burst_count - 1
            burst.timer = 0.075
            burst.angle = aim_angle
            burst.damage = damage * TUNING.weapon_damage.short_laser
            burst.pierce = power_level + math.floor(size_multiplier * 0.5)
            burst.radius = 5 * thickness
            burst.scale = thickness
            burst.charge_heat = charge_heat
            emitted = burst_count
        end
    elseif weapon_type == 2 then
        local count = 2 + power_level * 2
            + math.floor((size_multiplier - 1) * 2.25)
        for i = 1, count do
            local orbit = TAU * (i - 1) / count
            emitted = emitted + spawn_player_shot(
                player_x, player_y, aim_angle,
                TUNING.player_shot_speed * 0.72,
                damage * TUNING.weapon_damage.homing, 0,
                5 + size_multiplier * 0.45,
                0.68 + power_level * 0.07 + (size_multiplier - 1) * 0.08,
                orbit, charge_heat
            )
        end
    elseif weapon_type == 3 then
        local count = 1 + math.floor(power_level / 2)
        local ball_scale = 0.82 + power_level * 0.12
            + (size_multiplier - 1) * 0.18
        for i = 1, count do
            local spread = (i - (count + 1) / 2) * 0.10
            emitted = emitted + spawn_player_shot(
                muzzle_x, muzzle_y, aim_angle + spread,
                TUNING.player_shot_speed * 0.74,
                damage * TUNING.weapon_damage.ricochet,
                0, 7 * ball_scale, ball_scale, 0, charge_heat
            )
        end
    elseif weapon_type == 4 then
        local width_scale = (0.78 + power_level * 0.16)
            * (1 + (size_multiplier - 1) * 0.28)
        emitted = spawn_player_shot(
            muzzle_x, muzzle_y, aim_angle, TUNING.player_shot_speed * 0.92,
            damage * TUNING.weapon_damage.long_laser,
            power_level * 2 + math.floor(size_multiplier),
            6 * width_scale, width_scale, 0, charge_heat
        )
    else
        local count = 1 + (power_level - 1) * 2
        local bullet_scale = 0.72 + power_level * 0.10
            + (size_multiplier - 1) * 0.16
        local spread_step = 0.11 + power_level * 0.035
        for i = 1, count do
            local spread = (i - (count + 1) / 2) * spread_step
            emitted = emitted + spawn_player_shot(
                muzzle_x, muzzle_y, aim_angle + spread,
                TUNING.player_shot_speed * 1.18,
                damage * TUNING.weapon_damage.pulse,
                math.floor((size_multiplier - 1) * 0.5),
                5 * bullet_scale, bullet_scale, 0, charge_heat
            )
        end
    end
    return emitted
end

local function release_fire()
    local amount = clamp(charge_time / CHARGE_DURATION, 0, 1)
    local size = 1
    local profile = TUNING.weapon_stamina[weapon_type]
    local cost = profile.tap
    if charge_time < TUNING.charge_threshold then
        size = 1
    else
        size = 1 + amount * (MAX_CHARGE_SCALE - 1)
        cost = profile.charge + amount * profile.release
    end
    if spend_stamina(cost) then
        local shot_count = fire_weapon(size)
        if shot_count > 0 then
            local sound = CARD_TYPES.sound_assets.weapons[weapon_type]
            CARD_TYPES.queue_sound(
                sound,
                math.min(1, sound.gain + amount * 0.16),
                0.96 + power_level * 0.018 - amount * 0.08
            )
            if weapon_type == 4 then dash_time = 0 end
            apply_player_recoil(aim_angle, shot_count, size)
            if weapon_type == 4 then
                player_recoil_lock = math.max(
                    player_recoil_lock,
                    TUNING.long_laser_extend + TUNING.long_laser_hold
                        + TUNING.long_laser_retract + 0.10
                )
            end
        end
    end
    charge_flash = 0.13
end

local function update_stamina(dt, used)
    if used then return end
    stamina_delay = math.max(0, stamina_delay - dt)
    if stamina_delay > 0 then return end
    local recovery = overheated and TUNING.overheat_recovery or TUNING.stamina_recovery
    stamina = math.min(stamina_capacity, stamina + recovery * dt)
    if overheated and stamina >= stamina_capacity then
        stamina = stamina_capacity
        overheated = false
    end
end

local function update_player(move, boost, dash, fire, pointer, dt)
    local long_laser_active = CARD_TYPES[6].long_active
    if pointer then
        local target_x = pointer.position.x - WIDTH / 2 - shake_x
        local target_y = HEIGHT / 2 - pointer.position.y - shake_y
        local aim_x, aim_y, aim_length = normalized(
            target_x - player_x, target_y - player_y, math.cos(aim_angle), math.sin(aim_angle)
        )
        if aim_length > 10 then aim_angle = math.atan(aim_y, aim_x) end
    end
    local dx, dy = move.value.x, move.value.y
    dx, dy = normalized(dx, dy, 0, 0)
    if long_laser_active then dx, dy = 0, 0 end
    local used_stamina = false
    player_recoil_lock = math.max(0, player_recoil_lock - dt)
    dash_charge = math.min(1, dash_charge + dt * (TUNING.dash_recovery + dash_recovery_bonus))
    if not long_laser_active and dash.pressed and dash_charge >= 1 then
        if dx == 0 and dy == 0 then dash_x, dash_y = 0, 1 else dash_x, dash_y = dx, dy end
        dash_charge, dash_time = 0, TUNING.dash_duration
        dash_invulnerable = TUNING.dash_immunity + dash_immunity_bonus
    end
    dash_time = math.max(0, dash_time - dt)
    dash_invulnerable = math.max(0, dash_invulnerable - dt)
    local boosting = not long_laser_active and boost.down
        and not overheated and dash_time <= 0
    local speed_multiplier = 1
    if boosting then
        speed_multiplier = TUNING.boost_multiplier
        spend_stamina(TUNING.boost_stamina_per_second * dt)
        used_stamina = true
    end
    if long_laser_active then
        local recoil_drag = math.exp(-TUNING.long_laser_recoil_drag * dt)
        player_vx, player_vy = player_vx * recoil_drag, player_vy * recoil_drag
    elseif dash_time > 0 then
        player_vx = dash_x * TUNING.dash_speed
        player_vy = dash_y * TUNING.dash_speed
    elseif dx == 0 and dy == 0 then
        local coast = math.exp(-TUNING.player_coast_drag * dt)
        player_vx, player_vy = player_vx * coast, player_vy * coast
    else
        local response = 1 - math.exp(-TUNING.player_steering_response * dt)
        player_vx = player_vx
            + (dx * TUNING.player_speed * speed_multiplier - player_vx) * response
        player_vy = player_vy
            + (dy * TUNING.player_speed * speed_multiplier - player_vy) * response
    end
    player_x = clamp(player_x + player_vx * dt, FIELD.left + 26, FIELD.right - 26)
    player_y = clamp(player_y + player_vy * dt, FIELD.bottom + 34, FIELD.top - 34)
    if fire.pressed and not long_laser_active
        and not overheated and player_recoil_lock <= 0 then
        charging = true
        charge_time = 0
    end
    if fire.down and charging and not overheated then
        charge_time = math.min(CHARGE_DURATION, charge_time + dt)
        if charge_time >= TUNING.charge_threshold then
            local charge_amount = clamp(charge_time / CHARGE_DURATION, 0, 1)
            local profile = TUNING.weapon_stamina[weapon_type]
            spend_stamina(profile.hold * (0.45 + charge_amount * 0.55) * dt)
            used_stamina = true
        end
    end
    if fire.released then
        if charging and not overheated then release_fire() end
        charging = false
        charge_time = 0
        used_stamina = true
    end
    charge_flash = math.max(0, charge_flash - dt)
    update_stamina(dt, used_stamina)
end

local function retain_projectile_power(shot, remaining_strength)
    shot.strength = remaining_strength
    local ratio = clamp(
        remaining_strength / math.max(shot.max_strength, 0.0001), 0, 1
    )
    shot.damage = shot.max_damage * ratio
    local visual = 0.34 + math.sqrt(ratio) * 0.66
    if shot.weapon_kind == 4 then
        shot.scale_x = shot.spawn_scale_x * visual
        shot.radius = shot.spawn_radius * visual
        return
    end
    shot.scale = shot.spawn_scale * visual
    shot.scale_x = shot.spawn_scale_x * visual
    shot.scale_y = shot.spawn_scale_y * visual
    shot.radius = shot.spawn_radius * visual
end

local function beam_collision(shot, target_x, target_y, target_radius)
    if shot.weapon_kind == 4 then
        local axis_x, axis_y = math.cos(shot.angle), math.sin(shot.angle)
        local projection = clamp(
            (target_x - shot.beam_origin_x) * axis_x
                + (target_y - shot.beam_origin_y) * axis_y,
            0, shot.visible_length
        )
        return distance_squared(
            shot.beam_origin_x + axis_x * projection,
            shot.beam_origin_y + axis_y * projection,
            target_x, target_y
        ), 4 * shot.scale_x + target_radius
    end
    local axis_x, axis_y = math.cos(shot.angle), math.sin(shot.angle)
    local projection = clamp(
        (target_x - shot.x) * axis_x + (target_y - shot.y) * axis_y,
        -shot.beam_half * shot.scale_y, shot.beam_half * shot.scale_y
    )
    return distance_squared(
        shot.x + axis_x * projection, shot.y + axis_y * projection,
        target_x, target_y
    ), 2.5 * shot.scale_x + target_radius
end

local function destroy_enemy_bullet_with_shot(shot, pool)
    for i = 1, #pool do
        local hostile = pool[i]
        local collision2 = distance_squared(shot.x, shot.y, hostile.x, hostile.y)
        local collision_radius = shot.radius + hostile.radius
        if shot.weapon_kind == 1 or shot.weapon_kind == 4 then
            collision2, collision_radius = beam_collision(
                shot, hostile.x, hostile.y, hostile.radius
            )
        end
        if hostile.active and collision2 < collision_radius * collision_radius then
            local remaining = shot.strength - hostile.strength
            if shot.weapon_kind == 4 then
                if remaining >= 0 then
                    hostile.active = false
                    cancelled_bullets = cancelled_bullets + 1
                else
                    retain_projectile_power(hostile, -remaining)
                end
            elseif math.abs(remaining) <= 0.0001 then
                shot.active = false
                hostile.active = false
                cancelled_bullets = cancelled_bullets + 1
            elseif remaining > 0 then
                hostile.active = false
                cancelled_bullets = cancelled_bullets + 1
                retain_projectile_power(shot, remaining)
            else
                shot.active = false
                retain_projectile_power(hostile, -remaining)
            end
            spawn_impact(hostile.x, hostile.y)
            return true
        end
    end
    return false
end

local function update_player_shots(dt)
    for i = 1, #player_shots do
        local shot = player_shots[i]
        if shot.active then
            shot.age = shot.age + dt
            local orbiting = shot.weapon_kind == 2 and shot.age < shot.orbit_time
            if shot.weapon_kind == 4 then
                shot.angle = follow_angle(
                    shot.angle, aim_angle, TUNING.long_laser_aim_response, dt
                )
                local extend_end = TUNING.long_laser_extend
                local hold_end = extend_end + TUNING.long_laser_hold
                local retract_end = hold_end + TUNING.long_laser_retract
                local length_ratio, opacity = 1, 1
                if shot.age < extend_end then
                    length_ratio = ease_out_cubic(shot.age / extend_end)
                elseif shot.age > hold_end then
                    local retract = smoothstep(
                        (shot.age - hold_end) / TUNING.long_laser_retract
                    )
                    length_ratio = 1 - retract
                    opacity = 1 - retract
                end
                local root_x, root_y = math.cos(shot.angle), math.sin(shot.angle)
                shot.beam_origin_x = player_x + root_x * 29
                shot.beam_origin_y = player_y + root_y * 29
                shot.beam_length = CARD_TYPES.ray_distance(
                    shot.beam_origin_x, shot.beam_origin_y, shot.angle,
                    CARD_TYPES.beam_bounds
                )
                shot.visible_length = shot.beam_length * length_ratio
                shot.x = shot.beam_origin_x + root_x * shot.visible_length * 0.5
                shot.y = shot.beam_origin_y + root_y * shot.visible_length * 0.5
                shot.scale_y = shot.visible_length / (shot.beam_half * 2)
                shot.scale_x = shot.spawn_scale_x
                    * (0.82 + 0.18 * opacity)
                shot.visual_opacity = opacity
                if shot.age >= retract_end then
                    shot.active = false
                    CARD_TYPES[6].long_active = false
                end
            elseif orbiting then
                shot.orbit_angle = shot.orbit_angle + (8.5 + power_level * 0.7) * dt
                local orbit = shot.orbit_radius
                    + 4 * math.sin(shot.age * 18 + shot.orbit_angle)
                shot.x = player_x + math.cos(shot.orbit_angle) * orbit
                shot.y = player_y + math.sin(shot.orbit_angle) * orbit
                shot.angle = shot.orbit_angle + math.pi / 2
            elseif shot.weapon_kind == 2 then
                local target_x, target_y = nil, nil
                local nearest = math.huge
                for enemy_index = 1, unlocked_enemies do
                    local enemy = enemies[enemy_index]
                    if enemy.active and enemy.phase ~= "death" then
                        local distance2 = distance_squared(shot.x, shot.y, enemy.x, enemy.y)
                        if distance2 < nearest then
                            nearest = distance2
                            target_x, target_y = enemy.x, enemy.y
                        end
                    end
                end
                if target_x then
                    local desired = math.atan(target_y - shot.y, target_x - shot.x)
                    local delta = math.atan(
                        math.sin(desired - shot.angle), math.cos(desired - shot.angle)
                    )
                    shot.angle = shot.angle + clamp(
                        delta, -(5.2 + power_level * 0.55) * dt,
                        (5.2 + power_level * 0.55) * dt
                    )
                    local speed = TUNING.player_shot_speed * 0.72
                    shot.vx = math.cos(shot.angle) * speed
                    shot.vy = math.sin(shot.angle) * speed
                end
                shot.x = shot.x + shot.vx * dt
                shot.y = shot.y + shot.vy * dt
            else
                if shot.turn ~= 0 then
                    local turn = shot.turn * dt
                    local cosine, sine = math.cos(turn), math.sin(turn)
                    local vx = shot.vx * cosine - shot.vy * sine
                    local vy = shot.vx * sine + shot.vy * cosine
                    shot.vx, shot.vy = vx, vy
                    shot.angle = shot.angle + turn
                end
                shot.x = shot.x + shot.vx * dt
                shot.y = shot.y + shot.vy * dt
            end
            shot.hit_lock = math.max(0, shot.hit_lock - dt)

            local bounced = false
            if shot.weapon_kind == 3 then
                if shot.x < FIELD.left + shot.radius and shot.vx < 0 then
                    shot.x = FIELD.left + shot.radius
                    shot.vx = -shot.vx
                    bounced = true
                elseif shot.x > FIELD.right - shot.radius and shot.vx > 0 then
                    shot.x = FIELD.right - shot.radius
                    shot.vx = -shot.vx
                    bounced = true
                end
                if shot.y < FIELD.bottom + shot.radius and shot.vy < 0 then
                    shot.y = FIELD.bottom + shot.radius
                    shot.vy = -shot.vy
                    bounced = true
                elseif shot.y > FIELD.top - shot.radius and shot.vy > 0 then
                    shot.y = FIELD.top - shot.radius
                    shot.vy = -shot.vy
                    bounced = true
                end
                if bounced then
                    shot.bounces = shot.bounces - 1
                    shot.damage = shot.damage * 0.90
                    shot.strength = shot.strength * 0.90
                    shot.scale, shot.scale_x, shot.scale_y = shot.scale * 0.92,
                        shot.scale_x * 0.92, shot.scale_y * 0.92
                    shot.radius = shot.radius * 0.92
                    shot.angle = math.atan(shot.vy, shot.vx)
                    spawn_impact(shot.x, shot.y)
                    if shot.bounces <= 0 or shot.scale < 0.34 then shot.active = false end
                end
            elseif not orbiting and shot.weapon_kind ~= 4 then
                local margin = shot.weapon_kind == 4
                    and shot.beam_half * shot.scale_y + 28 or 28
                if shot.x < FIELD.left - margin or shot.x > FIELD.right + margin
                    or shot.y < FIELD.bottom - margin or shot.y > FIELD.top + margin then
                    shot.active = false
                end
            end

            if shot.active and not orbiting then
                destroy_enemy_bullet_with_shot(shot, enemy_shot_pools.ring)
                if shot.active then destroy_enemy_bullet_with_shot(shot, enemy_shot_pools.aim) end
                if shot.active then destroy_enemy_bullet_with_shot(shot, enemy_shot_pools.spiral) end
                if shot.active then
                    for enemy_index = 1, unlocked_enemies do
                        local enemy = enemies[enemy_index]
                        local collision2 = distance_squared(
                            shot.x, shot.y, enemy.x, enemy.y
                        )
                        local collision_radius = shot.radius + 19
                        if shot.weapon_kind == 1 or shot.weapon_kind == 4 then
                            collision2, collision_radius = beam_collision(
                                shot, enemy.x, enemy.y, 19
                            )
                        end
                        if enemy.active and enemy.phase ~= "death"
                            and (shot.last_enemy ~= enemy_index or shot.hit_lock <= 0)
                            and collision2 < collision_radius * collision_radius then
                            enemy.hp = enemy.hp - shot.damage
                            enemy.hit_time = 0.12
                            shot.last_enemy = enemy_index
                            shot.hit_lock = shot.weapon_kind == 3 and 0.14 or 0.08
                            spawn_impact(shot.x, shot.y)
                            if enemy.hp <= 0 then
                                CARD_TYPES.mark_enemy_destroyed(enemy)
                            end
                            if shot.weapon_kind == 3 then
                                local nx, ny = normalized(
                                    shot.x - enemy.x, shot.y - enemy.y, 0, 1
                                )
                                local dot = shot.vx * nx + shot.vy * ny
                                if dot < 0 then
                                    shot.vx = shot.vx - 2 * dot * nx
                                    shot.vy = shot.vy - 2 * dot * ny
                                else
                                    shot.vx, shot.vy = -shot.vx, -shot.vy
                                end
                                shot.angle = math.atan(shot.vy, shot.vx)
                                shot.bounces = shot.bounces - 1
                                shot.damage = shot.damage * 0.76
                                shot.strength = shot.strength * 0.76
                                shot.scale, shot.scale_x, shot.scale_y = shot.scale * 0.82,
                                    shot.scale_x * 0.82, shot.scale_y * 0.82
                                shot.radius = shot.radius * 0.82
                                if shot.bounces <= 0 or shot.scale < 0.34
                                    or shot.damage < 0.18 then shot.active = false end
                            elseif shot.weapon_kind == 4 then
                                -- A sustained beam keeps its visual lifetime while penetrating.
                            elseif shot.pierce <= 0 then
                                shot.active = false
                            else
                                shot.pierce = shot.pierce - 1
                            end
                            if choosing_card then return end
                            break
                        end
                    end
                end
            end
        end
    end
end

local function update_enemy_shot_pool(pool, dt)
    for i = 1, #pool do
        local shot = pool[i]
        if shot.active then
            if shot.turn ~= 0 then
                local turn = shot.turn * dt
                local cosine, sine = math.cos(turn), math.sin(turn)
                local vx = shot.vx * cosine - shot.vy * sine
                local vy = shot.vx * sine + shot.vy * cosine
                shot.vx, shot.vy = vx, vy
                shot.angle = shot.angle + turn
            end
            shot.x = shot.x + shot.vx * dt
            shot.y = shot.y + shot.vy * dt
            local inset = shot.radius + 2
            if shot.x < FIELD.left - inset or shot.x > FIELD.right + inset
                or shot.y < FIELD.bottom - inset or shot.y > FIELD.top + inset then
                shot.active = false
            elseif not game_is_over
                and distance_squared(shot.x, shot.y, player_x, player_y)
                    < (shot.radius + 10) * (shot.radius + 10) then
                shot.active = false
                damage_player(shot.damage)
            end
        end
    end
end

local function update_impacts(dt)
    for i = 1, #impacts do
        local impact = impacts[i]
        if impact.active then
            impact.age = impact.age + dt
            if impact.age >= impact.duration then impact.active = false end
        end
    end
end

local function update_actor_collisions()
    for i = 1, unlocked_enemies do
        local enemy = enemies[i]
        if enemy.active and enemy.phase ~= "death"
            and distance_squared(enemy.x, enemy.y, player_x, player_y) < 32 * 32 then
            damage_player(enemy.damage)
        end
    end
end

local function commit_background(ctx)
    for i = 1, #grid_sheets do
        local grid = grid_sheets[i]
        local offset = (run_time * CARD_TYPES.grid_speed + grid.phase)
            % (CARD_TYPES.grid_span * GRID.sheets)
        local y = CARD_TYPES.grid_span - offset
        grid.state.shift[2] = y - grid.base_y
        ctx:update(grid.object, grid.state)
    end
end

local function commit_enemy_shot_pool(ctx, pool)
    for i = 1, #pool do
        local shot = pool[i]
        shot.state.shift[1] = shot.x - shot.base_x
        shot.state.shift[2] = shot.y - shot.base_y
        shot.state.rotation = shot.angle
        shot.state.scale[1] = shot.scale
        shot.state.scale[2] = shot.scale
        shot.state.opacity = shot.active and 1 or 0
        ctx:update(shot.object, shot.state)
    end
end

local function commit_cards(ctx)
    local overlay_reveal = smoothstep(card_time / 0.36)
    level_overlay_state.shift[2] = -CARD_STAGE_Y + (1 - overlay_reveal) * 28
    local overlay_scale = 0.985 + overlay_reveal * 0.015
    level_overlay_state.scale[1] = overlay_scale
    level_overlay_state.scale[2] = overlay_scale
    level_overlay_state.opacity = choosing_card and overlay_reveal or 0
    ctx:update(level_overlay, level_overlay_state)

    local burst_progress = ease_out_cubic(card_time / 0.68)
    local burst_scale = 0.62 + burst_progress * 1.15
    local burst_in = smoothstep(card_time / 0.10)
    local burst_out = 1 - smoothstep((card_time - 0.24) / 0.52)
    level_up_burst_state.scale[1] = burst_scale
    level_up_burst_state.scale[2] = burst_scale
    level_up_burst_state.rotation = burst_progress * 0.24
    level_up_burst_state.opacity = choosing_card and burst_in * burst_out or 0
    ctx:update(level_up_burst, level_up_burst_state)

    level_ready_state.progress = clamp(card_time / CARD_INPUT_DELAY, 0, 1)
    level_ready_state.opacity = choosing_card and overlay_reveal or 0
    ctx:update(level_ready_line, level_ready_state)

    for slot = 1, #card_faces do
        local reveal_time = (card_time - CARD_REVEAL_START
            - (slot - 1) * CARD_REVEAL_STAGGER) / CARD_REVEAL_DURATION
        local reveal = ease_out_cubic(reveal_time)
        local reveal_opacity = smoothstep(reveal_time * 1.8)
        local hover_ready = smoothstep((card_time - CARD_HOVER_DELAY) / 0.18)
        local hover = card_hover_amount[slot] * hover_ready
        local hover_pulse = hover * 0.003 * math.sin(card_time * 5.5 + slot)
        local scale = (0.96 + reveal * 0.04) * (1 + hover * 0.042 + hover_pulse)
        local y = CARD_POSITIONS[slot].y - CARD_STAGE_Y
            - (1 - reveal) * 46 + hover * 10
        for type_index = 1, #card_faces[slot] do
            local face = card_faces[slot][type_index]
            local visible = choosing_card and card_choices[slot] == type_index
            face.state.shift[1] = CARD_POSITIONS[slot].x
            face.state.shift[2] = y
            face.state.scale[1] = scale
            face.state.scale[2] = scale
            face.state.opacity = visible and reveal_opacity or 0
            ctx:update(face.object, face.state)
            if face.title_wheel then
                face.title_wheel.state.shift[2] = (3 - CARD_TYPES[6].offer) * HEIGHT * 2
                face.level_wheel.state.shift[2] = (3 - CARD_TYPES[6].offer_level)
                    * HEIGHT * 2
                ctx:update(face.title_wheel.object, face.title_wheel.state)
                ctx:update(face.level_wheel.object, face.level_wheel.state)
            end
        end
        local focus = card_focuses[slot]
        focus.state.shift[1] = CARD_POSITIONS[slot].x
        focus.state.shift[2] = y
        focus.state.scale[1] = scale * 1.004
        focus.state.scale[2] = scale * 1.004
        focus.state.opacity = choosing_card and reveal_opacity * hover
            * (0.78 + 0.08 * math.sin(card_time * 5.5 + slot)) or 0
        ctx:update(focus.object, focus.state)
    end
end

local function commit_overlays(ctx)
    commit_background(ctx)
    battlefield_state.shift[1] = shake_x
    battlefield_state.shift[2] = shake_y
    battlefield_state.rotation = shake_rotation
    ctx:update(battlefield, battlefield_state)
    CARD_TYPES.aim.state.shift[1] = player_x - PLAYER_HOME.x
    CARD_TYPES.aim.state.shift[2] = player_y - PLAYER_HOME.y
    CARD_TYPES.aim.state.rotation = aim_angle
    CARD_TYPES.aim.state.scale[1] = CARD_TYPES.ray_distance(
        player_x, player_y, aim_angle
    )
    CARD_TYPES.aim.state.opacity = not choosing_card and not game_is_over and 0.60 or 0
    ctx:update(CARD_TYPES.aim.object, CARD_TYPES.aim.state)
    local player_opacity = game_is_over and 0.22 or 1
    if not game_is_over and (hit_invulnerable > 0 or dash_invulnerable > 0)
        and math.floor((hit_invulnerable + dash_invulnerable) * 18) % 2 == 0 then
        player_opacity = 0.28
    end
    player_state.shift[1] = player_x - PLAYER_HOME.x
    player_state.shift[2] = player_y - PLAYER_HOME.y
    player_state.rotation = aim_angle - math.pi / 2
    player_state.opacity = player_opacity
    ctx:update(player, player_state)

    local charge_amount = charging and clamp(charge_time / CHARGE_DURATION, 0, 1) or 0
    local charge_heat = smoothstep((charge_amount - 0.15) / 0.85)
    player_hot_state.shift[1] = player_state.shift[1]
    player_hot_state.shift[2] = player_state.shift[2]
    player_hot_state.rotation = player_state.rotation
    player_hot_state.opacity = player_opacity
        * (overheated and 1 or (charging and charge_heat or 0))
    ctx:update(player_hot, player_hot_state)
    charge_state.shift[1] = player_x - STAGE_X
    charge_state.shift[2] = player_y - STAGE_Y
    charge_state.rotation = aim_angle - math.pi / 2
    local charge_scale = 0.72 + charge_amount * 3.35
    if charge_flash > 0 then charge_scale = 1.55 end
    charge_state.scale[1] = charge_scale
    charge_state.scale[2] = charge_scale
    charge_state.opacity = charging and (0.26 + charge_amount * 0.72)
        or (charge_flash > 0 and charge_flash / 0.13 or 0)
    ctx:update(charge_visuals.base, charge_state)
    charge_cool_state.shift[1] = charge_state.shift[1]
    charge_cool_state.shift[2] = charge_state.shift[2]
    charge_cool_state.rotation = charge_state.rotation
    charge_cool_state.scale[1] = charge_scale
    charge_cool_state.scale[2] = charge_scale
    charge_cool_state.opacity = charge_state.opacity
    ctx:update(charge_visuals.cool, charge_cool_state)
    charge_hot_state.shift[1] = charge_state.shift[1]
    charge_hot_state.shift[2] = charge_state.shift[2]
    charge_hot_state.rotation = charge_state.rotation
    charge_hot_state.scale[1] = charge_scale
    charge_hot_state.scale[2] = charge_scale
    charge_hot_state.opacity = charging and charge_heat or 0
    ctx:update(charge_visuals.hot, charge_hot_state)

    local stamina_ratio = stamina / stamina_capacity
    local stamina_color = overheated and 3
        or (stamina_ratio > 0.5 and 1 or (stamina_ratio > 0.2 and 2 or 3))
    for i = 1, #stamina_curves do
        stamina_states[i].shift[1] = player_x - stamina_bases[i].x
        stamina_states[i].shift[2] = player_y - stamina_bases[i].y
        stamina_states[i].progress = stamina_ratio
        stamina_states[i].opacity = i == stamina_color and 1 or 0
        ctx:update(stamina_curves[i], stamina_states[i])
    end
    overheat_state.shift[1] = player_x + 48 - STAGE_X
    overheat_state.shift[2] = player_y + 5 - STAGE_Y
    overheat_state.opacity = overheated and 1 or 0
    ctx:update(overheat_label, overheat_state)
    for i = 1, #enemies do
        local enemy = enemies[i]
        local spawn_scale = enemy.phase == "entry" and clamp(enemy.phase_time / 0.36, 0.55, 1) or 1
        local hit_scale = enemy.hit_time > 0 and 1.14 or 1
        local dying = enemy.phase == "death"
        local death_shake = dying and clamp(enemy.phase_time / 0.24, 0, 1) or 0
        local shake_envelope = death_shake * death_shake
        local jitter_x = dying and math.sin(enemy.phase_time * 143 + enemy.phase_seed) * 9
            * shake_envelope or 0
        local jitter_y = dying and math.cos(enemy.phase_time * 181 + enemy.phase_seed) * 7
            * shake_envelope or 0
        enemy.state.shift[1] = enemy.x - enemy.base_x + jitter_x
        enemy.state.shift[2] = enemy.y - enemy.base_y + jitter_y
        if dying then
            enemy.state.rotation = math.sin(enemy.phase_time * 119 + enemy.phase_seed) * 0.24
                * shake_envelope
        elseif enemy.phase == "exit" then
            enemy.state.rotation = clamp((enemy.x - enemy.anchor_x) * -0.0012, -0.25, 0.25)
        else
            enemy.state.rotation = 0.07 * math.sin(run_time * 1.5 + enemy.phase_seed)
        end
        local death_pulse = dying and (1 + 0.10 * math.sin(enemy.phase_time * 96)
            * shake_envelope) or 1
        enemy.state.scale[1] = spawn_scale * hit_scale * death_pulse
        enemy.state.scale[2] = spawn_scale * hit_scale / death_pulse
        local death_fade = dying and (1 - smoothstep((enemy.phase_time - 0.24) / 0.12)) or 1
        enemy.state.opacity = enemy.active and death_fade or 0
        ctx:update(enemy.object, enemy.state)
        enemy.health_state.progress = enemy.active and not dying
            and clamp(enemy.hp / enemy.max_hp, 0, 1) or 0
        enemy.health_state.opacity = enemy.active and not dying and 1 or 0
        ctx:update(enemy.health, enemy.health_state)
    end
    for i = 1, #player_shots do
        local shot = player_shots[i]
        if shot.weapon_kind == 4 then
            shot.beam_state.shift[1] = shot.beam_origin_x - STAGE_X
            shot.beam_state.shift[2] = shot.beam_origin_y - STAGE_Y
            shot.beam_state.rotation = shot.angle
            shot.beam_state.scale[1] = math.max(shot.visible_length, 0.001)
            shot.beam_state.scale[2] = shot.scale_x
            shot.beam_state.opacity = shot.active and shot.visual_opacity
                * (1 - shot.charge_heat) or 0
            ctx:update(shot.beam_line, shot.beam_state)
            shot.beam_hot_state.shift[1] = shot.beam_state.shift[1]
            shot.beam_hot_state.shift[2] = shot.beam_state.shift[2]
            shot.beam_hot_state.rotation = shot.beam_state.rotation
            shot.beam_hot_state.scale[1] = shot.beam_state.scale[1]
            shot.beam_hot_state.scale[2] = shot.beam_state.scale[2]
            shot.beam_hot_state.opacity = shot.active and shot.visual_opacity
                * shot.charge_heat or 0
            ctx:update(shot.beam_hot_line, shot.beam_hot_state)
        else
            shot.state.shift[1] = shot.x - shot.base_x
            shot.state.shift[2] = shot.y - shot.base_y
            shot.state.rotation = shot.angle - math.pi / 2
            shot.state.scale[1] = shot.scale_x
            shot.state.scale[2] = shot.scale_y
            shot.state.opacity = shot.active and shot.visual_opacity or 0
            shot.state.fill = shot.charge_heat > 0 and "danger" or shot.base_fill
            ctx:update(shot.object, shot.state)
            if shot.heat_object then
                shot.heat_state.opacity = shot.active and shot.charge_heat or 0
                ctx:update(shot.heat_object, shot.heat_state)
            end
        end
    end
    for _, kind in ipairs({"ring", "aim", "spiral"}) do
        commit_enemy_shot_pool(ctx, enemy_shot_pools[kind])
    end
    for i = 1, #impacts do
        local impact = impacts[i]
        local progress = clamp(impact.age / impact.duration, 0, 1)
        impact.state.shift[1] = impact.x - impact.base_x
        impact.state.shift[2] = impact.y - impact.base_y
        if impact.kind == "death" then
            impact.state.scale[1], impact.state.scale[2] = 1, 1
            impact.state.rotation = 0
            impact.state.opacity = 0
        else
            impact.state.scale[1] = 0.55 + progress * 1.45
            impact.state.scale[2] = 0.55 + progress * 1.45
            impact.state.rotation = 0
            impact.state.opacity = impact.active and (1 - progress) or 0
        end
        ctx:update(impact.object, impact.state)
        local flare = ease_out_cubic(progress)
        impact.death_state.shift[1] = impact.x - impact.base_x
        impact.death_state.shift[2] = impact.y - impact.base_y
        impact.death_state.scale[1] = 0.28 + flare * 1.32
        impact.death_state.scale[2] = 0.28 + flare * 1.72
        impact.death_state.rotation = 0.018 * math.sin(progress * math.pi)
        impact.death_state.opacity = impact.active and impact.kind == "death"
            and (1 - smoothstep((progress - 0.08) / 0.92)) or 0
        ctx:update(impact.death_object, impact.death_state)
    end
    CARD_TYPES.commit_number(ctx, life_number, hp)
    CARD_TYPES.commit_number(ctx, score_number, kills)
    CARD_TYPES.commit_number(ctx, breach_number, breaches)
    CARD_TYPES.commit_number(ctx, threat_number, unlocked_enemies)
    CARD_TYPES.commit_number(ctx, power_number, power_level)
    weapon_indicators[1].state.shift[2] = (3 - weapon_type) * HEIGHT * 2
    ctx:update(weapon_indicators[1].object, weapon_indicators[1].state)
    CARD_TYPES.commit_number(ctx, round_number, math.floor(run_time / ROUND_PERIOD) + 1)
    dash_state.progress = dash_charge
    ctx:update(dash_gauge, dash_state)
    local game_over_scale = game_is_over and (1 + 0.018 * math.sin(run_time * 4)) or 1
    game_over_state.scale[1] = game_over_scale
    game_over_state.scale[2] = game_over_scale
    game_over_state.opacity = game_is_over and 1 or 0
    ctx:update(game_over, game_over_state)
    ctx:update(game_over_outer, game_over_outer_state)
    ctx:update(game_over_inner, game_over_inner_state)
    ctx:update(game_over_title, game_over_title_state)
    ctx:update(game_over_hint, game_over_hint_state)
end

local function pointer_card_slot(pointer)
    if not pointer then return 0 end
    for slot = 1, #CARD_POSITIONS do
        local center_x = CARD_POSITIONS[slot].x + WIDTH / 2
        local center_y = HEIGHT / 2 - CARD_POSITIONS[slot].y
        if math.abs(pointer.position.x - center_x) <= 92
            and math.abs(pointer.position.y - center_y) <= 150 then
            return slot
        end
    end
    return 0
end

local function update_card_selection(ctx, card1, card2, card3, choose, dt)
    card_time = card_time + dt
    card_hover = pointer_card_slot(ctx:pointer(PRIMARY_POINTER))
    local response = 1 - math.exp(-CARD_HOVER_RESPONSE * dt)
    for slot = 1, #card_hover_amount do
        local target = card_hover == slot and 1 or 0
        card_hover_amount[slot] = lerp(card_hover_amount[slot], target, response)
    end
    if card_time < CARD_INPUT_DELAY then
        card_pointer_slot = 0
        return
    end

    local slot = 0
    if card1.pressed then slot = 1
    elseif card2.pressed then slot = 2
    elseif card3.pressed then slot = 3 end
    if choose.pressed then card_pointer_slot = card_hover end
    if choose.released then
        if slot == 0 and card_pointer_slot > 0
            and card_hover == card_pointer_slot then
            slot = card_pointer_slot
        end
        card_pointer_slot = 0
    end
    if slot > 0 then apply_card(card_choices[slot]) end
end

local function cycle_weapon()
    weapon_type = weapon_type % #CARD_TYPES[6].weapons + 1
    power_level = CARD_TYPES[6].levels[weapon_type]
    CARD_TYPES[6].burst.remaining, CARD_TYPES[6].burst.timer = 0, 0
    charging, charge_time, charge_flash = false, 0, 0
    CARD_TYPES.queue_sound(CARD_TYPES.sound_assets.weapon_switch)
end

local runtime = vis.runtime(scene, {
    fixed_step = 1 / 120,
    max_steps = 8,
    update = function(ctx, dt)
        local move = ctx:action("Move")
        local boost = ctx:action("Boost")
        local dash = ctx:action("Dash")
        local fire = ctx:action("Fire")
        local switch_weapon = ctx:action("SwitchWeapon")
        local reset = ctx:action("Reset")
        if reset.pressed then
            reset_game()
        elseif switch_weapon.pressed and not game_is_over then
            cycle_weapon()
        end
        if not game_is_over then
            run_time = run_time + dt
            hit_invulnerable = math.max(0, hit_invulnerable - dt)
            update_player(move, boost, dash, fire, ctx:pointer(PRIMARY_POINTER), dt)
            CARD_TYPES[6].update_burst(dt)
            update_enemies(dt)
            update_player_shots(dt)
            local enemy_dt = dt * TUNING.enemy_time_scale(run_time)
            update_enemy_shot_pool(enemy_shot_pools.ring, enemy_dt)
            update_enemy_shot_pool(enemy_shot_pools.aim, enemy_dt)
            update_enemy_shot_pool(enemy_shot_pools.spiral, enemy_dt)
            update_impacts(dt)
            update_actor_collisions()
        end
        update_screen_shake(dt)
        CARD_TYPES.flush_sounds(ctx)
        commit_overlays(ctx)
    end,
})

runtime:bind_key("Move", "A", {-1, 0})
runtime:bind_key("Move", "ArrowLeft", {-1, 0})
runtime:bind_key("Move", "D", {1, 0})
runtime:bind_key("Move", "ArrowRight", {1, 0})
runtime:bind_key("Move", "W", {0, 1})
runtime:bind_key("Move", "ArrowUp", {0, 1})
runtime:bind_key("Move", "S", {0, -1})
runtime:bind_key("Move", "ArrowDown", {0, -1})
runtime:bind_pointer("Fire", 0, {1, 0}, PRIMARY_POINTER)
runtime:bind_key("SwitchWeapon", "Tab")
runtime:bind_key("Boost", "Shift")
runtime:bind_key("Dash", "Space")
runtime:bind_key("Reset", "R")

return scene
