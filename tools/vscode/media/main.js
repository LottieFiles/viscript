// The preview webview: draws the scene the extension sends into the canvas with the runtime's
// client, and runs the player around it: transport and playback range, speed, camera, fit and
// zoom, the series menu, the source inspector, performance stats, runtime audio and export.
import {frameElapsed, normalizeTime} from "./time.mjs";
import {
    clampPlaybackRange,
    playbackBounds,
    setPlaybackEnd,
    setPlaybackStart,
} from "./playback-range.mjs";
import {markdownToHtml} from "./markdown.mjs";
import {
    automaticWidePlayerWidth,
    cascadingMenuPlacement,
    floatingMenuPlacement,
    floatingPointMenuPlacement,
    manualStageHeight,
    normalStageMaxHeight,
    responsiveViewerLayout,
    widePlayerWidth,
} from "./viewer-layout.mjs";
import {
    clampPreviewScale,
    fitPreviewScale,
    previewAnchorTranslation,
    previewRenderPixelRatio,
    stepPreviewScale,
    wheelPreviewScale,
} from "./viewport.mjs";
import {saveGif, saveMp4} from "./savers.js";
import {adaptiveVscodeThemeKey, readAdaptiveVscodeTheme} from "./adaptive-theme.mjs";
import {advanceSeriesPlayback, seriesStepTarget} from "./series-step.mjs";
import {SOURCE_CONTEXT_LINE_COUNTS, previewPreferences} from "./preferences.mjs";
import {createRuntimeAudio, loadSceneAsset} from "./runtime-audio.mjs";
import {createCanvasInputRouter, isHostKeyTarget} from "./canvas-input.mjs";
import {advanceRetainedRuntime, sceneCanPlay} from "./retained-runtime.mjs";
import {createFontCache, loadWithFonts} from "./scene-fonts.mjs";
import {
    useVerticalWheelForHorizontalScroll,
    useVerticalWheelScrollChain,
} from "./horizontal-wheel.mjs";
import {highlightSourceLines, shikiTheme} from "./shiki/syntax-highlighter.js";
import {
    reconcileInspectorOpenState,
    resetSourceContextView,
    resolveActiveSourceIndex,
    sourceContextExpansionCount,
    sourceContextRowIsFocused,
    sourceContextRows,
} from "./source-inspector.mjs";

const vscode = acquireVsCodeApi();
const config = window.visPreviewConfig;

function requiredElement(selector) {
    const element = document.querySelector(selector);
    if (!element) throw new Error(`viscript Preview markup is missing required element ${selector}`);
    return element;
}

const canvas = document.querySelector("#preview");
const canvasWrap = document.querySelector("#canvas-wrap");
const stageShell = document.querySelector("#stage-shell");
const viewer = document.querySelector(".viewer");
const player = document.querySelector(".player");
const stageResizer = document.querySelector("#stage-resizer");
const wideResizer = document.querySelector("#wide-resizer");
const canvasStat = document.querySelector("#canvas-stat");
const empty = document.querySelector("#empty");
const errorCard = document.querySelector("#error-card");
const errorMessage = document.querySelector("#error-message");
const status = document.querySelector("#status");
const documentName = document.querySelector("#document-name");
const playButton = document.querySelector("#play");
const playIcon = playButton.querySelector(".play-icon");
const pauseIcon = playButton.querySelector(".pause-icon");
const restartButton = document.querySelector("#restart");
const loopButton = document.querySelector("#loop");
const scrub = document.querySelector("#scrub");
const timeline = document.querySelector(".timeline");
const timeOutput = document.querySelector("#time");
const speedValue = document.querySelector("#speed-value");
const speedButtons = [...document.querySelectorAll("[data-speed]")];
const cameraMode = document.querySelector("#camera-mode");
const cameraButtons = [...document.querySelectorAll("[data-camera]")];
const statValue = document.querySelector("#stat-value");
const statButtons = [...document.querySelectorAll("[data-stat]")];
const sourceLinesValue = document.querySelector("#source-lines-value");
const sourceLineButtons = [...document.querySelectorAll("[data-source-lines]")];
const errorTitle = document.querySelector("#error-title");
const fitModeButton = document.querySelector("#fit-mode");
const actualModeButton = document.querySelector("#actual-mode");
const previewResetButton = document.querySelector("#preview-reset");
const seriesMenu = document.querySelector("#series-menu");
const seriesMenuTrigger = seriesMenu.querySelector("summary");
const seriesPanel = seriesMenu.querySelector(".series-panel");
const wideSeriesSlot = document.querySelector("#wide-series-slot");
const seriesGroupItems = document.querySelector("#series-groups");
const seriesItems = document.querySelector("#series-items");
const seriesPosition = document.querySelector("#series-position");
const seriesPreviousButton = document.querySelector("#series-previous");
const seriesNextButton = document.querySelector("#series-next");
const descriptionContent = document.querySelector("#description-content");
const inspectorContent = requiredElement("#inspector-content");
const notesItem = requiredElement("#notes-item");
const codeContent = requiredElement("#code-content");
const playerMenu = document.querySelector("#player-menu");
const playerMenuTrigger = playerMenu.querySelector("summary");
const menuPanel = playerMenu.querySelector(".menu-panel");
const menuSubmenus = [...menuPanel.querySelectorAll(".menu-submenu")];
const optionValue = document.querySelector("#option-value");
const autoPlayNextButtons = [...document.querySelectorAll("[data-auto-play-next]")];
const forceCompactButtons = [...document.querySelectorAll("[data-force-compact]")];
const rangeInButton = document.querySelector("#range-in");
const rangeOutButton = document.querySelector("#range-out");
const rangeClearButton = document.querySelector("#range-clear");
const rangeValue = document.querySelector("#range-value");
const transport = document.querySelector(".transport");

const BOOTSTRAP = `return vis.scene { width = 16, height = 16, loop = false }`;
const PLAYER_MENU_WIDTH = 174;

let createVis;
let runtime;
let wasmBinary;
const sceneFonts = createFontCache(config.fonts);
// The fonts each document's scene was given when it last loaded, registered before it loads again.
const documentFonts = new Map();
let pendingScene;
let loadGeneration = 0;
let playing = false;
let looping = false;
let playbackSpeed = 1;
let {autoPlayNext, forceCompactLayout, sourceContextLineCount, showStats} = previewPreferences(vscode.getState(), config);
let measuredFps = "—";
let currentTime = 0;
let previousFrame = performance.now();
let animationFrame = 0;
let measuredFrames = 0;
let measureStarted = performance.now();
let busy = false;
let exportController;
let sceneId;
let playbackRange = null;
let sceneWidth = canvas.width;
let sceneHeight = canvas.height;
let previewScale = 1;
let renderPixelRatio = 1;
let renderPixelRatioFrame = 0;
let previewFits = true;
let previewPanX = 0;
let previewPanY = 0;
let manualStageHeightValue = null;
let stageResizePointerId = null;
let manualWidePlayerWidthValue = null;
let wideResizePointerId = null;
let canvasFullscreen = false;
let viewerLayout = "compact";
let currentScenePayload;
let activeHostThemeKey;
let themeRefreshFrame = 0;
let currentSeries;
let playerMenuAnchor;
let currentSourceContexts = [];
let activeSourceIndex = -1;
let notesOpen = true;
let openSourceKeys = new Set();
const sourceHighlightGenerations = new WeakMap();
let pendingSourceExpansionAnchor;
const runtimeAudio = createRuntimeAudio({
    runtime: () => runtime,
    time: sceneTime,
    rate: () => playbackSpeed,
    playing: () => playing,
});

function setStatus(label, state) {
    status.dataset.state = state;
    status.querySelector("span").textContent = label;
}

function decodeBase64(value) {
    const binary = atob(value);
    const output = new Uint8Array(binary.length);
    for (let index = 0; index < binary.length; index += 1) output[index] = binary.charCodeAt(index);
    return output;
}

function describeError(error) {
    if (error instanceof Error) return error.message;
    return String(error);
}

async function boot() {
    try {
        ({createVis} = await import(config.client));
        const wasmResponse = await fetch(config.wasm);
        if (!wasmResponse.ok) throw new Error(`Bundled WASM failed to load (${wasmResponse.status})`);
        wasmBinary = new Uint8Array(await wasmResponse.arrayBuffer());
        setStatus("RUNTIME READY", "ready");
        if (pendingScene) {
            const scene = pendingScene;
            pendingScene = undefined;
            await loadScene(scene);
        }
    } catch (error) {
        showError(describeError(error), "RUNTIME ERROR");
    }
}

async function loadScene(scene, {themeRefresh = false} = {}) {
    currentScenePayload = scene;
    documentName.textContent = scene.displayName || scene.fileName;
    documentName.title = scene.displayName || scene.fileName;
    const preserveSourceSelection = themeRefresh || scene.sceneId === sceneId;
    syncSourceInspector(scene.sourceContexts, scene.activeSourceIndex, {preserveSelection: preserveSourceSelection});
    syncSeries(scene.series);
    descriptionContent.innerHTML = markdownToHtml(scene.description);
    if (!createVis || !wasmBinary) {
        pendingScene = scene;
        setStatus("WAITING FOR RUNTIME", "loading");
        return;
    }
    const generation = ++loadGeneration;
    const wasPlaying = playing;
    const wasAudioStarted = runtimeAudio.started;
    exportController?.abort(new DOMException("Scene changed", "AbortError"));
    setStatus("COMPILING LUA", "loading");

    let candidate;
    try {
        if (scene.assetErrors.length) throw new Error(scene.assetErrors.join("\n"));
        candidate = await createVis(BOOTSTRAP, "bootstrap.lua", {wasmBinary});
        const hostTheme = readAdaptiveVscodeTheme();
        const hostThemeKey = adaptiveVscodeThemeKey(hostTheme);
        candidate.hostTheme(hostTheme);
        for (const asset of scene.assets) loadSceneAsset(candidate, asset, decodeBase64(asset.base64));
        const load = (engine) => engine.load(scene.source, scene.fileName);
        const fonts = await loadWithFonts(candidate, load, sceneFonts, {
            expected: documentFonts.get(scene.documentUri),
            current: () => generation === loadGeneration,
        });
        if (generation !== loadGeneration) {
            candidate.destroy();
            return;
        }
        documentFonts.set(scene.documentUri, fonts);

        const previous = runtime;
        canvasInputRouter.cancel();
        runtime = candidate;
        candidate = undefined;
        previous?.destroy();
        runtimeAudio.reset();
        activeHostThemeKey = runtime.adaptiveTheme ? hostThemeKey : undefined;
        const [width, height] = runtime.size();
        sceneWidth = width;
        sceneHeight = height;
        measuredFps = "—";
        if (width > 0 && height > 0) {
            document.documentElement.style.setProperty("--scene-aspect-ratio", `${width} / ${height}`);
        }
        syncViewerLayout();
        syncNormalLayoutLimit();
        syncCanvasStat();
        const sameScene = scene.sceneId === sceneId;
        playbackRange = sameScene ? clampPlaybackRange(playbackRange, runtime.duration) : null;
        const previousTime = currentTime;
        sceneId = scene.sceneId;
        currentTime = sameScene ? normalizeTime(previousTime, runtime.duration) : playbackBounds(playbackRange, runtime.duration).start;
        scrub.max = String(runtime.duration);
        scrub.value = String(currentTime);
        looping = runtime.loop;
        playing = themeRefresh ? wasPlaying && sceneCanPlay(runtime) : sceneCanPlay(runtime);
        if (wasAudioStarted) runtimeAudio.start();
        previousFrame = performance.now();
        empty.hidden = true;
        errorCard.hidden = true;
        canvas.style.opacity = "1";
        canvas.style.filter = "none";
        if (previewFits) fitPreview();
        else setPreviewScale(previewScale, {preserveAnchor: false});
        syncCamera();
        draw();
        syncControls();
        setStatus("PREVIEW READY", "ready");
        vscode.postMessage({type: "renderSuccess"});
    } catch (error) {
        candidate?.destroy();
        if (generation === loadGeneration) showError(describeError(error), "LUA ERROR", "SCENE REJECTED");
    }
}

function scheduleAdaptiveThemeRefresh() {
    cancelAnimationFrame(themeRefreshFrame);
    themeRefreshFrame = requestAnimationFrame(() => {
        themeRefreshFrame = requestAnimationFrame(() => {
            themeRefreshFrame = 0;
            if (currentSourceContexts.length) refreshOpenSourceHighlights();
            if (!runtime?.adaptiveTheme || !currentScenePayload) return;
            const nextKey = adaptiveVscodeThemeKey(readAdaptiveVscodeTheme());
            if (nextKey !== activeHostThemeKey) void loadScene(currentScenePayload, {themeRefresh: true});
        });
    });
}

const vscodeThemeObserver = new MutationObserver(scheduleAdaptiveThemeRefresh);
vscodeThemeObserver.observe(document.documentElement, {
    attributes: true,
    attributeFilter: ["class", "style", "data-vscode-theme-id", "data-vscode-theme-kind"],
});
vscodeThemeObserver.observe(document.body, {
    attributes: true,
    attributeFilter: ["class", "style", "data-vscode-theme-id", "data-vscode-theme-kind"],
});

function syncSeries(series) {
    currentSeries = series;
    syncSeriesStepControls();
    const groups = Array.isArray(series?.groups) ? series.groups : [];
    seriesGroupItems.replaceChildren();
    seriesItems.replaceChildren();
    seriesMenu.open = false;
    seriesGroupItems.hidden = groups.length === 0;
    seriesItems.dataset.empty = String(groups.length === 0);

    if (!groups.length) {
        const emptyState = document.createElement("span");
        emptyState.className = "series-empty";
        emptyState.textContent = "NO SERIES";
        seriesItems.append(emptyState);
    }

    const renderSeriesItems = (groupIndex) => {
        const group = groups[groupIndex];
        if (!group) return;
        for (const [index, button] of [...seriesGroupItems.children].entries()) {
            button.setAttribute("aria-pressed", String(index === groupIndex));
        }
        seriesItems.replaceChildren();
        group.items.forEach((label, itemIndex) => {
            const button = document.createElement("button");
            button.type = "button";
            button.textContent = `${String(itemIndex + 1).padStart(2, "0")} ${label}`;
            button.title = `Open ${group.label} ${itemIndex + 1}: ${label}`;
            button.setAttribute("aria-pressed", String(itemIndex === group.activeIndex));
            button.addEventListener("click", () => openSeriesItem(groupIndex, itemIndex));
            seriesItems.append(button);
        });
    };

    groups.forEach((group, groupIndex) => {
        const button = document.createElement("button");
        button.type = "button";
        button.textContent = group.label;
        button.title = `Show ${group.label}`;
        button.addEventListener("click", () => renderSeriesItems(groupIndex));
        seriesGroupItems.append(button);
    });
    const activeGroupIndex = Number.isInteger(series?.activeGroupIndex)
        ? Math.max(0, Math.min(groups.length - 1, series.activeGroupIndex))
        : 0;
    if (groups.length) renderSeriesItems(activeGroupIndex);
    syncSeriesPanelHost();
    requestAnimationFrame(revealActiveSeriesItem);
}

function revealActiveSeriesItem() {
    seriesItems.querySelector('button[aria-pressed="true"]')?.scrollIntoView({block: "nearest"});
}

function resetSeriesPanelPlacement() {
    seriesPanel.style.top = "auto";
    seriesPanel.style.bottom = "auto";
    seriesPanel.style.left = "0";
    seriesPanel.style.maxHeight = "";
}

function hostSeriesPanelInPopup() {
    if (seriesPanel.parentElement !== seriesMenu) {
        seriesMenu.append(seriesPanel);
        seriesMenu.dataset.positioned = "false";
        resetSeriesPanelPlacement();
    }
    wideSeriesSlot.hidden = true;
    delete player.dataset.seriesDocked;
    if (manualStageHeightValue !== null) applyManualStageHeight();
}

function syncSeriesPanelHost() {
    const groups = Array.isArray(currentSeries?.groups) ? currentSeries.groups : [];
    const docked = viewerLayout === "wide"
        && !canvasFullscreen
        && groups.length > 0
        && !seriesMenu.open;
    if (docked) {
        resetSeriesPanelPlacement();
        wideSeriesSlot.append(seriesPanel);
        wideSeriesSlot.hidden = false;
        player.dataset.seriesDocked = "true";
        if (manualStageHeightValue !== null) applyManualStageHeight();
        return;
    }
    hostSeriesPanelInPopup();
}

function syncSourceInspector(contexts, requestedIndex, {preserveSelection = false} = {}) {
    if (!preserveSelection) pendingSourceExpansionAnchor = undefined;
    const scrollTop = preserveSelection ? inspectorContent.scrollTop : 0;
    const sourceScrollPositions = preserveSelection
        ? new Map(currentSourceContexts.map((context, index) => {
            const excerpt = codeContent.querySelector(`.reference-item[data-index="${index}"] .reference-code`);
            return [context.id, {
                left: excerpt?.scrollLeft ?? 0,
                top: excerpt?.scrollTop ?? 0,
            }];
        }))
        : new Map();
    // Collapsing a context replaces it here, so the Scene payload a theme refresh reloads stays as sent.
    currentSourceContexts = [...contexts];
    activeSourceIndex = resolveActiveSourceIndex(
        currentSourceContexts,
        preserveSelection ? activeSourceIndex : requestedIndex,
    );
    const openState = reconcileInspectorOpenState(
        currentSourceContexts,
        {notesOpen, sourceKeys: [...openSourceKeys]},
        activeSourceIndex,
        preserveSelection,
    );
    notesOpen = openState.notesOpen;
    openSourceKeys = new Set(openState.sourceKeys);
    notesItem.open = notesOpen;
    renderSourceAccordion(sourceScrollPositions);
    inspectorContent.scrollTop = scrollTop;
    restoreSourceExpansionAnchor();
}

function renderSourceAccordion(sourceScrollPositions = new Map()) {
    codeContent.replaceChildren();
    currentSourceContexts.forEach((context, index) => {
        const sourceKey = context.id;
        const item = document.createElement("details");
        item.className = "reference-item source-reference-item";
        item.dataset.index = String(index);
        item.open = openSourceKeys.has(sourceKey);

        const summary = document.createElement("summary");
        summary.className = "reference-summary";
        summary.dataset.label = context.label || context.path || "Source context";
        summary.title = referenceSummaryTitle(summary.dataset.label, item.open);

        const summaryContent = document.createElement("span");
        summaryContent.className = "reference-summary-content";

        const chevron = document.createElement("span");
        chevron.className = "reference-chevron";
        chevron.setAttribute("aria-hidden", "true");

        const label = document.createElement("span");
        label.className = "reference-label";
        label.textContent = summary.dataset.label;

        const location = document.createElement("span");
        location.className = "reference-location";
        const lines = `${context.focusStartLine}–${context.focusEndLine}`;
        const contextPath = context.kind === "diff" && context.oldPath && context.oldPath !== context.path
            ? `${context.oldPath} → ${context.path}`
            : context.path;
        location.textContent = contextPath ? `${contextPath} · ${lines}` : lines;

        const see = document.createElement("button");
        see.className = "reference-see";
        see.type = "button";
        see.textContent = "SEE";
        see.title = `Open ${summary.dataset.label} in source editor`;
        see.setAttribute("aria-label", `Open ${summary.dataset.label} in source editor`);
        see.addEventListener("click", (event) => {
            event.preventDefault();
            event.stopPropagation();
            activeSourceIndex = index;
            vscode.postMessage({type: "openRelatedSource", index});
        });

        const codeFrame = document.createElement("div");
        codeFrame.className = "reference-code-frame";

        const codeHeader = document.createElement("div");
        codeHeader.className = "reference-code-header";
        codeHeader.append(location);
        if (context.kind === "diff") {
            const diffIndicator = document.createElement("span");
            diffIndicator.className = "reference-diff-indicator";
            diffIndicator.setAttribute("aria-label", "Diff source with inserted and removed lines");
            const added = document.createElement("span");
            added.className = "reference-diff-added";
            added.textContent = "+";
            const removed = document.createElement("span");
            removed.className = "reference-diff-removed";
            removed.textContent = "−";
            diffIndicator.append(added, removed);
            codeHeader.append(diffIndicator);
        }
        codeHeader.append(see);

        const excerpt = document.createElement("div");
        excerpt.className = "reference-code";
        excerpt.setAttribute("role", "region");
        excerpt.setAttribute(
            "aria-label",
            `${summary.dataset.label}, ${context.path || "source"}, reference lines ${lines}`,
        );
        useVerticalWheelScrollChain(excerpt, inspectorContent);

        summaryContent.append(chevron, label);
        summary.append(summaryContent);
        codeFrame.append(codeHeader, excerpt);
        item.append(summary, codeFrame);
        let interactive = !item.open;
        item.addEventListener("toggle", () => {
            summary.title = referenceSummaryTitle(summary.dataset.label, item.open);
            if (!interactive) return;
            if (!item.open) {
                openSourceKeys.delete(sourceKey);
                context = resetSourceContextView(context);
                currentSourceContexts[index] = context;
                if (pendingSourceExpansionAnchor?.id === context.id) pendingSourceExpansionAnchor = undefined;
                vscode.postMessage({
                    type: "resetSourceContext",
                    sceneId,
                    index,
                    id: context.id,
                });
                return;
            }
            openSourceKeys.add(sourceKey);
            activeSourceIndex = index;
            renderSourceExcerpt(excerpt, context, index);
        });
        codeContent.append(item);
        if (item.open) {
            renderSourceExcerpt(excerpt, context, index);
            const scrollPosition = sourceScrollPositions.get(sourceKey);
            excerpt.scrollLeft = scrollPosition?.left ?? 0;
            excerpt.scrollTop = scrollPosition?.top ?? 0;
            setTimeout(() => { interactive = true; }, 0);
        }
    });
}

function renderSourceExcerpt(excerpt, context, index) {
    const highlightGeneration = (sourceHighlightGenerations.get(excerpt) ?? 0) + 1;
    sourceHighlightGenerations.set(excerpt, highlightGeneration);
    excerpt.replaceChildren();
    const textElements = [];
    if (context.hasMoreAbove) {
        excerpt.append(sourceExpansionButton(excerpt, context, index, "above"));
    }
    for (const row of sourceContextRows(context)) {
        const line = document.createElement("div");
        line.className = "code-line";
        if (context.kind === "diff") line.classList.add("code-line-diff", `code-line-${row.kind}`);
        if (sourceContextRowIsFocused(context, row.lineNumber)) line.classList.add("code-line-focus");
        if (row.lineNumber !== undefined) line.dataset.lineNumber = String(row.lineNumber);

        const text = document.createElement("span");
        text.className = "code-line-text";
        text.textContent = row.text;
        textElements.push(text);
        if (context.kind === "diff") {
            const oldNumber = document.createElement("span");
            oldNumber.className = "code-line-number code-line-number-old";
            oldNumber.setAttribute("aria-hidden", "true");
            oldNumber.textContent = row.oldLineNumber === undefined ? "" : String(row.oldLineNumber);
            const newNumber = document.createElement("span");
            newNumber.className = "code-line-number code-line-number-new";
            newNumber.setAttribute("aria-hidden", "true");
            newNumber.textContent = row.newLineNumber === undefined ? "" : String(row.newLineNumber);
            const marker = document.createElement("span");
            marker.className = "code-line-marker";
            marker.setAttribute("aria-hidden", "true");
            marker.textContent = row.kind === "inserted" ? "+" : row.kind === "removed" ? "-" : " ";
            line.setAttribute("aria-label", diffRowLabel(row));
            line.append(oldNumber, newNumber, marker, text);
        } else {
            const number = document.createElement("span");
            number.className = "code-line-number";
            number.setAttribute("aria-hidden", "true");
            number.textContent = String(row.lineNumber);
            line.append(number, text);
        }
        excerpt.append(line);
    }
    if (context.unavailable) {
        const unavailable = document.createElement("div");
        unavailable.className = "code-more";
        unavailable.textContent = context.kind === "diff" ? "DIFF UNAVAILABLE" : "SOURCE CONTEXT UNAVAILABLE";
        excerpt.append(unavailable);
        return;
    }
    if (context.hasMoreBelow) {
        excerpt.append(sourceExpansionButton(excerpt, context, index, "below"));
    } else if (context.truncated) {
        const more = document.createElement("div");
        more.className = "code-more";
        more.textContent = "… MORE IN EDITOR";
        excerpt.append(more);
    }
    void highlightSourceLines(
        context.lines,
        context.language,
        shikiTheme(document.body.classList),
    ).then((highlightedLines) => {
        if (highlightGeneration !== sourceHighlightGenerations.get(excerpt)
            || context !== currentSourceContexts[index]
            || !excerpt.closest(".reference-item")?.open) return;
        for (const [index, tokens] of highlightedLines.entries()) {
            const text = textElements[index];
            if (!text) continue;
            text.replaceChildren(...tokens.map(codeTokenElement));
        }
    }).catch(() => {
        // Plain text remains visible when a grammar cannot be loaded.
    });
}

function diffRowLabel(row) {
    if (row.kind === "inserted") return `Inserted current line ${row.newLineNumber}: ${row.text}`;
    if (row.kind === "removed") return `Removed previous line ${row.oldLineNumber}: ${row.text}`;
    return `Previous line ${row.oldLineNumber}, current line ${row.newLineNumber}: ${row.text}`;
}

function sourceExpansionButton(excerpt, context, index, direction) {
    const count = sourceContextExpansionCount(context, direction, sourceContextLineCount);
    const button = document.createElement("button");
    button.className = `code-expand code-expand-${direction}`;
    button.type = "button";
    button.textContent = `SHOW ${count} ${count === 1 ? "LINE" : "LINES"} ${direction.toUpperCase()}`;
    button.setAttribute("aria-label", `Show ${count} more source ${count === 1 ? "line" : "lines"} ${direction}`);
    button.addEventListener("click", () => {
        const anchorLine = direction === "above" ? context.startLine : context.endLine;
        const anchor = excerpt.querySelector(`.code-line[data-line-number="${anchorLine}"]`);
        pendingSourceExpansionAnchor = {
            id: context.id,
            direction,
            startLine: context.startLine,
            endLine: context.endLine,
            line: anchorLine,
            top: anchor?.getBoundingClientRect().top,
        };
        for (const control of excerpt.querySelectorAll(".code-expand")) control.disabled = true;
        excerpt.setAttribute("aria-busy", "true");
        vscode.postMessage({
            type: "expandSourceContext",
            sceneId,
            index,
            id: context.id,
            direction,
        });
    });
    return button;
}

function restoreSourceExpansionAnchor() {
    const anchor = pendingSourceExpansionAnchor;
    if (!anchor || !Number.isFinite(anchor.top)) return;
    const index = currentSourceContexts.findIndex((context) => context.id === anchor.id);
    const context = currentSourceContexts[index];
    const expanded = anchor.direction === "above"
        ? context?.startLine < anchor.startLine
        : context?.endLine > anchor.endLine;
    if (!expanded) return;
    pendingSourceExpansionAnchor = undefined;
    const item = index < 0 ? undefined : codeContent.querySelector(`.reference-item[data-index="${index}"]`);
    const excerpt = item?.querySelector(".reference-code");
    const line = item?.querySelector(`.code-line[data-line-number="${anchor.line}"]`);
    if (!excerpt || !line) return;
    excerpt.scrollTop += line.getBoundingClientRect().top - anchor.top;
}

function refreshOpenSourceHighlights() {
    for (const item of codeContent.querySelectorAll(".reference-item[open]")) {
        const index = Number(item.dataset.index);
        const excerpt = item.querySelector(".reference-code");
        const context = currentSourceContexts[index];
        if (excerpt && context) renderSourceExcerpt(excerpt, context, index);
    }
}

function referenceSummaryTitle(label, open) {
    if (open) return `Collapse ${label}`;
    return `Expand ${label}`;
}

function codeTokenElement(token) {
    const span = document.createElement("span");
    span.className = "code-token";
    span.textContent = token.content;
    if (typeof token.color === "string") span.style.color = token.color;
    if ((token.fontStyle & 1) !== 0) span.style.fontStyle = "italic";
    if ((token.fontStyle & 2) !== 0) span.style.fontWeight = "bold";
    if ((token.fontStyle & 4) !== 0) span.style.textDecoration = "underline";
    return span;
}

function openSeriesItem(groupIndex, itemIndex) {
    seriesMenu.open = false;
    vscode.postMessage({type: "openSeries", groupIndex, itemIndex});
}

function syncSeriesStepControls() {
    const previous = seriesStepTarget(currentSeries, -1);
    const next = seriesStepTarget(currentSeries, 1);
    const groups = Array.isArray(currentSeries?.groups) ? currentSeries.groups : [];
    const group = Number.isInteger(currentSeries?.activeGroupIndex)
        ? groups[currentSeries.activeGroupIndex]
        : undefined;
    const itemCount = Array.isArray(group?.items) ? group.items.length : 0;
    const activeItem = Number.isInteger(group?.activeIndex) ? group.activeIndex : -1;
    const hasActiveItem = activeItem >= 0 && activeItem < itemCount;
    seriesPosition.textContent = hasActiveItem ? `${activeItem + 1}/${itemCount}` : "—";
    seriesMenuTrigger.setAttribute(
        "aria-label",
        hasActiveItem
            ? `Open animation series, ${group.label}, item ${activeItem + 1} of ${itemCount}`
            : "Open animation series",
    );
    seriesMenuTrigger.title = hasActiveItem
        ? `${group.label} · ${activeItem + 1}/${itemCount}`
        : "Animation series";
    seriesPreviousButton.disabled = !previous;
    seriesNextButton.disabled = !next;
    seriesPreviousButton.title = previous
        ? `Previous in ${previous.groupLabel}: ${previous.itemLabel}`
        : "No previous series item";
    seriesNextButton.title = next
        ? `Next in ${next.groupLabel}: ${next.itemLabel}`
        : "No next series item";
}

function showError(message, label, title = "RENDER INTERRUPTED") {
    playing = false;
    runtimeAudio.sync(false);
    errorTitle.textContent = title;
    errorMessage.textContent = message;
    errorCard.hidden = false;
    empty.hidden = true;
    canvas.style.opacity = "0.2";
    canvas.style.filter = "grayscale(0.7)";
    setStatus(label, "error");
    syncControls();
    vscode.postMessage({type: "renderError", message});
}

function draw() {
    if (!runtime) return;
    currentTime = normalizedTime(currentTime);
    try {
        runtime.draw(canvas, currentTime, renderPixelRatio);
        measuredFrames += 1;
        updatePerformance();
        const duration = runtime.duration;
        scrub.value = String(currentTime);
        const progress = duration > 0 ? (currentTime / duration) * 100 : 0;
        scrub.style.setProperty("--progress", `${Math.max(0, Math.min(100, progress))}%`);
        timeOutput.textContent = runtime.retainedLua && duration <= 0
            ? `${runtime.runtimeTime.toFixed(2)} · LIVE`
            : `${currentTime.toFixed(2)} / ${duration.toFixed(2)}`;
        syncPlaybackRange();
    } catch (error) {
        const detail = `${describeError(error)}\n\nseek ${currentTime.toFixed(6)} / ${runtime.duration.toFixed(6)} s`;
        showError(detail, "RENDER ERROR");
    }
}

function normalizedTime(value) {
    return normalizeTime(value, runtime?.duration ?? 0);
}

function syncPlaybackRange() {
    const duration = runtime?.duration ?? 0;
    if (playbackRange && duration > 0) {
        timeline.style.setProperty("--range-active", "1");
        timeline.style.setProperty("--range-start", `${playbackRange.start / duration * 100}%`);
        timeline.style.setProperty("--range-end", `${playbackRange.end / duration * 100}%`);
        rangeValue.textContent = `${playbackRange.start.toFixed(2)}–${playbackRange.end.toFixed(2)}`;
    } else {
        timeline.style.setProperty("--range-active", "0");
        timeline.style.setProperty("--range-start", "0%");
        timeline.style.setProperty("--range-end", "100%");
        rangeValue.textContent = "FULL";
    }
    rangeInButton.disabled = busy || !runtime || currentTime >= duration;
    rangeOutButton.disabled = busy || !runtime || currentTime <= 0;
    rangeClearButton.disabled = busy || !runtime || !playbackRange;
}

function updatePlaybackRange(range) {
    playbackRange = range;
    draw();
    syncControls();
    setStatus(
        playbackRange ? `RANGE ${playbackRange.start.toFixed(2)}–${playbackRange.end.toFixed(2)}` : "FULL RANGE",
        "ready",
    );
}

function syncPreviewScale() {
    canvasWrap.dataset.fit = String(previewFits);
    canvas.style.width = `${sceneWidth * previewScale}px`;
    canvas.style.height = `${sceneHeight * previewScale}px`;
    syncPreviewTransform();
    syncPreviewControls();
    syncRenderPixelRatio();
}

function syncRenderPixelRatio() {
    const next = previewRenderPixelRatio(
        sceneWidth,
        sceneHeight,
        previewScale,
        window.devicePixelRatio || 1,
    );
    if (next === renderPixelRatio) return;
    renderPixelRatio = next;
    if (!runtime || playing) return;
    cancelAnimationFrame(renderPixelRatioFrame);
    renderPixelRatioFrame = requestAnimationFrame(() => {
        renderPixelRatioFrame = 0;
        draw();
    });
}

function syncPreviewTransform() {
    canvas.style.transform = previewFits ? `translate(${previewPanX}px, ${previewPanY}px)` : "";
}

function syncPreviewControls() {
    fitModeButton.setAttribute("aria-current", String(previewFits));
    actualModeButton.setAttribute("aria-current", String(!previewFits));
    fitModeButton.disabled = !runtime || busy || previewFits;
    actualModeButton.disabled = !runtime || busy || !previewFits;
    previewResetButton.disabled = !runtime || busy;
}

function setPreviewScale(value, options = {}) {
    const next = clampPreviewScale(value);
    const wrapBounds = canvasWrap.getBoundingClientRect();
    const clientX = options.clientX ?? wrapBounds.left + wrapBounds.width / 2;
    const clientY = options.clientY ?? wrapBounds.top + wrapBounds.height / 2;
    const before = canvas.getBoundingClientRect();
    const anchorX = before.width > 0 ? Math.max(0, Math.min(1, (clientX - before.left) / before.width)) : 0.5;
    const anchorY = before.height > 0 ? Math.max(0, Math.min(1, (clientY - before.top) / before.height)) : 0.5;

    const fit = Boolean(options.fit);
    const preserveFitAnchor = fit && options.preserveAnchor !== false;
    if (!preserveFitAnchor) {
        previewPanX = 0;
        previewPanY = 0;
    }
    previewScale = next;
    previewFits = fit;
    syncPreviewScale();

    if (previewFits) {
        canvasWrap.scrollLeft = 0;
        canvasWrap.scrollTop = 0;
        if (preserveFitAnchor) {
            const translation = previewAnchorTranslation(before, canvas.getBoundingClientRect(), clientX, clientY);
            previewPanX += translation.x;
            previewPanY += translation.y;
            syncPreviewTransform();
        }
        return;
    }

    requestAnimationFrame(() => {
        if (options.preserveAnchor === false) {
            canvasWrap.scrollLeft = 0;
            canvasWrap.scrollTop = 0;
            return;
        }
        const after = canvas.getBoundingClientRect();
        canvasWrap.scrollLeft += after.left + anchorX * after.width - clientX;
        canvasWrap.scrollTop += after.top + anchorY * after.height - clientY;
    });
}

function fitPreview() {
    setPreviewScale(
        fitPreviewScale(sceneWidth, sceneHeight, canvasWrap.clientWidth, canvasWrap.clientHeight, 2),
        {fit: true, preserveAnchor: false},
    );
}

function resetPreviewTransform() {
    if (previewFits) fitPreview();
    else setPreviewScale(1, {preserveAnchor: false});
}

function applyManualStageHeight() {
    if (manualStageHeightValue === null || canvasFullscreen) return;
    const viewportHeight = viewerLayout === "wide"
        ? player.getBoundingClientRect().height
        : window.innerHeight;
    const height = manualStageHeight(
        manualStageHeightValue,
        viewportHeight,
        transport.getBoundingClientRect().height,
        stageResizer.getBoundingClientRect().height,
    );
    document.documentElement.style.setProperty("--manual-stage-height", `${height}px`);
    player.dataset.stageSizing = "manual";
    stageResizer.setAttribute("aria-valuenow", String(height));
}

function setManualStageHeight(value) {
    manualStageHeightValue = Math.max(0, Math.round(value));
    applyManualStageHeight();
}

function syncCanvasStat() {
    stageResizer.dataset.stats = String(showStats);
    canvasStat.textContent = `${sceneWidth} × ${sceneHeight} · ${measuredFps} FPS`;
    statValue.textContent = showStats ? "ENABLED" : "DISABLED";
    for (const button of statButtons) {
        button.setAttribute("aria-current", String((button.dataset.stat === "true") === showStats));
        button.disabled = (button.dataset.stat === "true") === showStats;
    }
}

function resetCanvasView() {
    manualStageHeightValue = null;
    manualWidePlayerWidthValue = null;
    delete player.dataset.stageSizing;
    stageResizer.removeAttribute("aria-valuenow");
    wideResizer.removeAttribute("aria-valuenow");
    document.documentElement.style.removeProperty("--manual-stage-height");
    document.documentElement.style.removeProperty("--wide-player-width");
    if (viewerLayout === "wide") syncWidePlayerWidth();
    else syncNormalLayoutLimit();
    resetPreviewTransform();
}

function syncNormalLayoutLimit() {
    if (canvasFullscreen || viewerLayout === "wide" || manualStageHeightValue !== null) return;
    const height = normalStageMaxHeight(
        window.innerHeight,
        transport.getBoundingClientRect().height,
        stageResizer.getBoundingClientRect().height,
        stageShell.getBoundingClientRect().width,
        sceneWidth,
        sceneHeight,
    );
    const propertyValue = `${height}px`;
    if (document.documentElement.style.getPropertyValue("--normal-stage-max-height") !== propertyValue) {
        document.documentElement.style.setProperty("--normal-stage-max-height", propertyValue);
    }
}

function syncWidePlayerWidth() {
    if (viewerLayout !== "wide" || canvasFullscreen) return;
    const viewportWidth = viewer.getBoundingClientRect().width;
    const resizerWidth = wideResizer.getBoundingClientRect().width;
    const width = manualWidePlayerWidthValue === null
        ? automaticWidePlayerWidth(viewportWidth, resizerWidth)
        : widePlayerWidth(manualWidePlayerWidthValue, viewportWidth, resizerWidth);
    document.documentElement.style.setProperty("--wide-player-width", `${width}px`);
    wideResizer.setAttribute("aria-valuenow", String(width));
}

function setManualWidePlayerWidth(value) {
    manualWidePlayerWidthValue = Math.max(0, Math.round(value));
    syncWidePlayerWidth();
}

function syncViewerLayout() {
    const next = responsiveViewerLayout(window.innerWidth, viewerLayout, forceCompactLayout);
    if (next === viewerLayout && document.body.dataset.layout === next) return;
    viewerLayout = next;
    document.body.dataset.layout = viewerLayout;
    syncSeriesPanelHost();
    stageResizer.setAttribute("aria-disabled", String(canvasFullscreen));
    const wideResizeEnabled = viewerLayout === "wide" && !canvasFullscreen;
    wideResizer.setAttribute("aria-disabled", String(!wideResizeEnabled));
    wideResizer.tabIndex = wideResizeEnabled ? 0 : -1;
    if (viewerLayout === "compact" && manualStageHeightValue === null) syncNormalLayoutLimit();
    if (viewerLayout === "wide") syncWidePlayerWidth();
    requestAnimationFrame(() => {
        if (previewFits) fitPreview();
    });
}

function syncPlayerMenuPlacement() {
    if (playerMenuAnchor) {
        const panelWidth = Math.min(PLAYER_MENU_WIDTH, Math.max(0, window.innerWidth - 8));
        const placement = floatingPointMenuPlacement(
            window.innerWidth,
            window.innerHeight,
            playerMenuAnchor.x,
            playerMenuAnchor.y,
            panelWidth,
        );
        menuPanel.style.top = placement.top === null ? "auto" : `${placement.top}px`;
        menuPanel.style.bottom = placement.bottom === null ? "auto" : `${placement.bottom}px`;
        menuPanel.style.left = `${placement.left}px`;
        menuPanel.style.right = "auto";
        menuPanel.style.maxHeight = `${placement.maxHeight}px`;
        return;
    }
    const placement = floatingMenuPlacement(
        window.innerWidth,
        window.innerHeight,
        playerMenuTrigger.getBoundingClientRect(),
    );
    menuPanel.style.top = placement.top === null ? "auto" : `${placement.top}px`;
    menuPanel.style.bottom = placement.bottom === null ? "auto" : `${placement.bottom}px`;
    menuPanel.style.left = "auto";
    menuPanel.style.right = `${placement.right}px`;
    menuPanel.style.maxHeight = `${placement.maxHeight}px`;
}

function syncSubmenuPlacement(submenu) {
    if (!submenu.open) return;
    const summary = submenu.querySelector(":scope > summary");
    const panel = submenu.querySelector(":scope > .submenu-panel");
    const panelBounds = panel.getBoundingClientRect();
    const placement = cascadingMenuPlacement(
        window.innerWidth,
        window.innerHeight,
        summary.getBoundingClientRect(),
        panelBounds.width || Math.min(152, Math.max(0, window.innerWidth - 8)),
        panelBounds.height,
    );
    panel.style.left = `${placement.left}px`;
    panel.style.top = `${placement.top}px`;
    panel.style.maxHeight = `${placement.maxHeight}px`;
    submenu.dataset.positioned = "true";
}

function syncSeriesMenuPlacement() {
    if (seriesPanel.parentElement === wideSeriesSlot) return;
    const triggerBounds = seriesMenuTrigger.getBoundingClientRect();
    const placement = floatingMenuPlacement(
        window.innerWidth,
        window.innerHeight,
        triggerBounds,
    );
    const measuredPanelWidth = seriesPanel.getBoundingClientRect().width;
    const panelWidth = measuredPanelWidth || Math.min(300, Math.max(0, window.innerWidth - 16));
    seriesPanel.style.top = placement.top === null ? "auto" : `${placement.top}px`;
    seriesPanel.style.bottom = placement.bottom === null ? "auto" : `${placement.bottom}px`;
    seriesPanel.style.left = `${Math.max(0, Math.min(triggerBounds.left, window.innerWidth - panelWidth))}px`;
    seriesPanel.style.maxHeight = `${placement.maxHeight}px`;
}

function prepareMenuPlacement(menu, trigger, syncPlacement) {
    trigger.addEventListener("click", (event) => {
        if (event.defaultPrevented || menu.open) return;
        syncPlacement();
        menu.dataset.positioned = "true";
    });
}

function syncOpenedMenuPlacement(menu, syncPlacement) {
    if (!menu.open) {
        menu.dataset.positioned = "false";
        return;
    }
    syncPlacement();
    menu.dataset.positioned = "true";
}

function closePlayerMenu() {
    playerMenu.open = false;
}

function syncAutoPlayNext() {
    for (const button of autoPlayNextButtons) {
        button.setAttribute("aria-current", String((button.dataset.autoPlayNext === "true") === autoPlayNext));
        button.disabled = (button.dataset.autoPlayNext === "true") === autoPlayNext;
    }
    syncOptionSummary();
}

function syncForceCompactLayout() {
    for (const button of forceCompactButtons) {
        button.setAttribute("aria-current", String((button.dataset.forceCompact === "true") === forceCompactLayout));
        button.disabled = (button.dataset.forceCompact === "true") === forceCompactLayout;
    }
    syncOptionSummary();
}

function syncSourceContextLineCount() {
    sourceLinesValue.textContent = String(sourceContextLineCount);
    for (const button of sourceLineButtons) {
        const current = Number(button.dataset.sourceLines) === sourceContextLineCount;
        button.setAttribute("aria-current", String(current));
        button.disabled = current;
    }
}

function syncOptionSummary() {
    const enabled = Number(autoPlayNext) + Number(forceCompactLayout);
    optionValue.textContent = enabled ? `${enabled} ENABLED` : "DISABLED";
}

function setCanvasFullscreen(enabled) {
    canvasFullscreen = Boolean(enabled);
    document.body.classList.toggle("canvas-fullscreen", canvasFullscreen);
    stageResizer.setAttribute("aria-disabled", String(canvasFullscreen));
    wideResizer.setAttribute("aria-disabled", String(canvasFullscreen || viewerLayout !== "wide"));
    if (canvasFullscreen) {
        delete player.dataset.stageSizing;
        document.documentElement.style.removeProperty("--manual-stage-height");
    }
    playerMenu.open = false;
    seriesMenu.open = false;
    syncSeriesPanelHost();
    if (!canvasFullscreen) {
        if (viewerLayout === "wide") syncWidePlayerWidth();
        if (manualStageHeightValue !== null) applyManualStageHeight();
        else if (viewerLayout === "compact") syncNormalLayoutLimit();
    }
    requestAnimationFrame(() => {
        fitPreview();
    });
}

function stepPreview(direction, clientX, clientY) {
    setPreviewScale(stepPreviewScale(previewScale, direction), {clientX, clientY});
}

function wheelPreview(deltaY, clientX, clientY) {
    if (!previewFits) return;
    const fitScale = fitPreviewScale(
        sceneWidth,
        sceneHeight,
        canvasWrap.clientWidth,
        canvasWrap.clientHeight,
        2,
    );
    setPreviewScale(wheelPreviewScale(previewScale, deltaY, fitScale), {clientX, clientY, fit: true});
}

function updatePerformance() {
    const now = performance.now();
    const elapsed = now - measureStarted;
    if (elapsed < 500) return;
    const fps = measuredFrames * 1000 / elapsed;
    measuredFps = fps.toFixed(0);
    syncCanvasStat();
    measuredFrames = 0;
    measureStarted = now;
}

function frame(now) {
    const elapsed = frameElapsed(now, previousFrame);
    previousFrame = now;
    if (playing && runtime) {
        const scaledElapsed = elapsed * playbackSpeed;
        try {
            const runtimeStep = advanceRetainedRuntime(runtime, scaledElapsed);
            runtimeAudio.play(runtimeStep);
            if (runtime.duration > 0) {
                const next = advanceSeriesPlayback(
                    {series: currentSeries, autoPlayNext, looping},
                    currentTime,
                    scaledElapsed,
                    runtime.duration,
                    playbackRange,
                );
                currentTime = next.time;
                if (next.ended) {
                    playing = false;
                    syncControls();
                    if (next.open) openSeriesItem(next.open.groupIndex, next.open.itemIndex);
                }
            }
            runtimeAudio.sync(false);
            draw();
        } catch (error) {
            showError(describeError(error), "RUNTIME ERROR");
        }
    }
    animationFrame = requestAnimationFrame(frame);
}

// Retained Lua runs on its own clock, other scenes on the transport's.
function sceneTime() {
    return runtime?.retainedLua ? runtime.runtimeTime : currentTime;
}

function syncControls() {
    playIcon.toggleAttribute("hidden", playing);
    pauseIcon.toggleAttribute("hidden", !playing);
    playButton.dataset.state = playing ? "playing" : "paused";
    playButton.setAttribute("aria-label", playing ? "Pause" : "Play");
    playButton.title = playing ? "Pause" : "Play";
    loopButton.setAttribute("aria-pressed", String(looping));
    loopButton.setAttribute("aria-label", looping ? "Disable loop" : "Enable loop");
    loopButton.title = looping ? "Loop on" : "Loop off";
    playButton.disabled = busy;
    restartButton.disabled = busy;
    scrub.disabled = busy;
    syncPlaybackRange();
    syncPreviewControls();
    syncCamera();
}

function syncCamera() {
    if (!runtime) return;
    const interactive = runtime.cameraMode === "interactive";
    canvas.dataset.cameraMode = runtime.cameraMode;
    cameraMode.textContent = `${runtime.cameraMode === "interactive" ? "" : "FIXED · "}${runtime.cameraView.toUpperCase()}`;
    for (const button of cameraButtons) {
        const action = button.dataset.camera;
        button.setAttribute("aria-current", String(action !== "reset" && action === `view${runtime.cameraView}`));
        button.disabled = !interactive || busy || (action !== "reset" && action === `view${runtime.cameraView}`);
    }
}

function moveCamera(action, x = 0, y = 0, redraw = true) {
    if (!runtime || busy) return false;
    try {
        if (!runtime.cameraInput(action, x, y)) {
            setStatus("FIXED CAMERA", "ready");
            return false;
        }
        syncCamera();
        if (redraw) draw();
        setStatus(`${runtime.cameraView.toUpperCase()} CAMERA`, "ready");
        return true;
    } catch (error) {
        showError(describeError(error), "CAMERA ERROR");
        return false;
    }
}

notesItem.addEventListener("toggle", () => {
    notesOpen = notesItem.open;
});

playButton.addEventListener("click", () => {
    if (!runtime) return;
    runtimeAudio.start();
    const {start, end} = playbackBounds(playbackRange, runtime.duration);
    if (!playing && (currentTime < start || currentTime >= end)) currentTime = start;
    playing = !playing;
    previousFrame = performance.now();
    runtimeAudio.sync(true);
    draw();
    syncControls();
});

restartButton.addEventListener("click", () => {
    currentTime = playbackBounds(playbackRange, runtime?.duration ?? 0).start;
    previousFrame = performance.now();
    runtimeAudio.sync(true);
    draw();
});

loopButton.addEventListener("click", () => {
    looping = !looping;
    syncControls();
});

seriesPreviousButton.addEventListener("click", () => {
    const target = seriesStepTarget(currentSeries, -1);
    if (target) openSeriesItem(target.groupIndex, target.itemIndex);
});

seriesNextButton.addEventListener("click", () => {
    const target = seriesStepTarget(currentSeries, 1);
    if (target) openSeriesItem(target.groupIndex, target.itemIndex);
});

scrub.addEventListener("input", () => {
    currentTime = normalizedTime(Number(scrub.value));
    previousFrame = performance.now();
    runtimeAudio.sync(true);
    draw();
});

rangeInButton.addEventListener("click", () => {
    if (!runtime) return;
    updatePlaybackRange(setPlaybackStart(playbackRange, currentTime, runtime.duration));
    closePlayerMenu();
});

rangeOutButton.addEventListener("click", () => {
    if (!runtime) return;
    updatePlaybackRange(setPlaybackEnd(playbackRange, currentTime, runtime.duration));
    closePlayerMenu();
});

rangeClearButton.addEventListener("click", () => {
    updatePlaybackRange(null);
    closePlayerMenu();
});

document.querySelector("#open-source").addEventListener("click", () => {
    vscode.postMessage({type: "openSource"});
    closePlayerMenu();
});
document.querySelector("#open-lua").addEventListener("click", () => {
    vscode.postMessage({type: "openLua"});
    closePlayerMenu();
});
document.querySelector("#error-source").addEventListener("click", () => vscode.postMessage({type: "openSource"}));
document.querySelector("#refresh").addEventListener("click", () => {
    vscode.postMessage({type: "refresh"});
    closePlayerMenu();
});

function dataUrl(data, mime) {
    return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.addEventListener("load", () => resolve(reader.result));
        reader.addEventListener("error", () => reject(reader.error || new Error("Could not encode export data")));
        reader.readAsDataURL(new Blob([data], {type: mime}));
    });
}

async function exportMedia(format, fps = 30) {
    if (!runtime || busy || !["png", "gif", "mp4"].includes(format)) return;
    const exportingRuntime = runtime;
    const exportRange = playbackRange ? {...playbackRange} : null;
    const resume = playing;
    const controller = new AbortController();
    busy = true;
    playing = false;
    runtimeAudio.sync(false);
    exportController = controller;
    syncControls();
    try {
        let bytes;
        let mime;
        if (format === "png") {
            bytes = await new Promise((resolve, reject) => {
                canvas.toBlob((value) => value ? resolve(value) : reject(new Error("PNG encoding failed")), "image/png");
            });
            mime = "image/png";
            setStatus("PNG READY", "ready");
        } else {
            if (format === "mp4" && !("VideoEncoder" in window)) {
                throw new Error("MP4 export requires WebCodecs support in this VS Code build");
            }
            const save = format === "gif" ? saveGif : saveMp4;
            mime = format === "gif" ? "image/gif" : "video/mp4";
            bytes = await save(
                exportingRuntime,
                fps,
                (done, total) => {
                    if (runtime === exportingRuntime) setStatus(`${format.toUpperCase()} ${done}/${total}`, "loading");
                },
                controller.signal,
                exportRange,
            );
            setStatus(`${format.toUpperCase()} READY`, "ready");
        }
        if (runtime !== exportingRuntime || controller.signal.aborted) return;
        vscode.postMessage({type: "saveMedia", format, dataUrl: await dataUrl(bytes, mime)});
    } catch (error) {
        if (error?.name !== "AbortError") {
            const message = describeError(error);
            setStatus(`${format.toUpperCase()} ERROR`, "error");
            vscode.postMessage({type: "exportError", message});
        }
    } finally {
        if (exportController === controller) {
            busy = false;
            if (runtime === exportingRuntime) playing = resume;
            if (runtime === exportingRuntime) runtimeAudio.sync(true);
            previousFrame = performance.now();
            syncControls();
            if (runtime === exportingRuntime) draw();
            exportController = undefined;
        }
    }
}

for (const button of speedButtons) {
    button.addEventListener("click", () => {
        playbackSpeed = Number(button.dataset.speed);
        runtimeAudio.sync(true);
        speedValue.textContent = button.textContent;
        for (const candidate of speedButtons) {
            candidate.setAttribute("aria-current", String(candidate === button));
            candidate.disabled = candidate === button;
        }
        closePlayerMenu();
    });
}

for (const button of statButtons) {
    button.addEventListener("click", () => {
        showStats = button.dataset.stat === "true";
        const state = vscode.getState();
        vscode.setState({
            ...(state && typeof state === "object" ? state : {}),
            showStats,
        });
        syncCanvasStat();
        closePlayerMenu();
    });
}
syncCanvasStat();

for (const button of sourceLineButtons) {
    button.addEventListener("click", () => {
        const count = Number(button.dataset.sourceLines);
        if (!SOURCE_CONTEXT_LINE_COUNTS.has(count)) return;
        sourceContextLineCount = count;
        const state = vscode.getState();
        vscode.setState({
            ...(state && typeof state === "object" ? state : {}),
            sourceContextLineCount,
        });
        vscode.postMessage({type: "setSourceContextLineCount", count: sourceContextLineCount});
        syncSourceContextLineCount();
        syncSourceInspector(currentSourceContexts, activeSourceIndex, {preserveSelection: true});
        closePlayerMenu();
    });
}
syncSourceContextLineCount();

for (const button of autoPlayNextButtons) {
    button.addEventListener("click", () => {
        autoPlayNext = button.dataset.autoPlayNext === "true";
        const state = vscode.getState();
        vscode.setState({
            ...(state && typeof state === "object" ? state : {}),
            autoPlayNext,
        });
        vscode.postMessage({type: "setAutoPlayNext", enabled: autoPlayNext});
        syncAutoPlayNext();
        closePlayerMenu();
    });
}
syncAutoPlayNext();

for (const button of forceCompactButtons) {
    button.addEventListener("click", () => {
        forceCompactLayout = button.dataset.forceCompact === "true";
        const state = vscode.getState();
        vscode.setState({
            ...(state && typeof state === "object" ? state : {}),
            forceCompactLayout,
        });
        vscode.postMessage({type: "setForceCompactLayout", enabled: forceCompactLayout});
        syncForceCompactLayout();
        syncViewerLayout();
        closePlayerMenu();
    });
}
syncForceCompactLayout();

for (const button of cameraButtons) {
    button.addEventListener("click", () => {
        moveCamera(button.dataset.camera);
        closePlayerMenu();
    });
}

fitModeButton.addEventListener("click", () => {
    fitPreview();
    closePlayerMenu();
});
actualModeButton.addEventListener("click", () => {
    setPreviewScale(1, {preserveAnchor: false});
    closePlayerMenu();
});
previewResetButton.addEventListener("click", () => {
    resetCanvasView();
    closePlayerMenu();
});
prepareMenuPlacement(playerMenu, playerMenuTrigger, () => {
    playerMenuAnchor = undefined;
    syncPlayerMenuPlacement();
});
prepareMenuPlacement(seriesMenu, seriesMenuTrigger, syncSeriesMenuPlacement);
seriesMenuTrigger.addEventListener("click", () => {
    if (player.dataset.seriesDocked !== "true") return;
    hostSeriesPanelInPopup();
}, {capture: true});
playerMenu.addEventListener("toggle", () => {
    if (playerMenu.open) seriesMenu.open = false;
    else {
        playerMenuAnchor = undefined;
        for (const submenu of menuSubmenus) {
            submenu.open = false;
            submenu.dataset.positioned = "false";
        }
    }
    syncOpenedMenuPlacement(playerMenu, syncPlayerMenuPlacement);
});
for (const submenu of menuSubmenus) {
    submenu.addEventListener("toggle", () => {
        if (submenu.open) {
            for (const candidate of menuSubmenus) {
                if (candidate !== submenu) {
                    candidate.open = false;
                    candidate.dataset.positioned = "false";
                }
            }
            syncSubmenuPlacement(submenu);
        } else {
            submenu.dataset.positioned = "false";
        }
    });
}
seriesMenu.addEventListener("toggle", () => {
    if (seriesMenu.open) playerMenu.open = false;
    syncOpenedMenuPlacement(seriesMenu, syncSeriesMenuPlacement);
    if (seriesMenu.open) revealActiveSeriesItem();
    else syncSeriesPanelHost();
});

new ResizeObserver(() => {
    if (previewFits) fitPreview();
}).observe(canvasWrap);

stageResizer.addEventListener("pointerdown", (event) => {
    if (canvasFullscreen || event.button !== 0) return;
    event.preventDefault();
    stageResizePointerId = event.pointerId;
    stageResizer.setPointerCapture(event.pointerId);
    stageResizer.dataset.dragging = "true";
    setManualStageHeight(event.clientY - player.getBoundingClientRect().top);
});
stageResizer.addEventListener("pointermove", (event) => {
    if (event.pointerId !== stageResizePointerId) return;
    setManualStageHeight(event.clientY - player.getBoundingClientRect().top);
});
stageResizer.addEventListener("keydown", (event) => {
    if (canvasFullscreen || !["ArrowUp", "ArrowDown"].includes(event.key)) return;
    event.preventDefault();
    const current = manualStageHeightValue ?? stageShell.getBoundingClientRect().height;
    const step = event.shiftKey ? 40 : 12;
    setManualStageHeight(current + (event.key === "ArrowDown" ? step : -step));
});
function finishStageResize(event) {
    if (event.pointerId !== stageResizePointerId) return;
    if (stageResizer.hasPointerCapture(event.pointerId)) stageResizer.releasePointerCapture(event.pointerId);
    stageResizePointerId = null;
    delete stageResizer.dataset.dragging;
}
stageResizer.addEventListener("pointerup", finishStageResize);
stageResizer.addEventListener("pointercancel", finishStageResize);
stageResizer.addEventListener("lostpointercapture", (event) => {
    if (event.pointerId !== stageResizePointerId) return;
    stageResizePointerId = null;
    delete stageResizer.dataset.dragging;
});

wideResizer.addEventListener("pointerdown", (event) => {
    if (canvasFullscreen || viewerLayout !== "wide" || event.button !== 0) return;
    event.preventDefault();
    wideResizePointerId = event.pointerId;
    wideResizer.setPointerCapture(event.pointerId);
    wideResizer.dataset.dragging = "true";
    setManualWidePlayerWidth(event.clientX - viewer.getBoundingClientRect().left);
});
wideResizer.addEventListener("pointermove", (event) => {
    if (event.pointerId !== wideResizePointerId) return;
    setManualWidePlayerWidth(event.clientX - viewer.getBoundingClientRect().left);
});
wideResizer.addEventListener("keydown", (event) => {
    if (canvasFullscreen || viewerLayout !== "wide" || !["ArrowLeft", "ArrowRight"].includes(event.key)) return;
    event.preventDefault();
    const current = manualWidePlayerWidthValue ?? player.getBoundingClientRect().width;
    const step = event.shiftKey ? 40 : 12;
    setManualWidePlayerWidth(current + (event.key === "ArrowRight" ? step : -step));
});
function finishWideResize(event) {
    if (event.pointerId !== wideResizePointerId) return;
    if (wideResizer.hasPointerCapture(event.pointerId)) wideResizer.releasePointerCapture(event.pointerId);
    wideResizePointerId = null;
    delete wideResizer.dataset.dragging;
}
wideResizer.addEventListener("pointerup", finishWideResize);
wideResizer.addEventListener("pointercancel", finishWideResize);
wideResizer.addEventListener("lostpointercapture", (event) => {
    if (event.pointerId !== wideResizePointerId) return;
    wideResizePointerId = null;
    delete wideResizer.dataset.dragging;
});

const layoutObserver = new ResizeObserver(() => {
    if (manualStageHeightValue !== null) applyManualStageHeight();
    else if (viewerLayout === "compact") syncNormalLayoutLimit();
});
layoutObserver.observe(stageShell);
layoutObserver.observe(transport);
window.addEventListener("resize", () => {
    syncViewerLayout();
    if (manualStageHeightValue === null) syncNormalLayoutLimit();
    else applyManualStageHeight();
    if (viewerLayout === "wide") syncWidePlayerWidth();
    syncPlayerMenuPlacement();
    for (const submenu of menuSubmenus) syncSubmenuPlacement(submenu);
    syncSeriesMenuPlacement();
    syncRenderPixelRatio();
});
syncViewerLayout();
syncNormalLayoutLimit();

window.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && (playerMenu.open || seriesMenu.open)) {
        playerMenu.open = false;
        seriesMenu.open = false;
        return;
    }
    if (event.key === "Escape" && canvasFullscreen) {
        event.preventDefault();
        setCanvasFullscreen(false);
        return;
    }
});

const canvasInputRouter = createCanvasInputRouter({
    canvas,
    wheelTarget: canvasWrap,
    focusTarget: canvasWrap,
    keyTarget: window,
    getRuntime: () => runtime,
    getSceneSize: () => [sceneWidth, sceneHeight],
    getTime: sceneTime,
    isBusy: () => busy,
    isFit: () => previewFits,
    camera: (action, x, y) => moveCamera(action, x, y, false),
    previewPan: (phase, dx, dy) => {
        if (phase !== "move") return;
        previewPanX += dx;
        previewPanY += dy;
        syncPreviewTransform();
    },
    previewWheel: (event) => wheelPreview(event.deltaY, event.clientX, event.clientY),
    previewKey: (event) => {
        if (!event.ctrlKey && !event.metaKey) return false;
        if (["+", "="].includes(event.key)) stepPreview(1);
        else if (event.key === "-") stepPreview(-1);
        else if (event.key === "0") setPreviewScale(1, {preserveAnchor: false});
        else return false;
        return true;
    },
    contextMenu: (event) => {
        playerMenuAnchor = {x: event.clientX, y: event.clientY};
        seriesMenu.open = false;
        for (const submenu of menuSubmenus) {
            submenu.open = false;
            submenu.dataset.positioned = "false";
        }
        playerMenu.dataset.positioned = "false";
        syncPlayerMenuPlacement();
        playerMenu.dataset.positioned = "true";
        playerMenu.open = true;
    },
    redraw: () => {
        syncCamera();
        if (!playing) draw();
    },
    ignoreKey: (event) => isHostKeyTarget(event.target),
    inputError: (error) => showError(describeError(error), "INPUT ERROR"),
});

const disposeHorizontalWheel = [
    useVerticalWheelForHorizontalScroll(seriesGroupItems),
    useVerticalWheelForHorizontalScroll(canvasWrap, () => canvasWrap.dataset.fit !== "true"),
];

document.addEventListener("pointerdown", (event) => {
    runtimeAudio.start();
    if (playerMenu.open && !playerMenu.contains(event.target)) playerMenu.open = false;
    if (seriesMenu.open && !seriesMenu.contains(event.target)) seriesMenu.open = false;
});

window.addEventListener("keydown", () => runtimeAudio.start(), {capture: true});

window.addEventListener("message", (event) => {
    if (event.data?.type === "scene") void loadScene(event.data);
    if (event.data?.type === "series") syncSeries(event.data.series);
    if (event.data?.type === "sourceContexts") {
        if (currentScenePayload) {
            currentScenePayload = {
                ...currentScenePayload,
                sourceContexts: event.data.sourceContexts,
                activeSourceIndex: event.data.activeSourceIndex,
            };
        }
        syncSourceInspector(event.data.sourceContexts, event.data.activeSourceIndex, {preserveSelection: true});
    }
    if (event.data?.type === "export") void exportMedia(event.data.format, Number(event.data.fps) || 30);
});

window.addEventListener("beforeunload", () => {
    cancelAnimationFrame(animationFrame);
    cancelAnimationFrame(themeRefreshFrame);
    vscodeThemeObserver.disconnect();
    exportController?.abort(new DOMException("Preview closed", "AbortError"));
    for (const dispose of disposeHorizontalWheel) dispose();
    canvasInputRouter.dispose();
    runtime?.destroy();
});

animationFrame = requestAnimationFrame(frame);
vscode.postMessage({type: "ready"});
void boot();
