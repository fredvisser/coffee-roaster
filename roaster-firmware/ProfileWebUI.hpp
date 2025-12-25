#ifndef PROFILE_WEB_UI_HPP
#define PROFILE_WEB_UI_HPP

#include <Arduino.h>

const char PROFILE_EDITOR_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>Roast Profile Editor</title>
  <style>
    * { box-sizing: border-box; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background:#0d1117; color:#c9d1d9; margin:0; padding:0; }
    .container { max-width: 1100px; margin: 0 auto; padding: 16px; }
    .header { display:flex; flex-direction:column; gap:12px; margin-bottom:16px; }
    h1 { font-size: 24px; color:#fff; margin:0; }
    .controls { display:flex; gap:8px; flex-wrap: wrap; align-items: center; }
    .control-group { display:flex; gap:8px; padding:8px; background:#161b22; border-radius:6px; border:1px solid #30363d; flex-wrap: wrap; }
    .control-group.primary { border-color:#1f6feb; }
    .btn { padding:10px 14px; background:#21262d; border:1px solid #30363d; border-radius:6px; color:#c9d1d9; cursor:pointer; font-size:14px; white-space:nowrap; transition: all 0.2s; }
    .btn:hover { background:#30363d; border-color:#1f6feb; }
    .btn:active { background:#30363d; }
    .btn.primary { background:#1f6feb; border-color:#1f6feb; color:#fff; font-weight:600; }
    .btn.primary:hover { background:#1a56db; }
    .btn.danger { background:#da3633; border-color:#da3633; color:#fff; }
    .btn.danger:hover { background:#b62324; }
    .input { padding:10px; background:#0d1117; border:1px solid #30363d; border-radius:6px; color:#c9d1d9; font-size:14px; min-width:150px; }
    .input:focus { outline:none; border-color:#1f6feb; }
    .grid { display:grid; grid-template-columns: repeat(auto-fit, minmax(320px, 1fr)); gap:16px; }
    .card { background:#161b22; border:1px solid #30363d; border-radius:8px; padding:16px; }
    .card h2 { margin:0 0 12px; font-size:18px; color:#fff; }
    .graph-wrapper { position:relative; width:100%; overflow:visible; border:1px solid #21262d; border-radius:8px; background:#0d1117; padding:4px; }
    .graph-wrapper svg { display:block; touch-action:none; }
    #graph { width:100%; min-height:300px; }
    .axis { stroke:#30363d; stroke-width:1; }
    .gridline { stroke:#21262d; stroke-width:1; }
    .sp-line { stroke:#58a6ff; stroke-width:2; fill:none; }
    .fan-line { stroke:#a371f7; stroke-width:2; fill:none; stroke-dasharray:5,3; }
    .sp-point { fill:#3fb950; stroke:#fff; stroke-width:1; cursor:grab; touch-action:none; }
    .sp-point.selected { fill:#f0883e; }
    .sp-point:active { cursor:grabbing; }
    .legend { font-size:12px; color:#8b949e; margin-top:8px; display:flex; gap:20px; }
    .list { display:flex; flex-direction:column; gap:8px; }
    .row { display:flex; align-items:center; gap:8px; flex-wrap:wrap; }
    label { font-size:13px; color:#8b949e; }
    input[type="checkbox"] { width:18px; height:18px; cursor:pointer; }
    .spinner { display:inline-block; width:14px; height:14px; border:2px solid #30363d; border-top-color:#1f6feb; border-radius:50%; animation:spin 0.6s linear infinite; margin-left:8px; }
    @keyframes spin { to { transform:rotate(360deg); } }
    #debugInfo { position:fixed; top:10px; right:10px; background:rgba(0,0,0,0.95); border:1px solid #f85149; border-radius:6px; padding:12px; font-family:monospace; font-size:11px; max-width:450px; z-index:1000; display:none; max-height:600px; overflow-y:auto; pointer-events:none; }
    #debugInfo.visible { display:block; }
    #debugInfo pre { margin:0; color:#58a6ff; line-height:1.5; white-space:pre-wrap; word-wrap:break-word; }
    #debugInfo strong { color:#f85149; display:block; margin-bottom:8px; font-size:13px; }
    .debug-section { margin:10px 0; padding:8px; background:#161b22; border-radius:4px; border:1px solid #30363d; }
    .debug-section h3 { margin:0 0 6px 0; color:#3fb950; font-size:11px; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>Roast Profile Editor</h1>
      <div class="control-group primary">
        <select id="profilesList" class="input" style="flex:1; min-width:200px;" onchange="autoLoadProfile()"></select>
        <button class="btn" onclick="createNew()">New</button>
        <button id="activateBtn" class="btn primary" onclick="activateSelected()">Activate</button>
      </div>
      <div class="control-group">
        <strong id="currentProfileName" style="flex:1; color:#c9d1d9; font-size:16px; padding:8px;">No profile loaded</strong>
        <button class="btn" onclick="showRenameDialog()">Rename</button>
        <button id="saveBtn" class="btn" onclick="saveProfile()" disabled>Save</button>
        <button class="btn danger" onclick="deleteSelected()">Delete</button>
        <button class="btn" onclick="undo()">Undo</button>
        <button class="btn" onclick="redo()">Redo</button>
        <button class="btn" onclick="exportJSON()">Export</button>
        <button class="btn" onclick="document.getElementById('importFile').click()">Import</button>
        <input id="importFile" type="file" accept="application/json" style="display:none" />
      </div>
    </div>

    <div class="grid">
      <div class="card">
        <h2>Temperature Setpoints</h2>
        <div class="graph-wrapper" id="graphContainer">
          <div id="graph"></div>
        </div>
        <div class="legend">
          <span><span style="color:#58a6ff;">━</span> Temperature (°F)</span>
          <span><span style="color:#a371f7;">┉</span> Fan Speed (%)</span>
        </div>
        <div class="controls" style="margin-top:8px;">
          <button class="btn" onclick="addPoint()">Add Point</button>
          <button class="btn" onclick="removeSelected()">Remove Selected</button>
          <label style="display:flex;align-items:center;gap:6px;">
            <input id="snapEnabled" type="checkbox" checked onchange="updateSnapSettings()" /> Snap
          </label>
          <label style="display:flex;align-items:center;gap:6px;">
            Time step (s)
            <input id="snapTime" class="input" type="number" min="1" max="60" value="5" style="width:70px" onchange="updateSnapSettings()" />
          </label>
          <label style="display:flex;align-items:center;gap:6px;">
            Temp step (F)
            <input id="snapTemp" class="input" type="number" min="1" max="50" value="5" style="width:70px" onchange="updateSnapSettings()" />
          </label>
          <label style="display:flex;align-items:center;gap:6px;">
            Roast time (min)
            <input id="roastTime" class="input" type="number" min="1" max="20" value="8" style="width:70px" onchange="renderGraph()" />
          </label>
        </div>
        <div class="legend">Time (s) on X-axis; Temperature (F) on Y-axis; Fan (%) per point</div>
      </div>
      <div class="card">
        <h2>Selected Point</h2>
        <div class="list">
          <div class="row"><label>Time (s)</label><input id="ptTime" class="input" type="number" min="0" step="1" /></div>
          <div class="row"><label>Temp (F)</label><input id="ptTemp" class="input" type="number" min="0" max="500" step="1" /></div>
          <div class="row"><label>Fan (%)</label><input id="ptFan" class="input" type="number" min="0" max="100" step="1" /></div>
          <button class="btn" onclick="applyPointEdit()">Apply</button>
        </div>
      </div>
    </div>
  </div>

  

  <script>
    const MAX_POINTS = 10;
    let setpoints = [];
    let selectedIndex = -1;
    let currentProfileName = '';
    let activeProfileName = '';  // Track which profile is currently active
    let hasUnsavedChanges = false;
    const graphPadding = { left: 40, right: 20, top: 20, bottom: 40 };
    const graphSize = { width: 800, height: 400 };
    // Debug removed
    let svg = null;
    let container = null;
    let history = [];
    let redoStack = [];
    const MAX_HISTORY = 100;
    const SNAP = { enabled: true, time: 5, temp: 5 };
    let isLoading = false;

    function updateProfileDisplay() {
      document.getElementById('currentProfileName').textContent = currentProfileName || 'No profile loaded';
      document.getElementById('saveBtn').disabled = !hasUnsavedChanges;
    }

    function markUnsaved() {
      hasUnsavedChanges = true;
      updateProfileDisplay();
    }

    function markSaved() {
      hasUnsavedChanges = false;
      updateProfileDisplay();
    }

    // Debug removed

    function computeGraphSize() {
      const containerEl = document.getElementById('graphContainer');
      if (!containerEl) return false;
      let rect = containerEl.getBoundingClientRect();
      // Fallbacks when layout not settled yet
      if (rect.width === 0 || rect.height === 0) {
        const w = containerEl.clientWidth || containerEl.offsetWidth || window.innerWidth || 800;
        const h = containerEl.clientHeight || containerEl.offsetHeight || Math.max(300, Math.floor(w * 0.5));
        rect = { width: w, height: h, left: 0, top: 0, right: w, bottom: h };
      }
      const minHeight = 300;
      const availableWidth = rect.width - 4;
      graphSize.width = Math.max(300, Math.min(availableWidth, 800));
      graphSize.height = Math.max(minHeight, Math.min(rect.width * 0.5, 500));
      return true;
    }

    async function init() {
      const profilesResp = await fetch('/api/profiles');
      const profilesData = await profilesResp.json();
      const activeName = profilesData.active || '';
      
      // Load active profile if exists
      if (activeName) {
        const resp = await fetch('/api/profile/' + encodeURIComponent(activeName));
        const data = await resp.json();
        setpoints = data.setpoints || [];
        currentProfileName = activeName;
        markSaved();
        updateProfileDisplay();
      }
      
      await loadProfilesList();
      document.getElementById('snapEnabled').checked = SNAP.enabled;
      document.getElementById('snapTime').value = SNAP.time;
      document.getElementById('snapTemp').value = SNAP.temp;
      document.getElementById('importFile').addEventListener('change', handleImportFile);

      requestAnimationFrame(() => {
        if (!computeGraphSize()) {
          setTimeout(() => { if (computeGraphSize()) renderGraph(); }, 0);
        } else {
          renderGraph();
        }
        // Debug removed
      });

      window.addEventListener('resize', () => {
        computeGraphSize();
        renderGraph();
      });
    }

    async function loadProfilesList() {
      const resp = await fetch('/api/profiles');
      const data = await resp.json();
      const sel = document.getElementById('profilesList');
      sel.innerHTML = '';
      activeProfileName = data.active || '';  // Store active profile name
      
      (data.profiles || []).forEach(name => {
        const opt = document.createElement('option');
        opt.value = name;
        opt.textContent = (name === activeProfileName) ? name + ' (Active)' : name;
        sel.appendChild(opt);
      });
      
      if (activeProfileName) {
        for (let i=0;i<sel.options.length;i++) {
          if (sel.options[i].value === activeProfileName) { sel.selectedIndex = i; break; }
        }
      }
      
      updateActivateButton();
    }

    function updateActivateButton() {
      const sel = document.getElementById('profilesList');
      const btn = document.getElementById('activateBtn');
      if (!btn) return;
      const selectedName = sel.value;
      // Disable if selected profile is already active
      btn.disabled = (selectedName === activeProfileName);
    }

    function getBounds() {
      const roastTimeMin = Number(document.getElementById('roastTime').value) || 8;
      const maxTime = roastTimeMin * 60; // Convert to seconds
      return { minT: 0, maxT: maxTime, minY: 0, maxY: 500 };
    }

    function renderGraph() {
      container = document.getElementById('graph');
      if (!container) return;
      container.innerHTML = '';
      computeGraphSize();

      svg = document.createElementNS('http://www.w3.org/2000/svg','svg');
      svg.setAttribute('width', graphSize.width);
      svg.setAttribute('height', graphSize.height);
      svg.setAttribute('viewBox', `0 0 ${graphSize.width} ${graphSize.height}`);
      svg.style.display = 'block';
      svg.style.width = graphSize.width + 'px';
      svg.style.height = graphSize.height + 'px';
      container.appendChild(svg);

      const svgRect = svg.getBoundingClientRect();
      // If rect is zero, keep rendering with computed graphSize; CTM fallback will handle coordinates
      if (svgRect.width === 0 || svgRect.height === 0) {
        // Attempt a delayed reflow but do not abort rendering
        requestAnimationFrame(() => { computeGraphSize(); renderGraph(); });
      }

      const b = getBounds();
      const xScale = x => graphPadding.left + (x - b.minT) / (b.maxT - b.minT) * (graphSize.width - graphPadding.left - graphPadding.right);
      const yScale = y => graphSize.height - graphPadding.bottom - (y - b.minY) / (b.maxY - b.minY) * (graphSize.height - graphPadding.top - graphPadding.bottom);

      for (let i=0;i<=5;i++) {
        const y = graphPadding.top + i*(graphSize.height - graphPadding.top - graphPadding.bottom)/5;
        const gl = document.createElementNS('http://www.w3.org/2000/svg','line');
        gl.setAttribute('x1', graphPadding.left); gl.setAttribute('y1', y);
        gl.setAttribute('x2', graphSize.width - graphPadding.right); gl.setAttribute('y2', y);
        gl.setAttribute('class','gridline'); svg.appendChild(gl);
      }

      const xAxis = document.createElementNS('http://www.w3.org/2000/svg','line');
      xAxis.setAttribute('x1', graphPadding.left); xAxis.setAttribute('y1', graphSize.height - graphPadding.bottom);
      xAxis.setAttribute('x2', graphSize.width - graphPadding.right); xAxis.setAttribute('y2', graphSize.height - graphPadding.bottom);
      xAxis.setAttribute('class','axis'); svg.appendChild(xAxis);
      
      // Add x-axis time markers
      const timeInterval = 60; // Every 60 seconds (1 minute)
      for (let t = 0; t <= b.maxT; t += timeInterval) {
        const x = xScale(t);
        const tick = document.createElementNS('http://www.w3.org/2000/svg','line');
        tick.setAttribute('x1', x); tick.setAttribute('y1', graphSize.height - graphPadding.bottom);
        tick.setAttribute('x2', x); tick.setAttribute('y2', graphSize.height - graphPadding.bottom + 5);
        tick.setAttribute('stroke', '#30363d'); tick.setAttribute('stroke-width', '1');
        svg.appendChild(tick);
        
        const label = document.createElementNS('http://www.w3.org/2000/svg','text');
        label.setAttribute('x', x); label.setAttribute('y', graphSize.height - graphPadding.bottom + 18);
        label.setAttribute('text-anchor', 'middle'); label.setAttribute('font-size', '11');
        label.setAttribute('fill', '#8b949e');
        label.textContent = (t / 60).toFixed(0) + 'm';
        svg.appendChild(label);
      }
      
      const yAxis = document.createElementNS('http://www.w3.org/2000/svg','line');
      yAxis.setAttribute('x1', graphPadding.left); yAxis.setAttribute('y1', graphPadding.top);
      yAxis.setAttribute('x2', graphPadding.left); yAxis.setAttribute('y2', graphSize.height - graphPadding.bottom);
      yAxis.setAttribute('class','axis'); svg.appendChild(yAxis);

      if (setpoints.length > 0) {
        // Temperature line
        const path = document.createElementNS('http://www.w3.org/2000/svg','path');
        let d='';
        setpoints.forEach((sp,i)=>{
          const x=xScale(sp.time), y=yScale(sp.temp);
          d += (i===0?`M ${x} ${y}`:` L ${x} ${y}`);
        });
        path.setAttribute('d', d);
        path.setAttribute('class','sp-line');
        svg.appendChild(path);
        
        // Fan speed line (scale 0-100% to 0-500 temp range for visualization)
        const fanPath = document.createElementNS('http://www.w3.org/2000/svg','path');
        let fanD='';
        setpoints.forEach((sp,i)=>{
          const x=xScale(sp.time);
          const fanScaled = (sp.fanSpeed || 0) * 5;  // Scale 0-100% to 0-500 range
          const y=yScale(fanScaled);
          fanD += (i===0?`M ${x} ${y}`:` L ${x} ${y}`);
        });
        fanPath.setAttribute('d', fanD);
        fanPath.setAttribute('class','fan-line');
        svg.appendChild(fanPath);
      }

      setpoints.forEach((sp,i)=>{
        const c = document.createElementNS('http://www.w3.org/2000/svg','circle');
        c.setAttribute('cx', xScale(sp.time)); 
        c.setAttribute('cy', yScale(sp.temp)); 
        c.setAttribute('r', 6);
        c.setAttribute('class','sp-point' + (i===selectedIndex?' selected':''));
        c.addEventListener('mousedown', (e)=> startDrag(e,i));
        c.addEventListener('touchstart', (e)=> startDrag(e,i), {passive: false});
        c.addEventListener('click', ()=> selectPoint(i));
        svg.appendChild(c);
      });

      for (let i=0;i<=5;i++) {
        const yVal = Math.round(b.minY + i*(b.maxY-b.minY)/5);
        const t = document.createElementNS('http://www.w3.org/2000/svg','text');
        t.setAttribute('x', 8); t.setAttribute('y', yScale(yVal));
        t.setAttribute('fill', '#8b949e'); t.setAttribute('font-size','11');
        t.textContent = yVal + ' F'; 
        svg.appendChild(t);
      }

      // Debug removed
    }

    function selectPoint(i) {
      selectedIndex = i;
      const sp = setpoints[i];
      document.getElementById('ptTime').value = sp.time;
      document.getElementById('ptTemp').value = sp.temp;
      document.getElementById('ptFan').value = sp.fanSpeed;
      renderGraph();
    }

    function startDrag(ev, i) {
      ev.preventDefault();
      ev.stopPropagation();
      const targetSVG = svg; // Use root SVG for consistency
      if (!targetSVG) return;
      const b = getBounds();
      const invX = xPix => (xPix - graphPadding.left) / (graphSize.width - graphPadding.left - graphPadding.right) * (b.maxT - b.minT) + b.minT;
      const invY = yPix => (graphSize.height - graphPadding.bottom - yPix) / (graphSize.height - graphPadding.top - graphPadding.bottom) * (b.maxY - b.minY) + b.minY;
      pushHistory();
      selectedIndex = i;

      const getCoords = (e) => {
        const clientX = e.touches ? e.touches[0].clientX : e.clientX;
        const clientY = e.touches ? e.touches[0].clientY : e.clientY;
        let rect = targetSVG.getBoundingClientRect();
        if (!rect.width && !rect.height) {
          const contEl = document.getElementById('graphContainer');
          const contRect = contEl ? contEl.getBoundingClientRect() : rect;
          rect = contRect;
        }
        const relX = clientX - rect.left;
        const relY = clientY - rect.top;
        const svgP = { x: relX, y: relY };
        // Debug removed
        return { x: svgP.x, y: svgP.y };
      };

      const move = (e) => {
        e.preventDefault();
        const loc = getCoords(e);
        let t = Math.round(invX(loc.x));
        let y = Math.round(invY(loc.y));
        const snapActive = SNAP.enabled && !e.shiftKey;
        if (snapActive) {
          const st = Math.max(1, Number(document.getElementById('snapTime').value) || SNAP.time);
          const sy = Math.max(1, Number(document.getElementById('snapTemp').value) || SNAP.temp);
          t = Math.round(t / st) * st;
          y = Math.round(y / sy) * sy;
        }
        t = Math.max(0, Math.min(1200, t));  // Limit to 20 minutes (1200 seconds)
        y = Math.max(0, Math.min(500, y));
        
        // Enforce first point at time=0
        if (i === 0) t = 0;
        
        setpoints[i].time = t;
        setpoints[i].temp = y;
        setpoints.sort((a,b)=>a.time-b.time);
        renderGraph();
        selectPoint(i); // Update Selected Point card during drag
      };

      const up = () => {
        markUnsaved();
        document.removeEventListener('mousemove', move);
        document.removeEventListener('mouseup', up);
        document.removeEventListener('touchmove', move);
        document.removeEventListener('touchend', up);
      };

      document.addEventListener('mousemove', move);
      document.addEventListener('mouseup', up);
      document.addEventListener('touchmove', move, { passive: false });
      document.addEventListener('touchend', up);
    }

    function addPoint() {
      if (setpoints.length >= MAX_POINTS) return alert('Max points reached');
      pushHistory();
      const st = Math.max(1, Number(document.getElementById('snapTime').value) || SNAP.time);
      let t = setpoints.length ? Math.min(setpoints[setpoints.length-1].time + st, 3600) : 0;
      while (setpoints.some(sp => sp.time === t)) {
        t = Math.min(t + st, 3600);
        if (t === 3600 && setpoints.some(sp => sp.time === 3600)) break;
      }
      const sy = Math.max(1, Number(document.getElementById('snapTemp').value) || SNAP.temp);
      const sp = { time: t, temp: Math.round(200 / sy) * sy, fanSpeed: 50 };
      setpoints.push(sp);
      setpoints.sort((a,b)=>a.time-b.time);
      selectedIndex = setpoints.length - 1;
      markUnsaved();
      renderGraph();
      selectPoint(selectedIndex);
    }

    function removeSelected() {
      if (selectedIndex < 0) return;
      pushHistory();
      setpoints.splice(selectedIndex, 1);
      selectedIndex = -1;
      markUnsaved();
      renderGraph();
    }

    function applyPointEdit() {
      if (selectedIndex < 0) return;
      let t = parseInt(document.getElementById('ptTime').value||'0',10);
      let temp = parseInt(document.getElementById('ptTemp').value||'0',10);
      const fan = parseInt(document.getElementById('ptFan').value||'0',10);
      
      // Enforce first point at time=0
      if (selectedIndex === 0 && t !== 0) {
        return alert('First point must be at time 0');
      }
      
      // Limit to 20 minutes (1200 seconds)
      if (t > 1200) {
        return alert('Time must be less than 20 minutes (1200 seconds)');
      }
      
      if (setpoints.some((sp, idx) => idx !== selectedIndex && sp.time === t)) {
        return alert('Another point already exists at this time');
      }
      if (SNAP.enabled) {
        const st = Math.max(1, Number(document.getElementById('snapTime').value) || SNAP.time);
        const sy = Math.max(1, Number(document.getElementById('snapTemp').value) || SNAP.temp);
        t = Math.round(t / st) * st;
        temp = Math.round(temp / sy) * sy;
      }
      if (temp<0||temp>500||fan<0||fan>100) return alert('Out of bounds');
      pushHistory();
      setpoints[selectedIndex] = { time: t, temp: temp, fanSpeed: fan };
      setpoints.sort((a,b)=>a.time-b.time);
      markUnsaved();
      renderGraph();
    }

    function cloneSetpoints() { return setpoints.map(sp => ({ time: sp.time, temp: sp.temp, fanSpeed: sp.fanSpeed })); }
    function pushHistory() { history.push(cloneSetpoints()); if (history.length > MAX_HISTORY) history.shift(); redoStack = []; }
    function undo() { if (history.length === 0) return; redoStack.push(cloneSetpoints()); setpoints = history.pop(); selectedIndex = -1; markUnsaved(); renderGraph(); }
    function redo() { if (redoStack.length === 0) return; history.push(cloneSetpoints()); setpoints = redoStack.pop(); selectedIndex = -1; markUnsaved(); renderGraph(); }

    function updateSnapSettings() {
      SNAP.enabled = !!document.getElementById('snapEnabled').checked;
      SNAP.time = Math.max(1, parseInt(document.getElementById('snapTime').value||'5',10));
      SNAP.temp = Math.max(1, parseInt(document.getElementById('snapTemp').value||'5',10));
    }

    function exportJSON() {
      const name = (document.getElementById('profileName').value||'').trim();
      const payload = { name, setpoints };
      const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
      const a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = 'profile-' + (name || 'unnamed') + '.json';
      a.click();
      URL.revokeObjectURL(a.href);
    }

    async function handleImportFile(e) {
      const file = e.target.files[0];
      if (!file) return;
      try {
        const text = await file.text();
        const obj = JSON.parse(text);
        if (!Array.isArray(obj.setpoints)) throw new Error('Invalid JSON: missing setpoints');
        const cleaned = obj.setpoints.map(sp => ({
          time: Math.max(0, Math.min(3600, parseInt(sp.time||0, 10))),
          temp: Math.max(0, Math.min(500, parseInt(sp.temp||0, 10))),
          fanSpeed: Math.max(0, Math.min(100, parseInt(sp.fanSpeed||0, 10)))
        })).filter(sp => Number.isFinite(sp.time) && Number.isFinite(sp.temp) && Number.isFinite(sp.fanSpeed));
        if (cleaned.length === 0 || cleaned.length > MAX_POINTS) throw new Error('Invalid setpoint count');
        pushHistory();
        setpoints = cleaned.sort((a,b)=>a.time-b.time);
        if (typeof obj.name === 'string') currentProfileName = obj.name;
        selectedIndex = -1;
        markUnsaved();
        renderGraph();
        e.target.value = '';
      } catch (err) {
        alert('Import failed: ' + err.message);
      }
    }

    async function saveProfile() {
      if (setpoints.length === 0) return alert('Add at least one point');
      if (!currentProfileName) return alert('No profile loaded');
      const payload = { setpoints, name: currentProfileName, activate: false };
      const resp = await fetch('/api/profile/' + encodeURIComponent(currentProfileName), { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(payload) });
      const data = await resp.json();
      if (!data.ok) return alert('Save failed: ' + (data.error || 'unknown'));
      markSaved();
      // Don't await - fire and forget to avoid blocking
      loadProfilesList().catch(err => console.error('Failed to refresh profile list:', err));
      alert('Profile saved: ' + currentProfileName);
    }

    async function showRenameDialog() {
      if (!currentProfileName) return alert('No profile loaded');
      const newName = prompt('Enter new profile name:', currentProfileName);
      if (!newName || newName === currentProfileName) return;
      try {
        // Save current setpoints under new name
        const payload = { setpoints, name: newName };
        const resp = await fetch('/api/profile/' + encodeURIComponent(newName), { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload) });
        const data = await resp.json();
        if (data.ok) {
          currentProfileName = newName;
          markSaved();
          await loadProfilesList();
          // Select the renamed profile in the list
          document.getElementById('profilesList').value = newName;
        } else {
          alert('Rename failed: ' + (data.error || 'unknown'));
        }
      } catch (err) {
        alert('Error renaming profile: ' + err.message);
      }
    }

    async function autoLoadProfile() {
      const sel = document.getElementById('profilesList');
      const name = sel.value;
      if (!name || isLoading) return;
      isLoading = true;
      const btn = sel.nextElementSibling;
      const origHTML = btn.innerHTML;
      btn.innerHTML = origHTML + '<span class="spinner"></span>';
      btn.disabled = true;
      try {
        const resp = await fetch('/api/profile/' + encodeURIComponent(name));
        if (!resp.ok) { alert('Failed to load profile'); return; }
        const data = await resp.json();
        setpoints = data.setpoints || [];
        selectedIndex = -1;
        currentProfileName = name;
        markSaved();
        renderGraph();
        updateActivateButton();
      } catch (err) {
        alert('Error loading profile: ' + err.message);
      } finally {
        btn.innerHTML = origHTML;
        btn.disabled = false;
        isLoading = false;
      }
    }

    async function activateSelected() {
      const sel = document.getElementById('profilesList');
      const name = sel.value;
      if (!name) return alert('Please select a profile');
      try {
        const resp = await fetch('/api/profile/' + encodeURIComponent(name) + '/activate', { method:'POST' });
        if (!resp.ok) {
          const text = await resp.text();
          return alert('Activation failed: ' + resp.status + ' ' + text);
        }
        const data = await resp.json();
        if (!data.ok) return alert('Activation failed: ' + (data.error || 'Unknown error'));
        activeProfileName = data.active || name;
        currentProfileName = data.active || '';
        const r = await fetch('/api/profile/' + encodeURIComponent(name)); const d = await r.json(); setpoints = d.setpoints||[]; markSaved(); renderGraph();
        await loadProfilesList();  // Refresh list to update (Active) markers
        alert('Profile activated: ' + name);
      } catch (err) {
        alert('Error activating profile: ' + err.message);
      }
    }

    async function deleteSelected() {
      const sel = document.getElementById('profilesList');
      const name = sel.value; 
      if (!name) return;
      if (!confirm('Delete profile "' + name + '"?')) return;
      const resp = await fetch('/api/profile/' + encodeURIComponent(name), { method:'DELETE' });
      if (resp.status === 409) { alert('Cannot delete active profile'); return; }
      await loadProfilesList();
    }

    async function createNew() {
      const name = prompt('Enter new profile name:', 'New Profile');
      if (!name) return;
      try {
        const payload = {
          name: name,
          activate: false,
          setpoints: [
            { time: 0, temp: 200, fanSpeed: 30 },
            { time: 180, temp: 350, fanSpeed: 50 },
            { time: 420, temp: 400, fanSpeed: 70 },
            { time: 600, temp: 444, fanSpeed: 80 }
          ]
        };
        const resp = await fetch('/api/profile/' + encodeURIComponent(name), { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload) });
        const data = await resp.json();
        if (data.ok) {
          pushHistory();
          setpoints = payload.setpoints;
          currentProfileName = name;
          selectedIndex = -1;
          markSaved();
          loadProfilesList().catch(err => console.error('Failed to refresh profile list:', err));
          renderGraph();
          // Select the new profile in the list
          document.getElementById('profilesList').value = name;
        } else {
          alert('Failed to create profile: ' + (data.error || 'unknown'));
        }
      } catch (err) {
        alert('Error creating profile: ' + err.message);
      }
    }

    async function renameProfile() {
      const newName = document.getElementById('profileName').value.trim();
      if (!newName) return;
      try {
        // Save current setpoints under new name
        const payload = { setpoints, name: newName };
        const resp = await fetch('/api/profile/' + encodeURIComponent(newName), { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload) });
        const data = await resp.json();
        if (data.ok) { await loadProfilesList(); }
      } catch (err) {
        console.error('Error renaming profile:', err);
      }
    }

    // Debug removed

    // Debug removed

    init();
  </script>
</body>
</html>
)rawliteral";

#endif // PROFILE_WEB_UI_HPP
