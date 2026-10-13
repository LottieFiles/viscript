export const MIN_PREVIEW_SCALE = 0.001;
export const MAX_PREVIEW_SCALE = 8;
export const MAX_RENDER_PIXEL_RATIO = 8;
export const MAX_RENDER_PIXELS = 16_777_216;
export const MAX_RENDER_DIMENSION = 16_384;

export function clampPreviewScale(value) {
    if (!Number.isFinite(value)) return 1;
    return Math.max(MIN_PREVIEW_SCALE, Math.min(MAX_PREVIEW_SCALE, value));
}

export function fitPreviewScale(sceneWidth, sceneHeight, viewportWidth, viewportHeight, padding = 0) {
    if (![sceneWidth, sceneHeight, viewportWidth, viewportHeight, padding].every(Number.isFinite)) return 1;
    if (sceneWidth <= 0 || sceneHeight <= 0 || viewportWidth <= 0 || viewportHeight <= 0) return 1;
    const availableWidth = Math.max(1, viewportWidth - padding);
    const availableHeight = Math.max(1, viewportHeight - padding);
    return clampPreviewScale(Math.min(availableWidth / sceneWidth, availableHeight / sceneHeight));
}

export function stepPreviewScale(scale, direction) {
    if (!Number.isFinite(direction) || direction === 0) return clampPreviewScale(scale);
    return clampPreviewScale(scale * (direction > 0 ? 1.1 : 1 / 1.1));
}

export function wheelPreviewScale(scale, deltaY, minimumScale = MIN_PREVIEW_SCALE) {
    const minimum = clampPreviewScale(minimumScale);
    if (!Number.isFinite(deltaY) || deltaY === 0) {
        return Math.max(minimum, clampPreviewScale(scale));
    }
    const boundedDelta = Math.max(-100, Math.min(100, deltaY));
    return Math.max(minimum, clampPreviewScale(scale * Math.exp(-boundedDelta * 0.0015)));
}

export function previewRenderPixelRatio(
    sceneWidth,
    sceneHeight,
    previewScale,
    devicePixelRatio = 1,
    maxPixels = MAX_RENDER_PIXELS,
) {
    const values = [sceneWidth, sceneHeight, previewScale, devicePixelRatio, maxPixels];
    if (!values.every(Number.isFinite) || values.some((value) => value <= 0)) return 1;
    const area = sceneWidth * sceneHeight;
    if (!Number.isFinite(area) || area <= 0) return 1;
    const desired = Math.min(
        MAX_RENDER_PIXEL_RATIO,
        Math.max(1, Math.ceil(previewScale * devicePixelRatio - 1e-6)),
    );
    const areaLimit = area > maxPixels ? 1 : Math.max(1, Math.floor(Math.sqrt(maxPixels / area)));
    const dimensionLimit = Math.max(
        1,
        Math.min(
            Math.floor(MAX_RENDER_DIMENSION / sceneWidth),
            Math.floor(MAX_RENDER_DIMENSION / sceneHeight),
        ),
    );
    return Math.max(1, Math.min(desired, areaLimit, dimensionLimit));
}

export function previewAnchorTranslation(before, after, clientX, clientY) {
    const values = [
        before?.left,
        before?.top,
        before?.width,
        before?.height,
        after?.left,
        after?.top,
        after?.width,
        after?.height,
        clientX,
        clientY,
    ];
    if (!values.every(Number.isFinite)
        || before.width <= 0
        || before.height <= 0
        || after.width <= 0
        || after.height <= 0) return {x: 0, y: 0};
    const anchorX = Math.max(0, Math.min(1, (clientX - before.left) / before.width));
    const anchorY = Math.max(0, Math.min(1, (clientY - before.top) / before.height));
    return {
        x: clientX - (after.left + anchorX * after.width),
        y: clientY - (after.top + anchorY * after.height),
    };
}
