#ifndef SERVER_CONFIG_WEB_UI_HPP
#define SERVER_CONFIG_WEB_UI_HPP

static const char SERVER_CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Roaster Server Settings</title>
  )rawliteral" ROASTER_WEB_STYLE R"rawliteral(
  <style>
    label { display: block; margin: 18px 0 6px; color: #AAA; }
    input { width: 100%; padding: 12px; border: 1px solid #444; background: #222222; color: inherit; font: inherit; }
    button { margin-top: 22px; padding: 11px 18px; border: 0; background: #009944; color: #fff; font: inherit; font-weight: 700; cursor: pointer; }
    button:hover { background: #007A36; }
    #status { min-height: 1.5em; color: #009944; }
    .note { color: #AAA; font-size: 14px; }
    #sessions { padding: 0; list-style: none; }
    #sessions li { display: flex; align-items: center; gap: 12px; margin-block: 8px; }
    #sessions button { flex-shrink: 0; margin-top: 0; }
    #sessions span { overflow-wrap: anywhere; }
  </style>
</head>
<body>
  )rawliteral" ROASTER_WEB_NAV("", "", "", "", "", ROASTER_WEB_CURRENT) R"rawliteral(
  <main class="web-shell">
    <header class="web-heading"><h1>Server settings</h1></header>
    <form id="settings" class="web-panel">
      <label for="hostname">mDNS name</label>
      <input id="hostname" name="hostname" maxlength="63" required pattern="[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?" autocomplete="off">
      <p class="note">The address will be this name followed by .local.</p>
      <button type="submit">Save settings</button>
    </form>
    <section class="web-panel">
      <h2>Paired sessions</h2>
      <ul id="sessions"></ul>
    </section>
    <p id="status" role="status" aria-live="polite"></p>
  </main>
  <script>
    const statusNode = document.getElementById('status');
    async function loadSettings() {
      const response = await fetch('/api/server', { cache: 'no-store' });
      if (!response.ok) throw new Error('Could not load server settings.');
      const settings = await response.json();
      document.getElementById('hostname').value = settings.hostname;
    }
    async function loadSessions() {
      const response = await fetch('/api/pair-sessions', { cache: 'no-store' });
      if (!response.ok) throw new Error('Could not load paired sessions.');
      const list = document.getElementById('sessions');
      list.replaceChildren();
      for (const session of await response.json()) {
        const row = list.appendChild(document.createElement('li'));
        const revoke = row.appendChild(document.createElement('button'));
        revoke.type = 'button';
        revoke.textContent = 'Revoke';
        revoke.setAttribute('aria-label', 'Revoke ' + session.name);
        const date = session.pairedAt ? new Date(session.pairedAt * 1000).toLocaleDateString() : 'date not recorded';
        const details = row.appendChild(document.createElement('span'));
        details.textContent = `${session.name} (${session.cli ? 'CLI' : 'Browser'}, ${date})${session.current ? ' - This session' : ''}`;
        revoke.onclick = async () => {
          revoke.disabled = true;
          try {
            const result = await fetch('/api/pair-revoke?id=' + encodeURIComponent(session.id), { method: 'POST' });
            if (!result.ok) throw new Error('Could not revoke session.');
            if (session.current) location.replace('/pair');
            else await loadSessions();
          } catch (error) {
            statusNode.textContent = error.message;
            revoke.disabled = false;
          }
        };
      }
    }
    document.getElementById('settings').addEventListener('submit', async (event) => {
      event.preventDefault();
      statusNode.textContent = '';
      const hostname = document.getElementById('hostname').value.trim();
      try {
        const response = await fetch('/api/server', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          cache: 'no-store',
          body: JSON.stringify({ hostname })
        });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || 'Could not save settings.');
        statusNode.textContent = 'Saved. If the address changed, pair again at http://' + result.hostname + '.local/.';
      } catch (error) {
        statusNode.textContent = error.message;
      }
    });
    loadSettings().catch((error) => { statusNode.textContent = error.message; });
    loadSessions().catch((error) => { statusNode.textContent = error.message; });
  </script>
</body>
</html>
)rawliteral";

#endif