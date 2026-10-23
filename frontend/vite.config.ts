import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('./', import.meta.url));

export default defineConfig({
  root,
  plugins: [react()],
  cacheDir: '.vite',
  base: '/',
  build: { sourcemap: false }
});
