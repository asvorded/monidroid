import esbuild from "esbuild";
import { resolve } from "path";

const root = import.meta.dirname;

// Naim process script
await esbuild.build({
  entryPoints: [resolve(root, "backend/main.ts")],
  bundle: true,
  platform: "node",
  external: ["electron"],
  outfile: resolve(root, "dist/main.js"),
});

// Preload script
await esbuild.build({
  entryPoints: [resolve(root, "backend/preload.ts")],
  bundle: true,
  platform: "node",
  external: ["electron"],
  outfile: resolve(root, "dist/preload.js"),
});