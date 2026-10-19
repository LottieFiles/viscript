// Types of the JavaScript scene API in bindings.js, checked by src/test/testTypes.ts.
export type Color = string;
export type ThemeColorRole = "background" | "foreground" | "muted" | "accent" | "secondary" |
    "success" | "warning" | "danger" | "info" | "surface" | "border" | "result" | "focus";
export type AnimCurvePreset = "linear" | "smooth" | "ease_in" | "ease_out" | "ease_in_out" |
    "gentle" | "snappy" | "back" | "bounce" | "elastic";
export type AnimCurve = AnimCurvePreset |
    Readonly<{preset: AnimCurvePreset; strength: number; reverse?: boolean}> |
    Readonly<{bezier: readonly [number, number, number, number]; strength: number; reverse?: boolean}>;
export interface AnimCurveFactory {
    preset(preset: AnimCurvePreset, strength?: number): AnimCurve;
    cubicBezier(x1: number, y1: number, x2: number, y2: number, strength?: number): AnimCurve;
    reverse(curve: AnimCurve): AnimCurve;
}
/** All inputs and the result must be finite; stiffness/mass must be positive and finite, damping/mass finite. */
export interface SpringResponseOptions {
    /** Positive mass in a consistent mass unit. */
    mass: number;
    /** Positive stiffness in mass units/s^2. */
    stiffness: number;
    /** Non-negative damping in mass units/s. */
    damping: number;
    /** Initial displacement from equilibrium in scene units. */
    displacement: number;
    /** Initial velocity in scene units/s; defaults to zero. */
    velocity?: number;
}
export type AudioBus = "music" | "effect" | "ui";
export interface SoundOptions {
    asset: string;
    bus?: AudioBus;
    begin?: number;
    end?: number;
    gain?: number;
    loop?: boolean;
}
export type CreateDirection = "forward" | "reverse" | "clockwise" | "counterclockwise";
export type GrowthEdge = "left" | "right" | "bottom" | "top";
export type Vec2 = readonly [number, number] | {x: number; y: number};
export type Vec3 = readonly [number, number, number] | {x: number; y: number; z: number};
export type WorldVector = Vec2 | Vec3;
export type Range = readonly [number, number, number] | {min: number; max: number; step: number};
export type Mat4 = readonly [
    number, number, number, number,
    number, number, number, number,
    number, number, number, number,
    number, number, number, number,
];

export interface Camera {
    mode?: "fixed" | "interactive";
    view?: "2d" | "3d";
    target?: WorldVector;
    eye?: WorldVector;
    up?: WorldVector;
    height?: number;
    projection?: "perspective" | "orthographic";
    fov?: number;
    near?: number;
    far?: number;
}

export interface CameraTarget {
    view?: "2d" | "3d";
    eye?: WorldVector;
    /** A point, or handles whose authored bounds' center the camera aims at, the eye keeping its offset. */
    target?: WorldVector | Handle | readonly Handle[];
    up?: WorldVector;
    height?: number;
    projection?: "perspective" | "orthographic";
    fov?: number;
    near?: number;
    far?: number;
}

export type ThemePreset = "pro_white" | "pro_black" | "adaptive_vscode";
export interface TextTheme {
    font?: string;
    size?: number;
    color?: Color;
}
export interface AxisTheme {
    x?: Color;
    y?: Color;
    z?: Color;
    grid?: Color;
    label?: Color;
}
export interface SemanticTheme {
    foreground?: Color;
    muted?: Color;
    accent?: Color;
    secondary?: Color;
    success?: Color;
    warning?: Color;
    danger?: Color;
    info?: Color;
    surface?: Color;
    border?: Color;
    result?: Color;
    focus?: Color;
}
export interface ThemeOptions {
    preset?: ThemePreset;
    background?: Color;
    text?: {
        h1?: TextTheme;
        h2?: TextTheme;
        h3?: TextTheme;
        text?: TextTheme;
        code?: TextTheme;
    };
    objects?: readonly Color[];
    object_width?: number;
    /** Corner radius of Theme-following Diagram nodes as a share (0 to 0.5) of the shorter node-size side; zones round 1.5 times as much, and 0 keeps them square. */
    roundness?: number;
    gradient?: boolean;
    end_gradient_stop?: Color;
    axis?: AxisTheme;
    colors?: SemanticTheme;
}

export interface SceneConfig {
    width?: number;
    height?: number;
    fps?: number;
    background?: Color;
    camera?: Camera;
    antialiasing?: boolean;
    loop?: boolean;
    theme?: ThemePreset | ThemeOptions;
}

export interface ViewportBounds {
    x: number;
    y: number;
    width: number;
    height: number;
}

export type StrokeDash =
    | readonly [number]
    | readonly [number, number]
    | readonly [number, number, number]
    | readonly [number, number, number, number];

/** How a scene transition hands one stage to the next; `match` is the default. */
export type SceneTransitionStyleName = "match" | "fade" | "slide" | "zoom_in" | "zoom_out";
/** The side a `slide` brings the incoming stage in from; `right` is the default. */
export type SceneTransitionSide = "left" | "right" | "bottom" | "top";
/** The id of an object in either stage, or a part of the viewport as fractions of it. */
export type SceneTransitionArea = string | ViewportBounds;
export interface SceneTransitionStyleOptions {
    style?: SceneTransitionStyleName;
    /** Only with `style: "slide"`. */
    from?: SceneTransitionSide;
    /** Required with `zoom_in` and `zoom_out`, and only there. */
    area?: SceneTransitionArea;
    /** Object-id zoom margin per side, relative to its larger normalized geometry side.
     * Finite and non-negative; 0.05 by default. Zero precedes aspect widening and clamping. */
    area_margin?: number;
    /** With a slide or a zoom, objects with the same id travel above both stages. */
    match?: boolean;
}
/** A style name, or the style with its options. */
export type SceneTransitionStyle = SceneTransitionStyleName | SceneTransitionStyleOptions;
interface SceneTransitionBaseOptions extends SceneTransitionStyleOptions {
    /** Seconds fully shown: one value for all stages, or one per stage. */
    hold?: number | readonly number[];
    /** Play each stage from the start of its incoming transition; false shows final frames. */
    live?: boolean;
    viewport?: ViewportBounds;
    /** One style for each transition, in order, instead of `style`. */
    styles?: readonly SceneTransitionStyle[];
}
export type SceneTransitionOptions = SceneTransitionBaseOptions & TimingOptions;

/** A color stop: a bare color (evenly spaced) or an `[offset, color]` pair with offset in 0..1. */
export type GradientStop = Color | readonly [number, Color];
/** 2 to 8 stops; offsets must be non-decreasing. Omit to keep the paint-color to end-stop ramp. */
export type GradientStops = readonly GradientStop[];
/** Explicit geometry is in the object's local coordinates; omitted geometry follows the shape. */
export interface LinearGradientOptions {
    type: "linear";
    stops?: GradientStops;
    from?: WorldVector;
    to?: WorldVector;
}
export interface RadialGradientOptions {
    type: "radial";
    stops?: GradientStops;
    center?: WorldVector;
    radius?: number;
    focal?: WorldVector;
    focalRadius?: number;
}
export interface ConicGradientOptions {
    type: "conic";
    stops?: GradientStops;
    center?: WorldVector;
    /** Start angle in degrees, clockwise from local +x as seen on screen. */
    angle?: number;
}
export type GradientOptions = LinearGradientOptions | RadialGradientOptions | ConicGradientOptions;

export type MaskMode = "alpha" | "inverse_alpha" | "luma" | "inverse_luma";
export type BlendMode =
    | "normal" | "multiply" | "screen" | "overlay" | "darken" | "lighten" | "color_dodge" | "color_burn"
    | "hard_light" | "soft_light" | "difference" | "exclusion" | "hue" | "saturation" | "color" | "luminosity"
    | "add";
export interface MaskOptions {target: ObjectHandle; mode?: MaskMode}
/** Offset and blur in screen pixels, offset +y up; omitted fields keep the current shadow (at creation: 40% black, [0, -4], 8). */
export interface ShadowOptions {color?: Color; offset?: Vec2; blur?: number}

export interface Style {
    id?: string;
    color?: Color;
    stroke?: Color;
    fill?: Color;
    /** `true` ramps from the paint color to the theme's end_gradient_stop; a color sets that end stop. */
    gradient?: boolean | Color | GradientOptions;
    width?: number;
    radius?: number;
    dash?: StrokeDash;
    dash_offset?: number;
    opacity?: number;
    progress?: number;
    layer?: number;
    /** Draw the object and its descendants into one layer, so a group's opacity fades it as a whole. */
    isolate?: boolean;
    /** Show the object only through the target, which then draws into the mask instead of the scene. */
    mask?: ObjectHandle | MaskOptions;
    /** Draw the object into one layer and combine it with what lies beneath, as CSS mix-blend-mode. */
    blend?: BlendMode;
    /** Gaussian blur sigma in screen pixels (0 to 256) for the object and its descendants as one layer. */
    blur?: number;
    /** A shadow cast by the object and its descendants as one layer. */
    shadow?: ShadowOptions;
}

/** A group draws nothing itself; the stroke, fill and width it sets pass to the shapes below it. */
export interface GroupOptions extends Omit<Style, "color" | "gradient" | "radius" | "dash" | "dash_offset"> {
    matrix?: Mat4;
    /** The texts below the group, at any depth, read as one block of text to the layout audit. */
    typeset?: boolean;
}
/** Object-local distances applied before transforms and drawing progress: one number for both
 * ends, or [start, end]. Values must be finite and nonnegative; a consumed segment is empty. */
export interface LineOptions extends Style {from: WorldVector; to: WorldVector; trimDistance?: number | [number, number]}
export interface ArrowOptions extends LineOptions {tail?: number; tip?: number}
export interface PointOptions extends Style {point: WorldVector}
export interface TextOptions extends Omit<Style, "gradient"> {
    text: string;
    point?: WorldVector;
    size?: number;
    align?: Vec2;
    font?: string;
    orientation?: "billboard" | "plane";
    role?: "h1" | "h2" | "h3" | "text" | "code";
    /** How far the glyphs lean right per unit of height above the baseline, -1 to 1. */
    slant?: number;
    /** Measure the text in its font while the script runs, so layout sees its box. */
    measure?: boolean;
    /** Sample string reserving a fixed box with the starting string; later strings may overflow. */
    reserve?: string;
    /** Center ASCII digits, minus and space in cells of the widest digit advance. */
    tabular?: boolean;
    /** Pin the first baseline at point, measured as the text is made; align then places it only along the line. */
    baseline?: boolean;
}
/** A text that writes a number; play, set and trackers animate it as `value`. */
export interface NumberOptions extends Omit<Style, "gradient"> {
    value?: number;
    /** Digits after the decimal point, 0 to 6. */
    decimals?: number;
    /** Write a plus sign before a positive value. */
    sign?: boolean;
    /** One printable ASCII character that groups the digits in threes, such as "," or " ". */
    separator?: string;
    prefix?: string;
    suffix?: string;
    point?: WorldVector;
    size?: number;
    align?: Vec2;
    font?: string;
    orientation?: "billboard" | "plane";
    role?: "h1" | "h2" | "h3" | "text" | "code";
    slant?: number;
    /** Box the number it starts with for layout. */
    measure?: boolean;
    /** Sample string reserving a fixed box with the starting string; later strings may overflow. */
    reserve?: string;
    /** Center ASCII digits, minus and space in cells of the widest digit advance. */
    tabular?: boolean;
    /** Pin the first baseline at point, measured as the number is made; align then places it only along the line. */
    baseline?: boolean;
}
/** Texts take a color but no stroke or outline. */
export type SpanStyle = Omit<Style, "stroke" | "gradient" | "width" | "radius" | "dash" | "dash_offset">;
/** A string in the row's style, one text with options of its own, or a formula the tex module sets inline. */
export type SpanPart = string | (SpanStyle & {
    text: string;
    size?: number;
    font?: string;
    role?: "h1" | "h2" | "h3" | "text" | "code";
    /** Raises the baseline by this fraction of the row's line height; negative lowers it. */
    rise?: number;
}) | TexSpanPart;
/** With the tex module (-Dmodules=tex), a formula set inline on the row's baseline in text style, as one box
 * that lines never break inside; a text or formula touching it with no blank between stays on its line. */
export interface TexSpanPart extends TexStyle {
    /** TeX math, such as "x^2", or its parts, each a group of its own below the formula. */
    tex: string | readonly TexPart[];
    /** Display style, as $$...$$; inline formulas take text style unless this is true. */
    display?: boolean;
    /** The row's size, font and role unless the part sets its own. */
    size?: number;
    font?: string;
    role?: "h1" | "h2" | "h3" | "text" | "code";
    /** Raises the baseline by this fraction of the row's line height; negative lowers it. */
    rise?: number;
}
/** The row's id, opacity, progress and layer effects go to its group; color, fill and layer to its texts. */
export interface SpansOptions extends SpanStyle {
    point?: WorldVector;
    align?: Vec2;
    size?: number;
    font?: string;
    orientation?: "billboard" | "plane";
    role?: "h1" | "h2" | "h3" | "text" | "code";
    /** Cut strings into one text per word or per letter; part objects stay whole. */
    split?: "none" | "words" | "letters";
    /** Start a new line before a text that would pass this width in world units. */
    wrap?: number;
    /** Baselines this many line heights apart; a line moves down only as far as keeps its ink out of the line above. */
    leading?: number;
    /** Pin the first baseline at point; align then places the lines only along it. */
    baseline?: boolean;
}
/** A formula draws glyphs and filled shapes; color and fill both set the fill that colors it. */
export type TexStyle = Omit<Style, "stroke" | "gradient" | "width" | "radius" | "dash" | "dash_offset">;
/** TeX of the formula's style, or one part with an id and style of its own. */
export type TexPart = string | (TexStyle & {text: string});
/** TeX math set by the tex module (-Dmodules=tex) in the scene's fonts, as one typeset group. */
export interface TexOptions extends TexStyle {
    /** TeX math, such as "a^2+b^2=c^2", or its parts, each a group of its own below the formula. */
    tex: string | readonly TexPart[];
    /** Where the formula's box, its width by its height and depth, is pinned. */
    point?: WorldVector;
    align?: Vec2;
    size?: number;
    font?: string;
    orientation?: "billboard" | "plane";
    role?: "h1" | "h2" | "h3" | "text" | "code";
    /** Display style, as $$...$$; false sets it in text style, as inline $...$. */
    display?: boolean;
    /** Pin the formula's baseline at point instead of its box; align then places it only along the line. */
    baseline?: boolean;
}
/** How an entry sits across its column. */
export type Justify = "left" | "center" | "right";
/** A number counts as `number` does. A string is set as it is in a table; in a matrix it is inline TeX
 * when the tex module is built in, and text otherwise. */
export type MatrixEntry = number | string;
/** The matrix's id, opacity, progress and layer effects go to its group; color and fill to its entries,
 * color, stroke and width to its brackets, and layer to every part. */
export interface MatrixOptions extends Omit<Style, "gradient" | "radius" | "dash" | "dash_offset"> {
    /** Rows of entries, all as long; at most 1024 entries. */
    rows: readonly (readonly MatrixEntry[])[];
    point?: WorldVector;
    align?: Vec2;
    brackets?: "square" | "round" | "bar" | "none";
    /** World units between columns and between rows, or one number for both. */
    gap?: number | Vec2;
    /** For every column, or one for each column. */
    justify?: Justify | readonly Justify[];
    /** Digits after the point of every number, 0 to 6; each takes the fewest it needs otherwise. */
    decimals?: number;
    /** Sample for every column, or one string per column; widths stay fixed. */
    reserve?: string | readonly string[];
    /** Use tabular digit cells in every entry, including string entries. */
    tabular?: boolean;
    size?: number;
    font?: string;
    orientation?: "billboard" | "plane";
    role?: "h1" | "h2" | "h3" | "text" | "code";
}
/** The table's id, opacity, progress and layer effects go to its group; color and fill to its entries,
 * stroke and width to its rules, and layer to every part but its stripes, which lie one layer below. */
export interface TableOptions extends Omit<Style, "gradient" | "radius" | "dash" | "dash_offset"> {
    /** The rows below the header, all as long; at most 1024 entries with the header. */
    rows: readonly (readonly MatrixEntry[])[];
    /** One entry above each column, in the accent color unless color or fill is set. */
    header?: readonly MatrixEntry[];
    point?: WorldVector;
    align?: Vec2;
    /** World units inside a cell around its entry, or one number for both; 0.8 and 0.3 lines by default. */
    padding?: number | Vec2;
    /** For every column, or one for each column. */
    justify?: Justify | readonly Justify[];
    /** Digits after the point of every number, 0 to 6; each takes the fewest it needs otherwise. */
    decimals?: number;
    /** Fill every second row below the header, in the theme's surface color or this one. */
    stripes?: boolean | string;
    /** The rule under the header, by default when there is one, and those between rows, columns, or
     * both with a frame around. */
    rules?: "header" | "rows" | "columns" | "grid" | "none";
    size?: number;
    font?: string;
    orientation?: "billboard" | "plane";
    role?: "h1" | "h2" | "h3" | "text" | "code";
}
export type CodeLanguage = "plain" | "lua" | "python" | "javascript" | "typescript" | "c" | "cpp";
export type CodeClass = "plain" | "keyword" | "string" | "number" | "comment" | "function" | "type" | "operator";
/** The block's id, opacity, progress and layer effects go to its group; layer to its tokens. */
export interface CodeOptions extends Omit<SpanStyle, "fill"> {
    text: string;
    /** How tokens split and color; plain splits the same way but colors every token alike. */
    language?: CodeLanguage;
    point?: WorldVector;
    align?: Vec2;
    size?: number;
    font?: string;
    orientation?: "billboard" | "plane";
    /** The space between baselines as a multiple of the line height; exclusive with pitch. */
    leading?: number;
    /** Positive distance between baselines in world units; exclusive with leading. */
    pitch?: number;
    /** Pin the first line's baseline at point; align.x places it and align.y is ignored. */
    baseline?: boolean;
    /** Hex colors or Theme roles for the classes it names, over color and the Theme's. */
    colors?: Partial<Record<CodeClass, Color>>;
}
/** A line, or the lines first to last, counting from 1. */
export type CodeLines = number | readonly [number, number];
/** Which matches of a text or a RegExp a search or an edit takes, and where it looks. */
export interface SearchOptions {
    /** The first match, the last, every one, or the nth from 1; the first by default. */
    match?: "first" | "last" | "all" | number;
    /** Look only in these lines; a RegExp sees them alone, so ^ and $ with the m flag match at their ends. */
    lines?: CodeLines;
    /** Take only matches that cut no token. */
    whole?: boolean;
}
/**
 * One change to the text of a code block. A string or a RegExp finds the text to change, and
 * the options of SearchOptions pick its matches; a line number or [first, last] names whole
 * lines instead. append and prepend add whole lines.
 */
export type CodeEdit =
    ({replace: string | RegExp; with: string} & SearchOptions) |
    {replace: CodeLines; with: string} |
    ({insert: string; before: string | RegExp} & SearchOptions) |
    ({insert: string; after: string | RegExp} & SearchOptions) |
    {insert: string; before: number} |
    {insert: string; after: number} |
    ({remove: string | RegExp} & SearchOptions) |
    {remove: CodeLines} |
    {append: string} |
    {prepend: string};
/** trimDistance uses the same object-local endpoint distances as LineOptions. */
export interface VectorOptions extends Style {value: WorldVector; origin?: WorldVector; trimDistance?: number | [number, number]; tail?: number; tip?: number}
export interface SpaceOptions extends Omit<Style, "gradient"> {
    x?: Range;
    y?: Range;
    z?: Range;
    matrix?: Mat4;
    axis_x?: Color;
    axis_y?: Color;
    axis_z?: Color;
    /** Axis and tick width in pixels; defaults to max(width, 1.5); zero hides strokes. */
    axis_width?: number;
    numbers?: boolean;
    number_mode?: "fixed" | "relative";
    number_size?: number;
    number_color?: Color;
    /** Ticks across the axes: true for one at every step of each range, or a step for each axis. */
    ticks?: boolean | SpaceTicks;
    /** Tick length in pixels, centered on the axis; 8 by default. */
    tick_length?: number;
}
/** The step between ticks on each axis, which sit at its multiples inside the range but not at 0; 0 or false
 * for none, true for the step of the range. */
export interface SpaceTicks {
    x?: number | boolean;
    y?: number | boolean;
    z?: number | boolean;
}
/** A value on an axis and its label: a string, which becomes a text below the space, or a handle of the scene,
 * such as a formula, which keeps its parent and moves beside the tick. */
export type SpaceLabel = readonly [number, string | ObjectHandle];
export interface SpaceLabelsOptions {
    /** World placement is authored once. Screen placement keeps billboard Text ink outward
     * from the projected space origin at every sampled camera; handles must be direct Text children.
     * Place screen labels before authoring the Space's first warp so their anchors and origin move together.
     * Screen placement does not take side or avoid other drawings or the canvas. */
    placement?: "world" | "screen";
    x?: readonly SpaceLabel[];
    y?: readonly SpaceLabel[];
    z?: readonly SpaceLabel[];
    /** Pixels between the end of a tick and its label; 4 by default. */
    gap?: number;
    /** Size of the texts made from strings; the space's number_size by default. */
    size?: number;
    /** Fill of the texts made from strings; the space's number_color by default. */
    color?: Color;
    /** Side of each axis its labels sit on; below x and left of y and z by default. */
    side?: {x?: "below" | "above"; y?: "left" | "right"; z?: "left" | "right"};
}
export type CellSampler = (x: number, y: number, time: number) => Color;
export type VoxelSampler = (x: number, y: number, z: number, time: number) => Color;
export interface CellSampleOptions {
    mode?: "full" | "padd";
    padding?: number;
    duration?: number;
    fps?: number;
}
export interface VoxelSampleOptions extends CellSampleOptions {}
export interface CircleOptions extends Style {center?: WorldVector; radius?: number}
/** Signed radians counterclockwise from +x; a stroke by default, or a filled pie with sector. */
/** Radius is positive; signed sweep is within -2*pi..2*pi. */
export interface ArcOptions extends Style {
    center?: WorldVector;
    radius?: number;
    start?: number;
    angle?: number;
    sector?: boolean;
}
/** The inner angle between from and to seen from vertex; children follow its bisector. */
/** Radius is positive and label_distance non-negative. A zero-length xy ray hides the mark. */
export interface AngleOptions extends Style {
    vertex?: WorldVector;
    from?: WorldVector;
    to?: WorldVector;
    radius?: number;
    other?: boolean;
    right?: "auto" | "never" | "always";
    label?: string | TextOptions | TexOptions;
    label_distance?: number;
}
export interface RectangleOptions extends Style {
    center?: WorldVector;
    size?: Vec2;
    corner?: number;
    /** Objects whose bounds, where the rectangle joins, set its center and size in place of those. */
    around?: Handle | readonly Handle[];
    /** Space added on every side of around: one number, or [x, y]. Defaults to 0.1. */
    padding?: number | Vec2;
}
/** Snapshot brace in its joining parent's coordinates. Offset, depth and gap default to 0.25,
 * 0.4 and 0.2 times the configured text size, converted at the authored camera for billboard text. */
export interface BraceOptions extends Style {
    /** Objects whose bounds, where the brace joins, it lies beside, on their side facing direction. */
    around?: Handle | readonly Handle[];
    /** The ends of a segment the brace spans in place of around, on the right of the way from one
     * to the other unless direction sets the side. */
    from?: WorldVector;
    to?: WorldVector;
    /** Where the brace lies from what it spans, and its tip points: "down" by default for around. */
    direction?: "down" | "up" | "left" | "right" | Vec2;
    /** Nonnegative parent-space distance from the spanned bounds or segment to the brace. */
    offset?: number;
    /** How far the tip stands out from the ends of the brace. */
    depth?: number;
    /** Set gap off the tip: a string as a text named `<id>/label`, or a handle in the same group moved
     * there by its bounds. */
    label?: string | ObjectHandle;
    /** Space between the tip and the label's bounds; a string label's bounds cover its ink. */
    gap?: number;
    size?: number;
    font?: string;
    orientation?: "billboard" | "plane";
    role?: "h1" | "h2" | "h3" | "text" | "code";
}
export interface PolygonOptions extends Style {points: readonly WorldVector[]}
/**
 * A line through points, or the graph of fn, which the builder samples where Lua plot would and cuts as it would,
 * then emits its endpoint and uncut samples. Either way the plot is one object, drawn on by vertices.
 */
export type PlotOptions = Style & ({
    /** A point with a coordinate that is not finite, such as NaN, breaks the line. */
    points: readonly WorldVector[];
    fn?: never;
    x_range?: never;
} | {
    points?: never;
    /** Returns undefined, null, NaN or an infinity where the graph is undefined, and the graph breaks there. */
    fn: (x: number) => number | null | undefined;
    /** Sampled from min every step up to max; at most 1000000 samples. */
    x_range: Range;
}) & {
    /** Solid active endpoint in pixels, using the sampled stroke property color independently of the plot gradient. Zero by default; positive also shows a manually set progress 0. */
    endpoint_radius?: number;
    /** [lo, hi]: the graph ends exactly on an edge where it leaves the band and starts again where it returns, and ends flat. */
    y_range?: readonly [number, number];
    /**
     * The x where the graph must not join: fn is sampled a thousandth of a step on either side of each instead, a
     * line of points breaks between two points on either side, and the plot ends flat.
     */
    discontinuities?: readonly number[];
};
export interface AreaOptions extends Style {
    fn: (x: number) => number;
    /** Sampled every step from min to max, the last step shortened to end at max. */
    x_range: Range;
    /** The lower edge: a number, 0 by default, or a second function. */
    base?: number | ((x: number) => number);
}
/** At most 1024 rectangles; provide fn or one value per interval. */
export interface RiemannOptions extends Omit<Style, "radius"> {
    /** The function the builder calls for the height of each rectangle. */
    fn?: (x: number) => number;
    /** The heights themselves, one per step, instead of fn. */
    values?: readonly number[];
    /** One rectangle per step from min to max, the last step shortened to end at max. */
    x_range: Range;
    /** Where in its step each rectangle reads fn; left by default. */
    sample?: "left" | "right" | "midpoint";
    /** The fill of the rectangles below the axis. */
    negative?: Color;
}
/** The field's id, opacity, progress and layer effects go to its group; color, stroke, width, dashes and layer
 * to its arrows. */
/** At most 1024 arrows. Provide fn or sampled values; length and scale are mutually exclusive. */
export interface VectorFieldOptions extends Omit<Style, "fill" | "gradient" | "radius"> {
    /** The field at a point, as [x, y]; the builder calls it at every sample and emits the vectors. */
    fn?: (x: number, y: number) => readonly [number, number];
    /** One vector for each sample, row by row from the bottom left, instead of fn. */
    values?: readonly (readonly [number, number])[];
    /** The columns of samples, from min every step up to max. */
    x_range: Range;
    /** The rows of samples, from min every step up to max. */
    y_range: Range;
    /** World units arrows approach as their vectors grow; 0.8 of the smaller step by default. */
    length?: number;
    /** Draws every vector at this multiple of its own length instead of approaching length. */
    scale?: number;
    /** Where an arrow stands on its sample; tail by default. */
    anchor?: "tail" | "middle";
    /** 2 to 8 colors the magnitudes spread over, from the least; the theme's accent, success, warning and
     * danger by default, unless color or stroke sets one for every arrow. */
    colors?: readonly Color[];
    /** The magnitudes the colors spread over; zero to the greatest by default. */
    magnitude?: Vec2;
    /** The arrowhead's length in pixels; 10 by default. */
    tip?: number;
}
export interface ContourLabelOptions {
    /** One fixed-point conversion, %.0f through %.6f; defaults to %.1f. */
    format?: "%.0f" | "%.1f" | "%.2f" | "%.3f" | "%.4f" | "%.5f" | "%.6f";
    /** Plane text size in contour-local units; defaults to 0.12. */
    size?: number;
    /** Repeat distance in local units; zero (default) places one label per curve. */
    spacing?: number;
    /** Local gap and collision padding; defaults to 0.04. */
    padding?: number;
    /** Keep tangent angles between -pi/2 and pi/2; defaults to true. */
    upright?: boolean;
}
/** One plot per level. Ranges need at least two samples per axis; at most 65536 grid samples and
 * 1000000 grid samples times levels. Generated contour-label handles are available in native reports,
 * not builder children()/find(); at most 4096 label slots per level. */
export type ContourOptions = Omit<Style, "fill" | "radius" | "gradient"> & {
    levels: readonly number[];
    x_range: Range;
    y_range: Range;
    /** One color per level, or a colormap spread over the smallest to largest level; overrides stroke. */
    colors?: readonly Color[] | "viridis" | "plasma" | "inferno" | "magma" | "cividis";
    /** Retained text children with tangent rotation and gaps; defaults to false. */
    labels?: boolean | ContourLabelOptions;
} & ({
    /** Sampled at construction; crossings follow the bilinear grid, rather than calling fn again. */
    fn: (x: number, y: number) => number;
    values?: never;
} | {
    fn?: never;
    /** One finite scalar per sample, row by row from the bottom left, x varying fastest. */
    values: readonly number[];
});
export interface RouteOptions extends Style {
    points: readonly WorldVector[];
    tail?: number;
    tip?: number;
}
export type PathCommand =
    {type: "move" | "line"; to: WorldVector} |
    {type: "quadratic"; control: WorldVector; to: WorldVector} |
    {type: "cubic"; control1: WorldVector; control2: WorldVector; to: WorldVector} |
    {type: "close"};
/** Each move starts a contour; the contours all close or all stay open, and several fill by the even-odd rule, cutting holes. */
export interface PathOptions extends Style {commands: readonly PathCommand[]; samples?: number}
export interface RegionOptions extends Style {
    /** 2 to 64 circles, rectangles, polygons or closed paths, folded from the left. */
    of: readonly ObjectHandle[];
}
export interface CurveOptions extends Style {
    from: WorldVector;
    control1: WorldVector;
    control2: WorldVector;
    to: WorldVector;
    samples?: number;
}
/** Named colormaps sampled at nine evenly spaced stops. */
export type Colormap = "viridis" | "plasma" | "inferno" | "magma" | "cividis";
export interface SurfaceColormap {
    map: Colormap;
    /** The heights of the map's first and last colors, the extent of the points by default; low above high runs it the other way. */
    range?: readonly [number, number];
    /** The direction heights run along, "z" by default; a vector is made a unit long. */
    axis?: "x" | "y" | "z" | Vec3;
}
/** At most 16384 points. Surface morphs require matching grids; callbacks supply endpoint colors only. */
export interface SurfaceOptions extends Style {
    points: readonly WorldVector[];
    size: readonly [number, number];
    mode?: "solid" | "mesh" | "solid_mesh";
    shading?: boolean;
    /**
     * A light fixed in the world in place of the camera's headlight: direction points from the
     * surface toward it, and a face keeps ambient (a half by default) of its color where the
     * light does not reach. Needs shading.
     */
    light?: {direction: Vec3; ambient?: number};
    /** Mesh line counts [columns, rows], each zero or 2..sample count. Requires mesh/solid_mesh mode.
     * Lines span the surface evenly and obey depth; omitted counts outline every face. */
    mesh?: readonly [number, number];
    /** Face colors from a height colormap, explicit map/range/axis, or one color per point.
     * A callback runs once per point. Faces sample the map at their center or average corner colors. */
    colors?: Colormap | SurfaceColormap | ((x: number, y: number, z: number) => Color) | readonly Color[];
}
/** Joins sampled closed outlines, or bounds when none cross the center ray. Groups use the outermost crossing. */
export interface ConnectorOptions extends Style {
    /** World distance beyond each outline along the connector. */
    padding?: number;
    tail?: number;
    tip?: number;
}
/**
 * Without an id the copy takes `<id>:copy`, then `<id>:copy2` and so on after the source's id; the
 * style options change the copy's own settings before it is attached.
 */
export interface CloneOptions extends Style {
    /** The copy's parent; beside the source by default. */
    parent?: ObjectHandle;
    /** Moves the copy by this world vector before it is attached; a connector follows its endpoints instead. */
    shift?: WorldVector;
}
/** Picture from a registered asset (SVG stays vector, PNG is raster; native builds also read JPG and WebP) or inline RGBA pixels. */
export type PictureOptions = Omit<Style, "color" | "stroke" | "fill" | "gradient" | "radius" | "dash" | "dash_offset"> & ({
    asset: string;
    pixels?: never;
    size?: never;
} | {
    asset?: never;
    pixels: readonly Color[];
    size: readonly [number, number];
}) & {
    center?: WorldVector;
    width?: number;
    filter?: "bilinear" | "nearest";
};
export type CellRegion = readonly [number, number, number, number] | {
    x: number;
    y: number;
    width: number;
    height: number;
};
export type CellOptions = Omit<Style, "stroke" | "fill" | "gradient" | "width" | "radius" | "dash" | "dash_offset"> & {
    origin?: WorldVector;
    size: readonly [number, number];
    mode?: "full" | "padd";
    padding?: number;
    depth?: number;
    texture?: string;
    source?: CellRegion;
    destination?: CellRegion;
    patches?: readonly {region: CellRegion; color: Color}[];
};

declare const handle: unique symbol;
export interface Handle {readonly [handle]: true}
export interface ObjectFactory {
    group(options?: GroupOptions): GroupHandle;
    space(options?: SpaceOptions): SpaceHandle;
    point(options: PointOptions): ObjectHandle;
    line(options: LineOptions): ObjectHandle;
    arrow(options: ArrowOptions): ObjectHandle;
    vector(options: VectorOptions): ObjectHandle;
    circle(options?: CircleOptions): ObjectHandle;
    arc(options?: ArcOptions): ObjectHandle;
    angle(options?: AngleOptions): ObjectHandle;
    rectangle(options?: RectangleOptions): ObjectHandle;
    /** A curly brace and the label at its tip, which is undefined without one. */
    brace(options: BraceOptions & {label: string | ObjectHandle}): [ObjectHandle, ObjectHandle];
    brace(options: BraceOptions): [ObjectHandle, ObjectHandle | undefined];
    polygon(options: PolygonOptions): ObjectHandle;
    plot(options: PlotOptions): ObjectHandle;
    /** The region between the graph of fn and base over x_range, as a polygon the builder samples. */
    area(options: AreaOptions): ObjectHandle;
    /** The rectangles of a Riemann sum; the group's children are the rectangles from the left. */
    riemann(options: RiemannOptions): GroupHandle;
    /** An arrow for the field at every sample of a grid; the group's children are the arrows. */
    vectorField(options: VectorFieldOptions): GroupHandle;
    /** Traces level curves from a sampled scalar field; the group's children follow levels. */
    contour(options: ContourOptions): GroupHandle;
    route(options: RouteOptions): ObjectHandle;
    path(options: PathOptions): ObjectHandle;
    /** The area any of the shapes covers, as a closed path where this factory adds it. */
    union(options: RegionOptions): ObjectHandle;
    /** The area all of the shapes cover. */
    intersection(options: RegionOptions): ObjectHandle;
    /** The area the first shape covers and none of the rest. */
    difference(options: RegionOptions): ObjectHandle;
    /** The area an odd number of the shapes cover. */
    exclusion(options: RegionOptions): ObjectHandle;
    curve(options: CurveOptions): ObjectHandle;
    surface(options: SurfaceOptions): ObjectHandle;
    text(options: TextOptions): ObjectHandle;
    number(options?: NumberOptions): ObjectHandle;
    /** Texts measured in their fonts and set side by side; the group's children are the texts, and the
     * formulas of tex parts as TexHandles. */
    spans(parts: readonly SpanPart[], options?: SpansOptions): GroupHandle;
    /** TeX math as a typeset group; set from parts, the group's children are the parts' groups. */
    tex(options: TexOptions): TexHandle;
    /** Source code in colored tokens; the group's children are the lines, and theirs the tokens. */
    code(options: CodeOptions): GroupHandle;
    /** Entries in rows and columns between brackets; the group's children are its rows, then its brackets. */
    matrix(options: MatrixOptions): GroupHandle;
    /** Entries in cells that tile a grid; the group's children are its rows, the header's first, then its
     * stripes and its rules. */
    table(options: TableOptions): GroupHandle;
    picture(options: PictureOptions): ObjectHandle;
    cell(options: CellOptions): ObjectHandle;
    connector(from: ObjectHandle, to: ObjectHandle, options?: ConnectorOptions): ObjectHandle;
}

export interface ObjectHandle extends Handle, ObjectFactory {
    label(options: TextOptions): ObjectHandle;
    moveTo(point: WorldVector): this;
    nextTo(target: ObjectHandle, direction: WorldVector, gap?: number): this;
    alignTo(target: ObjectHandle, direction: WorldVector): this;
    /** `null` removes the mask. */
    mask(target: ObjectHandle | null, mode?: MaskMode): this;
    /** The handles created below this one, in order. */
    children(): ObjectHandle[];
}

export interface GroupHandle extends ObjectHandle {
    arrange(direction?: WorldVector, gap?: number): this;
    arrangeGrid(columns: number, columnGap?: number, rowGap?: number): this;
    /** The rowth group directly below this one, counting from 1, as a line of a code block. */
    row(row: number): GroupHandle;
    /** The columnth handle directly below that row. */
    entry(row: number, column: number): ObjectHandle;
    /** The columnth handle directly below each row that has one, in order. */
    column(column: number): ObjectHandle[];
    /**
     * The tokens of a code block that the matches of find cover any part of, in order; a match
     * inside a token takes the whole token.
     */
    search(find: string | RegExp, options?: SearchOptions): ObjectHandle[];
}

export interface TexHandle extends GroupHandle {
    /** The nth part whose TeX reads tex, blanks aside, or else the nth glyph or shape whose TeX does,
     * such as "c" or "\\frac" for a bar; on a part, the nth of its own glyphs and shapes. */
    part(tex: string, nth?: number): TexHandle;
}

export interface SpaceHandle extends ObjectHandle {
    cell(options: CellOptions): ObjectHandle;
    cell(callback: CellSampler, options?: CellSampleOptions): GroupHandle;
    voxel(callback: VoxelSampler, options?: VoxelSampleOptions): GroupHandle;
    /**
     * Place labels beside the ticks of this space and return their handles in order. A string may come at any
     * time; a handle must be placed before the first play, as layout must.
     * With a space id, generated texts have ids <space-id>/label/<axis>/N, using the first free N from 1.
     */
    labels(options: SpaceLabelsOptions): ObjectHandle[];
}

declare const audioCueHandle: unique symbol;
export interface AudioCueHandle {
    readonly [audioCueHandle]: true;
    gain(value: number, begin: number, duration?: number, curve?: AnimCurve): this;
    gain(value: number, begin: number, options: TimingOptions): this;
}

export interface ViewOptions {
    /** The camera the view opens with, as a look over the scene's camera; its height or field of view spans the rectangle's height. */
    camera?: CameraTarget;
    /** Stroke widths, point radii, dashes and billboard text grow with the view's magnification over the scene's camera; true by default. */
    optical?: boolean;
    /** Fills the rectangle below what the view draws; the scene's background by default, and a transparent color lets the frame show through. */
    background?: Color;
    /** How opaque the whole view starts, from 0 to 1. */
    opacity?: number;
    /** The view draws the objects on this layer and below and sits above them, so objects on higher layers draw over it; above every layer by default. */
    layer?: number;
    /** Objects the view leaves out, each with everything below it; at most 256. */
    hide?: Handle | readonly Handle[];
    /** An object whose drawn center the view's camera keeps as its target. */
    follow?: Handle;
}

declare const viewHandle: unique symbol;
/** A view from SceneBuilder.view: its camera moves and the whole view fades with the calls the scene's camera takes. */
export interface ViewHandle {
    readonly [viewHandle]: true;
    look(camera: CameraTarget, duration?: number, curve?: AnimCurve): this;
    look(camera: CameraTarget, options: TimingOptions): this;
    orbit(radians: number, options?: OrbitOptions): this;
    /** Frame the targets in the view's rectangle with its 2D camera; no target returns to the camera the view opened with. */
    focus(target?: Handle | readonly Handle[] | null, options?: FocusOptions): this;
    /** Fade the whole view; the fades of one view follow one another. */
    fade(opacity: number, duration?: number, curve?: AnimCurve): this;
    fade(opacity: number, options: TimingOptions): this;
}

interface FadeEffectOptions {
    shift?: WorldVector;
    scale?: number;
    duration?: number;
}
export type FadeOptions = FadeEffectOptions & TimingOptions;

interface IndicateEffectOptions {
    color?: Color;
    scale?: number;
    duration?: number;
}
export type IndicateOptions = IndicateEffectOptions & TimingOptions;

interface WiggleEffectOptions {
    /** Peak growth about the bounds center; 1.1 by default. */
    scale?: number;
    /** Widest turn in radians either way; 0.0628 (a hundredth of a turn) by default. */
    angle?: number;
    /** How many times the target tips to a side; 6 by default. */
    swings?: number;
    /** Seconds; 2 by default. */
    duration?: number;
}
export type WiggleOptions = WiggleEffectOptions & TimingOptions;

interface CircumscribeEffectOptions {
    /** The frame; an ellipse passes through the corners of the padded bounds. */
    shape?: "rectangle" | "ellipse";
    /** Defaults to the theme's focus color. */
    color?: Color;
    /** Frame stroke in pixels, 3 by default; the bright stretch is twice as wide. */
    width?: number;
    /** World units between the target's bounds and the frame; 0.1 by default. */
    padding?: number;
    /** Share of the outline the bright stretch covers, above 0 and at most 1; 0.3 by default. */
    window?: number;
    /** Seconds; 1 by default. */
    duration?: number;
}
export type CircumscribeOptions = CircumscribeEffectOptions & TimingOptions;

interface PassingFlashEffectOptions {
    /** Defaults to the theme's focus color. */
    color?: Color;
    /** Stroke of the bright stretch in pixels; 6 by default. */
    width?: number;
    /** Share of the outline the bright stretch covers, above 0 and at most 1; 0.3 by default. */
    window?: number;
    /** Seconds; 1 by default. */
    duration?: number;
}
export type PassingFlashOptions = PassingFlashEffectOptions & TimingOptions;

interface FlashEffectOptions {
    /** Defaults to the theme's focus color. */
    color?: Color;
    /** Line stroke in pixels; 3 by default. */
    width?: number;
    /** World units between the target's bounds, or the point, and where the lines start; 0.1 by default. */
    padding?: number;
    /** World length of each line; 0.2 by default. */
    size?: number;
    /** How many lines burst out, a whole number from 1 to 64; 12 by default. */
    lines?: number;
    /** Seconds; 1 by default. */
    duration?: number;
}
export type FlashOptions = FlashEffectOptions & TimingOptions;

interface RippleEffectOptions {
    /** Defaults to the theme's focus color. */
    color?: Color;
    /** Ring stroke in pixels; 3 by default. */
    width?: number;
    /** World units between the target's outline and where the ring starts; 0 by default. */
    padding?: number;
    /** World units the ring grows on every side; 0.4 by default. */
    size?: number;
    /** Seconds; 1 by default. */
    duration?: number;
}
/** The curve eases the growth, "snappy" by default; the fade runs evenly. */
export type RippleOptions = RippleEffectOptions & TimingOptions;

interface AnimationProperties {
    shift: WorldVector;
    transform: Mat4;
    opacity: number;
    stroke: Color;
    fill: Color;
    dash_offset: number;
    tail: number;
    tip: number;
    progress: number;
    /** Stroke width in pixels; on a group it animates the shapes that take their width from it. */
    width: number;
    /** A circle, arc or angle radius in world units, or a point's radius in pixels. */
    radius: number;
    /** Arc center, start direction and signed sweep in radians. */
    center: WorldVector;
    start: number;
    angle: number;
    /** Angle vertex; from and to set its ray endpoints. */
    vertex: WorldVector;
    /** A text's size, or a rectangle's `[width, height]`. */
    size: number | Vec2;
    /** A rectangle's corner radius, at most half its shorter side. */
    corner: number;
    /** Finite nonnegative object-local endpoint distances for a line, arrow or vector:
     * one number for both ends, or [start, end]. Warped geometry keeps its existing trim. */
    trimDistance: number | [number, number];
    /** An end-stop color, or a gradient with the shape's type, stop count and explicit geometry. */
    gradient: Color | GradientOptions;
    /** Gaussian blur sigma in screen pixels, 0 to 256. */
    blur: number;
    shadow: ShadowOptions;
    /** The number a `number` shows, or the value of a `vector`. */
    value: number | WorldVector;
    /** Updated polygon/route/single-contour path points; their count stays fixed.
     * Plots accept changing counts and retain their band and discontinuity cuts. */
    points: readonly WorldVector[];
    /** The start of a line, arrow or curve. */
    from: WorldVector;
    /** The end of a line, arrow or curve. */
    to: WorldVector;
    /** The origin of a vector. */
    origin: WorldVector;
    /** The first control point of a curve. */
    control1: WorldVector;
    /** The second control point of a curve. */
    control2: WorldVector;
    /** Redraw a plot over its retained or supplied range. Matching grids blend y at fixed x and recut the band;
     * other states interpolate matching pieces or switch halfway. A contour child samples fn(x, y). */
    fn: (x: number, y: number) => number | null | undefined;
    /** A contour child's scalar samples, instead of fn; x varying fastest. */
    values: readonly number[];
    /** A contour child's new level, with fn or values; omitted keeps the previous level. */
    level: number;
    /** Plot graph samples, or contour child grid columns with fn or values; kept for later states. */
    x_range: Range;
    /** Plot band with fn or points, or contour child grid rows with fn or values; kept for later states. */
    y_range: readonly [number, number] | Range;
    /** With `fn` or `points`, the x where a plot's new graph must not join; the plot keeps them. */
    discontinuities: readonly number[];
    /**
     * The string a `text` or `label` changes into: from the left, characters turn into the new
     * string's while the length moves to its length. It is a property of its own, so a move,
     * fade or recolor may run beside it.
     */
    text: string;
    /**
     * The draw layer (see `layer` on factories). It changes at a step: while a play runs, the
     * object draws at the higher of its two layers, so it comes forward as the play begins and
     * goes back as it ends. The objects below the target move by as many layers as it does.
     */
    layer: number;
}

type AtLeastOne<T> = {
    [Key in keyof T]: Required<Pick<T, Key>> & Partial<Omit<T, Key>>
}[keyof T];

/** `retype: true` erases back to the beginning both strings share, then types the rest. */
type TextChoice = {retype?: never} | {text: string; retype?: boolean};

export type AnimationSpec = {target: ObjectHandle} & AtLeastOne<AnimationProperties> & TextChoice & (
    {shift?: WorldVector; transform?: never; arc?: never} |
    {shift?: never; transform?: Mat4; arc?: never} |
    {shift: WorldVector; transform?: never; arc: number} |
    {shift?: never; transform: Mat4; arc: number}
);

export type SetSpec = {target: ObjectHandle} & AtLeastOne<AnimationProperties> & TextChoice & (
    {shift?: WorldVector; transform?: never; arc?: never} |
    {shift?: never; transform?: Mat4; arc?: never}
);

declare const tracker: unique symbol;
/** A number that bound objects follow; `scene.track` moves it. */
export interface Tracker {
    readonly [tracker]: true;
    /** The value the tracker has reached at this point of the script. */
    get(): number;
}
/**
 * What a bound object shows for its trackers' values: any play property except arc, text and
 * layer, the points of a shape and a plot's fn among them. The shift is the offset from where
 * the object was when it was bound; a transform is absolute.
 */
export type BoundState = Partial<Omit<AnimationProperties, "shift" | "transform" | "text" | "layer">> & (
    {shift?: readonly [number, number] | readonly [number, number, number]; transform?: never; arc?: never} |
    {shift?: never; transform?: Mat4; arc?: never}
);

export type TimingOptions = {
    /** Seconds. Omitted values keep the operation's default; restore also accepts zero. */
    duration?: number;
    curve?: AnimCurve;
};
export type AnimationOptions = TimingOptions & {
    /** Each later target starts duration * lag seconds after the previous one. Non-negative. */
    lag?: number;
};
export type RevealOptions = AnimationOptions & {direction?: CreateDirection};
export type PivotAnchor = "center" | "left" | "right" | "top" | "bottom" |
    "top_left" | "top_right" | "bottom_left" | "bottom_right";
interface PivotMotionOptions {
    about?: PivotAnchor | WorldVector | Handle;
    shift?: WorldVector;
    arc?: number;
    lag?: number;
}
export type RotateOptions = PivotMotionOptions & {axis?: WorldVector} & AnimationOptions;
export type ScaleOptions = PivotMotionOptions & AnimationOptions;
/** The axis defaults to the camera's up direction. */
export type OrbitOptions = {axis?: WorldVector} & TimingOptions;
export type FocusOptions = TimingOptions & {
    /** The share of the targets' span left free on each side, 0.15 by default. */
    padding?: number;
};
export type MoveAlongOptions = {
    from?: number;
    to?: number;
    orient?: boolean;
} & AnimationOptions;

export type ArrangeOptions = TimingOptions & {
    /** The way the row runs, [1, 0] by default. */
    direction?: WorldVector;
    /** World units between neighbors, 0.25 by default. */
    gap?: number;
    /** Where the middle of the row lands in the items' parent, its origin by default. */
    center?: WorldVector;
};

export type MatchOptions = AnimationOptions & {
    /**
     * Snapshot outline guide: add its arc-length detour from its endpoint chord to each pair.
     * Keeps individual endpoints and spacing; at most 2048 samples, no live connectors or nonzero arc.
     */
    path?: Handle;
    /** Central angle in radians of the arc each traveling piece follows. */
    arc?: number;
    /**
     * Further pairs of pieces. A pair of circles, or of rectangles, travels as one shape; other
     * pieces that are not texts reading the same cross-fade while they travel.
     */
    pairs?: readonly (readonly [Handle, Handle])[];
    /**
     * Pair the remaining texts that read the same, and the pieces of formulas by their TeX, as by
     * default; false keeps only the given pairs, a pair of formula parts pairing the pieces below.
     */
    strings?: boolean;
    /**
     * Pair the remaining texts as a diff of lines does instead, for edited code: lines kept in
     * order, then moved lines, then the words a changed line shares with its replacement.
     */
    diff?: boolean;
    /**
     * Pair the remaining circles and rectangles with those of their kind in order, each shape of
     * the object with fewer pairing with an even run of the other's, so a bar splits into the
     * bars that replace it.
     */
    shapes?: boolean;
    /**
     * Share of the duration, above 0 and at most 1, in which unpaired pieces fade: out at the
     * start and in at the end, so 0.4 clears what leaves before what arrives shows.
     */
    fade?: number;
    /** Match a snapshot copy and preserve the source. Explicit pairs still name source pieces. */
    copy?: boolean;
};

export type MorphOptions = AnimationOptions & {
    /** Explicit one-based source and target contour indices; remaining pieces pair automatically. */
    pairs?: readonly (readonly [number, number])[];
    /**
     * Morph a copy of each source, made beside it now as clone() makes one, and leave the source
     * in place; one source may then give a copy to several targets.
     */
    copy?: boolean;
};

export type EditOptions = AnimationOptions & Pick<MatchOptions, "arc" | "fade"> & {
    /** The new block's id; by default the block's id with the first free .2, .3 and so on. */
    id?: string;
};

export type HighlightOptions = {
    /** The opacity of everything outside the selection, 0.25 by default. */
    dim?: number;
    /** Seconds; 0.5 by default. */
    duration?: number;
} & TimingOptions;

/** Mapping grid in each target's parent space. It must cover all target points and hold at most 65536 samples.
 * Spline interpolation is exact for maps quadratic in x and y. */
export type WarpOptions = TimingOptions & {
    /** The columns of samples, from min every step until one reaches max. */
    x_range: Range;
    /** The rows of samples, from min every step until one reaches max. */
    y_range: Range;
    /** The longest straight piece mapped without cutting it first, in the parent space of each target; 0.05 by default. */
    step?: number;
};

export interface SceneBuilder extends ObjectFactory {}
/** Definitions share Lua budgets: 4096 objects, 65536 clips, 1000000 points, 64 MiB native memory.
 * Retained geometry uses 12 bytes per point per state plus 512 bytes per clip. */
export class SceneBuilder {
    constructor(config?: SceneConfig);
    /** Transfers and seals child; author it fully before attachment. Scene cycles are rejected. */
    viewport(child: SceneBuilder, bounds: ViewportBounds): this;
    /** Draw this scene's own objects again in a rectangle of the frame through a camera of its own, as a magnifier or an inset; at most 8 per scene. */
    view(bounds: ViewportBounds, options?: ViewOptions): ViewHandle;
    /** Transfers and seals stages. Hold values are non-negative; styles excludes single-style options. */
    sceneTransition(stages: readonly SceneBuilder[], options?: SceneTransitionOptions): this;
    /** A cue the audio module plays; the Lua guards it and its gains by vis.audio, so a host without the
     * module plays the scene without sound. */
    sound(options: SoundOptions): AudioCueHandle;
    create(target: Handle | readonly Handle[], duration?: number, curve?: AnimCurve, lag?: number, direction?: CreateDirection): this;
    create(target: Handle | readonly Handle[], options: RevealOptions): this;
    uncreate(target: Handle | readonly Handle[], duration?: number, curve?: AnimCurve, lag?: number, direction?: CreateDirection): this;
    uncreate(target: Handle | readonly Handle[], options: RevealOptions): this;
    fillReveal(target: Handle | readonly Handle[], duration?: number, curve?: AnimCurve, lag?: number): this;
    fillReveal(target: Handle | readonly Handle[], options: AnimationOptions): this;
    drawBorderThenFill(target: Handle | readonly Handle[], duration?: number, curve?: AnimCurve, lag?: number, direction?: CreateDirection): this;
    drawBorderThenFill(target: Handle | readonly Handle[], options: RevealOptions): this;
    write(target: Handle | readonly Handle[], duration?: number, curve?: AnimCurve, lag?: number, direction?: CreateDirection): this;
    write(target: Handle | readonly Handle[], options: RevealOptions): this;
    fadeIn(target: Handle, options?: FadeOptions): this;
    fadeOut(target: Handle, options?: FadeOptions): this;
    growFromCenter(target: Handle, duration?: number, curve?: AnimCurve): this;
    growFromCenter(target: Handle, options: TimingOptions): this;
    growFromEdge(target: Handle, edge: GrowthEdge, duration?: number, curve?: AnimCurve): this;
    growFromEdge(target: Handle, edge: GrowthEdge, options: TimingOptions): this;
    shrinkToCenter(target: Handle, duration?: number, curve?: AnimCurve): this;
    shrinkToCenter(target: Handle, options: TimingOptions): this;
    /** An array of handles, such as the tokens a search finds, indicates at once. */
    indicate(target: Handle | readonly Handle[], options?: IndicateOptions): this;
    /** Grows the target a little while it turns back and forth, then leaves it as it was. */
    wiggle(target: Handle, options?: WiggleOptions): this;
    /** A frame around the target draws on and goes away while a bright stretch runs once around it. */
    circumscribe(target: Handle, options?: CircumscribeOptions): this;
    /** A bright stretch runs once along the outline of a line, curve, shape, path or connector and leaves nothing behind. */
    passingFlash(target: Handle, options?: PassingFlashOptions): this;
    /** Short lines burst out of the target, or out of a point in world coordinates, and go away. */
    flash(target: Handle | readonly [number, number] | readonly [number, number, number], options?: FlashOptions): this;
    /** A ring around the target grows outward while it fades away. */
    ripple(target: Handle, options?: RippleOptions): this;
    /** Source draws the interpolated shape until target takes over at completion.
     * Source then leaves the timeline; copy preserves it by morphing a sampled copy. */
    morph(source: Handle | readonly Handle[], target: Handle | readonly Handle[], duration?: number, curve?: AnimCurve, lag?: number | Omit<MorphOptions, keyof TimingOptions>): this;
    morph(source: Handle | readonly Handle[], target: Handle | readonly Handle[], options: MorphOptions): this;
    /** Cross-fades disjoint families and removes source from the timeline at completion.
     * Attach target at the cursor without clips; source clips must finish by the fade's end.
     * Objects remain Scene-owned and can still be sampled at earlier times. */
    fadeTransform(source: Handle, target: Handle, duration?: number, curve?: AnimCurve): this;
    fadeTransform(source: Handle, target: Handle, options: TimingOptions): this;
    /** Carries matching pieces into a newly attached target. Source must be at rest unless
     * copy snapshots it. Completion removes source or its temporary copy from the timeline. */
    matchTransform(source: Handle, target: Handle, duration?: number, curve?: AnimCurve, options?: Omit<MatchOptions, keyof TimingOptions>): this;
    matchTransform(source: Handle, target: Handle, options: MatchOptions): this;
    /**
     * The code block that block's text becomes after the edits, applied in order, in block's look,
     * place and parent; block turns into it as matchTransform with diff does.
     */
    edit(block: GroupHandle, edits: CodeEdit | readonly CodeEdit[], duration?: number, curve?: AnimCurve,
        options?: Omit<EditOptions, keyof TimingOptions>): GroupHandle;
    edit(block: GroupHandle, edits: CodeEdit | readonly CodeEdit[], options: EditOptions): GroupHandle;
    /** Lays a group's children, or sibling handles in order, one after another; an item added now grows in. */
    arrange(items: GroupHandle | readonly Handle[], duration?: number, curve?: AnimCurve, options?: Omit<ArrangeOptions, keyof TimingOptions>): this;
    arrange(items: GroupHandle | readonly Handle[], options: ArrangeOptions): this;
    /** Children first to last of target, counting from 1, stay at full opacity and the rest of target dims. */
    highlight(target: Handle, first: number, last: number, options?: HighlightOptions): this;
    highlight(target: Handle, first: number, options?: HighlightOptions): this;
    /** These objects below target stay at full opacity, with everything below them, and the rest dims. */
    highlight(target: Handle, selection: Handle | readonly Handle[], options?: HighlightOptions): this;
    /** Brings everything below target back to full opacity. */
    highlight(target: Handle, options?: HighlightOptions): this;
    shift(target: Handle, by: WorldVector, duration?: number, curve?: AnimCurve): this;
    shift(target: Handle, by: WorldVector, options: TimingOptions): this;
    rotate(target: Handle | readonly Handle[], radians: number, options?: RotateOptions): this;
    scale(target: Handle | readonly Handle[], factor: number | WorldVector, options?: ScaleOptions): this;
    /** Follows a copied outline, or a connector's live trimmed ends. Connector dependencies must not form a cycle. */
    moveAlong(target: Handle | readonly Handle[], path: Handle, options?: MoveAlongOptions): this;
    /**
     * Moves every point of the targets and of the objects below them straight to its image under fn, in the
     * parent space of each target; straight pieces are cut first so they bend. fn maps the plane, and each
     * point keeps its z; the builder calls it at every sample of the options and emits the images.
     */
    warp(target: Handle | readonly Handle[], fn: (x: number, y: number) => readonly [number, number],
        options: WarpOptions): this;
    warp(target: Handle | readonly Handle[], fn: (x: number, y: number) => readonly [number, number],
        duration: number, curve: AnimCurve | undefined, options: Omit<WarpOptions, keyof TimingOptions>): this;
    transform(target: Handle, matrix: Mat4, duration?: number, curve?: AnimCurve): this;
    transform(target: Handle, matrix: Mat4, options: TimingOptions): this;
    fade(target: Handle, opacity: number, duration?: number, curve?: AnimCurve): this;
    fade(target: Handle, opacity: number, options: TimingOptions): this;
    /**
     * Fades the from targets to opacity 0 and the to targets to 1 together. Unlike fadeTransform, it takes any
     * targets and keeps from in the scene, invisible.
     */
    transition(from: Handle | readonly Handle[], to: Handle | readonly Handle[], duration?: number, curve?: AnimCurve): this;
    transition(from: Handle | readonly Handle[], to: Handle | readonly Handle[], options: TimingOptions): this;
    stroke(target: Handle, color: Color, duration?: number, curve?: AnimCurve): this;
    stroke(target: Handle, color: Color, options: TimingOptions): this;
    fill(target: Handle, color: Color, duration?: number, curve?: AnimCurve): this;
    fill(target: Handle, color: Color, options: TimingOptions): this;
    play(specification: AnimationSpec | readonly AnimationSpec[], duration?: number, curve?: AnimCurve, lag?: number): this;
    play(specification: AnimationSpec | readonly AnimationSpec[], options: AnimationOptions): this;
    /** Applies the values at once: as the starting state before the timeline starts, at the cursor afterwards. */
    set(specification: SetSpec | readonly SetSpec[]): this;
    /**
     * Records the state each target shows now, with the objects below it: transform, opacity,
     * colors, gradient, sizes, layer, a text's string and a number's value. Each target keeps a
     * stack, so saves nest. Active text/geometry changes save their destinations. Adds no time.
     */
    save(target: Handle | readonly Handle[]): this;
    /**
     * Takes the newest saved state of each target and brings the target and the objects below it
     * back to it: over duration seconds, each later target lag of the duration later, or at once
     * when duration is 0, the default. Removed descendants stay removed; restores do not undo
     * warps or switch plain/counting text modes.
     */
    restore(target: Handle | readonly Handle[], duration?: number, curve?: AnimCurve, lag?: number): this;
    restore(target: Handle | readonly Handle[], options: AnimationOptions): this;
    /** A number for objects to follow, starting at value. */
    tracker(value: number): Tracker;
    /** Bind one object to tracker values and apply its initial state immediately.
     * Callbacks must be pure and cannot author scenes. One binding per object; owned animated properties
     * cannot overlap another animation, except additive shift. Shape point counts stay fixed except plots. */
    bind<T extends ObjectHandle>(target: T, trackers: Tracker | readonly Tracker[], fn: (...values: number[]) => BoundState): T;
    /** Bake bound states at Scene fps (up to 240), easing tracker values through curve.
     * Different trackers can overlap on a shared object; one tracker cannot run overlapping tracks.
     * Clips are flushed by animation calls, clone, warp, view, and view-camera calls; tracks cannot
     * merge after a flush. A call exceeding 65536 target segments fails before sampling. */
    track(tracker: Tracker, value: number, duration: number, curve?: AnimCurve): this;
    track(tracker: Tracker, value: number, options: TimingOptions): this;
    /** Moves several trackers at once, each to its value. */
    track(moves: readonly (readonly [Tracker, number])[], duration: number, curve?: AnimCurve): this;
    track(moves: readonly (readonly [Tracker, number])[], options: TimingOptions): this;
    /** Move the camera; omitted fields keep their value, and a target of handles centers the camera on them. */
    look(camera: CameraTarget, duration?: number, curve?: AnimCurve): this;
    look(camera: CameraTarget, options: TimingOptions): this;
    /** Circle a 3D camera about its target by any angle; 2 * Math.PI is one full turn. */
    orbit(radians: number, options?: OrbitOptions): this;
    /** Frame target bounds with a 2D camera. A target in a 3D camera is rejected.
     * Omit the target to restore the opening camera from either view. */
    focus(target?: Handle | readonly Handle[] | null, options?: FocusOptions): this;
    wait(duration?: number): this;
    /** Ends the family's visibility at the cursor; earlier samples retain it under Scene ownership. */
    remove(target: Handle): this;
    /** The handle created with this id, or null. */
    find(id: string): ObjectHandle | null;
    /** Snapshot a family without its animations, beside source or under options.parent.
     * Space, Cell, hidden sources, active morphs/warps, and masks crossing the family are rejected.
     * Supplying an id exposes derived child IDs through find(). */
    clone<T extends ObjectHandle>(source: T, options?: CloneOptions): T;
    at(time: number | string, callback: (scene: this) => void): this;
    parallel(...tracks: Array<(scene: this) => void>): this;
    parallel(tracks: ReadonlyArray<(scene: this) => void>): this;
    mark(name: string): this;
    compile(): string;
}

/**
 * Draws the numbers Lua's math.random draws after math.randomseed with the same seed: no bounds
 * a float from 0 up to 1, one bound m an integer from 1 to m, and two an integer from m to n.
 */
/** Bounds and seeds are safe integers. Empty ranges, random(0), and more than two bounds are rejected. */
export interface RandomGenerator {
    (): number;
    (upper: number): number;
    (lower: number, upper: number): number;
}

export const vis: Readonly<{
    /** Physical displacement at non-negative elapsed seconds; throws for invalid inputs or a non-finite result. */
    springResponse(seconds: number, options: SpringResponseOptions): number;
    animCurve: Readonly<AnimCurveFactory>;
    /** A generator seeded with the safe integers seed and seed2, 0 by default, as Lua's math.randomseed(seed, seed2). */
    random(seed: number, seed2?: number): RandomGenerator;
    scene(config?: SceneConfig): SceneBuilder;
}>;
export function compileScene(scene: SceneBuilder): string;
/** Dynamically evaluates trusted editor JavaScript. Do not expose it as a public execution endpoint. */
export function evaluateScene(source: string, name?: string): SceneBuilder;
