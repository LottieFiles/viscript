import { rm, symlink } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

// Quartz is distributed as a source checkout rather than a published npm package.
// Link the pinned, lockfile-verified upstream source into its expected workspace.
const root = fileURLToPath(new URL("../", import.meta.url));
const engine = path.join(root, "node_modules/@jackyzha0/quartz");
await rm(path.join(root, "quartz"), { recursive: true, force: true });
await symlink(path.relative(root, path.join(engine, "quartz")), path.join(root, "quartz"), "dir");
await rm(path.join(engine, "quartz.config.ts"), { force: true });
await symlink(path.relative(engine, path.join(root, "quartz.config.ts")), path.join(engine, "quartz.config.ts"));
