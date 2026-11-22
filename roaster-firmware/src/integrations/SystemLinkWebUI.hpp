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
  )rawliteral" ROASTER_WEB_STYLE R"rawliteral(
  <style>
    * { box-sizing: border-box; }
    .subtitle {
      color: #AAA;
      font-size: 14px;
      line-height: 1.5;
      font-weight: bold;
    }
    .content {
      padding: 24px;
      display: grid;
      gap: 20px;
    }
    .card {
      background: #3A3A3A;
      border: none;
      padding: 20px;
    }
    .card h2 {
      margin: 0 0 12px;
      font-size: 18px;
      text-transform: uppercase;
      font-weight: 800;
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
      color: #AAA;
      text-transform: uppercase;
      font-weight: 700;
    }
    input[type="text"],
    input[type="password"] {
      width: 100%;
      padding: 12px 14px;
      border: 1px solid #444;
      border-radius: 0;
      background: #222222;
      color: #FFF;
      font-size: 14px;
      font-weight: bold;
    }
    input:focus {
      outline: none;
      border-color: #009944;
    }
    .toggle {
      display: flex;
      align-items: center;
      gap: 12px;
      font-size: 14px;
      font-weight: bold;
      text-transform: uppercase;
    }
    .toggle input {
      width: 20px;
      height: 20px;
      accent-color: #009944;
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
      padding: 14px 20px;
      border-radius: 0;
      color: #fff;
      text-decoration: none;
      font-weight: 700;
      text-transform: uppercase;
      font-size: 14px;
      background: #444444;
      transition: background 0.2s;
    }
    button:hover, a.button:hover { background: #555555; }
    button.primary, a.button.primary {
      background: #009944;
    }
    button.primary:hover, a.button.primary:hover {
      background: #007A36;
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
      .content { padding: 18px; }
    }
    @media (max-width: 480px) {
      .content, .card {
        padding: 16px;
      }
      .grid {
        grid-template-columns: 1fr;
      }
      .actions > * {
        width: 100%;
      }
      .meta {
        gap: 10px;
      }
    }
  </style>
</head>
<body>
  )rawliteral" ROASTER_WEB_NAV("", "", "", "", ROASTER_WEB_CURRENT, "") R"rawliteral(
  <main class="web-shell shell">
    <header class="web-heading">
      <h1>SystemLink</h1>
      <div class="subtitle">Configure optional publishing to NI SystemLink. The roaster can write live tags during a roast and publish a test result, including a 1 Hz roast trace CSV, when the roast passes, is terminated, or errors out.</div>
    </header>

    <div class="content web-panel">
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
        <a class="button secondary" href="/">Back to Console</a>
      </div>
      <div class="status" id="status"></div>
    </div>
  </main>

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