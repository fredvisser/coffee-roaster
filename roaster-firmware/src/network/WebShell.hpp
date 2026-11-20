#pragma once

// ponytail: inline the shared chrome so pages render without another request.
#define ROASTER_WEB_STYLE R"webshell(
  <style>
    * { box-sizing: border-box; }
    html { scrollbar-gutter: stable; }
    body { font: 1rem/1.5 'Segoe UI', Inter, -apple-system, sans-serif; background: #333; color: #fff; margin: 0; padding: 20px; }
    .topnav, .web-shell { max-width: 1100px; margin-inline: auto; }
    .topnav { display: flex; flex-wrap: wrap; align-items: center; gap: 10px; padding: 14px; margin-bottom: 20px; background: #2D2D2D; border-bottom: 2px solid #444; }
    .topnav-logo { display: flex; align-items: center; gap: 10px; margin-right: auto; color: #4CDC8C; font-size: 20px; font-weight: 900; text-transform: uppercase; }
    .topnav a { flex: 1 1 100px; max-width: 130px; padding: 12px 10px; background: #3A3A3A; color: #eee; text-align: center; text-decoration: none; text-transform: uppercase; font-size: 14px; font-weight: 700; }
    .topnav a:hover { background: #444; color: #fff; }
    .topnav a[aria-current] { background: #009944; color: #111; }
    :focus-visible { outline: 2px solid #4CDC8C; outline-offset: 2px; }
    .web-heading, .web-panel { background: #2D2D2D; padding: 24px; }
    .web-heading { border-top: 4px solid #009944; margin: 0 0 18px; }
    .web-heading h1 { margin: 0; color: #fff; font-size: 28px; font-weight: 900; line-height: 1.25; text-transform: uppercase; }
    .web-heading .subtitle { margin-top: 8px; }
    .ota-frame { width: 100%; height: 36rem; border: 0; display: block; }
    @media (max-width: 960px) {
      .topnav-logo { flex: 1 0 100%; }
      .topnav a { max-width: none; }
    }
    @media (max-width: 540px) {
      body { padding: 12px; }
      .topnav { padding: 12px; gap: 8px; }
      .topnav a { flex: 1 1 calc((100% - 16px) / 3); font-size: 12px; }
      .web-heading, .web-panel { padding: 16px; }
      .web-heading h1 { font-size: 22px; }
    }
  </style>
)webshell"

#define ROASTER_WEB_CURRENT " aria-current=\"page\""
#define ROASTER_WEB_NAV(console, profiles, pid, update, systemlink, server) \
  "<nav class=\"topnav\" aria-label=\"Main navigation\">" \
  R"webshell(<div class="topnav-logo">
      <svg width="24" height="24" viewBox="0 0 24 24" fill="none" aria-hidden="true">
        <path d="M4.5,10 C2.5,15 9,17 12,17 C15,17 21.5,15 19.5,10 C17.5,5 11,3 8,3 C6.5,3 4.5,7 4.5,10 Z" fill="#009944"/>
      </svg>
      Roaster Control
    </div>)webshell" \
  "<a href=\"/console\"" console ">Console</a>" \
  "<a href=\"/profile\"" profiles ">Profiles</a>" \
  "<a href=\"/pid\"" pid ">PID</a>" \
  "<a href=\"/update\"" update ">Update</a>" \
  "<a href=\"/systemlink\"" systemlink ">SystemLink</a>" \
  "<a href=\"/server\"" server ">Server</a>" \
  "</nav>"

#define ROASTER_WEB_UPDATE_HEAD \
  "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">" \
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">" \
  "<title>Firmware Update</title>" ROASTER_WEB_STYLE "</head><body>" \
  ROASTER_WEB_NAV("", "", "", ROASTER_WEB_CURRENT, "", "") \
  "<main class=\"web-shell\"><header class=\"web-heading\"><h1>Firmware Update</h1></header>"

static const char ROASTER_WEB_UPDATE_HTML[] =
  ROASTER_WEB_UPDATE_HEAD
  "<iframe class=\"ota-frame\" src=\"/update?embedded=1\" title=\"Firmware uploader\"></iframe>"
  "</main></body></html>";

static const char ROASTER_PAIR_HTML[] =
  "<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Pair roaster</title>"
  ROASTER_WEB_STYLE R"pair(
<main class="web-shell">
  <header class="web-heading"><h1>Pair this browser</h1></header>
  <div class="web-panel">
    <p>On the roaster, open Wi-Fi settings, then Web access. Tap <strong>Pair browser/CLI</strong>.</p>
    <label>Session name <input id="name" maxlength="32" value="Browser"></label>
    <button id="pair" type="button">Request to pair</button>
    <p id="status" role="status"></p>
  </div>
</main>
<script>
  const button = document.querySelector('#pair');
  const status = document.querySelector('#status');
  async function post(url, headers) {
    const r = await fetch(url, {method: 'POST', headers, cache: 'no-store', signal: AbortSignal.timeout(6000)});
    const data = await r.json();
    if (!r.ok) throw new Error(data.error);
    return data;
  }
  button.onclick = async () => {
    button.disabled = true;
    try {
      const name = document.querySelector('#name').value;
      const offer = await post('/api/pair-request?name=' + encodeURIComponent(name));
      status.textContent = 'Check that the roaster shows ' + offer.code + ', then tap Approve.';
      const deadline = Date.now() + 60000;
      while (Date.now() < deadline) {
        await new Promise(resolve => setTimeout(resolve, 1000));
        if ((await post('/api/pair-claim', {
          'X-Roaster-Pair': offer.ticket,
          'X-Roaster-Paired-At': String(Math.floor(Date.now() / 1000))
        })).ok) {
          location.replace('/console');
          return;
        }
      }
      throw new Error('Pairing timed out.');
    } catch (e) {
      status.textContent = e.message;
      button.disabled = false;
    }
  };
</script>
</html>
)pair";
