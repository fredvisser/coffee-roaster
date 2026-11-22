// Run with: node tests/host/pid_page_test.cjs
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

async function check() {
  const source = fs.readFileSync(`${__dirname}/../../src/network/Network.hpp`, 'utf8');
  const page = source.split('server.on("/pid", HTTP_GET')[1].split(')rawliteral");')[0];
  const elements = Object.fromEntries([...page.matchAll(/ id="([^"]+)"/g)]
    .map(([, id]) => [id, { id, validity: { valid: true }, get valueAsNumber() { return Number(this.value); } }]));
  const posts = [];
  let tune = { phase: 'IDLE' }, gains = { kp: 80, ki: 0.364, kd: 500 };
  let pollStatus;
  const context = vm.createContext({
    document: { getElementById: id => elements[id] },
    setInterval: poll => { pollStatus = poll; },
    AbortSignal,
    fetch: async (path, options) => {
      if (options.method === 'POST') {
        posts.push(path);
        if (path.startsWith('/api/calibrate-pid?')) {
          assert.equal(new URL(path, 'http://roaster.local').searchParams.get('load'), '0.5 cups of beans');
          tune = { running: true, phase: 'IDENTIFY', elapsedSeconds: 30 };
        } else if (path === '/api/calibrate-pid/cancel') {
          tune = { lastError: 'cancelled' };
        } else if (path === '/api/pid/apply?id=14') {
          tune.outcome = 'applied';
          gains = { kp: 10.522, ki: 0.8094, kd: 0 };
        } else {
          assert.equal(path, '/api/pid');
          gains = JSON.parse(options.body);
        }
      }
      return { ok: true, json: async () => ({ ...(path === '/api/calibrate-pid/status' ? tune : gains) }) };
    }
  });
  const settle = () => new Promise(resolve => setImmediate(resolve));
  vm.runInContext(page.match(/<script>([\s\S]+?)<\/script>/)[1], context);
  await settle();
  assert.equal(elements.kp.value, 80);
  await elements.start.onclick();
  assert.equal(elements.cancel.hidden, false);
  assert.equal(elements.save.disabled, true);
  await elements.cancel.onclick();
  assert.match(elements['tune-status'].textContent, /cancelled/i);
  await elements.start.onclick();
  tune = { outcome: 'verified', runId: 14, elapsedSeconds: 198,
    model: { recommendedKp: 10.522, recommendedKi: 0.8094, recommendedKd: 0 } };
  pollStatus();
  await settle();
  assert.equal(elements.apply.hidden, false);
  assert.match(elements['tune-status'].textContent, /Proposed: Kp 10\.522/);
  assert.equal(gains.kp, 80);
  await elements.apply.onclick();
  assert.equal(elements.apply.hidden, true);
  assert.equal(elements.kp.value, 10.522);
  const submit = () => elements.manual.onsubmit({ preventDefault() {} });
  elements.kp.value = 0;
  submit();
  assert.match(elements['manual-message'].textContent, /greater than zero/);
  elements.kp.value = 11;
  submit();
  await settle();
  assert.deepEqual(gains, { kp: 11, ki: 0.8094, kd: 0 });
  assert.deepEqual(posts.map(path => path.startsWith('/api/calibrate-pid?') ? path.split('?')[0] : path),
    ['/api/calibrate-pid', '/api/calibrate-pid/cancel', '/api/calibrate-pid', '/api/pid/apply?id=14', '/api/pid']);
  console.log('PID page flow passed');
}

check().catch(error => { console.error(error); process.exitCode = 1; });
