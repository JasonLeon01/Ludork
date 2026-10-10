import { ApiError } from './errors.ts';

export function identifier(value: unknown, label: string, maxBytes = 64): string {
  if (typeof value !== 'string' || value.length === 0 ||
      value === '.' || value === '..' || !value.isWellFormed() || Buffer.byteLength(value, 'utf8') > maxBytes || hasControlCharacters(value)) {
    throw new ApiError(400, 'InvalidArgument', `${label} must be nonempty UTF-8 text of at most ${maxBytes} bytes without control characters.`);
  }
  return value;
}

export function hasControlCharacters(value: string): boolean {
  return [...value].some(character => character.charCodeAt(0) < 32 || character.charCodeAt(0) === 127);
}

export function encodeName(value: string): string {
  return Buffer.from(value, 'utf8').toString('hex');
}

export function decodeName(value: string): string | null {
  if (!/^(?:[a-f0-9]{2}){1,64}$/.test(value)) return null;
  const decoded = Buffer.from(value, 'hex').toString('utf8');
  return encodeName(decoded) === value ? decoded : null;
}

export function projectIdentifier(value: unknown): string {
  const name = identifier(value, 'Project');
  if (/[\\/]/u.test(name)) throw new ApiError(400, 'InvalidArgument', 'Project names cannot contain slash or backslash.');
  return name;
}
