import { createHash, randomBytes, scrypt, timingSafeEqual } from 'node:crypto';
import { promisify } from 'node:util';
import type { Request } from 'express';
import type { ProjectConfig } from './config.ts';
import { ApiError } from './errors.ts';

const derive = promisify(scrypt);
const sessionLifetime = 8 * 60 * 60 * 1000;

interface Session { project: string; expiresAt: number }

function hash(value: string): string { return createHash('sha256').update(value).digest('hex'); }
function matches(actual: string, expected: string): boolean {
  const left = Buffer.from(actual, 'hex');
  const right = Buffer.from(expected, 'hex');
  return left.length === right.length && timingSafeEqual(left, right);
}

export class Authentication {
  private readonly sessions = new Map<string, Session>();
  private readonly timer: ReturnType<typeof setInterval>;

  constructor() {
    this.timer = setInterval(() => {
      const now = Date.now();
      for (const [key, session] of this.sessions) if (session.expiresAt <= now) this.sessions.delete(key);
    }, 30_000);
    this.timer.unref();
  }

  close(): void { clearInterval(this.timer); this.sessions.clear(); }

  verifyKey(key: unknown, project: ProjectConfig): void {
    if (typeof key !== 'string' || !matches(hash(key), project.keyHash)) {
      throw new ApiError(401, 'AuthenticationFailed', 'Project credentials are invalid.');
    }
  }

  async login(project: string, config: ProjectConfig, key: unknown, password: unknown): Promise<string> {
    this.verifyKey(key, config);
    if (typeof password !== 'string' || !password || Buffer.byteLength(password, 'utf8') > 4096) {
      throw new ApiError(401, 'AuthenticationFailed', 'Project credentials are invalid.');
    }
    const derived = await derive(password, Buffer.from(config.passwordSalt, 'hex'), 64) as Buffer;
    if (!matches(derived.toString('hex'), config.passwordHash)) {
      throw new ApiError(401, 'AuthenticationFailed', 'Project credentials are invalid.');
    }
    const raw = randomBytes(32).toString('base64url');
    this.sessions.set(hash(raw), { project, expiresAt: Date.now() + sessionLifetime });
    return raw;
  }

  cookieName(project: string): string { return `ludork_${hash(project).slice(0, 16)}`; }

  private sessionKey(request: Request, project: string): string {
    const name = this.cookieName(project);
    const raw = request.headers.cookie?.split(';').map(part => part.trim())
      .find(part => part.startsWith(`${name}=`))?.slice(name.length + 1);
    return raw ? hash(raw) : '';
  }

  verifySession(request: Request, project: string): void {
    const session = this.sessions.get(this.sessionKey(request, project));
    if (!session || session.project !== project || session.expiresAt <= Date.now()) {
      throw new ApiError(401, 'AuthenticationFailed', 'Sign in to view this project.');
    }
  }

  logout(request: Request, project: string): void { this.sessions.delete(this.sessionKey(request, project)); }

  cookieOptions(project: string): { httpOnly: boolean; sameSite: 'strict'; path: string; maxAge: number } {
    return { httpOnly: true, sameSite: 'strict', path: `/${encodeURIComponent(project)}/api/v1/admin`, maxAge: sessionLifetime };
  }
}
