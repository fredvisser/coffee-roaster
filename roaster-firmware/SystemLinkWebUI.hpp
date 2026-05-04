#ifndef SYSTEMLINK_WEB_UI_HPP
#define SYSTEMLINK_WEB_UI_HPP

#include <Arduino.h>

const char SYSTEMLINK_CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>SystemLink Configuration</title>
  <style>
    * { box-sizing: border-box; }
    body {
      margin: 0;
      font-family: Georgia, 'Times New Roman', serif;
      background:
        radial-gradient(circle at top left, rgba(245, 208, 66, 0.16), transparent 28%),
        linear-gradient(160deg, #1d1a15 0%, #30261d 48%, #111214 100%);
      color: #f3eadb;
      min-height: 100vh;
      padding: 24px;
    }
    .topnav {
      max-width: 880px;
      margin: 0 auto 18px;
      padding: 14px;
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      background: rgba(24, 21, 18, 0.92);
      border: 1px solid rgba(242, 203, 117, 0.22);
      border-radius: 16px;
      box-shadow: 0 18px 40px rgba(0, 0, 0, 0.28);
    }
    .topnav a {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-width: 120px;
      padding: 10px 14px;
      border-radius: 999px;
      background: rgba(255, 255, 255, 0.06);
      border: 1px solid rgba(242, 203, 117, 0.14);
      color: #f3eadb;
      text-decoration: none;
      font-weight: 700;
      font-size: 14px;
    }
    .topnav a.active {
      background: linear-gradient(135deg, #f2cb75, #c48337);
      color: #22180e;
      border-color: transparent;
    }
    .shell {
      max-width: 880px;
      margin: 0 auto;
      background: rgba(24, 21, 18, 0.92);
      border: 1px solid rgba(242, 203, 117, 0.22);
      border-radius: 20px;
      overflow: hidden;
      box-shadow: 0 24px 60px rgba(0, 0, 0, 0.35);
    }
    .hero {
      padding: 28px;
      background: linear-gradient(135deg, rgba(191, 128, 44, 0.35), rgba(73, 42, 19, 0.15));
      border-bottom: 1px solid rgba(242, 203, 117, 0.15);
    }
    h1 {
      margin: 0 0 8px;
      font-size: 34px;
      letter-spacing: 0.02em;
    }
    .subtitle {
      color: #dbcab6;
      font-size: 15px;
      line-height: 1.5;
      max-width: 620px;
    }
    .content {
      padding: 28px;
      display: grid;
      gap: 20px;
    }
    .card {
      background: rgba(255, 255, 255, 0.03);
      border: 1px solid rgba(242, 203, 117, 0.12);
      border-radius: 16px;
      padding: 20px;
    }
    .card h2 {
      margin: 0 0 12px;
      font-size: 18px;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
      gap: 16px;
    }
    label {
      display: block;
      margin-bottom: 8px;
      font-size: 13px;
      color: #dbcab6;
      text-transform: uppercase;
      letter-spacing: 0.08em;
    }
    input[type="text"],
    input[type="password"] {
      width: 100%;
      padding: 12px 14px;
      border: 1px solid rgba(242, 203, 117, 0.18);
      border-radius: 12px;
      background: #181614;
      color: #f7f0e4;
      font-size: 15px;
    }
    input:focus {
      outline: none;
      border-color: #f2cb75;
      box-shadow: 0 0 0 3px rgba(242, 203, 117, 0.14);
    }
    .toggle {
      display: flex;
      align-items: center;
      gap: 12px;
      font-size: 16px;
    }
    .toggle input {
      width: 20px;
      height: 20px;
    }
    .actions {
      display: flex;
      gap: 12px;
      flex-wrap: wrap;
      align-items: center;
    }
    button, a.button {
      appearance: none;
      border: none;
      cursor: pointer;
      padding: 12px 18px;
      border-radius: 999px;
      text-decoration: none;
      font-weight: 700;
      font-size: 14px;
      transition: transform 0.18s ease, opacity 0.18s ease;
    }
    button:hover, a.button:hover {
      transform: translateY(-1px);
      opacity: 0.95;
    }
    .primary {
      background: linear-gradient(135deg, #f2cb75, #c48337);
      color: #22180e;
    }
    .secondary {
      background: rgba(255, 255, 255, 0.08);
      color: #f3eadb;
      border: 1px solid rgba(242, 203, 117, 0.14);
    }
    .status {
      min-height: 24px;
      color: #dbcab6;
      font-size: 14px;
    }
    .meta {
      display: flex;
      gap: 18px;
      flex-wrap: wrap;
      color: #bca890;
      font-size: 14px;
    }
    .hint {
      color: #bca890;
      font-size: 13px;
      line-height: 1.5;
      margin-top: 8px;
    }
    @media (max-width: 640px) {
      body { padding: 14px; }
      .hero, .content { padding: 18px; }
      h1 { font-size: 28px; }
    }
  </style>
</head>
<body>
  <nav class="topnav">
    <a href="/">Home</a>
    <a href="/console">Console</a>
    <a href="/profile">Profiles</a>
    <a href="/pid">PID</a>
    <a href="/update">Update</a>
    <a class="active" href="/systemlink">SystemLink</a>
  </nav>
  <div class="shell">
    <div class="hero">
      <h1>SystemLink</h1>
      <div class="subtitle">Configure optional publishing to NI SystemLink. The roaster can write live tags during a roast and publish a test result, including a 1 Hz roast trace CSV, when the roast passes, is terminated, or errors out.</div>
    </div>

    <div class="content">
      <div class="card">
        <div class="toggle">
          <input id="enabled" type="checkbox" />
          <strong>Enable SystemLink publishing</strong>
        </div>
        <div class="hint">When enabled, the roaster uses the configured API key and system identity to write live tags under the system minion prefix and publish completed roast results.</div>
      </div>

      <div class="card">
        <h2>Connection</h2>
        <div class="grid">
          <div>
            <label for="apiUrl">API Base URL</label>
            <input id="apiUrl" type="text" placeholder="https://dev-api.lifecyclesolutions.ni.com" />
          </div>
          <div>
            <label for="workspaceId">Workspace ID</label>
            <input id="workspaceId" type="text" placeholder="6dc0739e-e41a-47cf-a340-41cd3dcc1490" />
          </div>
          <div>
            <label for="systemId">System Minion ID</label>
            <input id="systemId" type="text" placeholder="376d27ed-1d82-4dcb-bdef-a07b0794c88d" />
          </div>
          <div>
            <label for="apiKey">API Key</label>
            <input id="apiKey" type="password" placeholder="Paste a new API key to replace the stored one" />
            <div class="hint" id="apiKeyState">No API key stored.</div>
          </div>
        </div>
      </div>

      <div class="card">
        <div class="meta">
          <span>Live tags: <strong>systemId.chamberTemp</strong>, <strong>systemId.targetTemp</strong>, <strong>systemId.roastState</strong>, <strong>systemId.roastProgress</strong></span>
          <span>Result payload: profile metadata, roast outcome, attached CSV trace</span>
        </div>
      </div>

      <div class="actions">
        <button class="primary" onclick="saveConfig()">Save Configuration</button>
        <button class="secondary" onclick="clearApiKey()">Clear Stored API Key</button>
        <a class="button secondary" href="/">Back to Home</a>
      </div>
      <div class="status" id="status"></div>
    </div>
  </div>

  <script>
    async function loadConfig() {
      const status = document.getElementById('status');
      status.textContent = 'Loading configuration...';
      try {
        const response = await fetch('/api/systemlink');
        const data = await response.json();
        document.getElementById('enabled').checked = !!data.enabled;
        document.getElementById('apiUrl').value = data.apiUrl || '';
        document.getElementById('workspaceId').value = data.workspaceId || '';
        document.getElementById('systemId').value = data.systemId || '';
        document.getElementById('apiKeyState').textContent = data.hasApiKey
          ? `Stored API key ${data.apiKeyMasked || ''}`
          : 'No API key stored.';
        status.textContent = '';
      } catch (error) {
        status.textContent = 'Failed to load SystemLink configuration.';
      }
    }

    async function saveConfig(clearKey = false) {
      const payload = {
        enabled: document.getElementById('enabled').checked,
        apiUrl: document.getElementById('apiUrl').value.trim(),
        workspaceId: document.getElementById('workspaceId').value.trim(),
        systemId: document.getElementById('systemId').value.trim(),
        apiKey: document.getElementById('apiKey').value.trim(),
        clearApiKey: clearKey
      };

      const status = document.getElementById('status');
      status.textContent = clearKey ? 'Clearing API key...' : 'Saving configuration...';

      try {
        const response = await fetch('/api/systemlink', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload)
        });

        if (!response.ok) {
          const message = await response.text();
          status.textContent = `Save failed: ${message}`;
          return;
        }

        document.getElementById('apiKey').value = '';
        status.textContent = clearKey ? 'Stored API key cleared.' : 'Configuration saved.';
        await loadConfig();
      } catch (error) {
        status.textContent = 'Save failed.';
      }
    }

    function clearApiKey() {
      saveConfig(true);
    }

    loadConfig();
  </script>
</body>
</html>
)rawliteral";

#endif // SYSTEMLINK_WEB_UI_HPP