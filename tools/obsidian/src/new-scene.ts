// What a new scene starts from: the scene a new code block or scene file holds, and the name a new
// scene file takes.

// A scene that names no theme is adaptive, and takes its colors from the Obsidian theme.
export const SCENE = `local scene = vis.scene {width = 640, height = 360, camera = {height = 7}}
local ball = scene:circle {center = {-3, 0}, radius = 0.5, fill = "accent"}
scene:grow_from_center(ball, {duration = 0.6})
scene:shift(ball, {6, 0}, {duration = 1.5})
scene:wait(0.5)
return scene
`;

/** The scene as a code block, for a note. */
export const CODE_BLOCK = `\`\`\`viscript\n${SCENE}\`\`\`\n`;

/**
 * The name a new file takes beside `taken`, the names in its folder: "Untitled.lua", or else the
 * first of "Untitled 1.lua", "Untitled 2.lua" and so on that is free, ignoring case as Obsidian
 * does for its own new files.
 */
export function untitledName(taken: Iterable<string>, extension: string): string {
    const names = new Set(Array.from(taken, (name) => name.toLowerCase()));
    for (let count = 0; ; count++) {
        const name = `Untitled${count ? ` ${count}` : ""}.${extension}`;
        if (!names.has(name.toLowerCase())) return name;
    }
}
