import { lstat, mkdir, readFile, readdir, rename, unlink, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { randomInt, randomUUID } from 'node:crypto';
import { ApiError, systemCode } from './errors.ts';
import { decodeName, encodeName, identifier } from './identity.ts';
import { recordValue, textValue, validateWire } from './wire.ts';
import type { WireValue } from './wire.ts';

const maxDataBytes = 1024 * 1024;

export class Storage {
  private readonly root: string;
  private readonly pending = new Map<string, Promise<void>>();
  constructor(root: string) { this.root = root; }
  private accountPath(project: string, account: string): string { return join(this.root, encodeName(project), encodeName(account)); }
  private filePath(project: string, account: string, category: string): string { return join(this.accountPath(project, account), `${encodeName(category)}.ldata`); }

  async accountExists(project: string, account: string): Promise<boolean> {
    try { if ((await lstat(this.accountPath(project, account))).isDirectory()) return true; }
    catch (error) { if (systemCode(error) === 'ENOENT') return false; }
    throw new ApiError(500, 'StorageFailure', 'The account directory could not be inspected.');
  }

  private async readFile(path: string): Promise<Map<string, WireValue> | null> {
    try {
      const bytes = await readFile(path);
      if (bytes.length > maxDataBytes) throw new Error('Oversized data file.');
      const document: unknown = JSON.parse(bytes.toString('utf8'));
      if (!document || typeof document !== 'object' || !('version' in document) || document.version !== 1 || !('data' in document)) throw new Error('Invalid data file.');
      const data = validateWire(document.data);
      if (data[0] !== 'table') throw new Error('A category must be a table.');
      return new Map(data[1].map(([key, value]) => [identifier(textValue(key, 'Field'), 'Field'), value]));
    } catch (error) {
      if (systemCode(error) === 'ENOENT') return null;
      throw new ApiError(500, 'StorageFailure', 'The category file could not be read.');
    }
  }

  async readCategory(project: string, account: string, category: string): Promise<WireValue | null> {
    const data = await this.readFile(this.filePath(project, account, category));
    return data === null ? null : recordValue(Object.fromEntries(data));
  }

  async readField(project: string, account: string, category: string, field: string): Promise<{ found: boolean; value: WireValue }> {
    const data = await this.readFile(this.filePath(project, account, category));
    return { found: data?.has(field) ?? false, value: data?.get(field) ?? ['nil'] };
  }

  async listFields(project: string, account: string, category: string, before: string | undefined, limit: number): Promise<{ entries: { key: string; value: WireValue }[]; nextCursor: string | null }> {
    const data = await this.readFile(this.filePath(project, account, category));
    if (data === null) return { entries: [], nextCursor: null };
    const beforeBytes = before === undefined ? undefined : Buffer.from(before, 'utf8');
    const keys = [...data.keys()].map(key => ({ key, bytes: Buffer.from(key, 'utf8') }))
      .filter(item => beforeBytes === undefined || Buffer.compare(item.bytes, beforeBytes) < 0)
      .sort((left, right) => Buffer.compare(right.bytes, left.bytes));
    const entries = keys.slice(0, limit).map(item => ({ key: item.key, value: data.get(item.key)! }));
    return { entries, nextCursor: keys.length > limit ? entries.at(-1)!.key : null };
  }

  async listAccounts(project: string, category: string | undefined, sampleCount: bigint): Promise<string[]> {
    const eligible: string[] = [];
    for (const account of await this.accounts(project)) {
      if (category !== undefined) {
        try { if (!(await lstat(this.filePath(project, account, category))).isFile()) throw new Error('Invalid category path.'); }
        catch (error) {
          if (systemCode(error) === 'ENOENT') continue;
          throw new ApiError(500, 'StorageFailure', 'The category file could not be inspected.');
        }
      }
      eligible.push(account);
    }
    if (sampleCount === 0n) return eligible.sort((left, right) => Buffer.compare(Buffer.from(left, 'utf8'), Buffer.from(right, 'utf8')));
    const count = sampleCount > BigInt(eligible.length) ? eligible.length : Number(sampleCount);
    for (let index = 0; index < count; ++index) {
      const selected = randomInt(index, eligible.length);
      [eligible[index], eligible[selected]] = [eligible[selected], eligible[index]];
    }
    return eligible.slice(0, count);
  }

  async writeField(project: string, account: string, category: string, field: string, value: WireValue): Promise<void> {
    const path = this.filePath(project, account, category);
    const previous = this.pending.get(path) ?? Promise.resolve();
    const update = previous.then(async () => {
      const data = await this.readFile(path) ?? new Map<string, WireValue>();
      if (value[0] === 'nil') data.delete(field); else data.set(field, value);
      const text = JSON.stringify({ version: 1, data: validateWire(recordValue(Object.fromEntries(data))) }) + '\n';
      if (Buffer.byteLength(text, 'utf8') > maxDataBytes) throw new ApiError(400, 'InvalidArgument', 'A category file cannot exceed 1 MiB.');
      const directory = this.accountPath(project, account);
      const temporary = join(directory, `.${randomUUID()}.tmp`);
      try {
        await mkdir(directory, { recursive: true });
        await writeFile(temporary, text, { mode: 0o600, flag: 'wx' });
        await rename(temporary, path);
      } catch { throw new ApiError(500, 'StorageFailure', 'The category file could not be saved.'); }
      finally { await unlink(temporary).catch(() => undefined); }
    });
    const settled = update.catch(() => undefined);
    this.pending.set(path, settled);
    try { await update; }
    finally { if (this.pending.get(path) === settled) this.pending.delete(path); }
  }

  private async names(directory: string, files: boolean): Promise<string[]> {
    try {
      const entries = await readdir(directory, { withFileTypes: true });
      return entries.flatMap(entry => {
        if (files ? !entry.isFile() || !entry.name.endsWith('.ldata') : !entry.isDirectory()) return [];
        const name = decodeName(files ? entry.name.slice(0, -6) : entry.name);
        return name === null ? [] : [name];
      }).sort((left, right) => Buffer.compare(Buffer.from(left), Buffer.from(right)));
    } catch (error) {
      if (systemCode(error) === 'ENOENT') return [];
      throw new ApiError(500, 'StorageFailure', 'The data directory could not be listed.');
    }
  }

  async accounts(project: string): Promise<string[]> { return this.names(join(this.root, encodeName(project)), false); }
  async categories(project: string, account: string): Promise<string[]> { return this.names(this.accountPath(project, account), true); }
}
