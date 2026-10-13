const LINE_PIXELS = 16;

function wheelPixels(value, mode, pageSize) {
    if (!Number.isFinite(value)) return 0;
    if (mode === 1) return value * LINE_PIXELS;
    if (mode === 2) return value * pageSize;
    return value;
}

export function horizontalWheelTarget(element, event) {
    if (event.defaultPrevented || event.ctrlKey || event.metaKey || event.altKey || event.shiftKey) return undefined;
    if (element.scrollWidth <= element.clientWidth + 1) return undefined;
    if (element.scrollHeight > element.clientHeight + 1) return undefined;

    const deltaX = wheelPixels(event.deltaX, event.deltaMode, element.clientWidth);
    const deltaY = wheelPixels(event.deltaY, event.deltaMode, element.clientWidth);
    if (!deltaY || Math.abs(deltaY) <= Math.abs(deltaX)) return undefined;

    const maximum = Math.max(0, element.scrollWidth - element.clientWidth);
    const current = Math.max(0, Math.min(maximum, element.scrollLeft));
    const target = Math.max(0, Math.min(maximum, current + deltaY));
    return target === current ? undefined : target;
}

export function verticalWheelTarget(element, event) {
    if (event.defaultPrevented || event.ctrlKey || event.metaKey || event.altKey || event.shiftKey) return undefined;
    if (element.scrollHeight <= element.clientHeight + 1) return undefined;

    const deltaX = wheelPixels(event.deltaX, event.deltaMode, element.clientHeight);
    const deltaY = wheelPixels(event.deltaY, event.deltaMode, element.clientHeight);
    if (!deltaY || Math.abs(deltaY) <= Math.abs(deltaX)) return undefined;

    return verticalScrollTarget(element, deltaY);
}

function verticalScrollTarget(element, deltaY) {
    const maximum = Math.max(0, element.scrollHeight - element.clientHeight);
    const current = Math.max(0, Math.min(maximum, element.scrollTop));
    const target = Math.max(0, Math.min(maximum, current + deltaY));
    return target === current ? undefined : target;
}

export function useVerticalWheelForHorizontalScroll(element, canHandle = () => true) {
    const onWheel = (event) => {
        if (!canHandle(event)) return;
        const top = verticalWheelTarget(element, event);
        if (top !== undefined) {
            element.scrollTop = top;
            event.preventDefault();
            return;
        }
        const target = horizontalWheelTarget(element, event);
        if (target === undefined) return;
        element.scrollLeft = target;
        event.preventDefault();
    };
    element.addEventListener("wheel", onWheel, {passive: false});
    return () => element.removeEventListener("wheel", onWheel);
}

export function useVerticalWheelScrollChain(element, ancestor) {
    const onWheel = (event) => {
        const elementTop = verticalWheelTarget(element, event);
        if (elementTop !== undefined) {
            const current = Math.max(0, Math.min(element.scrollHeight - element.clientHeight, element.scrollTop));
            const deltaY = wheelPixels(event.deltaY, event.deltaMode, element.clientHeight);
            const remaining = deltaY - (elementTop - current);
            if (Math.abs(remaining) < 0.01) return;
            const ancestorTop = verticalScrollTarget(ancestor, remaining);
            if (ancestorTop === undefined) return;
            element.scrollTop = elementTop;
            ancestor.scrollTop = ancestorTop;
            event.preventDefault();
            return;
        }
        const top = verticalWheelTarget(ancestor, event);
        if (top === undefined) return;
        ancestor.scrollTop = top;
        event.preventDefault();
    };
    element.addEventListener("wheel", onWheel, {passive: false});
    return () => element.removeEventListener("wheel", onWheel);
}
