import express from 'express';
import type { ErrorRequestHandler, Request, Response } from 'express';
import { ipWriteIntervalSeconds } from './config.ts';
import type { ServerConfig } from './config.ts';
import { Authentication } from './authentication.ts';
import { ApiError } from './errors.ts';
import { identifier } from './identity.ts';
import { jsonObject, parseJson, serializeJson } from './json.ts';
import { Storage } from './storage.ts';
import { WriteRateLimiter } from './write-rate-limiter.ts';

function send(response: Response, value: unknown, oversizedMessage?: string): void {
  const text = serializeJson(value);
  if (oversizedMessage && Buffer.byteLength(text, 'utf8') > 2 * 1024 * 1024) {
    throw new ApiError(400, 'InvalidArgument', oversizedMessage);
  }
  response.type('application/json').send(text);
}

function query(request: Request, allowed: string[]): Record<string, string> {
  const values: Record<string, string> = Object.create(null) as Record<string, string>;
  for (const [name, value] of Object.entries(request.query)) {
    if (!allowed.includes(name) || typeof value !== 'string') {
      throw new ApiError(400, 'InvalidArgument', 'Query parameters must be supported, unrepeated string values.');
    }
    values[name] = value;
  }
  return values;
}

function integerQuery(value: string | undefined, fallback: number, minimum: number, maximum: number, name: string): number {
  if (value === undefined) return fallback;
  const number = Number(value);
  if (!/^\d+$/.test(value) || !Number.isSafeInteger(number) || number < minimum || number > maximum) {
    throw new ApiError(400, 'InvalidArgument', `${name} must be an integer from ${minimum} to ${maximum}.`);
  }
  return number;
}

function sampleCountQuery(value: string | undefined): bigint {
  if (value === undefined) return 0n;
  if (!/^\d+$/.test(value)) throw new ApiError(400, 'InvalidArgument', 'sampleCount must be a nonnegative integer.');
  const count = BigInt(value);
  if (count > 9_223_372_036_854_775_807n) throw new ApiError(400, 'InvalidArgument', 'sampleCount must fit a signed 64-bit integer.');
  return count;
}

function body(request: Request): Record<string, unknown> {
  if (!request.is('application/json') || typeof request.body !== 'string') {
    throw new ApiError(400, 'InvalidArgument', 'Content-Type application/json and a JSON body are required.');
  }
  return jsonObject(parseJson(request.body));
}

function adminMutation(request: Request): void {
  if (request.get('X-Ludork-Admin') !== '1') {
    throw new ApiError(403, 'AuthenticationFailed', 'The management request header is required.');
  }
}

export function createApplication(config: ServerConfig, dataDirectory: string): { app: express.Express; close: () => void } {
  const app = express();
  const auth = new Authentication();
  const storage = new Storage(dataDirectory);
  const writeRate = new WriteRateLimiter();
  app.disable('x-powered-by');
  app.disable('etag');
  app.set('case sensitive routing', true);
  app.use((_request, response, next) => {
    response.set({ 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff', 'Referrer-Policy': 'no-referrer' });
    next();
  });
  app.use(express.text({ type: 'application/json', limit: '1mb' }));

  const router = express.Router({ mergeParams: true, caseSensitive: true });
  app.use('/:project/api/v1', (request, _response, next) => {
    const project = String(request.params.project);
    if (!Object.hasOwn(config.projects, project)) {
      throw new ApiError(404, 'InvalidArgument', 'The project does not exist.');
    }
    next();
  }, router);

  const projectName = (request: Request): string => String(request.params.project);
  const verifyKey = (request: Request): void => auth.verifyKey(request.get('X-Ludork-Key'), config.projects[projectName(request)]);
  const accountName = (request: Request): string => identifier(request.params.account, 'Account');
  const categoryName = (request: Request): string => identifier(request.params.category, 'Category');

  router.get('/health', (request, response) => send(response, { service: 'LudorkServer', project: projectName(request) }));

  router.get('/accounts', async (request, response) => {
    verifyKey(request);
    const values = query(request, ['sampleCount', 'category']);
    const sampleCount = sampleCountQuery(values.sampleCount);
    const category = values.category === undefined ? undefined : identifier(values.category, 'Category');
    const accounts = await storage.listAccounts(projectName(request), category, sampleCount);
    send(response, { accounts }, 'The account list exceeds 2 MiB. Request a smaller positive sampleCount.');
  });

  router.get('/accounts/:account', async (request, response) => {
    verifyKey(request);
    send(response, { exists: await storage.accountExists(projectName(request), accountName(request)) });
  });

  router.get('/accounts/:account/categories/:category/fields', async (request, response) => {
    verifyKey(request);
    const values = query(request, ['before', 'limit']);
    const before = values.before === undefined ? undefined : identifier(values.before, 'Cursor');
    const limit = integerQuery(values.limit, 50, 1, 100, 'limit');
    const result = await storage.listFields(projectName(request), accountName(request), categoryName(request), before, limit);
    send(response, result, 'The field page exceeds 2 MiB. Request a smaller limit.');
  });

  router.get('/accounts/:account/categories/:category/fields/:field', async (request, response) => {
    verifyKey(request);
    const result = await storage.readField(projectName(request), accountName(request), categoryName(request), identifier(request.params.field, 'Field'));
    send(response, result);
  });

  router.post('/accounts/:account/write-tokens', (request, response) => {
    verifyKey(request);
    send(response, auth.issueWriteToken(projectName(request), accountName(request)));
  });

  router.put('/accounts/:account/categories/:category/fields/:field', async (request, response) => {
    const project = projectName(request);
    const account = accountName(request);
    const category = categoryName(request);
    const field = identifier(request.params.field, 'Field');
    const payload = body(request);
    if (!Object.hasOwn(payload, 'value')) throw new ApiError(400, 'InvalidArgument', 'The JSON body must contain value.');
    auth.consumeWriteToken(request.get('Authorization'), project, account);
    writeRate.admit(project, request.socket.remoteAddress, ipWriteIntervalSeconds(config.projects[project].ipWriteIntervalSeconds));
    await storage.writeField(project, account, category, field, payload.value);
    send(response, {});
  });

  router.post('/admin/login', async (request, response) => {
    adminMutation(request);
    const project = projectName(request);
    const payload = body(request);
    const session = await auth.login(project, config.projects[project], payload.key, payload.password);
    response.cookie(auth.cookieName(project), session, auth.cookieOptions(project));
    send(response, { project });
  });

  router.use('/admin', (request, _response, next) => {
    auth.verifySession(request, projectName(request));
    next();
  });

  router.get('/admin/session', (request, response) => send(response, { project: projectName(request) }));
  router.post('/admin/logout', (request, response) => {
    adminMutation(request);
    const project = projectName(request);
    auth.logout(request, project);
    response.clearCookie(auth.cookieName(project), { httpOnly: true, sameSite: 'strict', path: auth.cookieOptions(project).path });
    send(response, {});
  });
  router.get('/admin/accounts', async (request, response) => send(response, { accounts: await storage.accounts(projectName(request)) }));
  router.get('/admin/accounts/:account/categories', async (request, response) => {
    send(response, { categories: await storage.categories(projectName(request), accountName(request)) });
  });
  router.get('/admin/accounts/:account/categories/:category', async (request, response) => {
    const data = await storage.readCategory(projectName(request), accountName(request), categoryName(request));
    if (data === null) throw new ApiError(404, 'InvalidArgument', 'The category does not exist.');
    send(response, data);
  });

  app.use((_request, _response) => { throw new ApiError(404, 'InvalidArgument', 'The endpoint does not exist.'); });
  const errors: ErrorRequestHandler = (error: unknown, _request, response, _next) => {
    if (response.headersSent) return;
    let failure = error instanceof ApiError ? error : new ApiError(500, 'ServerFailure', 'The server could not complete the request.');
    if (!(error instanceof ApiError) && (error instanceof URIError || (error && typeof error === 'object' && 'status' in error && [400, 413, 415].includes(Number(error.status))))) {
      failure = new ApiError(400, 'InvalidArgument', 'The request encoding is invalid or the body exceeds 1 MiB.');
    }
    response.status(failure.status);
    const retry = failure.code === 'RateLimited' ? failure.retryAfterSeconds : undefined;
    if (retry !== undefined) response.set('Retry-After', String(retry));
    send(response, { code: failure.code, message: failure.message, ...(retry === undefined ? {} : { retryAfterSeconds: retry }) });
  };
  app.use(errors);
  return { app, close: () => { auth.close(); writeRate.close(); } };
}
