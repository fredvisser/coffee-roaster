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
      <label for="username">Web username</label>
      <input id="username" value="admin" readonly>
      <label for="newPassword">New password</label>
      <input id="newPassword" name="newPassword" type="password" minlength="8" maxlength="8" pattern="[A-Za-z0-9]{8}" autocomplete="new-password">
      <p class="note">Leave blank to keep the current password. New passwords must be 8 letters or digits.</p>
      <button type="submit">Save settings</button>
    </form>
    <p id="status" role="status" aria-live="polite"></p>
  </main>
  <script>
    const statusNode = document.getElementById('status');
    async function loadSettings() {
      const response = await fetch('/api/server', { cache: 'no-store' });
      if (!response.ok) throw new Error('Could not load server settings.');
      const settings = await response.json();
      document.getElementById('hostname').value = settings.hostname;
      document.getElementById('username').value = settings.username;
    }
    document.getElementById('settings').addEventListener('submit', async (event) => {
      event.preventDefault();
      statusNode.textContent = '';
      const hostname = document.getElementById('hostname').value.trim();
      const newPassword = document.getElementById('newPassword').value;
      try {
        const response = await fetch('/api/server', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          cache: 'no-store',
          body: JSON.stringify({ hostname, newPassword })
        });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || 'Could not save settings.');
        document.getElementById('newPassword').value = '';
        statusNode.textContent = 'Saved. Reconnect at http://' + result.hostname + '.local/ and use the updated password if it changed.';
      } catch (error) {
        statusNode.textContent = error.message;
      }
    });
    loadSettings().catch((error) => { statusNode.textContent = error.message; });
  </script>
</body>
</html>
)rawliteral";

#endif