import { ApiError } from './errors.ts';

export function parseJson(text: string): unknown {
  try { return JSON.parse(text); }
  catch { throw new ApiError(400, 'InvalidArgument', 'The request must contain valid JSON.'); }
}

export function jsonObject(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw new ApiError(400, 'InvalidArgument', 'A JSON object is required.');
  return value as Record<string, unknown>;
}
