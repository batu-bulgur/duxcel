import { createServer } from '../../design/prototype/node_modules/vite/dist/node/index.js';
import react from '../../design/prototype/node_modules/@vitejs/plugin-react/dist/index.js';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import fs from 'node:fs';
import { startHttpServer, getStore, listSessions, getSessionWithAnnotations, eventBus } from 'agentation-mcp';

const directory = path.dirname(fileURLToPath(import.meta.url));
const project = path.resolve(directory, '../..');
const reference = path.join(project, 'design/prototype');
const local = path.join(directory, '.local');
fs.mkdirSync(local, { recursive: true });
const contextFile = path.join(local, 'feedback.json');
const database = fs.existsSync(contextFile) ? JSON.parse(fs.readFileSync(contextFile, 'utf8')) : { next: 1, issues: {} };
const allowedOrigins = ['http://localhost:4174', 'http://127.0.0.1:4174'];
process.env.AGENTATION_CORS_ORIGINS = allowedOrigins.join(',');
const save = () => {
  fs.writeFileSync(contextFile + '.tmp', JSON.stringify(database, null, 2));
  fs.renameSync(contextFile + '.tmp', contextFile);
};
const key = annotation => `${annotation.timestamp}:${annotation.elementPath}`;
function issueFor(annotation) {
  return database.issues[key(annotation)] ||= { id: `UX-${String(database.next++).padStart(3, '0')}`, capturedAt: new Date().toISOString() };
}
function fingerprint() {
  const hash = createHash('sha256');
  function add(file) {
    if (fs.statSync(file).isDirectory()) for (const name of fs.readdirSync(file).sort()) add(path.join(file, name));
    else { hash.update(path.relative(reference, file)); hash.update(fs.readFileSync(file)); }
  }
  for (const name of ['src', 'public', 'index.html', 'package-lock.json']) add(path.join(reference, name));
  return hash.digest('hex');
}
const provenance = { revision: execFileSync('git', ['rev-parse', 'HEAD'], { cwd: project, encoding: 'utf8' }).trim(), referenceSha256: fingerprint(), startedAt: new Date().toISOString() };
fs.writeFileSync(path.join(local, 'provenance.json'), JSON.stringify(provenance, null, 2));
const isOurs = session => { try { return allowedOrigins.includes(new URL(session.url).origin); } catch { return false; } };
function queue() {
  const annotations = listSessions().filter(isOurs).flatMap(session => getSessionWithAnnotations(session.id)?.annotations || []);
  return annotations.map(annotation => ({ ...issueFor(annotation), annotation })).sort((a, b) => a.id.localeCompare(b.id));
}

// The official server owns annotations, statuses, threads and durable SQLite storage.
// The sidecar only adds Duxcel-specific state/provenance; it never imports App internals.
getStore();
eventBus.subscribe(event => {
  if (!isOurs(getSessionWithAnnotations(event.sessionId) || {})) return;
  if (event.type.startsWith('annotation.') && event.payload?.elementPath) {
    const issue = issueFor(event.payload);
    issue.lastEvent = event.type;
    issue.annotation = event.payload;
    save();
  }
});
const feedbackServer = startHttpServer(4747, undefined, { host: '127.0.0.1' });

async function readJson(req) {
  let body = '';
  for await (const chunk of req) { body += chunk; if (body.length > 128 * 1024) throw new Error('Feedback context too large'); }
  return JSON.parse(body);
}
function json(res, value, status = 200) { res.writeHead(status, { 'Content-Type': 'application/json', 'Cache-Control': 'no-store' }); res.end(JSON.stringify(value)); }
const server = await createServer({
  configFile: false, root: reference, cacheDir: path.join(directory, 'node_modules/.vite'),
  resolve: { alias: { react: path.join(reference, 'node_modules/react'), 'react-dom': path.join(reference, 'node_modules/react-dom'), agentation: path.join(directory, 'node_modules/agentation') }, dedupe: ['react', 'react-dom'] },
  optimizeDeps: { include: ['react', 'react-dom/client', 'agentation'] },
  server: { host: '127.0.0.1', port: 4174, strictPort: true, fs: { allow: [reference, directory] }, proxy: { '/__agentation': { target: 'http://127.0.0.1:4747', changeOrigin: true, rewrite: url => url.replace(/^\/__agentation/, '') } } },
  plugins: [react(), {
    name: 'duxcel-review-sidecar',
    transformIndexHtml: { order: 'post', handler: () => [{ tag: 'script', attrs: { type: 'module', src: `/@fs/${directory}/review.jsx` }, injectTo: 'body' }] },
    configureServer(vite) {
      vite.middlewares.use(async (req, res, next) => {
        if (!req.url.startsWith('/__review')) return next();
        if (req.headers.origin && !allowedOrigins.includes(req.headers.origin)) return json(res, { error: 'Origin not allowed' }, 403);
        if (!['localhost:4174', '127.0.0.1:4174'].includes(req.headers.host)) return json(res, { error: 'Host not allowed' }, 403);
        try {
          const url = new URL(req.url, allowedOrigins[0]);
          if (url.pathname === '/__review/provenance' && req.method === 'GET') return json(res, provenance);
          if (url.pathname === '/__review/queue' && req.method === 'GET') { const issues = queue(); save(); return json(res, { provenance, issues }); }
          if (url.pathname === '/__review/context' && req.method === 'POST') {
            const { annotation, context } = await readJson(req);
            if (!annotation?.elementPath || !Number.isFinite(annotation.timestamp)) return json(res, { error: 'Missing target' }, 400);
            const issue = issueFor(annotation);
            issue.context ||= { ...context, provenance };
            issue.annotation ||= annotation;
            save(); return json(res, { id: issue.id });
          }
          if (url.pathname === '/__review/' && req.method === 'GET') {
            res.writeHead(200, { 'Content-Type': 'text/html', 'Cache-Control': 'no-store' }); return res.end(fs.readFileSync(path.join(directory, 'queue.html')));
          }
          return json(res, { error: 'Not found' }, 404);
        } catch (error) { console.error(error); return json(res, { error: error.message }, 500); }
      });
    }
  }]
});
await server.listen();
console.log('Duxcel review: http://localhost:4174/ • Queue: http://localhost:4174/__review/');
for (const signal of ['SIGINT', 'SIGTERM']) process.on(signal, async () => { await server.close(); feedbackServer.close(); process.exit(0); });
