import { resolve } from "path";
import { defineConfig } from "electron-vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  main: {
    build: {
      outDir: "out/main",
      rollupOptions: {
        input: resolve(__dirname, "src/main/index.ts"),
      },
    },
    resolve: {
      alias: {
        "@shared": resolve(__dirname, "shared"),
      },
    },
  },
  preload: {
    build: {
      outDir: "out/preload",
      rollupOptions: {
        input: resolve(__dirname, "src/preload/index.ts"),
      },
    },
  },
  renderer: {
    root: resolve(__dirname, "src/renderer"),
    build: {
      outDir: "../../out/renderer",
      rollupOptions: {
        input: resolve(__dirname, "src/renderer/index.html"),
      },
    },
    plugins: [react()],
    resolve: {
      alias: {
        "@shared": resolve(__dirname, "shared"),
      },
    },
  },
});
