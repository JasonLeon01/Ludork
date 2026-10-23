import { loadConfig, dataPath } from './config.ts';
import { createApplication } from './application.ts';
import { acquireInstanceLock } from './instance-lock.ts';
import { createServer } from 'node:http';

let releaseLock: (() => Promise<void>) | undefined;

try {
  const config = await loadConfig();
  const release = await acquireInstanceLock(dataPath);
  releaseLock = release;
  const { app, attach, close } = await createApplication(config, dataPath);
  const server = createServer(app);
  attach(server);
  server.requestTimeout = 15_000;
  server.headersTimeout = 10_000;
  let stopping = false;
  const stop = (): void => {
    if (stopping) return;
    stopping = true;
    const closed = close();
    const timeout = setTimeout(() => server.closeAllConnections(), 10_000);
    timeout.unref();
    server.close(() => {
      clearTimeout(timeout);
      void closed.then(release).finally(() => process.exit(process.exitCode ?? 0));
    });
  };
  process.on('SIGINT', stop);
  process.on('SIGTERM', stop);
  process.on('message', message => { if (message === 'shutdown') stop(); });
  process.on('disconnect', stop);
  server.once('error', error => {
    console.error(error.message.includes('EADDRINUSE') ? 'Port 7777 is already in use.' : 'The backend could not listen on port 7777.');
    process.exitCode = 1;
    stop();
  });
  server.once('listening', () => {
    console.log('LudorkServer backend listening on http://0.0.0.0:7777');
    process.send?.('ready');
  });
  server.listen(7777, '0.0.0.0');
} catch (error) {
  await releaseLock?.();
  console.error(error instanceof Error ? error.message.replaceAll(/(?:[A-Za-z]:)?[^\s]*projects\.json/g, 'project configuration') : 'Could not start LudorkServer.');
  process.exitCode = 1;
}
