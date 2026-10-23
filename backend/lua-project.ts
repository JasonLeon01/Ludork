import { fork } from 'node:child_process';
import type { ChildProcess, ForkOptions } from 'node:child_process';
import { ApiError } from './errors.ts';
import type { ProjectConfig } from './config.ts';
import { luaEntry } from './config.ts';
import type { LuaServices } from './lua-services.ts';
import type { Failure, Invocation, LuaReply } from './net-protocol.ts';
import type { WireValue } from './wire.ts';

interface Job {
  id: number;
  invocation: Invocation;
  bytes: number;
  resolve: (reply: LuaReply) => void;
  reject: (error: Error) => void;
  timer: ReturnType<typeof setTimeout>;
}

export class LuaProject {
  private child: ChildProcess | undefined;
  private readonly project: string;
  private readonly config: ProjectConfig;
  private readonly directory: string;
  private readonly services: LuaServices;
  private readonly onReset: () => void;
  private ready = false;
  private starting = false;
  private stopping = false;
  private nextId = 0;
  private queue: Job[] = [];
  private active: Job | undefined;
  private queuedBytes = 0;
  private readonly hostCalls = new Set<Promise<void>>();

  constructor(project: string, config: ProjectConfig, directory: string, services: LuaServices, onReset: () => void) {
    this.project = project; this.config = config; this.directory = directory; this.services = services; this.onReset = onReset;
  }

  async start(): Promise<void> {
    const options: ForkOptions & { windowsHide: boolean } = { execArgv: [], windowsHide: true, stdio: ['ignore', 'inherit', 'inherit', 'ipc'], serialization: 'advanced' };
    const child = fork(new URL('./lua-worker.js', import.meta.url), [], options);
    this.child = child;
    this.starting = true;
    await new Promise<void>((resolve, reject) => {
      const timer = setTimeout(() => { reject(new Error(`Lua startup timed out for project ${this.project}.`)); child.kill(); }, 10_000);
      let started = false;
      const fail = (message: string): void => {
        clearTimeout(timer);
        if (!started) reject(new Error(message));
        else if (this.child === child && !this.stopping) this.reset(new ApiError(500, 'ServerFailure', 'The project Lua process stopped.'));
      };
      child.on('error', () => fail(`The Lua process could not start for project ${this.project}.`));
      child.on('exit', () => fail(`The Lua process exited for project ${this.project}.`));
      child.on('message', (message: { type: string; id: number; hostId: number; name: string; args: WireValue; ok: boolean; reset?: boolean } & LuaReply & Failure) => {
        if (this.child !== child) return;
        if (message.type === 'ready') {
          clearTimeout(timer); started = true; this.starting = false; this.ready = true; resolve(); this.dispatch();
        } else if (message.type === 'fatal') {
          fail(`Lua scripts failed for project ${this.project}: ${message.message}`);
          child.kill();
        } else if (message.type === 'host_call' && this.active?.id === message.id) {
          const job = this.active;
          const operation = this.services.call(this.project, this.config, job.invocation, message.name, message.args).then(data => {
            if (this.child === child && this.active === job && child.connected) child.send({ type: 'host_result', hostId: message.hostId, ok: true, data });
          }).catch((error: unknown) => {
            const failure = error instanceof ApiError ? error : new ApiError(500, 'ServerFailure', 'The Lua host operation failed.');
            if (this.child === child && this.active === job && child.connected) child.send({ type: 'host_result', hostId: message.hostId, ok: false, code: failure.code, message: failure.message, retryAfterSeconds: failure.retryAfterSeconds });
          }).finally(() => { this.hostCalls.delete(operation); this.dispatch(); });
          this.hostCalls.add(operation);
        } else if (message.type === 'result' && this.active?.id === message.id) {
          if (message.reset) { this.reset(new ApiError(500, message.code, message.message)); return; }
          const job = this.active;
          this.active = undefined;
          this.queuedBytes -= job.bytes;
          clearTimeout(job.timer);
          if (message.ok) job.resolve({ data: message.data, events: message.events });
          else job.reject(new ApiError(500, message.code, message.message, message.retryAfterSeconds));
          this.dispatch();
        }
      });
      child.send({ type: 'init', directory: this.directory, entry: luaEntry(this.config.luaEntry), project: this.project });
    }).catch((error: unknown) => {
      if (this.child === child) {
        this.starting = false;
        this.ready = false;
        this.rejectAll(new ApiError(503, 'ServerFailure', 'The project Lua process could not start.'));
      }
      throw error;
    });
  }

  invoke(invocation: Invocation, bytes: number): Promise<LuaReply> {
    if ((!this.ready && !this.starting) || this.stopping) return Promise.reject(new ApiError(503, 'ServerFailure', 'The project Lua process is unavailable.'));
    if (this.queue.length + (this.active ? 1 : 0) >= 256 || this.queuedBytes + bytes > 8 * 1024 * 1024) return Promise.reject(new ApiError(429, 'RateLimited', 'The project request queue is full.', 1));
    return new Promise((resolve, reject) => {
      const job: Job = { id: ++this.nextId, invocation, bytes, resolve, reject, timer: setTimeout(() => this.timeout(job), 10_000) };
      this.queue.push(job); this.queuedBytes += bytes; this.dispatch();
    });
  }

  private dispatch(): void {
    if (this.active || this.hostCalls.size > 0 || !this.ready || this.stopping) return;
    this.active = this.queue.shift();
    if (this.active) this.child!.send({ type: 'invoke', id: this.active.id, invocation: this.active.invocation });
  }

  private timeout(job: Job): void {
    const error = new ApiError(504, 'Timeout', 'The server operation timed out. Its writes may already have completed.');
    if (this.active === job) { this.reset(error); return; }
    const index = this.queue.indexOf(job);
    if (index >= 0) { this.queue.splice(index, 1); this.queuedBytes -= job.bytes; job.reject(error); }
  }

  private reset(error: ApiError): void {
    const child = this.child;
    this.child = undefined; this.ready = false; this.starting = false;
    child?.kill();
    this.rejectAll(error);
    setTimeout(() => this.onReset(), 0);
    if (!this.stopping) void this.start().catch(() => { this.ready = false; });
  }

  private rejectAll(error: ApiError): void {
    const jobs = this.active ? [this.active, ...this.queue] : this.queue;
    this.active = undefined; this.queue = []; this.queuedBytes = 0;
    for (const job of jobs) { clearTimeout(job.timer); job.reject(error); }
  }

  async close(): Promise<void> {
    this.stopping = true; this.ready = false; this.starting = false;
    this.rejectAll(new ApiError(503, 'ServerFailure', 'The server is stopping.'));
    const child = this.child; this.child = undefined;
    if (child && child.exitCode === null && child.signalCode === null) await new Promise<void>(resolve => {
      const timer = setTimeout(() => child.kill(), 1_000);
      child.once('exit', () => { clearTimeout(timer); resolve(); });
      if (child.connected) child.send({ type: 'shutdown' }); else child.kill();
    });
    await Promise.allSettled(this.hostCalls);
  }
}
