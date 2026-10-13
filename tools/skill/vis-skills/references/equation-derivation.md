# Equation derivation

Use this when symbolic steps, a proof, or geometry-to-formula correspondence is the evidence. Follow the scene contract and [concept-explainer.md](concept-explainer.md); this file adds algebraic invariants.

## Establish the proof model

Before drawing, verify the derivation independently, including domains, sign branches, and possible extraneous solutions. Record each step with:

- exact expression or value, operation, and preserved invariant;
- stable identity and semantic color for every carried term;
- the geometry owner, dimensions, adjacency, and label for each visualized term;
- the settled formula and diagram state that the viewer must compare.

Build both formula and geometry from that ledger.

## Preserve equality on screen

When length, area, angle, projection, or count is evidence, derive its geometry from the algebra. Tiles and strips that complete a square must meet the claimed edges; equal areas must share computed measures; rearranged pieces must preserve total measure. If scale is intentionally illustrative, label that limitation and still preserve the incidence, ownership, and proportional relationships used by the proof. A positive-length area construction may establish a polynomial identity, but cannot literally represent negative solution lengths; state that boundary on screen when it matters. Arbitrary placement is not proof.

Represent expressions as measured `Text` pieces or Groups, such as `spans` rows, or as `tex` formulas when the host has the tex module, each carried term a part of its own; without that module, do not assume LaTeX, `MathTex`, or glyph-level morphing. A carried term keeps its identity and color in the expression itself, not only in a legend. Give a changed expression a new target. Use `fade_transform` for a representation change and `morph` only when the source may be consumed; reconstruct any proxy needed later.

## Stage the derivation

Reveal intuition or geometry before notation when both are present. Keep distinct diagram and formula lanes, and bound the maximum diagram extent below measured section headings and above captions before the timeline. At each step:

1. dim unchanged context without recoloring its meaning;
2. indicate the operated term or matching piece;
3. move or reveal only what the operation changes;
4. settle the new equality and hold it long enough to verify;
5. reveal the result only after its evidence is visible.

Avoid a footer legend when direct labels or expression colors carry the identity more clearly. Never shrink the proof to accommodate too many simultaneous steps; split the derivation or use another Scene.

## Review invariants

Treat every settled equality as a decisive frame in the scene-contract review matrix. At maximum density, verify diagram edges, tile incidence, formula baselines, term colors, signs, and meaning-bearing glyphs together. The final frame must retain the conclusion and enough source relation to check why it is true.
