import { readFile, readdir } from 'node:fs/promises';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { LuaFactory, LuaReturn } from 'wasmoon';
import type { LuaEngine } from 'wasmoon';
import { fieldValue, maxFrameBytes, networkName, textValue, validateArgs, validateWire } from './wire.ts';
import type { WireValue } from './wire.ts';
import type { Failure, Invocation, NetEvent } from './net-protocol.ts';
import { ApiError } from './errors.ts';

let engine: LuaEngine | undefined;
let active: { id: number; invocation: Invocation; events: NetEvent[]; bytes: number } | undefined;
let nextHostId = 0;
let projectName = '';
let faulted = false;
const pending = new Map<number, (value: unknown) => void>();

async function mount(factory: LuaFactory, source: string, target: string): Promise<void> {
  for (const entry of await readdir(source, { withFileTypes: true })) {
    if (entry.isDirectory() && entry.name !== 'stub') await mount(factory, join(source, entry.name), `${target}/${entry.name}`);
    else if (entry.isFile() && entry.name.endsWith('.lua') && !entry.name.endsWith('.d.lua')) await factory.mountFile(`${target}/${entry.name}`, await readFile(join(source, entry.name)));
  }
}

async function run(code: string, filename?: string): Promise<unknown> {
  const lua = engine!;
  const thread = lua.global.newThread();
  const reference = lua.global.getTop();
  const deadline = Date.now() + 9_000;
  faulted = false;
  try {
    if (filename) thread.loadFile(filename); else thread.loadString(code);
    thread.setTimeout(deadline);
    const result = (await thread.run(0))[0];
    if (Date.now() >= deadline) throw new ApiError(504, 'Timeout', 'The Lua execution time limit was exceeded.');
    return result;
  } catch (error) {
    if (Date.now() >= deadline) { faulted = true; throw new ApiError(504, 'Timeout', 'The Lua execution time limit was exceeded.'); }
    if (thread.lua.lua_status(thread.address) === LuaReturn.ErrorMem) faulted = true;
    throw error;
  } finally {
    thread.lua.lua_sethook(thread.address, null, 0, 0);
    thread.close();
    lua.global.remove(reference);
  }
}

async function initialize(projectDirectory: string, entry: string, name: string): Promise<void> {
  projectName = name;
  const factory = new LuaFactory();
  await mount(factory, fileURLToPath(new URL('../lua/', import.meta.url)), '/app');
  await mount(factory, projectDirectory, '/project');
  engine = await factory.createEngine({ enableProxy: false, injectObjects: false, traceAllocations: true });
  engine.global.setMemoryMax(64 * 1024 * 1024);
  engine.global.set('__HostCall', (name: string, text: string): Promise<unknown> => {
    if (!active || typeof text !== 'string' || Buffer.byteLength(text) > maxFrameBytes) throw new Error('Invalid Lua host request.');
    const args = validateWire(JSON.parse(text));
    const hostId = ++nextHostId;
    return new Promise(resolve => {
      pending.set(hostId, resolve);
      process.send?.({ type: 'host_call', id: active!.id, hostId, name, args });
    });
  });
  engine.global.set('__QueueEvent', (target: string, event: string, text: string): void => {
    if (!active || active.invocation.request.type !== 'rpc') throw new Error('Only RPC handlers can send notifications.');
    if (typeof target !== 'string' || (target && !/^[a-f0-9-]{36}$/.test(target))) throw new Error('Invalid notification session.');
    networkName(event, 'Event');
    const args = validateArgs(JSON.parse(text));
    const notification = { target, module: active.invocation.request.module, event, args };
    const bytes = Buffer.byteLength(JSON.stringify({ type: 'event', module: notification.module, event, args }));
    if (bytes > maxFrameBytes || active.events.length >= 256 || active.bytes + bytes > 8 * 1024 * 1024) throw new Error('The event queue is full.');
    active.bytes += bytes;
    active.events.push(notification);
  });
  await run(`package.path = "/app/?.lua;/project/?.lua"
package.cpath = ""
package.loadlib = nil
io = nil
os = nil
debug = nil
dofile = nil
loadfile = nil
load = nil
package.loaded.io = nil
package.loaded.os = nil
package.loaded.debug = nil
local Net = require("Framework.Net")
__Dispatch = Net.Dispatch`);
  await run('', `/project/${entry}`);
  process.send?.({ type: 'ready' });
}

function failure(error: unknown): Failure {
  return { code: error instanceof ApiError ? error.code : 'ServerFailure', message: error instanceof Error ? error.message : 'The Lua handler failed.' };
}

async function invoke(id: number, invocation: Invocation): Promise<void> {
  if (active) throw new Error('The Lua worker received overlapping invocations.');
  active = { id, invocation, events: [], bytes: 0 };
  try {
    engine!.global.set('__Request', { ...invocation.request, sessionId: invocation.sessionId, projectName, peerAddress: invocation.peer ?? '' });
    const text = await run('return __Dispatch(__Request)');
    if (pending.size > 0) { faulted = true; throw new Error('The Lua handler finished with unfinished host operations.'); }
    if (typeof text !== 'string' || Buffer.byteLength(text) > maxFrameBytes) throw new Error('The Lua response exceeds 2 MiB.');
    const result = validateWire(JSON.parse(text));
    const ok = fieldValue(result, 'ok');
    if (ok[0] !== 'bool') throw new Error('The Lua response is invalid.');
    if (!ok[1]) {
      const retry = fieldValue(result, 'retryAfterSeconds');
      const reset = fieldValue(result, 'reset');
      process.send?.({ type: 'result', id, ok: false, reset: reset[0] === 'bool' && reset[1], code: textValue(fieldValue(result, 'code'), 'Code'), message: textValue(fieldValue(result, 'message'), 'Message'), ...(retry[0] === 'int' ? { retryAfterSeconds: Number(retry[1]) } : {}) });
    } else {
      const data: WireValue | undefined = invocation.request.type === 'snapshot' ? fieldValue(result, 'data') : undefined;
      if (data && data[0] !== 'table') throw new Error('Snapshot must return an ordinary Lua table.');
      process.send?.({ type: 'result', id, ok: true, data, events: active.events });
    }
  } catch (error) { process.send?.({ type: 'result', id, ok: false, reset: faulted || pending.size > 0, ...failure(error) }); }
  finally { active = undefined; engine!.global.set('__Request', undefined); }
}

process.on('message', (message: { type: string; directory?: string; entry: string; project: string; id: number; hostId: number; invocation: Invocation; ok: boolean; data?: WireValue } & Failure) => {
  if (message.type === 'init') void initialize(message.directory!, message.entry, message.project).catch(error => { process.send?.({ type: 'fatal', ...failure(error) }); process.exitCode = 1; process.disconnect?.(); });
  else if (message.type === 'invoke') void invoke(message.id, message.invocation).catch(error => { process.send?.({ type: 'fatal', ...failure(error) }); process.disconnect?.(); });
  else if (message.type === 'host_result') {
    pending.get(message.hostId)?.(message);
    pending.delete(message.hostId);
  }
  else if (message.type === 'shutdown') { engine?.global.close(); process.exit(0); }
});
process.on('disconnect', () => process.exit(0));
