import type { WireValue } from './wire.ts';

export type NetRequest = { type: 'snapshot'; id: string; module: string; params: WireValue } | { type: 'rpc'; id: string; module: string; name: string; args: WireValue[] };
export interface Invocation { request: NetRequest; sessionId: string; peer: string | undefined }
export interface NetEvent { target: string; module: string; event: string; args: WireValue[] }
export interface LuaReply { data?: WireValue; events: NetEvent[] }
export interface Failure { code: string; message: string; retryAfterSeconds?: number }
