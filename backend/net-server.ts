import { randomUUID } from 'node:crypto';
import { join } from 'node:path';
import type { Server } from 'node:http';
import { WebSocket, WebSocketServer } from 'ws';
import { ApiError } from './errors.ts';
import { encodeName } from './identity.ts';
import { jsonObject, parseJson } from './json.ts';
import { invalid, maxFrameBytes, networkName, validateArgs, validateWire } from './wire.ts';
import type { ServerConfig } from './config.ts';
import type { Authentication } from './authentication.ts';
import type { LuaServices } from './lua-services.ts';
import type { NetEvent, NetRequest } from './net-protocol.ts';
import { LuaProject } from './lua-project.ts';

interface Session { socket: WebSocket; project: string; peer: string | undefined; pending: Set<string>; bytes: number; alive: boolean }

export class NetServer {
  private readonly config: ServerConfig;
  private readonly auth: Authentication;
  private readonly projects = new Map<string, LuaProject>();
  private readonly sessions = new Map<string, Session>();
  private readonly sockets = new WebSocketServer({ noServer: true, maxPayload: maxFrameBytes, perMessageDeflate: false });
  private readonly heartbeat: ReturnType<typeof setInterval>;

  constructor(config: ServerConfig, auth: Authentication, services: LuaServices, projectsDirectory: string) {
    this.config = config; this.auth = auth;
    for (const [name, project] of Object.entries(config.projects)) {
      this.projects.set(name, new LuaProject(name, project, join(projectsDirectory, encodeName(name)), services, () => {
        for (const session of this.sessions.values()) if (session.project === name) session.socket.close(1011, 'The project Lua process restarted.');
      }));
    }
    this.heartbeat = setInterval(() => {
      for (const session of this.sessions.values()) {
        if (!session.alive) { session.socket.terminate(); continue; }
        session.alive = false; session.socket.ping();
      }
    }, 30_000);
    this.heartbeat.unref();
  }

  async start(): Promise<void> {
    const results = await Promise.allSettled([...this.projects.values()].map(project => project.start()));
    for (const result of results) if (result.status === 'rejected') console.error(result.reason instanceof Error ? result.reason.message : 'A project Lua process could not start.');
  }

  attach(server: Server): void {
    server.on('upgrade', (request, socket, head) => {
      try {
        const match = /^\/([^/?]+)\/api\/v1\/net$/.exec(request.url ?? '');
        if (!match) invalid('The WebSocket endpoint does not exist.');
        const project = decodeURIComponent(match[1]);
        if (!Object.hasOwn(this.config.projects, project)) invalid('The project does not exist.');
        this.auth.verifyKey(request.headers['x-ludork-key'], this.config.projects[project]);
        this.sockets.handleUpgrade(request, socket, head, webSocket => this.connected(webSocket, project, request.socket.remoteAddress));
      } catch (error) {
        const status = error instanceof ApiError && error.code === 'AuthenticationFailed' ? 401 : 400;
        socket.end(`HTTP/1.1 ${status} ${status === 401 ? 'Unauthorized' : 'Bad Request'}\r\nConnection: close\r\nContent-Length: 0\r\n\r\n`);
      }
    });
  }

  private send(session: Session, message: unknown): void {
    if (session.socket.readyState !== WebSocket.OPEN) return;
    const text = JSON.stringify(message);
    const bytes = Buffer.byteLength(text);
    if (bytes > maxFrameBytes) invalid('The complete response exceeds 2 MiB. Request a smaller snapshot.');
    if (session.socket.bufferedAmount + bytes > 8 * 1024 * 1024) { session.socket.close(1013, 'The outgoing queue is full.'); return; }
    session.socket.send(text);
  }

  private connected(socket: WebSocket, project: string, peer: string | undefined): void {
    const id = randomUUID();
    const session: Session = { socket, project, peer, pending: new Set(), bytes: 0, alive: true };
    this.sessions.set(id, session);
    socket.on('pong', () => { session.alive = true; });
    socket.on('close', () => this.sessions.delete(id));
    socket.on('error', () => socket.terminate());
    socket.on('message', (bytes, binary) => {
      if (binary) { socket.close(1003, 'Only text frames are accepted.'); return; }
      const buffer = Array.isArray(bytes) ? Buffer.concat(bytes) : Buffer.from(bytes as ArrayBuffer);
      void this.request(session, id, buffer);
    });
    this.send(session, { type: 'ready', version: 1 });
  }

  private async request(session: Session, sessionId: string, bytes: Buffer): Promise<void> {
    let id: string | undefined;
    let admitted = false;
    try {
      const payload = jsonObject(parseJson(bytes.toString('utf8')));
      if (typeof payload.id !== 'string' || !/^[A-Za-z0-9._-]{1,128}$/.test(payload.id)) invalid('A request ID is required.');
      id = payload.id;
      if (session.pending.has(id)) { session.socket.close(1008, 'Duplicate pending request ID.'); return; }
      const module = networkName(payload.module, 'Module');
      let request: NetRequest;
      if (payload.type === 'snapshot') request = { type: 'snapshot', id, module, params: validateWire(payload.params) };
      else if (payload.type === 'rpc') request = { type: 'rpc', id, module, name: networkName(payload.name, 'Function'), args: validateArgs(payload.args) };
      else return invalid('The request type is not supported.');
      if (session.pending.size >= 256 || session.bytes + bytes.length > 8 * 1024 * 1024) throw new ApiError(429, 'RateLimited', 'The connection request queue is full.', 1);
      session.pending.add(id); session.bytes += bytes.length; admitted = true;
      const result = await this.projects.get(session.project)!.invoke({ request, sessionId, peer: session.peer }, bytes.length);
      this.send(session, { type: 'result', id, ok: true, ...(request.type === 'snapshot' ? { data: result.data } : {}) });
      for (const event of result.events) this.notify(session.project, event);
    } catch (error) {
      if (!id) { session.socket.close(1008, 'The network request is invalid.'); return; }
      const failure = error instanceof ApiError ? error : new ApiError(500, 'ServerFailure', 'The server could not complete the request.');
      const code = /^[A-Za-z][A-Za-z0-9]{0,63}$/.test(failure.code) ? failure.code : 'ServerFailure';
      this.send(session, { type: 'result', id, ok: false, code, message: failure.message.slice(0, 4096), ...(failure.retryAfterSeconds === undefined ? {} : { retryAfterSeconds: failure.retryAfterSeconds }) });
    } finally { if (admitted && id) { session.pending.delete(id); session.bytes -= bytes.length; } }
  }

  private notify(project: string, event: NetEvent): void {
    for (const [id, session] of this.sessions) {
      if (session.project === project && (!event.target || event.target === id)) this.send(session, { type: 'event', module: event.module, event: event.event, args: event.args });
    }
  }

  async close(): Promise<void> {
    clearInterval(this.heartbeat);
    for (const session of this.sessions.values()) session.socket.terminate();
    this.sessions.clear();
    this.sockets.close();
    await Promise.all([...this.projects.values()].map(project => project.close()));
  }
}
