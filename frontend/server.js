const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');

const PORT = Number(process.env.PORT || 3000);
const FRONTEND_DIR = path.join(__dirname, 'public');
const DEFAULT_BINARY = path.join(
  __dirname,
  '..',
  'build',
  process.platform === 'win32' ? 'mini_cpp_parser.exe' : 'mini_cpp_parser'
);
const PARSER_BINARY = path.resolve(process.env.PARSER_BIN || DEFAULT_BINARY);
const MAX_BODY_SIZE = 1024 * 1024;

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.svg': 'image/svg+xml'
};

function sendJson(response, statusCode, data) {
  response.writeHead(statusCode, {
    'Content-Type': 'application/json; charset=utf-8',
    'Cache-Control': 'no-store'
  });
  response.end(JSON.stringify(data));
}

function readRequestBody(request) {
  return new Promise((resolve, reject) => {
    let body = '';
    request.setEncoding('utf8');

    request.on('data', chunk => {
      body += chunk;
      if (body.length > MAX_BODY_SIZE) {
        reject(new Error('Request body is too large (limit: 1 MB).'));
        request.destroy();
      }
    });
    request.on('end', () => resolve(body));
    request.on('error', reject);
  });
}

function runParser(source) {
  return new Promise((resolve, reject) => {
    if (!fs.existsSync(PARSER_BINARY)) {
      reject(new Error(`Parser executable was not found: ${PARSER_BINARY}. Build the C++ project first.`));
      return;
    }

    const child = spawn(PARSER_BINARY, [], { windowsHide: true });
    let stdout = '';
    let stderr = '';
    let exceededOutputLimit = false;

    child.stdout.setEncoding('utf8');
    child.stderr.setEncoding('utf8');

    child.stdout.on('data', chunk => {
      stdout += chunk;
      if (stdout.length > 5 * 1024 * 1024) {
        exceededOutputLimit = true;
        child.kill();
      }
    });
    child.stderr.on('data', chunk => {
      stderr += chunk;
    });
    child.on('error', reject);
    child.on('close', code => {
      if (exceededOutputLimit) {
        reject(new Error('Parser response is too large.'));
        return;
      }
      if (code !== 0) {
        reject(new Error(stderr.trim() || `Parser process ended with code ${code}.`));
        return;
      }
      try {
        resolve(JSON.parse(stdout));
      } catch {
        reject(new Error('The C++ parser returned invalid JSON.'));
      }
    });

    child.stdin.end(source);
  });
}

async function handleRequest(request, response) {
  const url = new URL(request.url, `http://${request.headers.host || 'localhost'}`);

  if (request.method === 'POST' && url.pathname === '/api/parse') {
    try {
      const body = await readRequestBody(request);
      const payload = JSON.parse(body);
      if (!payload || typeof payload.source !== 'string') {
        sendJson(response, 400, { ok: false, error: 'Expected JSON with a string field named "source".' });
        return;
      }
      const result = await runParser(payload.source);
      sendJson(response, 200, result);
    } catch (error) {
      sendJson(response, 500, { ok: false, error: error.message || 'Unexpected server error.' });
    }
    return;
  }

  if (request.method !== 'GET' && request.method !== 'HEAD') {
    response.writeHead(405, { Allow: 'GET, HEAD, POST' });
    response.end('Method Not Allowed');
    return;
  }

  const pathname = url.pathname === '/' ? '/index.html' : url.pathname;
  const allowedFiles = new Set(['/index.html', '/style.css', '/app.js']);
  if (!allowedFiles.has(pathname)) {
    response.writeHead(404);
    response.end('Not Found');
    return;
  }

  const filePath = path.join(FRONTEND_DIR, path.basename(pathname));
  fs.readFile(filePath, (error, content) => {
    if (error) {
      response.writeHead(500);
      response.end('Could not read frontend file.');
      return;
    }
    response.writeHead(200, {
      'Content-Type': MIME_TYPES[path.extname(filePath)],
      'Cache-Control': 'no-cache'
    });
    response.end(request.method === 'HEAD' ? undefined : content);
  });
}

http.createServer(handleRequest).listen(PORT, '127.0.0.1', () => {
  console.log(`Mini C++ Parser is running at http://127.0.0.1:${PORT}`);
  console.log(`C++ executable: ${PARSER_BINARY}`);
});
