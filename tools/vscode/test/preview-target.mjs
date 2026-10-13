import assert from "node:assert/strict";
import {previewUriCandidate} from "../out/previewTarget.js";

const active = {scheme: "file", path: "/workspace/scene.lua"};
const explicit = {scheme: "file", path: "/workspace/other.lua"};

assert.equal(previewUriCandidate(undefined, active), active);
assert.equal(previewUriCandidate({command: "editor/title"}, active), active);
assert.equal(previewUriCandidate(explicit, active), explicit);
assert.equal(previewUriCandidate(undefined, undefined), undefined);

console.log("Preview target resolution passed");
