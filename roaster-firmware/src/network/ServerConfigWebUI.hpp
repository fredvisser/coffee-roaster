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
    * { box-sizing: border-box; }
    :root { color-scheme: dark; font-family: 'Segoe UI', Inter, -apple-system, sans-serif; background: #333333; color: #FFFFFF; }
    body { margin: 0; padding: 24px; }
    .topnav { max-width: 1100px; margin: 0 auto 20px; padding: 14px; display: flex; flex-wrap: wrap; gap: 10px; background: #2D2D2D; border-bottom: 2px solid #444; align-items: center; }
    .topnav-logo { display: flex; align-items: center; gap: 10px; margin-right: auto; font-weight: 900; font-size: 20px; color: #009944; text-transform: uppercase; letter-spacing: 1px; }
    .topnav a { display: inline-flex; align-items: center; justify-content: center; min-width: 100px; padding: 12px 16px; background: #3A3A3A; color: #CCCCCC; text-decoration: none; font-weight: 700; text-transform: uppercase; font-size: 14px; transition: 0.2s; }
    .topnav a:hover { background: #444444; color: #FFF; }
    .topnav a.active { background: #009944; color: #fff; }
    main { width: min(100%, 560px); margin: 0 auto; }
    h1 { font-size: 24px; margin: 0 0 20px; }
    label { display: block; margin: 18px 0 6px; color: #AAA; }
    input { width: 100%; padding: 12px; border: 1px solid #444; background: #222222; color: inherit; font: inherit; }
    button { margin-top: 22px; padding: 11px 18px; border: 0; background: #009944; color: #fff; font: inherit; font-weight: 700; cursor: pointer; }
    button:hover { background: #007A36; }
    #status { min-height: 1.5em; color: #009944; }
    .note { color: #AAA; font-size: 14px; }
    @media (max-width: 480px) {
      body { padding: 12px; }
      .topnav { padding: 12px; gap: 8px; }
      .topnav-logo { width: 100%; margin-right: 0; justify-content: center; font-size: 18px; }
      .topnav a { flex: 1 1 calc(50% - 8px); min-width: 0; padding: 12px 10px; font-size: 12px; }
    }
  </style>
</head>
<body>
  <nav class="topnav" aria-label="Main navigation">
    <div class="topnav-logo">
      <svg width="24" height="24" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
        <path d="M4.5,10 C2.5,15 9,17 12,17 C15,17 21.5,15 19.5,10 C17.5,5 11,3 8,3 C5,3 6.5,5 4.5,10 Z" fill="#009944" />
        <path d="M5,10 Q12,14 19,9" stroke="#2D2D2D" stroke-width="1.5" fill="none" />
        <path d="M9,18 Q8,20 10,22" stroke="#009944" stroke-width="2" fill="none" stroke-linecap="round" />
        <path d="M12,19 Q11,21 13,23" stroke="#009944" stroke-width="2" fill="none" stroke-linecap="round" />
        <path d="M15,18 Q14,20 16,22" stroke="#009944" stroke-width="2" fill="none" stroke-linecap="round" />
      </svg>
      Roaster Control
    </div>
    <a href="/console">Console</a>
    <a href="/profile">Profiles</a>
    <a href="/pid">PID</a>
    <a href="/update">Update</a>
    <a href="/systemlink">SystemLink</a>
    <a class="active" href="/server" aria-current="page">Server</a>
  </nav>
  <main>
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