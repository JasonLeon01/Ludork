export class ApiFailure extends Error {
  readonly status: number;
  constructor(status: number, message: string) { super(message); this.status = status; }
}

export async function request(project: string, path: string, options?: RequestInit): Promise<string> {
  const response = await fetch(`/${encodeURIComponent(project)}/api/v1/admin${path}`, {
    ...options, credentials: 'same-origin', headers: { 'X-Ludork-Admin': '1', ...options?.headers }
  });
  const text = await response.text();
  if (!response.ok) {
    let message = `Request failed (${response.status})`;
    try { message = (JSON.parse(text) as { message: string }).message || message; } catch { /* HTTP proxies may return a non-JSON error. */ }
    throw new ApiFailure(response.status, message);
  }
  return text;
}

export function segment(value: string): string { return encodeURIComponent(value); }
