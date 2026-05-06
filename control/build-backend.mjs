import esbuild from "esbuild";
import { resolve } from "path";

const root = import.meta.dirname;

const debug = process.argv.includes("--debug");

// Naim process script
await esbuild.build({
  entryPoints: [resolve(root, "backend/main.ts")],
  bundle: true,
  sourcemap: debug,
  platform: "node",
  external: ["electron"],
  outfile: resolve(root, "dist/main.js"),
});

// Preload script
await esbuild.build({
  entryPoints: [resolve(root, "backend/preload.ts")],
  bundle: true,
  sourcemap: debug,
  platform: "node",
  external: ["electron"],
  outfile: resolve(root, "dist/preload.js"),
});