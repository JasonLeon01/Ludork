import { ipWriteIntervalSeconds } from './config.ts';
import type { ProjectConfig } from './config.ts';
import { identifier } from './identity.ts';
import { arrayValue, countValue, fieldValue, invalid, recordValue, stringValue, textValue } from './wire.ts';
import type { WireValue } from './wire.ts';
import type { Invocation } from './net-protocol.ts';
import type { Storage } from './storage.ts';
import type { WriteRateLimiter } from './write-rate-limiter.ts';

export class LuaServices {
  private readonly storage: Storage;
  private readonly limiter: WriteRateLimiter;
  constructor(storage: Storage, limiter: WriteRateLimiter) { this.storage = storage; this.limiter = limiter; }

  async call(project: string, config: ProjectConfig, invocation: Invocation, name: string, args: WireValue): Promise<WireValue> {
    const argument = (index: number): WireValue => fieldValue(args, index);
    const named = (index: number, label: string): string => identifier(textValue(argument(index), label), label);
    const optionalName = (index: number, label: string): string | undefined => argument(index)[0] === 'nil' ? undefined : named(index, label);
    if (name === 'AccountExists') return ['bool', await this.storage.accountExists(project, named(1, 'Account'))];
    if (name === 'ListAccounts') {
      const count = countValue(argument(1), 0n, 9_223_372_036_854_775_807n, 'sampleCount');
      return arrayValue((await this.storage.listAccounts(project, optionalName(2, 'Category'), count)).map(stringValue));
    }
    if (name === 'ReadField') {
      const result = await this.storage.readField(project, named(1, 'Account'), named(2, 'Category'), named(3, 'Field'));
      return recordValue({ found: ['bool', result.found], value: result.value });
    }
    if (name === 'ListFields') {
      const limit = Number(countValue(argument(4), 50n, 100n, 'limit'));
      if (limit < 1) invalid('limit must be from 1 through 100.');
      const result = await this.storage.listFields(project, named(1, 'Account'), named(2, 'Category'), optionalName(3, 'Cursor'), limit);
      return recordValue({ entries: arrayValue(result.entries.map(entry => recordValue({ key: stringValue(entry.key), value: entry.value }))), nextCursor: result.nextCursor === null ? ['nil'] : stringValue(result.nextCursor) });
    }
    if (invocation.request.type !== 'rpc') invalid('Snapshot handlers cannot write data or consume a write interval.');
    if (name === 'AdmitWrite') {
      if (invocation.request.module !== 'Storage' || invocation.request.name !== 'WriteField') invalid('The configured write interval applies only to Storage.WriteField.');
      named(1, 'Account'); named(2, 'Category'); named(3, 'Field');
      this.limiter.admit(project, invocation.peer, ipWriteIntervalSeconds(config.ipWriteIntervalSeconds));
      return ['nil'];
    }
    if (name === 'WriteField') {
      await this.storage.writeField(project, named(1, 'Account'), named(2, 'Category'), named(3, 'Field'), argument(4));
      return ['nil'];
    }
    return invalid('The Lua host operation is not supported.');
  }
}
