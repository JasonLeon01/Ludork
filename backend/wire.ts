import { ApiError } from './errors.ts';

export type WireValue = ['nil'] | ['bool', boolean] | ['int', string] | ['float', string] | ['string', string] | ['table', [WireValue, WireValue][]];
export const maxFrameBytes = 2 * 1024 * 1024;
export const maxDepth = 64;
export const maxNodes = 65_536;
const minimumInteger = -(1n << 63n);
const maximumInteger = (1n << 63n) - 1n;

export function invalid(message: string): never { throw new ApiError(400, 'InvalidArgument', message); }

function keyIdentity(value: WireValue): string {
  switch (value[0]) {
    case 'bool': return `b:${value[1]}`;
    case 'string': return `s:${value[1]}`;
    case 'int': return `i:${value[1]}`;
    case 'float': {
      const number = Number(value[1]);
      if (Number.isInteger(number)) {
        const integer = BigInt(number);
        if (integer >= minimumInteger && integer <= maximumInteger) return `i:${integer}`;
      }
      return `f:${number}`;
    }
    default: return invalid('Table keys must be booleans, finite numbers, or strings.');
  }
}

export function validateWire(value: unknown, budget = { nodes: 0 }): WireValue {
  function visit(item: unknown, depth: number): WireValue {
    if (++budget.nodes > maxNodes || depth > maxDepth) invalid('Lua data exceeds the depth or node limit.');
    if (!Array.isArray(item)) invalid('A tagged Lua value is required.');
    if (item[0] === 'nil' && item.length === 1) return item as WireValue;
    if (item.length !== 2) invalid('The Lua value has an invalid shape.');
    const tag: unknown = item[0];
    const content: unknown = item[1];
    if (tag === 'bool' && typeof content === 'boolean') return item as WireValue;
    if (tag === 'int' && typeof content === 'string' && /^-?(?:0|[1-9]\d*)$/.test(content) && content !== '-0' && content.length <= 20) {
      const integer = BigInt(content);
      if (integer >= minimumInteger && integer <= maximumInteger) return item as WireValue;
    }
    if (tag === 'float' && typeof content === 'string' && content.length <= 32 && /^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?$/.test(content) && Number.isFinite(Number(content))) {
      if (Number(content) !== 0 || !/[1-9]/.test(content.split(/[eE]/)[0])) return item as WireValue;
    }
    if (tag === 'string' && typeof content === 'string' && /^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(content) && Buffer.from(content, 'base64').toString('base64') === content) return item as WireValue;
    if (tag === 'table' && Array.isArray(content)) {
      const keys = new Set<string>();
      for (const entry of content) {
        if (!Array.isArray(entry) || entry.length !== 2) invalid('A table entry must contain a key and a value.');
        const key = visit(entry[0], depth + 1);
        const field = visit(entry[1], depth + 1);
        if (field[0] === 'nil') invalid('A Lua table cannot contain a nil entry.');
        const identity = keyIdentity(key);
        if (keys.has(identity)) invalid('A table contains duplicate Lua-equivalent keys.');
        keys.add(identity);
      }
      return item as WireValue;
    }
    return invalid('The Lua value has an invalid type or content.');
  }
  return visit(value, 0);
}

export function validateArgs(value: unknown): WireValue[] {
  if (!Array.isArray(value) || value.length > maxNodes) return invalid('RPC arguments must be an array of tagged Lua values.');
  const budget = { nodes: 0 };
  return value.map(item => validateWire(item, budget));
}

export function networkName(value: unknown, label: string): string {
  if (typeof value !== 'string' || !/^[A-Za-z0-9._-]{1,128}$/.test(value)) return invalid(`${label} must contain 1-128 ASCII letters, digits, dots, underscores or hyphens.`);
  return value;
}

export function stringValue(value: string): WireValue { return ['string', Buffer.from(value, 'utf8').toString('base64')]; }
export function integerValue(value: bigint | number): WireValue { return ['int', String(value)]; }
export function recordValue(fields: Record<string, WireValue>): WireValue {
  return ['table', Object.entries(fields).filter(([, value]) => value[0] !== 'nil').map(([key, value]) => [stringValue(key), value])];
}
export function arrayValue(values: WireValue[]): WireValue { return ['table', values.map((value, index) => [integerValue(index + 1), value])]; }
export function fieldValue(value: WireValue, key: string | number): WireValue {
  if (value[0] !== 'table') return invalid('A table is required.');
  const expected = keyIdentity(typeof key === 'number' ? integerValue(key) : stringValue(key));
  return value[1].find(([entry]) => keyIdentity(entry) === expected)?.[1] ?? ['nil'];
}
export function textValue(value: WireValue, label: string): string {
  if (value[0] !== 'string') return invalid(`${label} must be a string.`);
  const bytes = Buffer.from(value[1], 'base64');
  const text = bytes.toString('utf8');
  if (!Buffer.from(text, 'utf8').equals(bytes)) invalid(`${label} must be valid UTF-8.`);
  return text;
}
export function countValue(value: WireValue, fallback: bigint, maximum: bigint, label: string): bigint {
  if (value[0] === 'nil') return fallback;
  if (value[0] !== 'int') return invalid(`${label} must be a nonnegative integer.`);
  const count = BigInt(value[1]);
  if (count < 0n || count > maximum) invalid(`${label} is outside the supported range.`);
  return count;
}
