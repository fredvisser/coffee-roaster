#ifndef SENSOR_CALIBRATION_WEB_UI_HPP
#define SENSOR_CALIBRATION_WEB_UI_HPP

#include <Arduino.h>

const char SENSOR_CALIBRATION_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Sensor Calibration</title>
  <style>
    * { box-sizing: border-box; }
    body { margin: 0; padding: 20px; background: #333; color: #fff; font-family: 'Segoe UI', Inter, -apple-system, sans-serif; line-height: 1.5; }
    .topnav { max-width: 1100px; margin: 0 auto 20px; padding: 14px; display: flex; flex-wrap: wrap; gap: 10px; background: #2d2d2d; border-bottom: 2px solid #444; align-items: center; }
    .topnav-logo { display: flex; align-items: center; gap: 10px; margin-right: auto; color: #009944; font-size: 20px; font-weight: 900; text-transform: uppercase; letter-spacing: 1px; }
    .topnav a { display: inline-flex; align-items: center; justify-content: center; min-width: 100px; padding: 12px 16px; background: #3a3a3a; color: #ccc; text-decoration: none; font-size: 14px; font-weight: 700; text-transform: uppercase; }
    .topnav a:hover { background: #444; color: #fff; }
    .topnav a.active { background: #009944; color: #fff; }
    .page { max-width: 1100px; margin: 0 auto; display: grid; gap: 16px; }
    .card { min-width: 0; padding: 20px; background: #2d2d2d; border-top: 4px solid #009944; }
    h1, h2 { margin: 0; color: #fff; font-weight: 800; text-transform: uppercase; }
    h1 { font-size: 24px; }
    h2 { font-size: 17px; }
    .note { margin: 8px 0 0; color: #aaa; font-size: 14px; }
    .warning { margin-top: 14px; padding: 12px; background: #3a2d20; border-left: 4px solid #f0883e; color: #ffd5b5; }
    .status-grid { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 10px; margin-top: 18px; }
    .metric { min-width: 0; padding: 12px; background: #222; border: 1px solid #444; }
    .metric-label { color: #aaa; font-size: 12px; font-weight: 700; text-transform: uppercase; }
    .metric-value { margin-top: 4px; color: #fff; font-family: monospace; font-size: 20px; font-weight: 900; overflow-wrap: anywhere; }
    .setpoint-controls { display: grid; grid-template-columns: minmax(180px, 1fr) minmax(180px, 1fr); gap: 10px; margin-top: 16px; }
    .setpoint-controls label { display: flex; flex-direction: column; gap: 6px; color: #aaa; font-size: 12px; font-weight: 700; text-transform: uppercase; }
    .setpoint-controls button { min-height: 44px; align-self: end; }
    .setpoint-status { margin-top: 12px; padding: 12px; background: #222; border-left: 4px solid #009944; color: #fff; font-weight: 700; }
    button { border: 0; cursor: pointer; font: inherit; font-weight: 700; }
    table { width: 100%; border-collapse: collapse; }
    th, td { padding: 10px 8px; border-bottom: 1px solid #444; text-align: left; }
    th { color: #aaa; font-size: 12px; text-transform: uppercase; }
    input { width: 100%; min-width: 100px; padding: 10px; border: 1px solid #555; background: #222; color: #fff; font: inherit; font-weight: 700; }
    input:focus { outline: none; border-color: #009944; }
    .capture { width: auto; margin: 0; padding: 10px 12px; background: #444; color: #fff; }
    .capture:hover { background: #555; }
    .actions { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 18px; }
    .actions button { flex: 1 1 180px; padding: 13px 16px; color: #fff; text-transform: uppercase; }
    .primary { background: #009944; }
    .secondary { background: #444; }
    .danger { background: #a30000; }
    button:disabled { cursor: not-allowed; opacity: .45; }
    #message { min-height: 24px; margin-top: 14px; color: #aaa; font-weight: 700; }
    .fit { display: grid; grid-template-columns: repeat(4, minmax(0, 1fr)); gap: 10px; margin-top: 16px; }
    .fit .metric-value { font-size: 16px; }
    @media (max-width: 700px) {
      body { padding: 12px; }
      .topnav-logo { width: 100%; margin-right: 0; justify-content: center; font-size: 18px; }
      .topnav a { flex: 1 1 calc(50% - 10px); min-width: 0; padding: 11px 8px; font-size: 12px; }
      .card { padding: 16px; }
      .status-grid, .fit { grid-template-columns: repeat(2, minmax(0, 1fr)); }
      .setpoint-controls { grid-template-columns: 1fr; }
      table { display: block; overflow-x: auto; }
      th, td { min-width: 130px; }
      th:first-child, td:first-child { min-width: 48px; }
    }
  </style>
</head>
<body>
  <nav class="topnav">
    <div class="topnav-logo">Roaster Control</div>
    <a href="/console">Console</a>
    <a href="/profile">Profiles</a>
    <a href="/pid">PID</a>
    <a class="active" href="/calibration">Calibration</a>
    <a href="/update">Update</a>
    <a href="/systemlink">SystemLink</a>
  </nav>

  <main class="page">
    <section class="card">
      <h1>Temperature Sensor Calibration</h1>
      <p class="note">Compare the bean thermocouple with an external probe at several stable temperatures. Capture at least three points spanning 50F or more. The controller fits a small linear correction for displayed temperature and PID control.</p>
      <div class="warning">Place the external probe beside the bean sensor. Set a raw temperature below, wait for the page to report SETTLED, then capture the raw value and enter the external probe reading. Safety cutoffs continue to use the uncorrected thermocouple reading.</div>
      <div class="status-grid">
        <div class="metric"><div class="metric-label">Roaster State</div><div class="metric-value" id="state">--</div></div>
        <div class="metric"><div class="metric-label">Bean Raw</div><div class="metric-value" id="beanRaw">--</div></div>
        <div class="metric"><div class="metric-label">Bean Corrected</div><div class="metric-value" id="beanCorrected">--</div></div>
        <div class="metric"><div class="metric-label">Capture Status</div><div class="metric-value" id="captureStatus">Ready</div></div>
      </div>
    </section>

    <section class="card">
      <h2>Calibration Temperature</h2>
      <p class="note">The controller regulates the raw bean thermocouple to this target. You can set another target while holding, or cool back to idle before applying the final correction.</p>
      <div class="setpoint-controls">
        <label>Raw bean setpoint F<input id="setpointRaw" type="number" step="1" min="50" max="450" placeholder="250"></label>
        <button class="primary" id="setpointButton" onclick="setCalibrationSetpoint()">Set Temperature</button>
        <button class="secondary" id="coolButton" onclick="coolCalibrationRoaster()" disabled>Cool to Idle</button>
      </div>
      <div class="setpoint-status" id="setpointStatus">No calibration temperature is active.</div>
      <div id="setpointMessage"></div>
    </section>

    <section class="card">
      <h2>Calibration Points</h2>
      <p class="note">Reference is the external probe value; raw is captured from the bean MAX6675 channel.</p>
      <table>
        <thead><tr><th>#</th><th>Reference F</th><th>Raw F</th><th>Action</th></tr></thead>
        <tbody id="points"></tbody>
      </table>
      <div class="actions">
        <button class="primary" id="apply" onclick="applyCalibration()">Apply Calibration</button>
        <button class="secondary" onclick="addPoint()">Add Point</button>
        <button class="danger" onclick="resetCalibration()">Reset Selected Sensor</button>
      </div>
      <div id="message"></div>
    </section>

    <section class="card">
      <h2>Active Correction</h2>
      <div class="fit">
        <div class="metric"><div class="metric-label">Sensor</div><div class="metric-value">Bean</div></div>
        <div class="metric"><div class="metric-label">Formula</div><div class="metric-value" id="fitFormula">raw</div></div>
        <div class="metric"><div class="metric-label">Fit Error</div><div class="metric-value" id="fitRmse">--</div></div>
        <div class="metric"><div class="metric-label">Points</div><div class="metric-value" id="fitPoints">0</div></div>
      </div>
    </section>
  </main>

  <script>
    const points = [];
    let latestStatus = null;
    let captureInProgress = false;

    function makePoint() { return { referenceTempF: '', rawTempF: '' }; }
    for (let index = 0; index < 3; index++) {
      points.push(makePoint());
    }

    function formatTemp(value) {
      return Number.isFinite(Number(value)) ? Number(value).toFixed(1) + 'F' : '--';
    }

    function renderPoints() {
      const body = document.getElementById('points');
      body.innerHTML = points.map((point, index) => `
        <tr>
          <td>${index + 1}</td>
          <td><input type="number" step="0.1" min="0" max="600" data-field="referenceTempF" data-index="${index}" value="${point.referenceTempF}"></td>
          <td><input type="number" step="0.1" min="0" max="600" data-field="rawTempF" data-index="${index}" value="${point.rawTempF}"></td>
          <td><button class="capture" data-index="${index}">Capture</button></td>
        </tr>`).join('');

      body.querySelectorAll('input').forEach(input => {
        input.addEventListener('input', event => {
          points[Number(event.target.dataset.index)][event.target.dataset.field] = event.target.value;
          updateApplyState();
        });
      });
      body.querySelectorAll('.capture').forEach(button => {
        button.addEventListener('click', () => capturePoint(Number(button.dataset.index)));
      });
      updateApplyState();
    }

    async function getCalibrationStatus() {
      const response = await fetch('/api/calibration');
      if (!response.ok) throw new Error('Unable to read calibration status');
      return response.json();
    }

    function updateLive(status) {
      latestStatus = status;
      document.getElementById('state').textContent = status.state || '--';
      document.getElementById('beanRaw').textContent = formatTemp(status.bean?.raw);
      document.getElementById('beanCorrected').textContent = formatTemp(status.bean?.corrected);
      const hold = status.calibration || {};
      const holdActive = hold.active === true;
      const canEdit = status.state === 'IDLE' || status.state === 'CALIBRATION_HOLD';
      document.getElementById('coolButton').disabled = !holdActive;
      document.querySelectorAll('.capture').forEach(button => {
        button.disabled = captureInProgress || hold.settled !== true;
      });
      document.getElementById('apply').disabled = !canEdit || points.filter(point => Number.isFinite(Number(point.referenceTempF)) && Number.isFinite(Number(point.rawTempF))).length < 3 || captureInProgress;
      if (holdActive) {
        const actual = formatTemp(hold.actualRaw);
        const target = formatTemp(hold.targetRaw);
        const stableFor = Number(hold.stableForSeconds || 0);
        document.getElementById('setpointStatus').textContent = hold.settled === true
          ? `SETTLED at ${actual} (target ${target})`
          : `Holding ${target}; actual ${actual}; in range for ${stableFor}s`;
        if (document.activeElement !== document.getElementById('setpointRaw')) {
          document.getElementById('setpointRaw').value = Number(hold.targetRaw).toFixed(0);
        }
        document.getElementById('setpointButton').textContent = 'Set Next Temperature';
      } else {
        document.getElementById('setpointStatus').textContent = status.state === 'COOLING' ? 'Cooling to idle...' : 'No calibration temperature is active.';
        document.getElementById('setpointButton').textContent = 'Set Temperature';
      }
      renderFit();
    }

    function renderFit() {
      const fit = latestStatus?.bean?.fit;
      if (!fit?.enabled) {
        document.getElementById('fitFormula').textContent = 'raw';
        document.getElementById('fitRmse').textContent = '--';
        document.getElementById('fitPoints').textContent = '0';
        return;
      }
      const sign = Number(fit.offset) >= 0 ? '+' : '-';
      document.getElementById('fitFormula').textContent = `${Number(fit.slope).toFixed(4)} x raw ${sign} ${Math.abs(Number(fit.offset)).toFixed(1)}F`;
      document.getElementById('fitRmse').textContent = `${Number(fit.rmse).toFixed(2)}F`;
      document.getElementById('fitPoints').textContent = fit.pointCount;
    }

    function updateApplyState() {
      const completePoints = points.filter(point => Number.isFinite(Number(point.referenceTempF)) && Number.isFinite(Number(point.rawTempF)));
      const canEdit = latestStatus?.state === 'IDLE' || latestStatus?.state === 'CALIBRATION_HOLD';
      document.getElementById('apply').disabled = !canEdit || completePoints.length < 3 || captureInProgress;
    }

    async function capturePoint(index) {
      if (captureInProgress) return;
      if (latestStatus?.calibration?.settled !== true) {
        document.getElementById('message').textContent = 'Wait for the raw temperature to settle at the active setpoint.';
        return;
      }
      captureInProgress = true;
      document.getElementById('captureStatus').textContent = 'Sampling...';
      document.querySelectorAll('.capture').forEach(button => button.disabled = true);
      try {
        const readings = [];
        for (let sample = 0; sample < 5; sample++) {
          const status = await getCalibrationStatus();
          const value = Number(status.bean?.raw);
          if (Number.isFinite(value)) readings.push(value);
          if (sample < 4) await new Promise(resolve => setTimeout(resolve, 250));
        }
        if (readings.length < 3) throw new Error('Not enough valid sensor readings');
        const average = readings.reduce((sum, value) => sum + value, 0) / readings.length;
        points[index].rawTempF = average.toFixed(1);
        renderPoints();
        document.getElementById('captureStatus').textContent = `Captured ${formatTemp(average)}`;
        document.getElementById('message').textContent = 'Check the captured value, then continue to the next stable setpoint.';
      } catch (error) {
        document.getElementById('captureStatus').textContent = 'Capture failed';
        document.getElementById('message').textContent = error.message;
      } finally {
        captureInProgress = false;
        updateApplyState();
      }
    }

    function addPoint() {
      if (points.length < 8) {
        points.push(makePoint());
        renderPoints();
      }
    }

    async function setCalibrationSetpoint() {
      const target = Number(document.getElementById('setpointRaw').value);
      if (!Number.isFinite(target) || target < 50 || target > 450) {
        document.getElementById('setpointMessage').textContent = 'Enter a raw setpoint between 50F and 450F.';
        return;
      }
      document.getElementById('setpointButton').disabled = true;
      document.getElementById('setpointMessage').textContent = 'Starting temperature hold...';
      try {
        const response = await fetch('/api/calibration/setpoint?targetRaw=' + encodeURIComponent(target), { method: 'POST' });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || 'Setpoint was rejected');
        updateLive(result);
        document.getElementById('setpointMessage').textContent = 'Temperature hold started.';
      } catch (error) {
        document.getElementById('setpointMessage').textContent = error.message;
      } finally {
        document.getElementById('setpointButton').disabled = false;
      }
    }

    async function coolCalibrationRoaster() {
      document.getElementById('coolButton').disabled = true;
      document.getElementById('setpointMessage').textContent = 'Cooling to idle...';
      try {
        const response = await fetch('/api/calibration/setpoint?action=cool', { method: 'POST' });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || 'Cooling was rejected');
        updateLive(result);
      } catch (error) {
        document.getElementById('setpointMessage').textContent = error.message;
      }
    }

    function buildPoints() {
      return points
        .filter(point => Number.isFinite(Number(point.referenceTempF)) && Number.isFinite(Number(point.rawTempF)))
        .map(point => ({ rawTempF: Number(point.rawTempF), referenceTempF: Number(point.referenceTempF) }));
    }

    async function applyCalibration() {
      const completePoints = buildPoints();
      if (completePoints.length < 3) {
        document.getElementById('message').textContent = 'Enter and capture at least three complete points.';
        return;
      }
      document.getElementById('apply').disabled = true;
      document.getElementById('message').textContent = 'Fitting and saving correction...';
      try {
        const response = await fetch('/api/calibration', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ beanPoints: completePoints }) });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || 'Calibration was rejected');
        updateLive(result);
        document.getElementById('message').textContent = 'Calibration saved.';
      } catch (error) {
        document.getElementById('message').textContent = error.message;
      } finally {
        updateApplyState();
      }
    }

    async function resetCalibration() {
      if (!latestStatus || (latestStatus.state !== 'IDLE' && latestStatus.state !== 'CALIBRATION_HOLD')) {
        document.getElementById('message').textContent = 'Reset is only available while the roaster is idle or holding a setpoint.';
        return;
      }
      try {
        const response = await fetch('/api/calibration/reset', { method: 'POST' });
        const result = await response.json();
        if (!response.ok) throw new Error(result.error || 'Reset was rejected');
        updateLive(result);
        document.getElementById('message').textContent = 'Bean sensor reset to raw readings.';
      } catch (error) {
        document.getElementById('message').textContent = error.message;
      }
    }

    async function refresh() {
      try {
        updateLive(await getCalibrationStatus());
      } catch (error) {
        document.getElementById('message').textContent = error.message;
      }
    }

    renderPoints();
    refresh();
    setInterval(refresh, 1000);
  </script>
</body>
</html>
)rawliteral";

#endif