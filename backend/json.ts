import { ApiError } from './errors.ts';

export const maxJsonBytes = 1024 * 1024;
const exactJson = JSON as JSON & { rawJSON: (text: string) => object; isRawJSON: (value: unknown) => boolean };

export function parseJson(text: string): unknown {
  try {
    return JSON.parse(text, (_key: string, value: unknown, context?: { source: string }) =>
      typeof value === 'number' ? exactJson.rawJSON(context!.source) : value);
  } catch {
    throw new ApiError(400, 'InvalidArgument', 'The request must contain valid JSON.');
  }
}

export function jsonObject(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== 'object' || Array.isArray(value) || exactJson.isRawJSON(value)) {
    throw new ApiError(400, 'InvalidArgument', 'A JSON object is required.');
  }
  return value as Record<string, unknown>;
}

export function serializeJson(value: unknown): string {
  const result = JSON.stringify(value);
  if (result === undefined) throw new ApiError(400, 'InvalidArgument', 'The value must be JSON.');
  return result;
}
