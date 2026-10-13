export const DEFAULT_NORMAL_STAGE_HEIGHT = 260;
export const MIN_MANUAL_STAGE_HEIGHT = 64;
export const WIDE_LAYOUT_HYSTERESIS = 40;
export const MIN_WIDE_PLAYER_WIDTH = 280;
export const MIN_WIDE_INSPECTOR_WIDTH = 280;
export const MAX_WIDE_INSPECTOR_WIDTH = 380;
export const WIDE_INSPECTOR_FRACTION = 0.32;
export const WIDE_RESIZER_WIDTH = 7;
export const WIDE_LAYOUT_AUTHORING_REFERENCE_WIDTH = 960;
export const WIDE_LAYOUT_REFERENCE_WIDTH = Math.floor(
    WIDE_LAYOUT_AUTHORING_REFERENCE_WIDTH * 0.7 / 100,
) * 100;
export const WIDE_LAYOUT_MIN_WIDTH = MIN_WIDE_PLAYER_WIDTH + WIDE_RESIZER_WIDTH + MIN_WIDE_INSPECTOR_WIDTH;
export const WIDE_LAYOUT_EXIT_WIDTH = Math.max(
    WIDE_LAYOUT_REFERENCE_WIDTH - WIDE_LAYOUT_HYSTERESIS,
    WIDE_LAYOUT_MIN_WIDTH,
);

export function responsiveViewerLayout(viewportWidth, current = "compact", forceCompact = false) {
    if (forceCompact) return "compact";
    if (!Number.isFinite(viewportWidth) || viewportWidth < 0) return "compact";
    if (current === "wide") {
        return viewportWidth < WIDE_LAYOUT_EXIT_WIDTH ? "compact" : "wide";
    }
    return viewportWidth >= WIDE_LAYOUT_REFERENCE_WIDTH ? "wide" : "compact";
}

export function widePlayerWidth(
    desiredWidth,
    viewportWidth,
    resizerWidth = WIDE_RESIZER_WIDTH,
    inspectorWidth = MIN_WIDE_INSPECTOR_WIDTH,
) {
    const values = [desiredWidth, viewportWidth, resizerWidth, inspectorWidth];
    if (!values.every(Number.isFinite) || viewportWidth <= 0 || values.slice(2).some((value) => value < 0)) {
        return MIN_WIDE_PLAYER_WIDTH;
    }
    const available = Math.max(0, viewportWidth - resizerWidth);
    const maximum = Math.min(available, Math.max(MIN_WIDE_PLAYER_WIDTH, available - inspectorWidth));
    const minimum = Math.min(MIN_WIDE_PLAYER_WIDTH, maximum);
    return Math.round(Math.max(minimum, Math.min(desiredWidth, maximum)));
}

export function automaticWidePlayerWidth(
    viewportWidth,
    resizerWidth = WIDE_RESIZER_WIDTH,
) {
    if (!Number.isFinite(viewportWidth) || viewportWidth <= 0 || !Number.isFinite(resizerWidth) || resizerWidth < 0) {
        return MIN_WIDE_PLAYER_WIDTH;
    }
    const inspectorWidth = Math.max(
        MIN_WIDE_INSPECTOR_WIDTH,
        Math.min(viewportWidth * WIDE_INSPECTOR_FRACTION, MAX_WIDE_INSPECTOR_WIDTH),
    );
    return widePlayerWidth(viewportWidth, viewportWidth, resizerWidth, inspectorWidth);
}

export function floatingMenuPlacement(viewportWidth, viewportHeight, trigger, gap = 7) {
    const spaceAbove = Math.max(0, trigger.top - gap);
    const spaceBelow = Math.max(0, viewportHeight - trigger.bottom - gap);
    const opensAbove = spaceAbove >= spaceBelow;
    return {
        top: opensAbove ? null : trigger.bottom + gap,
        bottom: opensAbove ? viewportHeight - trigger.top + gap : null,
        right: Math.max(0, viewportWidth - trigger.right),
        maxHeight: Math.floor(opensAbove ? spaceAbove : spaceBelow),
    };
}

export function floatingPointMenuPlacement(viewportWidth, viewportHeight, x, y, panelWidth, gap = 7, edge = 8) {
    const width = Math.min(Math.max(0, panelWidth), Math.max(0, viewportWidth - edge * 2));
    const preferredLeft = viewportWidth - x - gap >= width ? x + gap : x - gap - width;
    const left = Math.max(edge, Math.min(preferredLeft, viewportWidth - edge - width));
    return {
        ...floatingMenuPlacement(viewportWidth, viewportHeight, {top: y, right: x, bottom: y}, gap),
        left: Math.floor(left),
    };
}

export function cascadingMenuPlacement(
    viewportWidth,
    viewportHeight,
    trigger,
    panelWidth,
    panelHeight,
    gap = 2,
    edge = 4,
) {
    const availableWidth = Math.max(0, viewportWidth - edge * 2);
    const width = Math.min(Math.max(0, panelWidth), availableWidth);
    const opensRight = viewportWidth - trigger.right - gap >= width;
    const preferredLeft = opensRight ? trigger.right + gap : trigger.left - gap - width;
    const left = Math.max(edge, Math.min(preferredLeft, viewportWidth - edge - width));
    const availableHeight = Math.max(0, viewportHeight - edge * 2);
    const height = Math.min(Math.max(0, panelHeight), availableHeight);
    const top = Math.max(edge, Math.min(trigger.top, viewportHeight - edge - height));
    return {left: Math.floor(left), top: Math.floor(top), maxHeight: Math.floor(availableHeight)};
}

export function normalStageMaxHeight(
    viewportHeight,
    transportHeight,
    resizerHeight = 7,
    viewportWidth = 0,
    sceneWidth = 0,
    sceneHeight = 0,
) {
    const values = [viewportHeight, transportHeight, resizerHeight];
    if (!values.every(Number.isFinite) || viewportHeight <= 0 || values.slice(1).some((value) => value < 0)) {
        return DEFAULT_NORMAL_STAGE_HEIGHT;
    }
    const availableHeight = Math.max(0, viewportHeight - transportHeight - resizerHeight);
    const widthValues = [viewportWidth, sceneWidth, sceneHeight];
    if (!widthValues.every(Number.isFinite) || widthValues.some((value) => value <= 0)) {
        return Math.floor(availableHeight);
    }
    const widthAwareHeight = viewportWidth * sceneHeight / sceneWidth;
    return Math.floor(Math.min(availableHeight, widthAwareHeight));
}

export function manualStageHeight(
    desiredHeight,
    viewportHeight,
    transportHeight,
    resizerHeight = 7,
) {
    const values = [desiredHeight, viewportHeight, transportHeight, resizerHeight];
    if (!values.every(Number.isFinite) || viewportHeight <= 0 || values.slice(2).some((value) => value < 0)) {
        return DEFAULT_NORMAL_STAGE_HEIGHT;
    }
    const maximum = Math.max(0, viewportHeight - transportHeight - resizerHeight);
    const minimum = Math.min(MIN_MANUAL_STAGE_HEIGHT, maximum);
    return Math.round(Math.max(minimum, Math.min(desiredHeight, maximum)));
}
