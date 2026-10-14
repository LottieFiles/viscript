// Example collections shown on the Examples page.
import {COLLECTIONS as BASE} from "./algorithms.js";
import {MATH} from "./math.js";
import {DIAGRAMS} from "./diagrams.js";
import {INTERACTIVE} from "./interactive.js";
import algorithms from "./js/algorithms.js";
import dataStructures from "./js/data-structures.js";
import math from "./js/math.js";
import diagrams from "./js/diagrams.js";

// Each collection's episodes gain `js` when a JavaScript version exists; interactive scenes stay Lua-only.
const JS = {algorithms, "data-structures": dataStructures, math, diagrams};
export const COLLECTIONS = [...BASE, MATH, DIAGRAMS, INTERACTIVE].map((c) => ({
  ...c, episodes: c.episodes.map((e) => ({...e, js: JS[c.id]?.[e.id] ?? null})),
}));
