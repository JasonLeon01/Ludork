import { mkdir, readFile, unlink, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { randomUUID } from 'node:crypto';
import { systemCode } from './errors.ts';

export async function acquireInstanceLock(dataDirectory: string): Promise<() => Promise<void>> {
  await mkdir(dataDirectory, { recursive: true });
  const path = join(dataDirectory, '.server.lock');
  const contents = JSON.stringify({ pid: process.pid, id: randomUUID() });
  for (let attempt = 0; attempt < 3; ++attempt) {
    try {
      await writeFile(path, contents, { flag: 'wx', mode: 0o600 });
      return async () => {
        if (await readFile(path, 'utf8').catch(() => '') === contents) await unlink(path).catch(() => undefined);
      };
    } catch (error) {
      if (systemCode(error) !== 'EEXIST') throw error;
      const existing = await readFile(path, 'utf8').catch(() => '');
      let pid: number;
      try { pid = Number((JSON.parse(existing) as { pid: unknown }).pid); }
      catch (parseError) { throw new Error('The instance lock is incomplete. Confirm no server is running before removing data/.server.lock.', { cause: parseError }); }
      if (!Number.isSafeInteger(pid) || pid <= 0) throw new Error('Invalid instance lock. Confirm no server is running before removing data/.server.lock.', { cause: error });
      try { process.kill(pid, 0); }
      catch (probeError) {
        if (systemCode(probeError) === 'ESRCH') {
          if (await readFile(path, 'utf8').catch(() => '') === existing) await unlink(path).catch(() => undefined);
          continue;
        }
      }
      throw new Error('Another server may be using this data directory. Stop it before starting another instance.', { cause: error });
    }
  }
  throw new Error('Could not acquire the data directory lock.');
}
