// The preview webview: plays the scene the extension sends on the canvas with the shared player,
// and runs the viewer around it: transport and playback range, speed, camera, the series menu,
// the source inspector and export.
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
import {saveGif, saveMp4, savePng} from "./savers.js";
import {adaptiveVscodeThemeKey, readAdaptiveVscodeTheme} from "./adaptive-theme.mjs";
import {autoAdvanceSeriesTarget, playbackEnd, seriesStepTarget} from "./series-step.mjs";
import {SOURCE_CONTEXT_LINE_COUNTS, previewPreferences} from "./preferences.mjs";
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
import {createRenderHost, fontTable, VisPlayer} from "./player/player.js";

const vscode = acquireVsCodeApi();
const config = window.visPreviewConfig;

function requiredElement(selector) {
    const element = document.querySelector(selector);
    if (!element) throw new Error(`viscript Preview markup is missing required element ${selector}`);
    return element;
}

const canvas = document.querySelector("#preview");
const stageShell = document.querySelector("#stage-shell");
const viewer = document.querySelector(".viewer");
const player = document.querySelector(".player");
const stageResizer = document.querySelector("#stage-resizer");
const wideResizer = document.querySelector("#wide-resizer");
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
const cameraControlButtons = [...document.querySelectorAll("[data-camera-control]")];
const sourceLinesValue = document.querySelector("#source-lines-value");
const sourceLineButtons = [...document.querySelectorAll("[data-source-lines]")];
const errorTitle = document.querySelector("#error-title");
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

const PLAYER_MENU_WIDTH = 174;
// Scenes that use the UI or Input modules take pointer and key input as their timeline plays; on
// any other, the canvas plays and pauses the scene and moves its camera.
const TAKES_INPUT = /\bvis\s*\.\s*(?:ui|input)\b/;

let looping = false;
let {autoPlayNext, forceCompactLayout, sourceContextLineCount, cameraControl} = previewPreferences(vscode.getState(), config);
let busy = false;
let exportController;
let sceneId;
let playbackRange = null;
// The current scene's assets as the extension sent them, base64 by content id, and the list the
// player loads.
let assetData = new Map();
let sceneResources = {assets: []};
let manualStageHeightValue = null;
let stageResizePointerId = null;
let manualWidePlayerWidthValue = null;
let wideResizePointerId = null;
let canvasFullscreen = false;
let viewerLayout = "compact";
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

// Scenes draw on the main thread, where browsers play audio.
const host = createRenderHost({
    mode: "inline",
    server: {url: config.server},
    async wasm() {
        const response = await fetch(config.wasm);
        if (!response.ok) throw new Error(`Bundled WASM failed to load (${response.status})`);
        return response.arrayBuffer();
    },
    // By the names scenes set text in; each is fetched when a scene first needs it.
    fonts: fontTable(config.fonts),
    assets: {
        async bytes(id) {
            const data = assetData.get(id);
            if (data === undefined) throw new Error("The scene's assets changed while it loaded");
            return decodeBase64(data).buffer;
        },
    },
});
const visPlayer = new VisPlayer({
    host,
    canvas,
    resources: () => sceneResources,
    theme: () => {
        const theme = readAdaptiveVscodeTheme();
        activeHostThemeKey = adaptiveVscodeThemeKey(theme);
        return theme;
    },
    end: "stop",
    camera: cameraControl,
    audio: true,
    // The viewer shows one scene, and keeps it while the panel is hidden.
    releaseDelay: Infinity,
    label: "viscript scene",
});

function loadScene(scene) {
    documentName.textContent = scene.displayName || scene.fileName;
    documentName.title = scene.displayName || scene.fileName;
    const sameScene = scene.sceneId === sceneId;
    syncSourceInspector(scene.sourceContexts, scene.activeSourceIndex, {preserveSelection: sameScene});
    syncSeries(scene.series);
    descriptionContent.innerHTML = markdownToHtml(scene.description);
    exportController?.abort(new DOMException("Scene changed", "AbortError"));
    // The scene before keeps playing behind the error.
    if (scene.assetErrors.length) {
        showError(scene.assetErrors.join("\n"), "LUA ERROR", "SCENE REJECTED");
        return;
    }
    assetData = new Map(scene.assets.map((asset) => [asset.id, asset.base64]));
    sceneResources = {assets: scene.assets.map(({id, name, mime}) => ({id, name, mime}))};
    setStatus("COMPILING LUA", "loading");
    // The same document keeps its time, range and whether it plays; another starts over.
    void visPlayer.load({
        source: scene.source,
        name: scene.fileName,
        interactive: TAKES_INPUT.test(scene.source) ? "timeline" : false,
    });
    if (sameScene) return;
    sceneId = scene.sceneId;
    playbackRange = null;
    visPlayer.clearRange();
    visPlayer.seek(0);
    visPlayer.play();
}

visPlayer.on("load", () => {
    const {width, height, duration, loop} = visPlayer.info;
    errorCard.hidden = true;
    empty.hidden = true;
    delete visPlayer.canvas.dataset.failed;
    document.documentElement.style.setProperty("--scene-aspect-ratio", `${width} / ${height}`);
    document.documentElement.style.setProperty("--scene-ratio", String(width / height));
    syncViewerLayout();
    syncNormalLayoutLimit();
    looping = loop;
    syncEnd();
    playbackRange = clampPlaybackRange(playbackRange, duration);
    applyPlaybackRange();
    showTime(visPlayer.time, duration);
    syncControls();
    setStatus("PREVIEW READY", "ready");
    vscode.postMessage({type: "renderSuccess"});
});

const ERROR_LABELS = {
    load: ["LUA ERROR", "SCENE REJECTED"],
    resource: ["ASSET ERROR", "SCENE REJECTED"],
    engine: ["RUNTIME ERROR", "SCENE REJECTED"],
    timeout: ["TIMEOUT", "SCENE REJECTED"],
    render: ["RENDER ERROR", "RENDER INTERRUPTED"],
    runtime: ["RUNTIME ERROR", "RENDER INTERRUPTED"],
};
visPlayer.on("error", ({detail}) => showError(detail.message, ...(ERROR_LABELS[detail.phase] ?? ERROR_LABELS.render)));
visPlayer.on("frame", ({detail}) => showTime(detail.time, detail.duration));
for (const type of ["play", "pause", "stop", "statechange"]) visPlayer.on(type, syncControls);
visPlayer.on("complete", () => {
    const next = autoAdvanceSeriesTarget(currentSeries, autoPlayNext);
    if (next) openSeriesItem(next.groupIndex, next.itemIndex);
    else if (looping && playbackRange) visPlayer.playRange(playbackRange.start, playbackRange.end);
    syncControls();
});
visPlayer.on("camera", ({detail}) => {
    syncCamera();
    setStatus(`${detail.view.toUpperCase()} CAMERA`, "ready");
});

// With auto-play next on and an item to go to, the scene plays once and opens it; otherwise the
// loop button decides.
function syncEnd() {
    visPlayer.configure({end: playbackEnd({series: currentSeries, autoPlayNext, looping})});
}

function scheduleAdaptiveThemeRefresh() {
    cancelAnimationFrame(themeRefreshFrame);
    themeRefreshFrame = requestAnimationFrame(() => {
        themeRefreshFrame = requestAnimationFrame(() => {
            themeRefreshFrame = 0;
            if (currentSourceContexts.length) refreshOpenSourceHighlights();
            if (!visPlayer.info?.adaptiveTheme) return;
            if (adaptiveVscodeThemeKey(readAdaptiveVscodeTheme()) === activeHostThemeKey) return;
            // The scene loads again in the new colors, keeping its time and whether it plays.
            exportController?.abort(new DOMException("Theme changed", "AbortError"));
            void visPlayer.reload();
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
    syncEnd();
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
    // A copy, since collapsing a context replaces it here and the message keeps its own.
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
    errorTitle.textContent = title;
    errorMessage.textContent = message;
    errorCard.hidden = false;
    empty.hidden = true;
    visPlayer.canvas.dataset.failed = "true";
    setStatus(label, "error");
    syncControls();
    vscode.postMessage({type: "renderError", message});
}

function showTime(time, duration) {
    scrub.max = String(duration);
    scrub.value = String(time);
    const progress = duration > 0 ? (time / duration) * 100 : 0;
    scrub.style.setProperty("--progress", `${Math.max(0, Math.min(100, progress))}%`);
    // A retained runtime without a timeline runs on its own clock.
    timeOutput.textContent = visPlayer.live && duration <= 0 ? "LIVE" : `${time.toFixed(2)} / ${duration.toFixed(2)}`;
    syncPlaybackRange();
}

function syncPlaybackRange() {
    const duration = visPlayer.duration;
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
    const loaded = Boolean(visPlayer.info);
    rangeInButton.disabled = busy || !loaded || visPlayer.time >= duration;
    rangeOutButton.disabled = busy || !loaded || visPlayer.time <= 0;
    rangeClearButton.disabled = busy || !loaded || !playbackRange;
}

// The player plays a range from its start; the viewer keeps the time and whether it plays.
function applyPlaybackRange() {
    if (!playbackRange) {
        visPlayer.clearRange();
        return;
    }
    const {time, playing} = visPlayer;
    visPlayer.playRange(playbackRange.start, playbackRange.end);
    visPlayer.seek(time);
    if (!playing) visPlayer.pause();
}

function updatePlaybackRange(range) {
    playbackRange = range;
    applyPlaybackRange();
    syncControls();
    setStatus(
        playbackRange ? `RANGE ${playbackRange.start.toFixed(2)}–${playbackRange.end.toFixed(2)}` : "FULL RANGE",
        "ready",
    );
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
}

function syncNormalLayoutLimit() {
    if (canvasFullscreen || viewerLayout === "wide" || manualStageHeightValue !== null) return;
    const {width: sceneWidth, height: sceneHeight} = visPlayer.info ?? {width: canvas.width, height: canvas.height};
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
}

function syncControls() {
    const playing = visPlayer.playing;
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
    previewResetButton.disabled = !visPlayer.info || busy;
    // An export draws its own frames; the canvas takes no input meanwhile.
    visPlayer.canvas.inert = busy;
    syncPlaybackRange();
    syncCamera();
}

function syncCamera() {
    const info = visPlayer.info;
    for (const button of cameraControlButtons) {
        const current = (button.dataset.cameraControl === "true") === cameraControl;
        button.setAttribute("aria-current", String(current));
        button.disabled = current;
    }
    if (!info) return;
    const interactive = info.cameraMode === "interactive";
    const view = visPlayer.cameraView;
    // What a pointer on the canvas does: the scene's own input, the camera, or play and pause.
    visPlayer.canvas.dataset.input = visPlayer.live ? "scene"
        : interactive && cameraControl ? "camera"
        : info.duration > 0 ? "playback" : "none";
    cameraMode.textContent = `${interactive ? "" : "FIXED · "}${view.toUpperCase()}`;
    for (const button of cameraButtons) {
        const action = button.dataset.camera;
        const current = action !== "reset" && action === `view${view}`;
        button.setAttribute("aria-current", String(current));
        button.disabled = !interactive || busy || current;
    }
}

notesItem.addEventListener("toggle", () => {
    notesOpen = notesItem.open;
});

playButton.addEventListener("click", () => {
    if (!visPlayer.info) return;
    if (visPlayer.playing) {
        visPlayer.pause();
        return;
    }
    const {start, end} = playbackBounds(playbackRange, visPlayer.duration);
    if (visPlayer.time < start || visPlayer.time >= end) visPlayer.seek(start);
    visPlayer.play();
});

restartButton.addEventListener("click", () => {
    visPlayer.seek(playbackBounds(playbackRange, visPlayer.duration).start);
});

loopButton.addEventListener("click", () => {
    looping = !looping;
    syncEnd();
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

scrub.addEventListener("input", () => visPlayer.seek(Number(scrub.value)));

rangeInButton.addEventListener("click", () => {
    if (!visPlayer.info) return;
    updatePlaybackRange(setPlaybackStart(playbackRange, visPlayer.time, visPlayer.duration));
    closePlayerMenu();
});

rangeOutButton.addEventListener("click", () => {
    if (!visPlayer.info) return;
    updatePlaybackRange(setPlaybackEnd(playbackRange, visPlayer.time, visPlayer.duration));
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

// Exports draw their frames apart from the canvas, while playback holds.
async function exportMedia(format, fps = 30) {
    const info = visPlayer.info;
    if (!info || busy || !["png", "gif", "mp4"].includes(format)) return;
    const exportRange = playbackRange ? {...playbackRange} : null;
    const resume = visPlayer.playing;
    const controller = new AbortController();
    busy = true;
    visPlayer.pause();
    exportController = controller;
    syncControls();
    const scene = {
        size: () => [info.width, info.height],
        duration: info.duration,
        time: visPlayer.time,
        capture: (time, ratio) => visPlayer.capture({time, ratio}),
    };
    try {
        let bytes;
        let mime;
        if (format === "png") {
            bytes = await savePng(scene, window.devicePixelRatio || 1);
            mime = "image/png";
            setStatus("PNG READY", "ready");
        } else {
            if (format === "mp4" && !("VideoEncoder" in window)) {
                throw new Error("MP4 export requires WebCodecs support in this VS Code build");
            }
            const save = format === "gif" ? saveGif : saveMp4;
            mime = format === "gif" ? "image/gif" : "video/mp4";
            bytes = await save(
                scene,
                fps,
                (done, total) => {
                    if (!controller.signal.aborted) setStatus(`${format.toUpperCase()} ${done}/${total}`, "loading");
                },
                controller.signal,
                exportRange,
            );
            setStatus(`${format.toUpperCase()} READY`, "ready");
        }
        if (controller.signal.aborted) return;
        vscode.postMessage({type: "saveMedia", format, dataUrl: await dataUrl(bytes, mime)});
    } catch (error) {
        // A scene that changed or loaded again ends the export without an error.
        if (controller.signal.aborted || error?.name === "AbortError") return;
        setStatus(`${format.toUpperCase()} ERROR`, "error");
        vscode.postMessage({type: "exportError", message: describeError(error)});
    } finally {
        if (exportController === controller) {
            busy = false;
            exportController = undefined;
            if (resume) visPlayer.play();
            syncControls();
        }
    }
}

for (const button of speedButtons) {
    button.addEventListener("click", () => {
        visPlayer.configure({rate: Number(button.dataset.speed)});
        speedValue.textContent = button.textContent;
        for (const candidate of speedButtons) {
            candidate.setAttribute("aria-current", String(candidate === button));
            candidate.disabled = candidate === button;
        }
        closePlayerMenu();
    });
}

for (const button of cameraControlButtons) {
    button.addEventListener("click", () => {
        cameraControl = button.dataset.cameraControl === "true";
        const state = vscode.getState();
        vscode.setState({
            ...(state && typeof state === "object" ? state : {}),
            cameraControl,
        });
        visPlayer.configure({camera: cameraControl});
        syncCamera();
        closePlayerMenu();
    });
}
syncCamera();

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
        syncEnd();
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
        if (!visPlayer.moveCamera(button.dataset.camera)) setStatus("FIXED CAMERA", "ready");
        closePlayerMenu();
    });
}

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

canvas.addEventListener("contextmenu", (event) => {
    event.preventDefault();
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
});

const disposeHorizontalWheel = useVerticalWheelForHorizontalScroll(seriesGroupItems);

document.addEventListener("pointerdown", (event) => {
    if (playerMenu.open && !playerMenu.contains(event.target)) playerMenu.open = false;
    if (seriesMenu.open && !seriesMenu.contains(event.target)) seriesMenu.open = false;
});

window.addEventListener("message", (event) => {
    if (event.data?.type === "scene") loadScene(event.data);
    if (event.data?.type === "series") syncSeries(event.data.series);
    if (event.data?.type === "sourceContexts") {
        syncSourceInspector(event.data.sourceContexts, event.data.activeSourceIndex, {preserveSelection: true});
    }
    if (event.data?.type === "export") void exportMedia(event.data.format, Number(event.data.fps) || 30);
});

window.addEventListener("beforeunload", () => {
    cancelAnimationFrame(themeRefreshFrame);
    vscodeThemeObserver.disconnect();
    exportController?.abort(new DOMException("Preview closed", "AbortError"));
    disposeHorizontalWheel();
    visPlayer.destroy();
});

syncControls();
vscode.postMessage({type: "ready"});
