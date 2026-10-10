import { createHash, randomBytes, scryptSync } from 'node:crypto';
import { mkdir, readFile, rename, unlink, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { hasControlCharacters, projectIdentifier } from '../backend/identity.ts';
import { ipWriteIntervalSeconds } from '../backend/config.ts';

const root = fileURLToPath(new URL('../', import.meta.url));
const [project, key, password, ...extra] = process.argv.slice(2);

try {
  if (Number(process.versions.node.split('.')[0]) !== 24) throw new Error('Node.js 24 is required.');
  if (extra.length || !project || !key || !password) throw new Error('Usage: deploy <project> <key> <password>');
  projectIdentifier(project);
  if (!/^[\x21-\x7e]{1,4096}$/.test(key)) throw new Error('The key must contain 1-4096 visible ASCII characters without spaces.');
  if (!password.isWellFormed() || Buffer.byteLength(password, 'utf8') > 4096 || hasControlCharacters(password)) throw new Error('Password must contain 1-4096 UTF-8 bytes without control characters.');

  const configDirectory = join(root, 'config');
  const path = join(configDirectory, 'projects.json');
  let config = { version: 1, projects: {} };
  try { config = JSON.parse(await readFile(path, 'utf8')); }
  catch (error) { if (error.code !== 'ENOENT') throw new Error('Existing configuration could not be read; it has not been replaced.', { cause: error }); }
  if (config.version !== 1 || !config.projects || typeof config.projects !== 'object' || Array.isArray(config.projects)) throw new Error('Existing configuration is invalid; it has not been replaced.');
  const interval = ipWriteIntervalSeconds(Object.hasOwn(config.projects, project) ? config.projects[project].ipWriteIntervalSeconds : undefined);

  console.log('Installing locked dependencies (no frontend build)...');
  const install = spawnSync(process.platform === 'win32' ? process.env.ComSpec ?? 'cmd.exe' : 'npm', process.platform === 'win32' ? ['/d', '/s', '/c', 'npm ci'] : ['ci'], {
    cwd: root, stdio: 'inherit', windowsHide: true
  });
  if (install.error || install.status !== 0) throw new Error('npm ci failed. Check Node.js 24, npm, and network access.');

  const salt = randomBytes(16);
  Object.defineProperty(config.projects, project, { value: {
    keyHash: createHash('sha256').update(key).digest('hex'),
    passwordSalt: salt.toString('hex'),
    passwordHash: scryptSync(password, salt, 64).toString('hex'),
    ipWriteIntervalSeconds: interval
  }, enumerable: true, configurable: true, writable: true });
  await mkdir(configDirectory, { recursive: true, mode: 0o700 });
  const temporary = join(configDirectory, `projects.${randomBytes(8).toString('hex')}.tmp`);
  try {
    await writeFile(temporary, JSON.stringify(config, null, 2) + '\n', { mode: 0o600, flag: 'wx' });
    await rename(temporary, path);
  } finally { await unlink(temporary).catch(() => undefined); }
  await writeFile(join(root, 'start.sh'), '#!/usr/bin/env sh\nset -eu\ncd -- "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"\nexec node tools/start.mjs\n', { mode: 0o700 });
  await writeFile(join(root, 'start.bat'), '@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nnode tools\\start.mjs\r\nexit /b %errorlevel%\r\n');
  console.log(`Configured project ${project}; existing user data was preserved.`);
  console.log('Run start.sh (Linux/macOS) or start.bat (Windows). Restart an existing service to load changed credentials.');
  console.log(`Viewer: http://<host>:3333/${encodeURIComponent(project)}/`);
  console.log(`Client URL: http://<host>:7777/${encodeURIComponent(project)}`);
} catch (error) {
  console.error(error instanceof Error ? error.message : 'Deployment failed.');
  process.exitCode = 1;
}
