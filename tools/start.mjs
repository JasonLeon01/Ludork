import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { join } from 'node:path';
import { access } from 'node:fs/promises';

const root = fileURLToPath(new URL('../', import.meta.url));
const children = new Set();
let stopping = false;
let exitCode = 0;
let forceTimer;

function shutdown(code = 0) {
  if (stopping) return;
  stopping = true;
  exitCode = code;
  for (const child of children) {
    if (child.connected) child.send('shutdown');
    else child.kill('SIGTERM');
  }
  forceTimer = setTimeout(() => {
    for (const child of children) child.kill('SIGKILL');
  }, 12_000);
  forceTimer.unref();
  if (children.size === 0) process.exit(exitCode);
}

function launch(args, ipc = false) {
  const child = spawn(process.execPath, args, {
    cwd: root, stdio: ipc ? ['inherit', 'inherit', 'inherit', 'ipc'] : 'inherit', windowsHide: true
  });
  children.add(child);
  child.on('error', () => {
    if (!child.pid) children.delete(child);
    console.error('A service process could not start.');
    shutdown(1);
  });
  child.on('exit', code => {
    children.delete(child);
    if (!stopping) {
      console.error('A service process exited; stopping both services.');
      shutdown(code === 0 ? 1 : code ?? 1);
    }
    if (children.size === 0) {
      clearTimeout(forceTimer);
      process.exit(exitCode);
    }
  });
  return child;
}

if (Number(process.versions.node.split('.')[0]) !== 24) {
  console.error('Node.js 24 is required.');
  process.exit(1);
}
process.on('SIGINT', () => shutdown());
process.on('SIGTERM', () => shutdown());
process.on('message', message => { if (message === 'shutdown') shutdown(); });
process.on('disconnect', () => shutdown());
try { await access(join(root, 'backend/main.js')); await access(join(root, 'frontend/index.html')); }
catch {
  console.error('Start the built deployment package. Run npm ci and npm run build, copy dist/LudorkServer to a separate directory, then run deploy and start there.');
  process.exit(1);
}
const backend = launch([join(root, 'backend', 'main.js')], true);
const timeout = setTimeout(() => { console.error('Backend startup timed out.'); shutdown(1); }, 15_000);
timeout.unref();
backend.on('message', message => {
  if (message !== 'ready' || stopping) return;
  clearTimeout(timeout);
  const management = launch([join(root, 'backend', 'management.js')], true);
  const managementTimeout = setTimeout(() => { console.error('Management startup timed out.'); shutdown(1); }, 15_000);
  managementTimeout.unref();
  management.on('message', state => {
    if (state !== 'ready' || stopping) return;
    clearTimeout(managementTimeout);
    console.log('Use http://<host>:3333/<project>/ to sign in. Press Ctrl+C to stop both services.');
    process.send?.('ready');
  });
});
