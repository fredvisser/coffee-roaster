#ifndef SERVER_CONFIG_WEB_UI_HPP
#define SERVER_CONFIG_WEB_UI_HPP

static const char SERVER_CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Roaster Server Settings</title>
  <style>
    :root { color-scheme: dark; font-family: system-ui, sans-serif; background: #171b1a; color: #f1f3ef; }
    body { margin: 0; padding: 24px; }
    main { width: min(100%, 560px); margin: 0 auto; }
    h1 { font-size: 24px; margin: 0 0 20px; }
    nav { display: flex; gap: 16px; margin-bottom: 24px; }
    a { color: #70d9a5; }
    label { display: block; margin: 18px 0 6px; color: #b9c4be; }
    input { box-sizing: border-box; width: 100%; padding: 12px; border: 1px solid #52625a; border-radius: 4px; background: #242b27; color: inherit; font: inherit; }
    button { margin-top: 22px; padding: 11px 18px; border: 0; border-radius: 4px; background: #70d9a5; color: #102019; font: inherit; font-weight: 700; }
    #status { min-height: 1.5em; color: #70d9a5; }
    .note { color: #b9c4be; font-size: 14px; }
  </style>
</head>
<body>
  <main>
    <nav><a href="/console">Console</a><a href="/systemlink">SystemLink</a></nav>
    <h1>Server settings</h1>
    <form id="settings">
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