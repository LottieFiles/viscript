# Revision evidence

Read this for a PR, commit, refactor, or any claim that depends on two revisions. Read the real diff and the unchanged context around it. Never reconstruct previous or deleted code from the current file, prose, or memory.

## Select fair evidence

Resolve both revisions to full 40-character SHAs with `git rev-parse`. Compare the same scenario on both sides and find the first behavioral divergence. Patch and hunk order only locate evidence; `links` still follow execution. Title the behavioral claim, not a path, hash, or "before/after". Use plain source links for unchanged context, and add `diff` only where the claim depends on the revision.

## Diff link

```json
{
  "label": "Ownership moves to the caller",
  "source": "src/raster/span.cpp",
  "range": {"startLine": 41, "endLine": 58},
  "diff": {"base": "<full SHA>", "head": "<full SHA>", "oldPath": "src/raster/old_span.cpp"}
}
```

- `range` is required and addresses the head (current) file. Cover the smallest coherent current block with the governing input or condition, the decisive operation, and the immediate result, not only the changed line.
- **Rename.** `source` is the current path and `oldPath` the previous one.
- **Added file.** Omit `oldPath`. The base must resolve, the path must be absent at the base, and the checked-out file must equal the head blob. The Viewer then shows the range as inserted rows. Never use this rule to hide a missing rename path.
- **Deletion only.** Anchor the range to the nearest truthful surviving current line; removed rows come only from the base blob. If no honest anchor exists, omit the link.

## What the extension verifies

The extension reads blobs through VS Code's built-in Git extension. It renders the diff only when the repository `HEAD` equals `head`, `base` resolves, and the open file's text, unsaved edits included, equals the head blob. Anything else shows `DIFF UNAVAILABLE`: another checkout, local edits, a missing revision, or a wrong path or range. It never switches revisions or guesses code. So set `head` to the commit the reader will have checked out, and expect diff links to become unavailable once that branch moves. Diff **SEE** opens VS Code's native Diff Editor with the base on the left.

Keep paths, hashes, and large patches off the Canvas. A short verified fragment may orient the Scene; the Inspector carries the full evidence.
