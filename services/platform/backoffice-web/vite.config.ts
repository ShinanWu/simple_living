import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  base: "/backoffice/",
  plugins: [react()],
  test: {
    environment: "jsdom",
  },
});
