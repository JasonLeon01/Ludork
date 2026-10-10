import { isIP } from 'node:net';
import { ApiError } from './errors.ts';

const nanosecondsPerSecond = 1_000_000_000n;

function normalizedAddress(address: string): string {
  const lower = address.toLowerCase();
  if (!lower.startsWith('::ffff:')) return lower;
  const suffix = lower.slice(7);
  if (isIP(suffix) === 4) return suffix;
  const parts = /^([a-f0-9]{1,4}):([a-f0-9]{1,4})$/.exec(suffix);
  if (!parts) return lower;
  const high = Number.parseInt(parts[1], 16);
  const low = Number.parseInt(parts[2], 16);
  return [high >>> 8, high & 255, low >>> 8, low & 255].join('.');
}

export class WriteRateLimiter {
  private readonly deadlines = new Map<string, Map<string, bigint>>();
  private readonly timer: ReturnType<typeof setInterval>;

  constructor() {
    this.timer = setInterval(() => {
      const now = process.hrtime.bigint();
      for (const [project, addresses] of this.deadlines) {
        for (const [address, deadline] of addresses) if (deadline <= now) addresses.delete(address);
        if (addresses.size === 0) this.deadlines.delete(project);
      }
    }, 30_000);
    this.timer.unref();
  }

  admit(project: string, address: string | undefined, intervalSeconds: number): void {
    if (intervalSeconds === 0) return;
    if (!address) throw new ApiError(500, 'ServerFailure', 'The client address could not be determined.');
    const ip = normalizedAddress(address);
    const now = process.hrtime.bigint();
    const addresses = this.deadlines.get(project);
    const remaining = (addresses?.get(ip) ?? now) - now;
    if (remaining > 0n) {
      const retryAfterSeconds = Number((remaining + nanosecondsPerSecond - 1n) / nanosecondsPerSecond);
      throw new ApiError(429, 'RateLimited', 'This IP address must wait before writing to this project again.', retryAfterSeconds);
    }
    const projectAddresses = addresses ?? new Map<string, bigint>();
    projectAddresses.set(ip, now + BigInt(intervalSeconds) * nanosecondsPerSecond);
    if (!addresses) this.deadlines.set(project, projectAddresses);
  }

  close(): void { clearInterval(this.timer); this.deadlines.clear(); }
}
