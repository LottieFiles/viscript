// The plugin's settings tab, declared for Obsidian's searchable settings.
import {AbstractInputSuggest, PluginSettingTab, type App, type Setting, type SettingDefinitionItem} from "obsidian";
import {DEFAULT_FONT_FOLDER, fontCount} from "./font-service.ts";
import type ViscriptPlugin from "./main.ts";
import {SceneBlock} from "./scene.ts";
import type {ViscriptSettings} from "./settings.ts";

type FontNameKey = "textFontName" | "codeFontName";

// The text and code fonts as set, so letters a choice lacks are seen at once, in the adaptive
// theme, which takes the note's colors and sets text in the text font.
const PREVIEW = `local s = vis.scene {width = 720, height = 132}
s:text {text = "Aa Gg Rr 0123 · 가나다", point = {0, 0.9}, size = 22}
s:text {text = "local answer = 42 -- 코드", point = {0, -1.05}, role = "code", size = 18}
return s`;

class FontSuggest extends AbstractInputSuggest<string> {
    readonly #families: () => Promise<string[]>;

    constructor(app: App, input: HTMLInputElement, families: () => Promise<string[]>) {
        super(app, input);
        this.#families = families;
    }

    protected async getSuggestions(query: string): Promise<string[]> {
        const wanted = query.trim().toLowerCase();
        return (await this.#families()).filter((family) => family.toLowerCase().includes(wanted));
    }

    renderSuggestion(value: string, el: HTMLElement): void {
        el.setText(value);
    }
}

export class ViscriptSettingTab extends PluginSettingTab {
    readonly #plugin: ViscriptPlugin;
    #status: (() => void) | null = null;

    constructor(app: App, plugin: ViscriptPlugin) {
        super(app, plugin);
        this.#plugin = plugin;
    }

    override getSettingDefinitions(): SettingDefinitionItem[] {
        const settings = () => this.#plugin.settings;
        return [
            {
                name: "Autoplay",
                desc: "Play scenes as soon as they appear. With this off, a scene waits on its first frame until you press play. Scenes start paused either way when your system asks for reduced motion.",
                control: {type: "toggle", key: "autoplay"},
            },
            {
                name: "At the end",
                desc: "What a scene does when it reaches its last frame.",
                control: {
                    type: "dropdown", key: "end",
                    options: {auto: "Replay, looping seamlessly where the scene loops", crossfade: "Replay, fading into the start",
                        loop: "Replay at once", stop: "Stop on the last frame"},
                },
            },
            {
                name: "Controls",
                desc: "The play button and timeline under a scene in a note. A scene opened in its own tab always has them.",
                control: {type: "dropdown", key: "controls", options: {hover: "Show on hover", always: "Always show", never: "Hide"}},
            },
            {
                name: "Frame rate",
                desc: "The most frames a second a scene draws. Lower rates save battery.",
                control: {type: "dropdown", key: "maxFps", options: {"0": "Up to 60, or 30 on phones", "30": "30", "24": "24", "15": "15"}},
            },
            {
                name: "Sharpness",
                desc: "Balanced draws a little under the screen's resolution on dense screens; Sharpest matches it, at more cost.",
                control: {type: "dropdown", key: "quality", options: {auto: "Balanced", high: "Sharpest"}},
            },
            {
                type: "group",
                heading: "Fonts",
                items: [
                    {
                        name: "Text font",
                        desc: "The font for a scene's text where the scene names none. Pretendard, the default, ships with the plugin, Korean included, as do the other fonts the themes use, so scenes look the same on every device.",
                        control: {
                            type: "dropdown", key: "textFont",
                            options: {default: "Pretendard (bundled)", custom: "Another font"},
                        },
                    },
                    {
                        name: "Text font name",
                        visible: () => settings().textFont === "custom",
                        render: (setting) => this.#fontName(setting, "textFontName"),
                    },
                    {
                        name: "Code font",
                        desc: "The font for code and the code text role.",
                        control: {
                            type: "dropdown", key: "codeFont",
                            options: {default: "JetBrains Mono (bundled)", custom: "Another font"},
                        },
                    },
                    {
                        name: "Code font name",
                        visible: () => settings().codeFont === "custom",
                        render: (setting) => this.#fontName(setting, "codeFontName"),
                    },
                    {
                        name: "Font folder",
                        desc: "A scene that names a font, as font = \"Noto Serif KR\", finds it in this vault folder by its name or its file's name. The folder syncs with the vault, so your other devices have the same fonts.",
                        control: {type: "folder", key: "fontFolder", placeholder: "Example: fonts"},
                    },
                    {
                        name: "Download fonts",
                        desc: `A font a scene names that is not in the font folder is downloaded from Fontsource, through jsDelivr, and saved in the font folder (${DEFAULT_FONT_FOLDER} if none is set), where your other devices find it too. Without it, or for a font Fontsource does not have, that text is drawn in the text font.`,
                        aliases: ["Fontsource", "network", "Google Fonts"],
                        control: {type: "toggle", key: "downloadFonts"},
                    },
                    {name: "Fonts found", render: (setting) => this.#fontStatus(setting)},
                    {name: "Preview", searchable: false, render: (setting) => this.#preview(setting)},
                ],
            },
            {
                type: "group",
                heading: "Advanced",
                items: [
                    {
                        name: "Render in a worker",
                        desc: "Draw scenes off Obsidian's main thread, which keeps typing smooth. Turn this off only if scenes do not appear.",
                        control: {type: "toggle", key: "worker"},
                    },
                    {
                        name: "Open .lua files as scenes",
                        desc: "Show a .lua file as its scene, with the source to edit beside it. Takes effect when Obsidian next starts.",
                        control: {type: "toggle", key: "openFiles"},
                    },
                ],
            },
        ];
    }

    override getControlValue(key: string): unknown {
        const value = this.#plugin.settings[key as keyof ViscriptSettings];
        return key === "maxFps" ? String(value) : value;
    }

    override async setControlValue(key: string, value: unknown): Promise<void> {
        await this.#plugin.update({[key]: key === "maxFps" ? Number(value) : value} as Partial<ViscriptSettings>);
        this.refreshDomState();
        if (key === "fontFolder") this.#status?.();
    }

    override hide(): void {
        super.hide();
        this.#status = null;
    }

    async #families(): Promise<string[]> {
        const {folder} = await this.#plugin.fonts.scan();
        // Names starting with a dot are a system's own, kept out of lists.
        return folder.families().filter((family) => !family.startsWith("."));
    }

    #fontName(setting: Setting, key: FontNameKey): void {
        setting.setDesc("Its family or full name, such as Noto Sans KR, from the font folder or downloaded as below. Until it can be had, the bundled font stands in.");
        setting.addSearch((search) => {
            search.setPlaceholder("Font name").setValue(this.#plugin.settings[key]);
            const save = (value: string) => void this.#plugin.update({[key]: value.trim()});
            // Saved when the field is left or a suggestion is picked, not on each letter.
            search.inputEl.addEventListener("change", () => save(search.getValue()));
            new FontSuggest(this.app, search.inputEl, () => this.#families()).onSelect((value) => {
                search.setValue(value);
                save(value);
            });
        });
    }

    #fontStatus(setting: Setting): () => void {
        let current = true;
        const show = () => {
            setting.setDesc("Looking for fonts…");
            this.#plugin.fonts.scan().then((scan) => {
                if (!current) return;
                const parts = [`${fontCount(scan.folder.size)} in the font folder, besides the two bundled ones`];
                if (scan.unusable) parts.push(`${scan.unusable} files left out as fonts scenes cannot draw`);
                setting.setDesc(`${parts.join("; ")}.`);
            }, (error: unknown) => {
                if (current) setting.setDesc(`The fonts could not be read: ${error instanceof Error ? error.message : String(error)}`);
            });
        };
        setting.addButton((button) => button.setButtonText("Look again").onClick(() => {
            this.#plugin.rescanFonts();
            show();
        }));
        this.#status = show;
        show();
        return () => { current = false; };
    }

    #preview(setting: Setting): () => void {
        setting.setDesc("Text and code as scenes draw them with these fonts.");
        setting.setClass("viscript-preview-setting");
        const box = setting.settingEl.createDiv({cls: "viscript-preview"});
        const block = new SceneBlock(this.#plugin, box, {controls: false, autoplay: false, label: "Font preview"});
        block.load();
        block.show({source: PREVIEW, language: "lua", name: "preview.lua", path: ""});
        return () => block.unload();
    }
}
