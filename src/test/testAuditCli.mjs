import assert from "node:assert/strict";
import { copyFileSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { auditor, font, fontOptions, monoFont } from "./testNode.mjs";

const [cliArg, goodArg, transientArg, finalArg] = process.argv.slice(2);
if (!cliArg || !goodArg || !transientArg || !finalArg) {
    throw new Error("Usage: testAuditCli.mjs <viscript> <good> <transient> <final>");
}

const run = auditor(resolve(cliArg), fontOptions);
const good = resolve(goodArg);
const transient = resolve(transientArg);
const finalOnly = resolve(finalArg);
const fixtureDirectory = dirname(good);
const transitionLedger = join(fixtureDirectory, "layout-transition-ledger.lua");
const offcanvasTransient = join(fixtureDirectory, "layout-offcanvas-transient.lua");
const crossSceneTextGap = join(fixtureDirectory, "layout-cross-scene-text-gap.lua");
const opaqueViewportOcclusion = join(fixtureDirectory, "layout-opaque-viewport-occlusion.lua");
const viewOcclusion = join(fixtureDirectory, "layout-view-occlusion.lua");
const ancestorContainment = join(fixtureDirectory, "layout-ancestor-containment.lua");
const numericColonIds = join(fixtureDirectory, "layout-numeric-colon-id.lua");
const ownership = [
    "--require-containment-ledger",
    "--contain", "audit:label", "audit:body", "12",
];

const passed = JSON.parse(run(good, [...ownership, "--width", "900", "--fps", "24"]).stdout);
assert.equal(passed.schema, "viscript.layout-audit/v1");
assert.equal(passed.valid, true);
assert.deepEqual(passed.issues, []);
assert.equal(passed.profile.width, 900);
assert.equal(passed.profile.fps, 24);
assert.equal(passed.sampling.finalSampleIncluded, true);
assert.equal(passed.summary.failingGeometrySamples, 0);
assert.equal(passed.policy.font.path, font);
assert.deepEqual(passed.policy.font.paths, [font, monoFont]);
const fontDirectory = mkdtempSync(join(tmpdir(), "vis-audit-fonts-"));
try {
    const secondFont = join(fontDirectory, "Second Family.ttf");
    copyFileSync(font, secondFont);
    const twoFonts = JSON.parse(run(good, ["--font", secondFont]).stdout);
    assert.equal(twoFonts.valid, true);
    assert.equal(twoFonts.policy.font.path, font);
    assert.deepEqual(twoFonts.policy.font.paths, [font, monoFont, secondFont]);
} finally {
    rmSync(fontDirectory, { recursive: true, force: true });
}
assert.deepEqual(passed.policy.thresholds, passed.thresholds);
assert.deepEqual(passed.policy.containmentLedger, {
    required: true,
    relationships: [{
        textId: "audit:label",
        panelId: "audit:body",
        inset: 12,
        textMatched: true,
        panelMatched: true,
        matched: true,
    }],
    uncontained: [],
});
assert.deepEqual(passed.policy.textOverlapAllowances, []);
assert.deepEqual(passed.policy.occludedSceneAllowances, []);
assert.deepEqual(passed.policy.viewportStretch, {allowed: false});
assert.equal(JSON.parse(run(good, [
    "--panel-inset", "12",
    "--require-containment-ledger",
    "--contain", "audit:label", "audit:body",
]).stdout).valid, true, "an omitted relationship inset must inherit --panel-inset");

const transientFirst = run(transient, ownership, 1);
const transientSecond = run(transient, ownership, 1);
assert.equal(transientFirst.stdout, transientSecond.stdout, "audit JSON must be deterministic");
const transientReport = JSON.parse(transientFirst.stdout);
assert.equal(transientReport.valid, false);
assert.equal(transientReport.sampling.encodedFrames, 10);
assert.equal(transientReport.sampling.samples, 11);
assert.equal(JSON.parse(run(transient, [
    ...ownership, "--max-samples", "11",
], 1).stdout).sampling.samples, 11, "the exact native sample cap must be accepted");
const cappedTransient = run(transient, [...ownership, "--max-samples", "10"], 4);
assert.match(cappedTransient.stderr, /sample count 11 exceeds the configured limit 10/);
const transientIssue = transientReport.issues.find((issue) => issue.code === "containment_overflow");
assert.ok(transientIssue);
assert.equal(transientIssue.scenePath, "root");
assert.equal(transientIssue.subject.id, "audit:label");
assert.equal(transientIssue.related.id, "audit:body");
assert.equal(transientIssue.worstSample.kind, "frame");
assert.equal(transientIssue.worstSample.index, 5);
assert.ok(transientIssue.overflow.right > 90);

const finalReport = JSON.parse(run(finalOnly, ownership, 1).stdout);
assert.equal(finalReport.sampling.encodedFrames, 1);
assert.equal(finalReport.sampling.samples, 2);
assert.equal(finalReport.issues[0].firstSample.kind, "final");

const textResult = run(transient, [...ownership, "--format", "text"], 1);
assert.match(textResult.stdout, /viscript audit: FAIL/);
assert.match(textResult.stdout, /containment_overflow/);
assert.match(textResult.stdout, /text#\d+ id=audit:label/);
assert.match(textResult.stdout, /rectangle#\d+ id=audit:body/);
assert.match(textResult.stdout, /relatedScenePath=root/);
assert.match(textResult.stdout, /measurement\[actual=.*required=.*deficit=.*\]/);
assert.match(textResult.stdout, /bounds\[subject=\[x=.*related=\[x=/);
assert.match(textResult.stdout, /overflow\[left=.*top=.*right=.*bottom=.*\]/);

const missingOwner = JSON.parse(run(good, ["--require-containment-ledger"], 1).stdout);
assert.ok(missingOwner.issues.some((issue) => issue.code === "missing_containment_owner"));
assert.equal(JSON.parse(run(good, [
    "--require-containment-ledger", "--allow-uncontained", "audit:label",
]).stdout).valid, true);

const duplicateOwner = run(good, [
    "--contain", "audit:label", "audit:body",
    "--contain", "audit:label", "audit:body", "8",
], 2);
assert.match(duplicateOwner.stderr, /Usage:/);
assert.match(run(good, [
    "--contain", "audit:label", "--require-containment-ledger",
], 2).stderr, /Usage:/);
assert.match(run(good, [
    "--require-containment-ledger",
    "--allow-uncontained", "audit:label",
    "--allow-uncontained", "audit:label",
], 2).stderr, /Usage:/);
assert.match(run(good, [
    "--allow-uncontained", "audit:label",
], 2).stderr, /Usage:/, "--allow-uncontained is invalid when the containment ledger is disabled");
assert.match(run(good, [
    "--allow-text-overlap", "audit:label", "audit:label",
], 2).stderr, /Usage:/, "overlap allowances require both exact Scene paths and IDs");
assert.equal(run(good, ["--font", join(tmpdir(), "missing-vis-font.ttf")], 3).status, 3);

const transitionBase = [
    "--fps", "20",
    "--require-containment-ledger",
    "--allow-uncontained", "transition:first",
    "--allow-uncontained", "transition:middle",
    "--allow-uncontained", "transition:last",
];
const transitionGeometry = JSON.parse(run(transitionLedger, transitionBase, 1).stdout);
assert.deepEqual(transitionGeometry.policy.containmentLedger.uncontained, [
    {id: "transition:first", matched: true},
    {id: "transition:middle", matched: true},
    {id: "transition:last", matched: true},
]);
const transitionOverlap = transitionGeometry.issues.find((issue) => {
    return issue.code === "text_gap" && issue.scenePath === "root/transition:0" &&
        issue.relatedScenePath === "root/transition:1";
});
assert.ok(transitionOverlap, "transition geometry must include both rendered stages");
assert.deepEqual(new Set([
    transitionOverlap.visual?.kind,
    transitionOverlap.relatedVisual?.kind,
]), new Set(["fade-out", "fade-in"]));
assert.equal(transitionOverlap.visual.counterpart, null, "an unmatched fade has no authored counterpart");
assert.equal(transitionOverlap.relatedVisual.counterpart, null, "an unmatched fade has no authored counterpart");
const completeTransitionLedger = JSON.parse(run(transitionLedger, [
    ...transitionBase,
    "--allow-text-overlap", "root/transition:0", "transition:first",
        "root/transition:1", "transition:middle",
    "--allow-text-overlap", "root/transition:1", "transition:middle",
        "root/transition:2", "transition:last",
]).stdout);
assert.equal(completeTransitionLedger.valid, true);
assert.ok(completeTransitionLedger.policy.textOverlapAllowances.every((allowance) => {
    return allowance.first.matched && allowance.second.matched && allowance.matched && allowance.used &&
        !allowance.duplicate;
}), "the report must preserve matched and used state for every overlap allowance");

const incompleteTransitionLedger = JSON.parse(run(transitionLedger, [
    "--require-containment-ledger",
    "--allow-uncontained", "transition:first",
    "--allow-uncontained", "transition:last",
], 1).stdout);
const laterStageOwner = incompleteTransitionLedger.issues.find((issue) => {
    return issue.code === "missing_containment_owner" && issue.subject.id === "transition:middle";
});
assert.ok(laterStageOwner, "even a transition stage skipped by frame sampling must enter the ledger");
assert.equal(laterStageOwner.scenePath, "root/transition:1");
const skippedStage = incompleteTransitionLedger.issues.find((issue) => {
    return issue.code === "never_visible_text" && issue.subject.id === "transition:middle";
});
assert.ok(skippedStage, "a delivery profile that skips a whole stage must fail explicitly");
assert.equal(skippedStage.scenePath, "root/transition:1");
assert.equal(incompleteTransitionLedger.summary.failingGeometrySamples, 0);

const offcanvasReport = JSON.parse(run(offcanvasTransient, [
    "--require-containment-ledger",
    "--allow-uncontained", "offcanvas:label",
], 1).stdout);
const offcanvasIssue = offcanvasReport.issues.find((issue) => issue.code === "canvas_overflow");
assert.ok(offcanvasIssue, "a fully clipped transient Text must fail the canvas audit");
assert.equal(offcanvasIssue.subject.id, "offcanvas:label");
assert.equal(offcanvasIssue.worstSample.kind, "frame");
assert.equal(offcanvasIssue.worstSample.index, 1);
assert.ok(Math.abs(offcanvasIssue.worstSample.time - 0.05) < 1e-9);

const crossSceneBase = [
    "--require-containment-ledger",
    "--allow-uncontained", "root:text",
    "--allow-uncontained", "viewport:text",
];
const crossSceneReport = JSON.parse(run(crossSceneTextGap, crossSceneBase, 1).stdout);
const crossSceneIssue = crossSceneReport.issues.find((issue) => {
    const ids = new Set([issue.subject?.id, issue.related?.id]);
    return issue.code === "text_gap" && ids.has("root:text") && ids.has("viewport:text");
});
assert.ok(crossSceneIssue, "root and Viewport Text bounds must share one collision space");
assert.equal(crossSceneIssue.scenePath, "root");
assert.equal(crossSceneIssue.relatedScenePath, "root/viewport:0");
assert.ok(!crossSceneReport.issues.some((issue) => issue.code === "text_occluded"),
    "a transparent Viewport background must not hide the underlying Text");
assert.equal(JSON.parse(run(crossSceneTextGap, [
    ...crossSceneBase,
    "--allow-text-overlap", "root", "root:text", "root/viewport:0", "viewport:text",
]).stdout).valid, true);
const opaqueViewportReport = JSON.parse(run(opaqueViewportOcclusion, crossSceneBase, 1).stdout);
const occludedText = opaqueViewportReport.issues.find((issue) => issue.code === "text_occluded");
assert.ok(occludedText, "fully covered Text must fail unless its Scene is explicitly allowed");
assert.equal(occludedText.scenePath, "root");
assert.equal(occludedText.relatedScenePath, "root/viewport:0");
assert.equal(occludedText.subject.id, "root:text");
assert.ok(occludedText.firstSample && occludedText.lastSample && occludedText.worstSample,
    "occlusion must retain sampled first/last/worst evidence");
assert.ok(!opaqueViewportReport.issues.some((issue) => {
    return issue.subject?.id === "root:text" &&
        ["canvas_overflow", "text_gap", "containment_overflow"].includes(issue.code);
}), "occluded Text must skip subsequent geometry checks");
assert.equal(JSON.parse(run(opaqueViewportOcclusion, [
    ...crossSceneBase, "--allow-occluded-scene", "root",
]).stdout).valid, true);
const allowedOcclusionReport = JSON.parse(run(opaqueViewportOcclusion, [
    ...crossSceneBase, "--allow-occluded-scene", "root",
]).stdout);
assert.deepEqual(allowedOcclusionReport.policy.occludedSceneAllowances, [{
    scenePath: "root", matched: true, used: true, duplicate: false,
}]);
const viewBase = ["--require-containment-ledger", "--allow-uncontained", "root:text"];
const viewReport = JSON.parse(run(viewOcclusion, viewBase, 1).stdout);
const viewOccluded = viewReport.issues.find((issue) => issue.code === "text_occluded");
assert.ok(viewOccluded, "an opaque view must hide the Text of its scene that it covers and does not show");
assert.equal(viewOccluded.scenePath, "root");
assert.equal(viewOccluded.relatedScenePath, "root/view:0");
assert.equal(viewOccluded.subject.id, "root:text");
assert.equal(JSON.parse(run(viewOcclusion, [...viewBase, "--allow-occluded-scene", "root"]).stdout).valid, true);

const staleOcclusionAllowance = JSON.parse(run(good, [
    ...ownership, "--allow-occluded-scene", "root/viewport:99",
], 1).stdout);
const unmatchedOcclusionPath = staleOcclusionAllowance.issues.find((issue) => {
    return issue.code === "unmatched_occluded_scene_allowance" &&
        issue.subject.id === "root/viewport:99";
});
assert.ok(unmatchedOcclusionPath, "an occlusion allowance must match an exact inventoried Scene path");
assert.equal(unmatchedOcclusionPath.firstSample, null, "allowance validation is a static contract check");
assert.deepEqual(staleOcclusionAllowance.policy.occludedSceneAllowances[0], {
    scenePath: "root/viewport:99", matched: false, used: false, duplicate: false,
});
const unusedOcclusionAllowance = JSON.parse(run(good, [
    ...ownership, "--allow-occluded-scene", "root",
], 1).stdout);
const unusedOcclusionPath = unusedOcclusionAllowance.issues.find((issue) => {
    return issue.code === "unused_occluded_scene_allowance" && issue.subject.id === "root";
});
assert.ok(unusedOcclusionPath, "an allowance must not silently survive after its occlusion disappears");
assert.equal(unusedOcclusionPath.firstSample, null, "stale allowance validation is static");
assert.deepEqual(unusedOcclusionAllowance.policy.occludedSceneAllowances[0], {
    scenePath: "root", matched: true, used: false, duplicate: false,
});
const duplicateOcclusionAllowance = JSON.parse(run(opaqueViewportOcclusion, [
    ...crossSceneBase,
    "--allow-occluded-scene", "root",
    "--allow-occluded-scene", "root",
], 1).stdout);
const duplicateOcclusionPath = duplicateOcclusionAllowance.issues.find((issue) => {
    return issue.code === "duplicate_occluded_scene_allowance" && issue.subject.id === "root";
});
assert.ok(duplicateOcclusionPath, "duplicate occlusion allowances must be reported as static issues");
assert.equal(duplicateOcclusionPath.firstSample, null);

assert.equal(JSON.parse(run(ancestorContainment, [
    "--require-containment-ledger",
    "--contain", "ancestor:label", "ancestor:body", "12",
]).stdout).valid, true, "a Rectangle's own paint bounds can contain its child label");

assert.equal(JSON.parse(run(numericColonIds, [
    "--require-containment-ledger",
    "--contain", "numeric:label:34", "numeric:body:12", "12",
]).stdout).valid, true, "numeric colon suffixes belong to IDs, not the inset grammar");

const temporary = mkdtempSync(join(tmpdir(), "vis-audit-cli-"));
try {
    // A character the font has no glyph for would vanish from the frame, so it fails the audit.
    const glyphs = join(temporary, "glyphs.lua");
    writeFileSync(glyphs, `
local scene = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
scene:text {id="set", text="A \u2229 B \u222a C", point={0,0.8}}
scene:text {id="plain", text="A and B", point={0,0}}
scene:text {id="again", text="B \u2229 C", point={0,-0.8}}
return scene
`);
    const glyphReport = JSON.parse(run(glyphs, [], 1).stdout);
    const glyphIssues = glyphReport.issues.filter((issue) => issue.code === "missing_glyphs");
    assert.equal(glyphIssues.length, 2, JSON.stringify(glyphReport.issues));
    assert.equal(glyphIssues[0].subject.id, "set");
    assert.deepEqual(glyphIssues[0].glyphs, {missing: 2, first: "\u2229"});
    assert.equal(glyphIssues[1].subject.id, "again");
    assert.deepEqual(glyphIssues[1].glyphs, {missing: 1, first: "\u2229"});
    assert.equal(glyphIssues[0].repair, "use-a-font-with-these-glyphs");
    const glyphText = run(glyphs, ["--format", "text"], 1).stdout;
    assert.ok(glyphText.includes("[missing_glyphs] root text#1 id=set glyphs[missing=2 first=\u2229]"), glyphText);
    // The check adds to the ledger rather than replacing it, so every text still needs an owner.
    const owners = JSON.parse(run(glyphs, ["--require-containment-ledger"], 1).stdout).issues
        .filter((issue) => issue.code === "missing_containment_owner").map((issue) => issue.subject.id);
    assert.deepEqual(owners, ["set", "plain", "again"]);

    // Plane text sizes are world units; a point size there makes one em taller than the view.
    const plane = join(temporary, "plane.lua");
    writeFileSync(plane, `
local scene = vis.scene {width=400, height=240, fps=20, theme="pro_white", camera={view="2d", height=4}}
scene:text {id="points", text="Hi", point={-1,0}, size=30, orientation="plane"}
scene:text {id="world", text="Hi", point={1,0}, size=0.6, orientation="plane"}
scene:text {id="billboard", text="Hi", point={0,-1}, size=30}
return scene
`);
    const planeIssues = JSON.parse(run(plane, [], 1).stdout).issues
        .filter((issue) => issue.code === "plane_text_size");
    assert.deepEqual(planeIssues.map((issue) => [issue.subject.id, issue.size, issue.repair]),
        [["points", {em: 30, viewHeight: 4}, "give-plane-text-size-in-world-units"]]);
    assert.ok(run(plane, ["--format", "text"], 1).stdout
        .includes("[plane_text_size] root text#1 id=points size[em=30.000 viewHeight=4.000]"));

    // A text that changes its string shows characters of every string it changes between.
    const changing = join(temporary, "changing.lua");
    writeFileSync(changing, `
local scene = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
local into = scene:text {id="into", text="A and B", point={0,0.8}}
local away = scene:text {id="away", text="A ∪ B", point={0,-0.8}}
scene:tween({{target=into, text="A ∩ B"}, {target=away, text="A or B", retype=true}}, {duration = 1})
return scene
`);
    const changingIssues = JSON.parse(run(changing, [], 1).stdout).issues
        .filter((issue) => issue.code === "missing_glyphs").map((issue) => [issue.subject.id, issue.glyphs]);
    assert.deepEqual(changingIssues, [["into", {missing: 1, first: "∩"}], ["away", {missing: 1, first: "∪"}]]);

    // A spans row reads as one block of text: its letters may touch, and its id names them.
    const typeset = join(temporary, "typeset.lua");
    writeFileSync(typeset, `
local scene = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
scene:spans {"ab", {"2", rise=0.4, size=12}, split="letters", id="row", point={0,1}}
scene:text {id="near", text="near", point={0,-1}}
return scene
`);
    const typesetReport = JSON.parse(run(typeset).stdout);
    assert.equal(typesetReport.valid, true, JSON.stringify(typesetReport.issues));
    const crowded = join(temporary, "crowded.lua");
    writeFileSync(crowded, `
local scene = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
scene:spans {"ab", {"2", rise=0.4, size=12}, split="letters", id="row", point={0,1}}
scene:text {id="over", text="over", point={0,1}}
return scene
`);
    const crowdedGaps = JSON.parse(run(crowded, [], 1).stdout).issues.filter((issue) => issue.code === "text_gap");
    assert.ok(crowdedGaps.length > 0);
    assert.ok(crowdedGaps.every((issue) => [issue.subject.id, issue.related.id].includes("over")),
        JSON.stringify(crowdedGaps));

    // A typeset group reads every text below it as one block, so its rows may sit close.
    const stacked = join(temporary, "stacked.lua");
    writeFileSync(stacked, `
local scene = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
local block = scene:group {typeset=true, id="block"}
block:spans {"first line", point={0, 0.15}, id="first"}
block:spans {"second line", point={0, -0.15}, id="second"}
scene:text {id="near", text="near", point={0,-1.5}}
return scene
`);
    const stackedReport = JSON.parse(run(stacked).stdout);
    assert.equal(stackedReport.valid, true, JSON.stringify(stackedReport.issues));
    const loose = join(temporary, "loose.lua");
    writeFileSync(loose, `
local scene = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
scene:spans {"first line", point={0, 0.15}, id="first"}
scene:spans {"second line", point={0, -0.15}, id="second"}
return scene
`);
    const looseGaps = JSON.parse(run(loose, [], 1).stdout).issues.filter((issue) => issue.code === "text_gap");
    assert.ok(looseGaps.length > 0, "rows outside a typeset group keep the gap rule between them");

    // A code block is typeset: its tokens touch and its lines stack tight, and its id names them.
    const listing = join(temporary, "listing.lua");
    writeFileSync(listing, `
local scene = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
scene:code {text="local x = f(1)\\n\\n  return x -- done", language="lua", id="listing", point={0,0.5}}
scene:text {id="near", text="near", point={0,-1.5}}
return scene
`);
    const listingReport = JSON.parse(run(listing).stdout);
    assert.equal(listingReport.valid, true, JSON.stringify(listingReport.issues));

    // The pieces of a match, or the two objects of a cross-fade, meet on the way: that is the
    // handoff itself. The same two blocks fading on their own still collide.
    const edit = (step) => {
        const path = join(temporary, "edit.lua");
        writeFileSync(path, `
local scene = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
local look = {language="lua", point={0,0.5}, size=20}
local before = scene:code(vis.merge(look, {text="local x = 1\\nlocal y = 2", id="before"}))
scene:wait(0.5)
local after = scene:code(vis.merge(look, {text="local y = 3\\nlocal x = 1", id="after"}))
${step}
return scene
`);
        return path;
    };
    const diffReport = JSON.parse(run(edit(`scene:match_transform(before, after, {duration = 1, curve = "linear", diff = true})`)).stdout);
    assert.equal(diffReport.valid, true, JSON.stringify(diffReport.issues));
    const fadeReport = JSON.parse(run(edit("scene:fade_transform(before, after, {duration = 1})")).stdout);
    assert.equal(fadeReport.valid, true, JSON.stringify(fadeReport.issues));
    const apart = JSON.parse(run(edit(`scene:parallel(
    function() scene:fade_out(before, {duration = 1}) end,
    function() scene:fade_in(after, {duration = 1}) end)`), [], 1).stdout);
    assert.ok(apart.issues.some((issue) => issue.code === "text_gap"
        && issue.subject.id.startsWith("before/") && issue.related.id.startsWith("after/")));

    // A match from a copy lets go of the copied pieces that a target piece takes over as it
    // begins, and a clone copies a child that no longer shows: such texts never show, so they
    // are not missing from the frames.
    const copied = join(temporary, "copied.lua");
    writeFileSync(copied, `
local scene = vis.scene {
    width=400, height=240, fps=20, theme="pro_white",
    camera={mode="fixed", view="2d", target={0,0}, height=4},
}
local before = scene:group {id="before"}
before:text {id="before/sum", text="a + b", point={0,0.8}, align={0.5,0.5}}
local note = scene:group {id="note"}
note:text {id="note/kept", text="x", point={-2.4,0.8}, align={0.5,0.5}}
local gone = note:text {id="note/gone", text="y", point={-2.4,-0.8}, align={0.5,0.5}}
scene:wait(0.5)
scene:remove(gone)
local after = scene:group {id="after"}
after:text {id="after/sum", text="a + b", point={0,-0.8}, align={0.5,0.5}}
scene:match_transform(before, after, {duration = 1, curve = "smooth", copy=true})
scene:clone(note, {id="twin", shift={4.8,0}})
scene:wait(0.5)
return scene
`);
    const copiedReport = JSON.parse(run(copied).stdout);
    assert.equal(copiedReport.valid, true, JSON.stringify(copiedReport.issues));

    const overlap = join(temporary, "overlap.lua");
    writeFileSync(overlap, `
local scene = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
scene:text {id="text:a", text="alpha", point={0,0}, role="text"}
scene:text {id="text:b", text="beta", point={0,0}, role="text"}
return scene
`);
    const overlapBase = [
        "--require-containment-ledger",
        "--allow-uncontained", "text:a",
        "--allow-uncontained", "text:b",
    ];
    const overlapReport = JSON.parse(run(overlap, overlapBase, 1).stdout);
    assert.ok(overlapReport.issues.some((issue) => issue.code === "text_gap"));
    assert.equal(JSON.parse(run(overlap, [
        ...overlapBase, "--allow-text-overlap", "root", "text:a", "root", "text:b",
    ]).stdout).valid, true);

    const unusedOverlap = JSON.parse(run(overlap, [
        ...overlapBase, "--allow-text-overlap", "root", "text:a", "root", "text:a",
    ], 1).stdout);
    assert.ok(unusedOverlap.issues.some((issue) => issue.code === "unused_overlap_allowance"));
    assert.deepEqual(unusedOverlap.policy.textOverlapAllowances[0], {
        first: {scenePath: "root", id: "text:a", matched: true},
        second: {scenePath: "root", id: "text:a", matched: true},
        matched: true,
        used: false,
        duplicate: false,
    });

    const separated = join(temporary, "separated.lua");
    writeFileSync(separated, `
local scene = vis.scene {
    width=400, height=240, fps=20, theme="pro_white",
    camera={mode="fixed", view="2d", target={0,0}, height=4},
}
scene:text {id="text:left", text="left", point={-2,0}, role="text"}
scene:text {id="text:right", text="right", point={2,0}, role="text"}
return scene
`);
    const separatedReport = JSON.parse(run(separated, [
        "--require-containment-ledger",
        "--allow-uncontained", "text:left",
        "--allow-uncontained", "text:right",
        "--allow-text-overlap", "root", "text:left", "root", "text:right",
    ], 1).stdout);
    assert.ok(separatedReport.issues.some((issue) => issue.code === "unused_overlap_allowance"),
        "co-visible but sufficiently separated Text must not consume an overlap allowance");

    const unmatchedOverlap = JSON.parse(run(overlap, [
        ...overlapBase,
        "--allow-text-overlap", "root/viewport:99", "text:a", "root", "text:b",
    ], 1).stdout);
    const unmatchedOverlapIssue = unmatchedOverlap.issues.find((issue) => {
        return issue.code === "unmatched_overlap_allowance";
    });
    assert.ok(unmatchedOverlapIssue);
    assert.equal(unmatchedOverlapIssue.scenePath, "root/viewport:99");
    assert.equal(unmatchedOverlapIssue.relatedScenePath, "root");
    assert.equal(unmatchedOverlapIssue.firstSample, null);

    const duplicateOverlap = JSON.parse(run(overlap, [
        ...overlapBase,
        "--allow-text-overlap", "root", "text:a", "root", "text:b",
        "--allow-text-overlap", "root", "text:b", "root", "text:a",
    ], 1).stdout);
    assert.equal(duplicateOverlap.policy.textOverlapAllowances.length, 2);
    assert.ok(duplicateOverlap.policy.textOverlapAllowances.every((allowance) => allowance.duplicate));
    assert.equal(duplicateOverlap.issues.filter((issue) => issue.code === "duplicate_overlap_allowance").length, 1);

    const sameIdOverlap = join(temporary, "same-id-overlap.lua");
    writeFileSync(sameIdOverlap, `
local child = vis.scene {
    width=400, height=240, fps=20, theme="pro_white", background="#ffffff00",
}
child:text {id="shared:text", text="child", point={0,0}, role="text"}
local root = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
root:text {id="shared:text", text="root", point={0,0}, role="text"}
root:viewport(child, {x=0, y=0, width=1, height=1})
return root
`);
    const sameIdBase = ["--require-containment-ledger", "--allow-uncontained", "shared:text"];
    assert.ok(JSON.parse(run(sameIdOverlap, sameIdBase, 1).stdout).issues.some((issue) => {
        return issue.code === "text_gap" && issue.relatedScenePath === "root/viewport:0";
    }));
    assert.equal(JSON.parse(run(sameIdOverlap, [
        ...sameIdBase, "--allow-text-overlap", "root", "shared:text",
            "root/viewport:0", "shared:text",
    ]).stdout).valid, true);
    const wronglyScopedOverlap = JSON.parse(run(sameIdOverlap, [
        ...sameIdBase, "--allow-text-overlap", "root", "shared:text", "root", "shared:text",
    ], 1).stdout);
    assert.ok(wronglyScopedOverlap.issues.some((issue) => issue.code === "text_gap"),
        "an allowance for the wrong exact Scene pair must not suppress a collision");
    assert.ok(wronglyScopedOverlap.issues.some((issue) => issue.code === "unused_overlap_allowance"));

    const untaggedOverlap = join(temporary, "untagged-overlap.lua");
    writeFileSync(untaggedOverlap, `
local child = vis.scene {
    width=400, height=240, fps=20, theme="pro_white", background="#ffffff00",
}
child:text {text="child", point={0,0}, role="text"}
local root = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
root:text {text="root", point={0,0}, role="text"}
root:viewport(child, {x=0, y=0, width=1, height=1})
return root
`);
    const untaggedTextReport = run(untaggedOverlap, ["--format", "text"], 1).stdout;
    assert.match(untaggedTextReport,
        /text#\d+ with text#\d+ \[relatedScenePath=root\/viewport:0\]/,
        "text diagnostics must retain type, handle, and related Scene path without authored IDs");

    const transitionMorph = join(temporary, "transition-morph.lua");
    writeFileSync(transitionMorph, `
local function stage(x)
    local scene = vis.scene {
        width=400, height=240, fps=20, theme="pro_white",
        camera={mode="fixed", view="2d", target={0,0}, height=4},
    }
    scene:rectangle {
        id="shared:panel", center={x,0}, size={2.4,1.2},
        fill="surface", stroke="border", width=2,
    }
    scene:text {id="shared:morph", text="moving", point={x,0}, role="text"}
    return scene
end
local root = vis.scene {width=400, height=240, fps=20, loop=false, theme="pro_white"}
root:scene_transition({stage(-1), stage(1)}, {duration=0.1, hold=0, curve="linear"})
return root
`);
    const morphReport = JSON.parse(run(transitionMorph, [
        "--require-containment-ledger", "--contain", "shared:morph", "shared:panel", "8",
    ]).stdout);
    assert.equal(morphReport.valid, true,
        "one morph visual must satisfy and mark both authored Text/container identities");
    assert.ok(!morphReport.issues.some((issue) => issue.code === "text_gap"),
        "one morph visual must never collide with itself");

    const morphDiagnostic = join(temporary, "transition-morph-diagnostic.lua");
    writeFileSync(morphDiagnostic, `
local function stage(x)
    local scene = vis.scene {
        width=400, height=240, fps=20, theme="pro_white",
        background="#ffffff00",
        camera={mode="fixed", view="2d", target={0,0}, height=4},
    }
    scene:text {id="diagnostic:morph", text="moving", point={x,0}, role="text"}
    return scene
end
local root = vis.scene {
    width=400, height=240, fps=20, loop=false, theme="pro_white",
    camera={mode="fixed", view="2d", target={0,0}, height=4},
}
root:text {id="diagnostic:blocker", text="blocker", point={0,0}, role="text"}
root:scene_transition({stage(-1.5), stage(1.5)}, {duration=0.1, hold=0, curve="linear"})
return root
`);
    const morphDiagnosticReport = JSON.parse(run(morphDiagnostic, [
        "--require-containment-ledger",
        "--allow-uncontained", "diagnostic:morph",
        "--allow-uncontained", "diagnostic:blocker",
    ], 1).stdout);
    const morphCollision = morphDiagnosticReport.issues.find((issue) => {
        const ids = new Set([issue.subject?.id, issue.related?.id]);
        return issue.code === "text_gap" && ids.has("diagnostic:morph") && ids.has("diagnostic:blocker");
    });
    assert.ok(morphCollision, "the interpolated morph bounds must collide at the midpoint");
    const morphEvidence = morphCollision.visual?.kind === "morph"
        ? morphCollision.visual : morphCollision.relatedVisual;
    assert.equal(morphEvidence.kind, "morph");
    assert.equal(morphEvidence.counterpart.id, "diagnostic:morph");
    assert.match(morphEvidence.counterpartScenePath, /^root\/transition:[01]$/,
        "a morph diagnostic must retain the other authored stage identity");
    assert.equal(morphDiagnosticReport.issues.filter((issue) => issue.code === "text_gap").length, 1,
        "one morph visual may collide with a blocker only once");

    const chainedMorph = join(temporary, "transition-chained-morph.lua");
    writeFileSync(chainedMorph, `
local function stage(x)
    local scene = vis.scene {
        width=400, height=240, fps=40, theme="pro_white", background="#ffffff00",
        camera={mode="fixed", view="2d", target={0,0}, height=4},
    }
    scene:text {id="diagnostic:chain", text="moving", point={x,0}, role="text"}
    return scene
end
local root = vis.scene {
    width=400, height=240, fps=40, loop=false, theme="pro_white",
    camera={mode="fixed", view="2d", target={0,0}, height=4},
}
root:text {id="diagnostic:blocker", text="blocker", point={0,0}, role="text"}
root:scene_transition({stage(-1.5), stage(1.5), stage(-1.5)}, {
    duration=0.1, hold=0, curve="linear",
})
return root
`);
    const chainedBase = [
        "--require-containment-ledger",
        "--allow-uncontained", "diagnostic:chain",
        "--allow-uncontained", "diagnostic:blocker",
    ];
    const chainedReport = JSON.parse(run(chainedMorph, chainedBase, 1).stdout);
    const chainedPaths = chainedReport.issues.filter((issue) => issue.code === "text_gap").map((issue) => {
        return issue.subject?.id === "diagnostic:chain" ? issue.scenePath : issue.relatedScenePath;
    });
    assert.deepEqual(chainedPaths, ["root/transition:0", "root/transition:1"]);
    const exactChainedReport = JSON.parse(run(chainedMorph, [
        ...chainedBase,
        "--allow-text-overlap", "root/transition:1", "diagnostic:chain",
        "root", "diagnostic:blocker",
    ], 1).stdout);
    const remainingChainedPaths = exactChainedReport.issues
        .filter((issue) => issue.code === "text_gap")
        .map((issue) => issue.subject?.id === "diagnostic:chain"
            ? issue.scenePath : issue.relatedScenePath);
    assert.deepEqual(remainingChainedPaths, ["root/transition:0"],
        "an exact allowance must not match another morph stage through its counterpart");

    const roundedSampleBoundary = join(temporary, "rounded-sample-boundary.lua");
    writeFileSync(roundedSampleBoundary, `
local scene = vis.scene {width=400, height=240, fps=30, theme="pro_white"}
for _ = 1, 256 do
    scene:wait(0.15611969166666667)
end
return scene
`);
    const roundedSampleResult = run(roundedSampleBoundary, ["--max-samples", "1200"], 4);
    assert.match(roundedSampleResult.stderr,
        /sample count 1201 exceeds the configured limit 1200/,
        "the native float timeline must enforce the host's actual work bound");

    const transitionDissolve = join(temporary, "transition-dissolve.lua");
    writeFileSync(transitionDissolve, `
local function stage(text)
    local scene = vis.scene {
        width=400, height=240, fps=40, theme="pro_white",
        camera={mode="fixed", view="2d", target={0,0}, height=4},
    }
    scene:text {id="shared:dissolve", text=text, point={0,0}, role="text"}
    return scene
end
local root = vis.scene {width=400, height=240, fps=40, loop=false, theme="pro_white"}
root:scene_transition({stage("before"), stage("after")}, {
    duration=0.1, hold=0, curve="linear",
})
return root
`);
    const dissolveReport = JSON.parse(run(transitionDissolve, [
        "--require-containment-ledger", "--allow-uncontained", "shared:dissolve",
    ]).stdout);
    assert.equal(dissolveReport.valid, true,
        "a changed-Text dissolve must paint both identities without inventing a self-collision");

    const transitionEndpoints = join(temporary, "transition-endpoints.lua");
    writeFileSync(transitionEndpoints, `
local function stage(id, text)
    local scene = vis.scene {
        width=400, height=240, fps=1, theme="pro_white",
        camera={mode="fixed", view="2d", target={0,0}, height=4},
    }
    scene:text {id=id, text=text, point={0,0}, role="text"}
    return scene
end
local root = vis.scene {width=400, height=240, fps=1, loop=false, theme="pro_white"}
root:scene_transition({stage("endpoint:first", "first"), stage("endpoint:last", "last")}, {
    duration=0.1, hold=0, curve="linear",
})
return root
`);
    const endpointReport = JSON.parse(run(transitionEndpoints, [
        "--require-containment-ledger",
        "--allow-uncontained", "endpoint:first",
        "--allow-uncontained", "endpoint:last",
    ]).stdout);
    assert.equal(endpointReport.sampling.samples, 2, "the endpoint fixture must sample only t=0 and exact final");
    assert.equal(endpointReport.valid, true,
        "zero-opacity endpoint fades must not collide, while the exact final target still counts as painted");

    const nestedTransition = join(temporary, "nested-transition.lua");
    writeFileSync(nestedTransition, `
local function stage(width, height, childWidth, childHeight, id, text)
    local child = vis.scene {
        width=childWidth, height=childHeight, fps=20, theme="pro_white",
        camera={mode="fixed", view="2d", target={0,0}, height=4},
    }
    child:text {id=id, text=text, point={0,0}, role="text"}
    local scene = vis.scene {width=width, height=height, fps=20, theme="pro_white"}
    scene:viewport(child, {x=0, y=0, width=1, height=1})
    return scene
end
local first = stage(400, 240, 200, 120, "nested:first", "first nested")
local last = stage(800, 400, 400, 200, "nested:last", "last nested")
local root = vis.scene {width=800, height=450, fps=20, loop=false, theme="pro_white"}
root:scene_transition({first, last}, {duration=0.1, hold=0, curve="linear"})
return root
`);
    const nestedBase = [
        "--allow-viewport-stretch",
        "--require-containment-ledger",
        "--allow-uncontained", "nested:first",
        "--allow-uncontained", "nested:last",
    ];
    const nestedReport = JSON.parse(run(nestedTransition, nestedBase, 1).stdout);
    const nestedIssue = nestedReport.issues.find((issue) => {
        return issue.code === "text_gap" &&
            issue.scenePath === "root/transition:0/viewport:0" &&
            issue.relatedScenePath === "root/transition:1/viewport:0";
    });
    assert.ok(nestedIssue,
        "physical crossfade geometry must retain nested viewport paths across different stage dimensions");
    assert.equal(JSON.parse(run(nestedTransition, [
        ...nestedBase, "--allow-text-overlap", "root/transition:0/viewport:0", "nested:first",
            "root/transition:1/viewport:0", "nested:last",
    ]).stdout).valid, true);

    const stretched = join(temporary, "stretched.lua");
    writeFileSync(stretched, `
local root = vis.scene {width=800, height=450, fps=20, theme="pro_white"}
local child = vis.scene {width=400, height=400, fps=20, theme="pro_white"}
child:text {id="child:text", text="child", point={0,0}, role="text"}
root:viewport(child, {x=0.25, y=0.25, width=0.5, height=0.5})
return root
`);
    const stretchBase = [
        "--require-containment-ledger", "--allow-uncontained", "child:text",
    ];
    const stretchReport = JSON.parse(run(stretched, stretchBase, 1).stdout);
    assert.ok(stretchReport.issues.some((issue) => issue.code === "viewport_stretch"));
    assert.deepEqual(stretchReport.policy.viewportStretch, {allowed: false});
    const allowedStretchReport = JSON.parse(run(stretched, [
        ...stretchBase, "--allow-viewport-stretch",
    ]).stdout);
    assert.equal(allowedStretchReport.valid, true);
    assert.deepEqual(allowedStretchReport.policy.viewportStretch, {allowed: true});

    const repeatedIds = join(temporary, "repeated-ids.lua");
    writeFileSync(repeatedIds, `
local child = vis.scene {width=200, height=240, fps=20, theme="pro_white"}
child:text {id="shared:label", text="child", point={0,0}, role="text"}
local root = vis.scene {width=400, height=240, fps=20, theme="pro_white"}
root:rectangle {
    id="shared:body", center={-2,0}, size={2.2,1.2},
    fill="surface", stroke="border", width=2,
}
root:text {id="shared:label", text="root", point={-2,0}, role="text"}
root:viewport(child, {x=0.5, y=0, width=0.5, height=1})
return root
`);
    const repeatedReport = JSON.parse(run(repeatedIds, [
        "--contain", "shared:label", "shared:body", "5",
    ], 1).stdout);
    const missingRepeatedOwner = repeatedReport.issues.find((issue) => {
        return issue.code === "unmatched_containment" && issue.subject.id === "shared:label";
    });
    assert.ok(missingRepeatedOwner, "a containment rule must apply independently in every Scene");
    assert.equal(missingRepeatedOwner.scenePath, "root/viewport:0");
    assert.deepEqual(repeatedReport.policy.containmentLedger.relationships[0], {
        textId: "shared:label",
        panelId: "shared:body",
        inset: 5,
        textMatched: true,
        panelMatched: false,
        matched: false,
    }, "policy matched means the complete relationship resolved in every Text-bearing Scene");

    // The shapes of an emphasis raise no issue of their own, even drawn around texts.
    const emphasized = join(temporary, "emphasized.lua");
    writeFileSync(emphasized, `
local s = vis.scene {width=400, height=240, fps=10, theme="pro_white"}
local title = s:text {id="title", text="attention", point={0,1}, role="h2"}
local note = s:text {id="note", text="weights", point={0,-1}, role="text"}
s:circumscribe(title)
s:circumscribe(note, {shape="ellipse"})
s:passing_flash(s:line {id="rule", from={-1.5,0}, to={1.5,0}})
s:flash(title)
s:flash({0, 0}, {lines=6})
s:ripple(note, {padding=0.1})
return s
`);
    const emphasizedReport = JSON.parse(run(emphasized).stdout);
    assert.equal(emphasizedReport.valid, true);
    assert.deepEqual(emphasizedReport.issues, []);
} finally {
    rmSync(temporary, { recursive: true, force: true });
}

console.log("viscript audit CLI tests passed");
