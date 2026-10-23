import { access, cp, lstat, mkdir, mkdtemp, readFile, realpath, rename, rm, writeFile } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import { basename, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { setTimeout as delay } from 'node:timers/promises';
import { build } from 'vite';

const root = fileURLToPath(new URL('../', import.meta.url));
const resolvedRoot = await realpath(root);
const distribution = join(root, 'dist');
const destination = join(distribution, 'LudorkServer');
const privateDirectories = ['config', 'data', 'projects', 'logs'];
let staging;

async function checkDirectory(path, expected) {
  let state;
  try { state = await lstat(path); }
  catch (error) { if (error.code === 'ENOENT') return; throw error; }
  if (!state.isDirectory() || state.isSymbolicLink() || await realpath(path) !== expected) throw new Error(`The build directory must be a real directory inside the source checkout: ${path}`);
}

async function ensureBuildDestination() {
  await checkDirectory(distribution, join(resolvedRoot, 'dist'));
  await checkDirectory(destination, join(resolvedRoot, 'dist', 'LudorkServer'));
  for (const name of privateDirectories) {
    try { await access(join(destination, name)); }
    catch (error) { if (error.code === 'ENOENT') continue; throw error; }
    throw new Error(`The build output contains private ${name}/ content. Move the deployed server outside dist/LudorkServer before building; no files were deleted.`);
  }
}

try {
  if (Number(process.versions.node.split('.')[0]) !== 24) throw new Error('Node.js 24 is required.');
  await ensureBuildDestination();
  await mkdir(distribution, { recursive: true });
  await checkDirectory(distribution, join(resolvedRoot, 'dist'));
  staging = await mkdtemp(join(distribution, '.LudorkServer-build-'));
  await checkDirectory(staging, join(resolvedRoot, 'dist', basename(staging)));
  const compile = spawnSync(process.execPath, [join(root, 'node_modules/typescript/bin/tsc'), '--project', join(root, 'tsconfig.build.json'), '--outDir', join(staging, 'backend')], { cwd: root, stdio: 'inherit', windowsHide: true });
  if (compile.error || compile.status !== 0) throw new Error('Backend compilation failed.');
  await build({ configFile: join(root, 'frontend/vite.config.ts'), build: { outDir: join(staging, 'frontend'), emptyOutDir: false } });
  for (const name of ['lua', 'third-party', 'README.md', '.emmyrc.json', 'deploy.sh', 'deploy.bat']) {
    await cp(join(root, name), join(staging, name), { recursive: true });
  }
  await mkdir(join(staging, 'tools'));
  for (const name of ['deploy.mjs', 'start.mjs']) await cp(join(root, 'tools', name), join(staging, 'tools', name));
  await writeFile(join(staging, 'start.sh'), '#!/usr/bin/env sh\nset -eu\ncd -- "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"\nexec node tools/start.mjs\n', { mode: 0o755 });
  await writeFile(join(staging, 'start.bat'), '@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nnode tools\\start.mjs\r\nexit /b %errorlevel%\r\n');
  const source = JSON.parse(await readFile(join(root, 'package.json'), 'utf8'));
  const runtime = { name: source.name, version: source.version, private: true, type: 'module', engines: source.engines, scripts: { start: 'node tools/start.mjs', server: 'node backend/main.js' }, dependencies: source.dependencies };
  const lock = JSON.parse(await readFile(join(root, 'package-lock.json'), 'utf8'));
  lock.packages = Object.fromEntries(Object.entries(lock.packages).filter(([name, value]) => name && !value.dev));
  lock.packages = { '': { name: runtime.name, version: runtime.version, dependencies: runtime.dependencies, engines: runtime.engines }, ...lock.packages };
  await writeFile(join(staging, 'package.json'), JSON.stringify(runtime, null, 2) + '\n');
  await writeFile(join(staging, 'package-lock.json'), JSON.stringify(lock, null, 2) + '\n');
  await ensureBuildDestination();
  await rm(destination, { recursive: true, force: true });
  for (let attempt = 0; ; attempt++) {
    try { await rename(staging, destination); break; }
    catch (error) {
      if (process.platform !== 'win32' || error.code !== 'EPERM' || attempt >= 4) throw error;
      await delay(100 * (attempt + 1));
      await ensureBuildDestination();
    }
  }
  staging = undefined;
  console.log(`Built ${destination}. Copy this directory to a separate deployment location before running deploy.`);
} catch (error) {
  console.error(error instanceof Error ? error.message : 'The server build failed.');
  process.exitCode = 1;
} finally {
  if (staging) {
    await checkDirectory(distribution, join(resolvedRoot, 'dist'));
    await checkDirectory(staging, join(resolvedRoot, 'dist', basename(staging)));
    await rm(staging, { recursive: true, force: true });
  }
}
