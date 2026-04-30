import { cpSync, existsSync, rmSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { resolve } from "node:path";
import { defineConfig } from "vite";
import vue from "@vitejs/plugin-vue";

const projectDir = fileURLToPath(new URL(".", import.meta.url));

function copyReplayStaticDirectories() {
  return {
    name: "copy-replay-static-directories",
    apply: "build",
    closeBundle() {
      for (const directoryName of ["assets", "data"]) {
        const sourceDir = resolve(projectDir, directoryName);
        const targetDir = resolve(projectDir, `dist/${directoryName}`);
        if (!existsSync(sourceDir)) {
          continue;
        }

        rmSync(targetDir, { recursive: true, force: true });
        cpSync(sourceDir, targetDir, { recursive: true });
      }
    }
  };
}

export default defineConfig({
  plugins: [vue(), copyReplayStaticDirectories()],
  base: "./",
  build: {
    assetsDir: "vite-assets"
  }
});