import express from 'express';
import type { ErrorRequestHandler } from 'express';
import { createServer, request as proxyRequest } from 'node:http';
import type { IncomingHttpHeaders } from 'node:http';
import { isIP } from 'node:net';
import { join } from 'node:path';
import { root } from './config.ts';

const app = express();
const assets = join(root, 'frontend', 'assets');
const index = join(root, 'frontend', 'index.html');
const allowedHosts = new Set((process.env.LUDORK_ALLOWED_HOSTS ?? '').split(',').map(value => value.trim().toLowerCase()).filter(Boolean));

function allowedHost(host: string | undefined): boolean {
  if (!host || /[\s/\\@?#]/.test(host)) return false;
  try {
    const hostname = new URL(`http://${host}`).hostname.toLowerCase();
    return isIP(hostname.replace(/^\[|\]$/g, '')) !== 0 || hostname === 'localhost' || hostname.endsWith('.localhost') || allowedHosts.has(hostname);
  } catch { return false; }
}

function proxyHeaders(headers: IncomingHttpHeaders): IncomingHttpHeaders {
  const result = { ...headers };
  const excluded = ['connection', 'keep-alive', 'proxy-authenticate', 'proxy-authorization', 'te', 'trailer', 'transfer-encoding', 'upgrade', ...(headers.connection ?? '').split(',').map(value => value.trim().toLowerCase())];
  for (const name of excluded) delete result[name];
  return result;
}

app.disable('x-powered-by');
app.disable('etag');
app.set('case sensitive routing', true);
app.use((request, response, next) => {
  response.set({ 'X-Content-Type-Options': 'nosniff', 'Referrer-Policy': 'no-referrer', 'Cache-Control': 'no-store' });
  if (!allowedHost(request.headers.host)) { response.status(403).type('text').send('This host is not allowed.'); return; }
  next();
});
app.use((request, response, next) => {
  if (!/^\/[^/]+\/api\/v1(?:\/|$)/.test(request.path)) { next(); return; }
  const upstream = proxyRequest({ hostname: '127.0.0.1', port: 7777, method: request.method, path: request.originalUrl, headers: proxyHeaders(request.headers) }, result => {
    response.writeHead(result.statusCode ?? 502, proxyHeaders(result.headers));
    result.on('error', () => response.destroy());
    result.pipe(response);
  });
  upstream.setTimeout(15_000, () => upstream.destroy());
  upstream.on('error', () => {
    if (response.headersSent) response.destroy();
    else response.status(502).json({ code: 'ServerFailure', message: 'The backend is unavailable.' });
  });
  request.on('aborted', () => upstream.destroy());
  response.on('close', () => { if (!response.writableEnded) upstream.destroy(); });
  request.pipe(upstream);
});
app.get(['/', '/:project', '/:project/'], (_request, response) => response.sendFile(index));
app.use('/assets', express.static(assets, { dotfiles: 'deny', index: false, redirect: false, fallthrough: false, immutable: true, maxAge: '1y' }));
app.use((_request, response) => { response.status(404).type('text').send('The resource does not exist.'); });
const errors: ErrorRequestHandler = (_error, _request, response, _next) => {
  if (!response.headersSent) response.status(404).type('text').send('The resource does not exist.');
};
app.use(errors);

const server = createServer(app);
server.requestTimeout = 15_000;
server.headersTimeout = 10_000;
let stopping = false;
function stop(): void {
  if (stopping) return;
  stopping = true;
  const timeout = setTimeout(() => server.closeAllConnections(), 10_000);
  timeout.unref();
  server.close(() => { clearTimeout(timeout); process.exit(process.exitCode ?? 0); });
}
process.on('SIGINT', stop);
process.on('SIGTERM', stop);
process.on('disconnect', stop);
process.on('message', message => { if (message === 'shutdown') stop(); });
server.once('error', error => {
  console.error(error.message.includes('EADDRINUSE') ? 'Port 3333 is already in use.' : 'The management website could not listen on port 3333.');
  process.exitCode = 1;
  stop();
});
server.once('listening', () => { console.log('LudorkServer management listening on http://0.0.0.0:3333'); process.send?.('ready'); });
server.listen(3333, '0.0.0.0');
