import assert from "node:assert/strict";
import {
    horizontalWheelTarget,
    useVerticalWheelForHorizontalScroll,
    useVerticalWheelScrollChain,
    verticalWheelTarget,
} from "../media/horizontal-wheel.mjs";

function element(overrides = {}) {
    return {
        scrollWidth: 500,
        clientWidth: 200,
        scrollHeight: 30,
        clientHeight: 30,
        scrollLeft: 40,
        ...overrides,
    };
}

assert.equal(horizontalWheelTarget(element(), {deltaX: 0, deltaY: 24, deltaMode: 0}), 64);
assert.equal(horizontalWheelTarget(element(), {deltaX: 0, deltaY: -80, deltaMode: 0}), 0);
assert.equal(horizontalWheelTarget(element({scrollLeft: 280}), {deltaX: 0, deltaY: 80, deltaMode: 0}), 300);
assert.equal(horizontalWheelTarget(element(), {deltaX: 0, deltaY: 2, deltaMode: 1}), 72);
assert.equal(horizontalWheelTarget(element(), {deltaX: 0, deltaY: 1, deltaMode: 2}), 240);

assert.equal(horizontalWheelTarget(element({scrollWidth: 200}), {deltaX: 0, deltaY: 24}), undefined);
assert.equal(horizontalWheelTarget(element({scrollHeight: 80}), {deltaX: 0, deltaY: 24}), undefined);
assert.equal(horizontalWheelTarget(element(), {deltaX: 30, deltaY: 20}), undefined);
assert.equal(horizontalWheelTarget(element(), {deltaX: 0, deltaY: 24, ctrlKey: true}), undefined);
assert.equal(horizontalWheelTarget(element({scrollLeft: 300}), {deltaX: 0, deltaY: 24}), undefined);

assert.equal(verticalWheelTarget(element({scrollHeight: 500, clientHeight: 200, scrollTop: 40}), {
    deltaX: 0,
    deltaY: 24,
    deltaMode: 0,
}), 64);
assert.equal(verticalWheelTarget(element({scrollHeight: 500, clientHeight: 200, scrollTop: 290}), {
    deltaX: 0,
    deltaY: 24,
    deltaMode: 0,
}), 300);
assert.equal(verticalWheelTarget(element({scrollHeight: 200, clientHeight: 200, scrollTop: 0}), {
    deltaX: 0,
    deltaY: 24,
}), undefined);

let listener;
let options;
const rail = element();
rail.addEventListener = (type, value, valueOptions) => {
    assert.equal(type, "wheel");
    listener = value;
    options = valueOptions;
};
rail.removeEventListener = (type, value) => {
    assert.equal(type, "wheel");
    assert.equal(value, listener);
};
const dispose = useVerticalWheelForHorizontalScroll(rail);
assert.deepEqual(options, {passive: false});
let prevented = false;
listener({deltaX: 0, deltaY: 25, preventDefault: () => { prevented = true; }});
assert.equal(rail.scrollLeft, 65);
assert.equal(prevented, true);
dispose();

let verticalListener;
const inspector = element({scrollHeight: 500, clientHeight: 200, scrollTop: 40});
inspector.addEventListener = (_type, value) => { verticalListener = value; };
inspector.removeEventListener = () => {};
let verticalPrevented = false;
useVerticalWheelForHorizontalScroll(inspector);
verticalListener({
    deltaX: 0,
    deltaY: 25,
    deltaMode: 0,
    preventDefault: () => { verticalPrevented = true; },
});
assert.equal(inspector.scrollTop, 65);
assert.equal(inspector.scrollLeft, 40);
assert.equal(verticalPrevented, true);

let gatedListener;
const fitCanvas = element({scrollHeight: 500, clientHeight: 200, scrollTop: 40});
fitCanvas.addEventListener = (_type, value) => { gatedListener = value; };
fitCanvas.removeEventListener = () => {};
let fitPrevented = false;
useVerticalWheelForHorizontalScroll(fitCanvas, () => false);
gatedListener({
    deltaX: 0,
    deltaY: 25,
    deltaMode: 0,
    preventDefault: () => { fitPrevented = true; },
});
assert.equal(fitCanvas.scrollTop, 40);
assert.equal(fitCanvas.scrollLeft, 40);
assert.equal(fitPrevented, false);

let chainListener;
const excerpt = element({scrollHeight: 120, clientHeight: 120, scrollTop: 0});
excerpt.addEventListener = (_type, value, valueOptions) => {
    chainListener = value;
    assert.deepEqual(valueOptions, {passive: false});
};
excerpt.removeEventListener = () => {};
const chainInspector = element({scrollHeight: 500, clientHeight: 200, scrollTop: 40});
let chainPrevented = false;
useVerticalWheelScrollChain(excerpt, chainInspector);
chainListener({
    deltaX: 0,
    deltaY: 25,
    deltaMode: 0,
    preventDefault: () => { chainPrevented = true; },
});
assert.equal(excerpt.scrollTop, 0);
assert.equal(chainInspector.scrollTop, 65);
assert.equal(chainPrevented, true);

excerpt.scrollHeight = 500;
excerpt.clientHeight = 200;
excerpt.scrollTop = 40;
chainInspector.scrollTop = 65;
chainPrevented = false;
chainListener({
    deltaX: 0,
    deltaY: 25,
    deltaMode: 0,
    preventDefault: () => { chainPrevented = true; },
});
assert.equal(excerpt.scrollTop, 40, "native scrolling remains owned by the excerpt");
assert.equal(chainInspector.scrollTop, 65);
assert.equal(chainPrevented, false);

excerpt.scrollTop = 300;
chainListener({
    deltaX: 0,
    deltaY: 25,
    deltaMode: 0,
    preventDefault: () => { chainPrevented = true; },
});
assert.equal(chainInspector.scrollTop, 90, "the inspector resumes scrolling at the excerpt boundary");
assert.equal(chainPrevented, true);

excerpt.scrollTop = 290;
chainInspector.scrollTop = 90;
chainPrevented = false;
chainListener({
    deltaX: 0,
    deltaY: 25,
    deltaMode: 0,
    preventDefault: () => { chainPrevented = true; },
});
assert.equal(excerpt.scrollTop, 300, "the excerpt consumes its remaining downward range");
assert.equal(chainInspector.scrollTop, 105, "the remaining downward delta continues through the inspector");
assert.equal(chainPrevented, true);

console.log("horizontal wheel passed");
