import esbuild from "esbuild";
import { resolve } from "path";
import fs from "fs/promises";

import pkg from "./package.json" with { type: "json" };

const root = import.meta.dirname;
const dist = resolve(root, "dist");

const debug = process.argv.includes("--debug");

const common = {
  bundle: true,
  sourcemap: debug,
  minify: !debug,
  platform: "node",
  external: ["electron"],
};

// Clear files in dist
for (const entry of await fs.readdir(dist)) {
  await fs.rm(resolve(dist, entry), {recursive: true});
}

// Main process script
await esbuild.build({
  entryPoints: [resolve(root, "backend/main.ts")],
  outfile: resolve(dist, "main.js"),
  ...common,
});

// Preload script
await esbuild.build({
  entryPoints: [resolve(root, "backend/preload.ts")],
  outfile: resolve(dist, "preload.js"),
  ...common,
});

// package.json
await fs.writeFile(resolve(dist, "package.json"), JSON.stringify({
  name: pkg.name,
  version: pkg.version,
  main: "main.js",
}, null, 2));

// static
await fs.cp(resolve(root, "static"), resolve(dist, "static"), { recursive: true });
