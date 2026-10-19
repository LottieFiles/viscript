import assert from "node:assert/strict";
import {previewPreferences} from "../media/preferences.mjs";

const stored = {autoPlayNext: true, forceCompactLayout: true, sourceContextLineCount: 20};

// A new webview starts from the extension's preferences, with stats off.
assert.deepEqual(previewPreferences(undefined, stored), {
    autoPlayNext: true,
    forceCompactLayout: true,
    sourceContextLineCount: 20,
    showStats: false,
});
assert.deepEqual(previewPreferences(undefined, {}), {
    autoPlayNext: false,
    forceCompactLayout: false,
    sourceContextLineCount: 5,
    showStats: false,
});

// Its own saved state wins, even where it turned a preference off.
assert.deepEqual(previewPreferences({autoPlayNext: false, forceCompactLayout: false, sourceContextLineCount: 10, showStats: true}, stored), {
    autoPlayNext: false,
    forceCompactLayout: false,
    sourceContextLineCount: 10,
    showStats: true,
});

// Line counts other than 5, 10 and 20 show 5 lines.
assert.equal(previewPreferences({sourceContextLineCount: 7}, stored).sourceContextLineCount, 5);
assert.equal(previewPreferences(undefined, {sourceContextLineCount: 12}).sourceContextLineCount, 5);

console.log("preview preferences passed");
