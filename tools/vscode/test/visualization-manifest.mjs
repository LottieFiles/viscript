import assert from "node:assert/strict";
import {
    matchingVisualizationAnimation,
    matchingVisualizationLocations,
    matchingVisualizationUsage,
    normalizeWorkspacePath,
    parseVisualizationManifest,
    visualizationSeriesGroups,
} from "../out/visualizationManifest.js";


const parsed = parseVisualizationManifest({
    version: 2,
    visualizations: [{
        source: "src/renderer/tvg/tvggSwFill.cpp",
        symbol: "tvg::SwFill::function",
        animation: ".vscode/vis/software-fill/animations/tvggSwFill.function.lua",
        series: "software-fill",
        title: "SwFill function",
        description: "# SwFill\n\nExplains the raster operation.",
        links: [{
            label: "SwFill declaration",
            source: "src/renderer/tvg/tvggSwFill.h",
            line: 42,
            range: {startLine: 40, endLine: 48},
        }],
    }],
});
assert.deepEqual(parsed.errors, []);
assert.equal(parsed.entries[0].links[0].line, 42);
assert.deepEqual(parsed.entries[0].links[0].range, {startLine: 40, endLine: 48});
assert.equal(parsed.entries[0].series, "software-fill");
assert.match(parsed.entries[0].description, /raster operation/);
assert.equal(normalizeWorkspacePath("./src\\renderer\\file.cpp"), "src/renderer/file.cpp");
assert.equal(normalizeWorkspacePath("../outside.lua"), undefined);
assert.equal(
    matchingVisualizationAnimation(parsed.entries, ".vscode/vis/software-fill/animations/tvggSwFill.function.lua"),
    parsed.entries[0],
);
assert.equal(matchingVisualizationAnimation(parsed.entries, "animations/unmapped.lua"), undefined);

const primaryLocations = matchingVisualizationLocations(parsed.entries, "src/renderer/tvg/tvggSwFill.cpp");
assert.equal(primaryLocations.length, 1);
assert.equal(primaryLocations[0].relatedLabel, undefined);
const relatedLocations = matchingVisualizationLocations(parsed.entries, "src/renderer/tvg/tvggSwFill.h");
assert.equal(relatedLocations.length, 1);
assert.equal(relatedLocations[0].relatedLabel, "SwFill declaration");
assert.equal(relatedLocations[0].line, 42);
assert.deepEqual(relatedLocations[0].range, {startLine: 40, endLine: 48});
assert.equal(
    matchingVisualizationUsage(parsed.entries, "function", ["src/renderer/tvg/tvggSwFill.cpp"]),
    parsed.entries[0],
);
assert.equal(
    matchingVisualizationUsage(parsed.entries, "function", ["src/renderer/tvg/tvggSwFill.h"]),
    parsed.entries[0],
);
assert.equal(
    matchingVisualizationUsage(parsed.entries, "function", ["src/renderer/tvg/tvggSwFillDecl.h"]),
    parsed.entries[0],
);
assert.equal(matchingVisualizationUsage(parsed.entries, "function", []), parsed.entries[0]);
assert.equal(matchingVisualizationUsage(parsed.entries, "unrelated", []), undefined);

const rangedLinks = parseVisualizationManifest({
    version: 2,
    visualizations: [{
        source: "src/source.cpp",
        symbol: "sourceFunction",
        animation: ".vscode/vis/test-series/animations/source.lua",
        series: "test-series",
        links: [
            {
                label: "Single-line expression",
                source: "src/expression.cpp",
                range: {startLine: 12, endLine: 12, startColumn: 5, endColumn: 18},
            },
            {
                label: "Multi-line expression",
                source: "src/expression.cpp",
                range: {startLine: 20, endLine: 24, startColumn: 9, endColumn: 3},
            },
        ],
    }],
});
assert.deepEqual(rangedLinks.errors, []);
assert.deepEqual(rangedLinks.entries[0].links.map((link) => link.range), [
    {startLine: 12, endLine: 12, startColumn: 5, endColumn: 18},
    {startLine: 20, endLine: 24, startColumn: 9, endColumn: 3},
]);
assert.equal(matchingVisualizationLocations(rangedLinks.entries, "src/expression.cpp").length, 2);

const baseCommit = "1111111111111111111111111111111111111111";
const headCommit = "ABCDEFABCDEFABCDEFABCDEFABCDEFABCDEFABCD";
const diffLinks = parseVisualizationManifest({
    version: 2,
    visualizations: [{
        source: "src/source.cpp",
        symbol: "sourceFunction",
        animation: ".vscode/vis/test-series/animations/source.lua",
        series: "test-series",
        links: [{
            label: "Renamed dispatch",
            source: "src/new/dispatch.cpp",
            range: {startLine: 20, endLine: 32},
            diff: {
                base: baseCommit,
                head: headCommit,
                oldPath: "src/old/dispatch.cpp",
            },
        }],
    }],
});
assert.deepEqual(diffLinks.errors, []);
assert.deepEqual(diffLinks.entries[0].links[0].diff, {
    base: baseCommit,
    head: headCommit.toLowerCase(),
    oldPath: "src/old/dispatch.cpp",
});

for (const [diff, range, expectedError] of [
    [{base: "HEAD^", head: headCommit}, {startLine: 1, endLine: 2}, "full 40-character"],
    [{base: baseCommit, head: "main"}, {startLine: 1, endLine: 2}, "full 40-character"],
    [{base: baseCommit, head: headCommit, oldPath: "../old.cpp"}, {startLine: 1, endLine: 2}, "inside the workspace"],
    [{base: baseCommit, head: headCommit}, undefined, "range is required"],
]) {
    const result = parseVisualizationManifest({
        version: 2,
        visualizations: [{
            source: "src/source.cpp",
            symbol: "sourceFunction",
            animation: ".vscode/vis/test-series/animations/source.lua",
            series: "test-series",
            links: [{label: "Invalid diff", source: "src/related.cpp", range, diff}],
        }],
    });
    assert.equal(result.entries[0].links.length, 0);
    assert.ok(result.errors.some((error) => error.includes(expectedError)), JSON.stringify(result.errors));
}

for (const [range, expectedError] of [
    ["12-18", "must be an object"],
    [{startLine: 0, endLine: 3}, "startLine"],
    [{startLine: 8, endLine: 7}, "must not be before"],
    [{startLine: 8, endLine: 8, startColumn: 2}, "must be provided together"],
    [{startLine: 8, endLine: 8, endColumn: 5}, "must be provided together"],
    [{startLine: 8, endLine: 8, startColumn: 5, endColumn: 5}, "must be greater"],
]) {
    const result = parseVisualizationManifest({
        version: 2,
        visualizations: [{
            source: "src/source.cpp",
            symbol: "sourceFunction",
            animation: ".vscode/vis/test-series/animations/source.lua",
            series: "test-series",
            links: [{label: "Invalid range", source: "src/related.cpp", range}],
        }],
    });
    assert.equal(result.entries.length, 1);
    assert.equal(result.entries[0].links.length, 0);
    assert.ok(result.errors.some((error) => error.includes(expectedError)), JSON.stringify(result.errors));
}

const ambiguous = parseVisualizationManifest({
    version: 2,
    visualizations: [
        parsed.entries[0],
        {
            ...parsed.entries[0],
            source: "src/other/SwFill.cpp",
            animation: ".vscode/vis/software-fill/animations/other.lua",
        },
    ],
});
assert.equal(matchingVisualizationUsage(ambiguous.entries, "function", []), undefined);
assert.equal(matchingVisualizationUsage(ambiguous.entries, "function", ["src/unlisted/SwFill.h"]), undefined);
assert.equal(
    matchingVisualizationUsage(ambiguous.entries, "function", ["src/other/SwFill.cpp"]),
    ambiguous.entries[1],
);
const ambiguousAnimation = [ambiguous.entries[0], {...ambiguous.entries[1], animation: ambiguous.entries[0].animation}];
assert.equal(
    matchingVisualizationAnimation(ambiguousAnimation, ambiguous.entries[0].animation),
    undefined,
);

const sharedRelatedSource = parseVisualizationManifest({
    version: 2,
    visualizations: [
        {
            ...parsed.entries[0],
            animation: ".vscode/vis/software-fill/animations/first.lua",
            links: [{label: "Shared step", source: "src/shared.cpp", symbol: "sharedStep"}],
        },
        {
            ...parsed.entries[0],
            animation: ".vscode/vis/software-fill/animations/second.lua",
            links: [{label: "Shared step", source: "src/shared.cpp", symbol: "sharedStep"}],
        },
    ],
});
const sharedLocations = matchingVisualizationLocations(sharedRelatedSource.entries, "src/shared.cpp");
assert.equal(sharedLocations.length, 2);

const series = parseVisualizationManifest({
    version: 2,
    visualizations: [
        parsed.entries[0],
        {
            ...parsed.entries[0],
            source: "src/renderer/tvg/tvggRaster.cpp",
            animation: ".vscode/vis/software-fill/animations/raster.lua",
        },
        {
            ...parsed.entries[0],
            source: "src/other.cpp",
            animation: ".vscode/vis/other/animations/other.lua",
            series: "other",
        },
        {
            ...parsed.entries[0],
            source: "src/standalone.cpp",
            animation: ".vscode/vis/standalone/animations/standalone.lua",
            series: "standalone",
        },
    ],
});
assert.deepEqual(
    visualizationSeriesGroups(series.entries).map((group) => ({
        label: group.label,
        animations: group.entries.map((entry) => entry.animation),
    })),
    [
        {label: "software-fill", animations: [
            ".vscode/vis/software-fill/animations/tvggSwFill.function.lua",
            ".vscode/vis/software-fill/animations/raster.lua",
        ]},
        {label: "other", animations: [".vscode/vis/other/animations/other.lua"]},
        {label: "standalone", animations: [".vscode/vis/standalone/animations/standalone.lua"]},
    ],
);

for (const [visualization, expectedError] of [
    [{
        source: "src/source.cpp",
        symbol: "sourceFunction",
        animation: ".vscode/vis/animations/legacy.lua",
        series: "software-fill",
    }, "must match"],
    [{
        source: "src/source.cpp",
        symbol: "sourceFunction",
        animation: ".vscode/vis/software-fill/animations/mismatch.lua",
        series: "other",
    }, "must match"],
    [{
        source: "src/source.cpp",
        symbol: "sourceFunction",
        animation: ".vscode/vis/software-fill/animations/nested/scene.lua",
        series: "software-fill",
    }, "must match"],
    [{
        source: "src/source.cpp",
        symbol: "sourceFunction",
        animation: ".vscode/vis/software-fill/animations/scene.lua",
    }, "series"],
    [{
        source: "src/source.cpp",
        symbol: "sourceFunction",
        animation: ".vscode/vis/software/fill/animations/scene.lua",
        series: "software/fill",
    }, "one directory name"],
    [{
        source: "src/source.cpp",
        symbol: "sourceFunction",
        animation: ".vscode\\vis\\software-fill\\animations\\scene.lua",
        series: "software-fill",
    }, "forward slashes"],
]) {
    const result = parseVisualizationManifest({version: 2, visualizations: [visualization]});
    assert.equal(result.entries.length, 0);
    assert.ok(result.errors.some((error) => error.includes(expectedError)), JSON.stringify(result.errors));
}

const invalid = parseVisualizationManifest({
    version: 1,
    visualizations: [{source: "/tmp/file.cpp", symbol: "", animation: "../animation.lua", series: ""}],
});
assert.equal(invalid.entries.length, 0);
assert.ok(invalid.errors.some((error) => error.includes("version")));
assert.ok(invalid.errors.some((error) => error.includes("inside the workspace")));
assert.ok(invalid.errors.some((error) => error.includes("series")));

console.log("code visualization manifest passed");
