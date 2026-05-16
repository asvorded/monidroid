import { defineConfig } from 'vite';
import tailwindcss from '@tailwindcss/vite';
import svgr from 'vite-plugin-svgr';

export default defineConfig({
  build: {
    rollupOptions: {
      onwarn(warning, warn) {
        // Suppress "Module level directives cause errors when bundled" warnings
        if (warning.code !== "MODULE_LEVEL_DIRECTIVE") {
          warn(warning);
        }
      },
    },
  },
  plugins: [
    tailwindcss(),
    svgr(),
  ],
  base: ""
});
