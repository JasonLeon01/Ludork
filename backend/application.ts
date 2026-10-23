import express from 'express';
import type { ErrorRequestHandler, Request, Response } from 'express';
import type { Server } from 'node:http';
import { projectsPath } from './config.ts';
import type { ServerConfig } from './config.ts';
import { Authentication } from './authentication.ts';
import { ApiError } from './errors.ts';
import { identifier } from './identity.ts';
import { jsonObject, parseJson } from './json.ts';
import { Storage } from './storage.ts';
import { WriteRateLimiter } from './write-rate-limiter.ts';
import { LuaServices } from './lua-services.ts';
import { NetServer } from './net-server.ts';

function send(response: Response, value: unknown): void { response.json(value); }
function body(request: Request): Record<string, unknown> {
  if (!request.is('application/json') || typeof request.body !== 'string') throw new ApiError(400, 'InvalidArgument', 'Content-Type application/json and a JSON body are required.');
  return jsonObject(parseJson(request.body));
}
function adminMutation(request: Request): void {
  if (request.get('X-Ludork-Admin') !== '1') throw new ApiError(403, 'AuthenticationFailed', 'The management request header is required.');
}

export async function createApplication(config: ServerConfig, dataDirectory: string, projectsDirectory = projectsPath): Promise<{ app: express.Express; attach: (server: Server) => void; close: () => Promise<void> }> {
  const app = express();
  const auth = new Authentication();
  const storage = new Storage(dataDirectory);
  const writeRate = new WriteRateLimiter();
  const net = new NetServer(config, auth, new LuaServices(storage, writeRate), projectsDirectory);
  try { await net.start(); }
  catch (error) { auth.close(); writeRate.close(); throw error; }
  app.disable('x-powered-by'); app.disable('etag'); app.set('case sensitive routing', true);
  app.use((_request, response, next) => {
    response.set({ 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff', 'Referrer-Policy': 'no-referrer' }); next();
  });
  app.use(express.text({ type: 'application/json', limit: '1mb' }));
  const router = express.Router({ mergeParams: true, caseSensitive: true });
  app.use('/:project/api/v1', (request, _response, next) => {
    if (!Object.hasOwn(config.projects, String(request.params.project))) throw new ApiError(404, 'InvalidArgument', 'The project does not exist.');
    next();
  }, router);
  const projectName = (request: Request): string => String(request.params.project);
  const accountName = (request: Request): string => identifier(request.params.account, 'Account');
  const categoryName = (request: Request): string => identifier(request.params.category, 'Category');

  router.get('/health', (request, response) => send(response, { service: 'LudorkServer', project: projectName(request) }));
  router.post('/admin/login', async (request, response) => {
    adminMutation(request);
    const project = projectName(request);
    const payload = body(request);
    const session = await auth.login(project, config.projects[project], payload.key, payload.password);
    response.cookie(auth.cookieName(project), session, auth.cookieOptions(project));
    send(response, { project });
  });
  router.use('/admin', (request, _response, next) => { auth.verifySession(request, projectName(request)); next(); });
  router.get('/admin/session', (request, response) => send(response, { project: projectName(request) }));
  router.post('/admin/logout', (request, response) => {
    adminMutation(request);
    const project = projectName(request);
    auth.logout(request, project);
    response.clearCookie(auth.cookieName(project), { httpOnly: true, sameSite: 'strict', path: auth.cookieOptions(project).path });
    send(response, {});
  });
  router.get('/admin/accounts', async (request, response) => send(response, { accounts: await storage.accounts(projectName(request)) }));
  router.get('/admin/accounts/:account/categories', async (request, response) => send(response, { categories: await storage.categories(projectName(request), accountName(request)) }));
  router.get('/admin/accounts/:account/categories/:category', async (request, response) => {
    const value = await storage.readCategory(projectName(request), accountName(request), categoryName(request));
    if (value === null) throw new ApiError(404, 'InvalidArgument', 'The category does not exist.');
    send(response, { value });
  });

  app.use(() => { throw new ApiError(404, 'InvalidArgument', 'The endpoint does not exist.'); });
  const errors: ErrorRequestHandler = (error: unknown, _request, response, _next) => {
    if (response.headersSent) return;
    let failure = error instanceof ApiError ? error : new ApiError(500, 'ServerFailure', 'The server could not complete the request.');
    if (!(error instanceof ApiError) && (error instanceof URIError || (error && typeof error === 'object' && 'status' in error && [400, 413, 415].includes(Number(error.status))))) failure = new ApiError(400, 'InvalidArgument', 'The request encoding is invalid or the body exceeds 1 MiB.');
    response.status(failure.status);
    if (failure.retryAfterSeconds !== undefined) response.set('Retry-After', String(failure.retryAfterSeconds));
    send(response, { code: failure.code, message: failure.message, ...(failure.retryAfterSeconds === undefined ? {} : { retryAfterSeconds: failure.retryAfterSeconds }) });
  };
  app.use(errors);
  return { app, attach: server => net.attach(server), close: async () => { auth.close(); writeRate.close(); await net.close(); } };
}
