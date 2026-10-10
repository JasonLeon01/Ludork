import { createServer } from 'vite';
import { fileURLToPath } from 'node:url';

let server;
let stopping = false;

async function shutdown(code = 0) {
  if (stopping) return;
  stopping = true;
  await server?.close();
  process.exit(code);
}

process.on('SIGINT', () => void shutdown());
process.on('SIGTERM', () => void shutdown());
process.on('disconnect', () => void shutdown());
process.on('message', message => { if (message === 'shutdown') void shutdown(); });

try {
  server = await createServer({
    configFile: fileURLToPath(new URL('../frontend/vite.config.ts', import.meta.url)),
    clearScreen: false
  });
  await server.listen();
  server.printUrls();
} catch (error) {
  console.error(error instanceof Error ? error.message : 'The frontend could not start.');
  await shutdown(1);
}
