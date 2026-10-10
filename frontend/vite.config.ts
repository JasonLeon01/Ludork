import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import { fileURLToPath } from 'node:url';

const root = fileURLToPath(new URL('./', import.meta.url));

export default defineConfig({
  root,
  plugins: [react()],
  cacheDir: '.vite',
  server: {
    host: '0.0.0.0',
    port: 3333,
    strictPort: true,
    allowedHosts: (process.env.LUDORK_ALLOWED_HOSTS ?? '').split(',').map(value => value.trim()).filter(Boolean),
    cors: false,
    fs: {
      strict: true,
      allow: [root],
      deny: ['**/.git/**', '**/.env*', '**/config/**', '**/data/**', '**/backend/**', '**/tools/**', '**/vite.config.ts']
    },
    proxy: { '^/[^/]+/api/v1(?:/|$)': { target: 'http://127.0.0.1:7777', changeOrigin: false } }
  }
});
