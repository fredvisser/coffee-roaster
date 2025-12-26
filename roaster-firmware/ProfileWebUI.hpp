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
    .fan-point { fill:#a371f7; stroke:#fff; stroke-width:1; cursor:grab; touch-action:none; opacity:0.6; }
    .fan-point:hover { opacity:1; }
    .fan-point.selected { fill:#d29ff7; opacity:1; }
    .fan-point:active { cursor:grabbing; }
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
        </div>
        <div class="legend">Time (s) on X-axis; Temperature (F) on left Y-axis; Fan (%) on right Y-axis and per point; drag points to edit</div>
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

  <div id="toast" style="position:fixed; bottom:20px; right:20px; background:#161b22; border:1px solid #30363d; color:#c9d1d9; padding:10px 14px; border-radius:6px; box-shadow:0 8px 24px rgba(0,0,0,0.3); display:none; z-index:2000;"></div>

  <div id="inputModal" style="position:fixed; top:0; left:0; right:0; bottom:0; background:rgba(0,0,0,0.7); display:none; align-items:center; justify-content:center; z-index:3000;">
    <div style="background:#161b22; border:1px solid #30363d; border-radius:8px; padding:20px; min-width:400px; max-width:500px;">
      <h3 id="modalTitle" style="margin:0 0 16px; color:#fff; font-size:18px;"></h3>
      <input id="modalInput" type="text" class="input" style="width:100%; margin-bottom:16px;" />
      <div style="display:flex; gap:8px; justify-content:flex-end;">
        <button class="btn" onclick="closeInputModal()">Cancel</button>
        <button class="btn primary" onclick="confirmInputModal()">OK</button>
      </div>
    </div>
  </div>

  <script>
    const MAX_POINTS = 10;
    let setpoints = [];
    let selectedIndex = -1;
    let currentProfileId = '';
    let currentProfileName = '';
    let activeProfileId = '';  // Track which profile is currently active
    let activeProfileName = '';
    let hasUnsavedChanges = false;
    const graphPadding = { left: 40, right: 40, top: 20, bottom: 40 };
    const graphSize = { width: 800, height: 400 };
    // Debug removed
    let svg = null;
    let container = null;
    let history = [];
    let redoStack = [];
    const MAX_HISTORY = 100;
    const SNAP = { enabled: true, time: 15, temp: 5 };
    let roastTimeMinutes = 8;
    let isLoading = false;
    let toastTimer = null;
    let inputModalResolve = null;

    function showToast(message, kind = 'info') {
      const el = document.getElementById('toast');
      if (!el) return;
      el.textContent = message;
      // Simple coloring by kind
      const colors = {
        info: '#58a6ff',
        success: '#3fb950',
        warn: '#f0883e',
        error: '#f85149'
      };
      el.style.borderColor = '#30363d';
      el.style.boxShadow = '0 8px 24px rgba(0,0,0,0.3)';
      el.style.display = 'block';
      el.style.opacity = '1';
      // Add left accent via box-shadow inset
      el.style.boxShadow = `inset 4px 0 0 ${colors[kind] || colors.info}, 0 8px 24px rgba(0,0,0,0.3)`;
      if (toastTimer) clearTimeout(toastTimer);
      toastTimer = setTimeout(() => {
        el.style.opacity = '0';
        setTimeout(() => { el.style.display = 'none'; }, 300);
      }, 5000);
    }

    function showInputModal(title, defaultValue = '') {
      return new Promise((resolve) => {
        const modal = document.getElementById('inputModal');
        const titleEl = document.getElementById('modalTitle');
        const input = document.getElementById('modalInput');
        
        titleEl.textContent = title;
        input.value = defaultValue;
        modal.style.display = 'flex';
        
        inputModalResolve = resolve;
        
        // Focus input and select text
        setTimeout(() => {
          input.focus();
          input.select();
        }, 100);
        
        // Handle Enter key
        input.onkeydown = (e) => {
          if (e.key === 'Enter') {
            e.preventDefault();
            confirmInputModal();
          } else if (e.key === 'Escape') {
            e.preventDefault();
            closeInputModal();
          }
        };
      });
    }

    function closeInputModal() {
      const modal = document.getElementById('inputModal');
      modal.style.display = 'none';
      if (inputModalResolve) {
        inputModalResolve(null);
        inputModalResolve = null;
      }
    }

    function confirmInputModal() {
      const input = document.getElementById('modalInput');
      const value = input.value.trim();
      const modal = document.getElementById('inputModal');
      modal.style.display = 'none';
      if (inputModalResolve) {
        inputModalResolve(value);
        inputModalResolve = null;
      }
    }

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
      await loadProfilesList();
      // Load active profile if available
      if (activeProfileId) {
        // Ensure the select box reflects the active profile
        const sel = document.getElementById('profilesList');
        if (sel && sel.value !== activeProfileId) {
            sel.value = activeProfileId;
        }

        const resp = await fetch('/api/profile/' + encodeURIComponent(activeProfileId));
        if (resp.ok) {
          const data = await resp.json();
          setpoints = data.setpoints || [];
          currentProfileId = data.id;
          currentProfileName = data.name || '';
          markSaved();
          updateProfileDisplay();
          updateRoastTimeFromSetpoints();
        }
      } else {
        // If no active profile, load the first one if list not empty
        const sel = document.getElementById('profilesList');
        if (sel && sel.options.length > 0) {
            await autoLoadProfile();
        }
      }
      const snapEl = document.getElementById('snapEnabled');
      if (snapEl) snapEl.checked = SNAP.enabled;
      document.getElementById('importFile').addEventListener('change', handleImportFile);

      requestAnimationFrame(() => {
        if (!computeGraphSize()) {
          setTimeout(() => { if (computeGraphSize()) renderGraph(); }, 0);
        } else {
          renderGraph();
        }
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
      activeProfileId = data.active || '';
      activeProfileName = '';

      (data.profiles || []).forEach(p => {
        const opt = document.createElement('option');
        opt.value = p.id;
        opt.textContent = p.name + (p.active ? ' (Active)' : '');
        opt.dataset.name = p.name;
        if (p.active) activeProfileName = p.name;
        sel.appendChild(opt);
      });

      const targetId = currentProfileId || activeProfileId;
      if (targetId) {
        // Try to select the target profile
        let found = false;
        for (let i = 0; i < sel.options.length; i++) {
          if (sel.options[i].value === targetId) { 
            sel.selectedIndex = i; 
            found = true;
            break; 
          }
        }
        // If not found (and we have options), default to first
        if (!found && sel.options.length > 0) {
             sel.selectedIndex = 0;
        }
      } else if (sel.options.length > 0) {
          sel.selectedIndex = 0;
      }

      updateActivateButton();
    }

    function updateActivateButton() {
      const sel = document.getElementById('profilesList');
      const btn = document.getElementById('activateBtn');
      if (!btn) return;
      const selectedId = sel.value;
      // Disable if selected profile is already active
      btn.disabled = (selectedId === activeProfileId);
    }

    function getBounds() {
      const maxTime = roastTimeMinutes * 60; // Convert to seconds
      return { minT: 0, maxT: maxTime, minY: 0, maxY: 500 };
    }

    function updateRoastTimeFromSetpoints() {
      if (!Array.isArray(setpoints) || setpoints.length === 0) return;
      const lastTime = Math.max.apply(null, setpoints.map(sp => sp.time || 0));
      const seconds = Math.min(1200, Math.max(0, lastTime + 120));
      roastTimeMinutes = Math.max(1, Math.ceil(seconds / 60));
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
      
      // Add right y-axis for fan speed %
      const yAxisRight = document.createElementNS('http://www.w3.org/2000/svg','line');
      yAxisRight.setAttribute('x1', graphSize.width - graphPadding.right); yAxisRight.setAttribute('y1', graphPadding.top);
      yAxisRight.setAttribute('x2', graphSize.width - graphPadding.right); yAxisRight.setAttribute('y2', graphSize.height - graphPadding.bottom);
      yAxisRight.setAttribute('class','axis'); svg.appendChild(yAxisRight);
      
      // Add fan speed % labels on right axis
      for (let i=0;i<=5;i++) {
        const fanPct = Math.round(i * 100 / 5);
        const yVal = (i * 100 / 5) * 5;  // Convert back to temp range (0-100 maps to 0-500)
        const t = document.createElementNS('http://www.w3.org/2000/svg','text');
        t.setAttribute('x', graphSize.width - graphPadding.right + 8); t.setAttribute('y', yScale(yVal));
        t.setAttribute('fill', '#a371f7'); t.setAttribute('font-size','11');
        t.textContent = fanPct + '%'; 
        svg.appendChild(t);
      }

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
        
        // Add fan speed point for dragging (scaled 0-100 to 0-500 range)
        const fanC = document.createElementNS('http://www.w3.org/2000/svg','circle');
        const fanScaled = (sp.fanSpeed || 0) * 5;
        fanC.setAttribute('cx', xScale(sp.time)); 
        fanC.setAttribute('cy', yScale(fanScaled)); 
        fanC.setAttribute('r', 5);
        fanC.setAttribute('class','fan-point' + (i===selectedIndex?' selected':''));
        fanC.setAttribute('data-index', i);
        fanC.addEventListener('mousedown', (e)=> startFanDrag(e,i));
        fanC.addEventListener('touchstart', (e)=> startFanDrag(e,i), {passive: false});
        fanC.addEventListener('click', ()=> selectPoint(i));
        svg.appendChild(fanC);
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
    
    function startFanDrag(ev, i) {
      ev.preventDefault();
      ev.stopPropagation();
      const targetSVG = svg;
      if (!targetSVG) return;
      const b = getBounds();
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
        const relY = clientY - rect.top;
        return { y: relY };
      };

      const move = (e) => {
        e.preventDefault();
        const loc = getCoords(e);
        let fanVal = Math.round(invY(loc.y) / 5);  // Convert from temp range back to 0-100%
        fanVal = Math.max(0, Math.min(100, fanVal));
        setpoints[i].fanSpeed = fanVal;
        renderGraph();
        selectPoint(i);
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
          const st = SNAP.time;
          const sy = SNAP.temp;
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
        updateRoastTimeFromSetpoints();
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
      if (setpoints.length >= MAX_POINTS) { showToast('Max points reached','warn'); return; }
      pushHistory();
      const st = SNAP.time;
      let t = setpoints.length ? Math.min(setpoints[setpoints.length-1].time + st, 3600) : 0;
      while (setpoints.some(sp => sp.time === t)) {
        t = Math.min(t + st, 3600);
        if (t === 3600 && setpoints.some(sp => sp.time === 3600)) break;
      }
      const sy = SNAP.temp;
      const sp = { time: t, temp: Math.round(200 / sy) * sy, fanSpeed: 100 };
      setpoints.push(sp);
      setpoints.sort((a,b)=>a.time-b.time);
      selectedIndex = setpoints.length - 1;
      markUnsaved();
      updateRoastTimeFromSetpoints();
      renderGraph();
      selectPoint(selectedIndex);
    }

    function removeSelected() {
      if (selectedIndex < 0) return;
      pushHistory();
      setpoints.splice(selectedIndex, 1);
      selectedIndex = -1;
      markUnsaved();
      updateRoastTimeFromSetpoints();
      renderGraph();
    }

    function applyPointEdit() {
      if (selectedIndex < 0) return;
      let t = parseInt(document.getElementById('ptTime').value||'0',10);
      let temp = parseInt(document.getElementById('ptTemp').value||'0',10);
      const fan = parseInt(document.getElementById('ptFan').value||'0',10);
      
      // Enforce first point at time=0
      if (selectedIndex === 0 && t !== 0) {
        showToast('First point must be at time 0','error');
      }
      pushHistory();
      const bounds = getBounds();
      t = Math.min(Math.max(bounds.minT, t), bounds.maxT);
      temp = Math.min(Math.max(bounds.minY, temp), bounds.maxY);
      const sp = setpoints[selectedIndex];
      sp.time = t;
      sp.temp = temp;
      sp.fanSpeed = Math.min(100, Math.max(0, fan));
      setpoints.sort((a,b)=>a.time-b.time);
      selectedIndex = setpoints.findIndex(p => p === sp);
      markUnsaved();
      updateRoastTimeFromSetpoints();
      renderGraph();
      selectPoint(selectedIndex);
    }

    function cloneSetpoints() { return setpoints.map(sp => ({ time: sp.time, temp: sp.temp, fanSpeed: sp.fanSpeed })); }
    function pushHistory() { history.push(cloneSetpoints()); if (history.length > MAX_HISTORY) history.shift(); redoStack = []; }
    function undo() { if (history.length === 0) return; redoStack.push(cloneSetpoints()); setpoints = history.pop(); selectedIndex = -1; markUnsaved(); renderGraph(); }
    function redo() { if (redoStack.length === 0) return; history.push(cloneSetpoints()); setpoints = redoStack.pop(); selectedIndex = -1; markUnsaved(); renderGraph(); }

    function updateSnapSettings() {
      SNAP.enabled = !!document.getElementById('snapEnabled').checked;
    }

    function exportJSON() {
      const name = currentProfileName || 'unnamed';
      const payload = { name, setpoints };
      const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
      const a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = 'profile-' + name + '.json';
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
            fanSpeed: Math.max(0, Math.min(100, parseInt(sp.fanSpeed||100, 10)))
        })).filter(sp => Number.isFinite(sp.time) && Number.isFinite(sp.temp) && Number.isFinite(sp.fanSpeed));
        if (cleaned.length === 0 || cleaned.length > MAX_POINTS) throw new Error('Invalid setpoint count');
        pushHistory();
        setpoints = cleaned.sort((a,b)=>a.time-b.time);
        currentProfileId = '';
        if (typeof obj.name === 'string') currentProfileName = obj.name;
        selectedIndex = -1;
        markUnsaved();
        updateRoastTimeFromSetpoints();
        renderGraph();
        e.target.value = '';
      } catch (err) {
        showToast('Import failed: ' + err.message,'error');
      }
    }

    async function saveProfile() {
      if (setpoints.length === 0) { showToast('Add at least one point','warn'); return; }
      const payload = { setpoints, name: currentProfileName || 'Unnamed', activate: false };
      let resp;
      if (currentProfileId) {
        resp = await fetch('/api/profile/' + encodeURIComponent(currentProfileId), { method:'PUT', headers:{'Content-Type':'application/json'}, body: JSON.stringify(payload) });
      } else {
        resp = await fetch('/api/profiles', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(payload) });
      }
      const data = await resp.json();
      if (!data.ok) { showToast('Save failed: ' + (data.error || 'unknown'),'error'); return; }
      currentProfileId = data.id || currentProfileId;
      currentProfileName = data.name || currentProfileName;
      markSaved();
      loadProfilesList().catch(err => console.error('Failed to refresh profile list:', err));
      
      if (currentProfileId === activeProfileId) {
        try {
          await fetch('/api/profile/' + encodeURIComponent(currentProfileId) + '/activate', { method:'POST' });
          showToast('Profile saved and reapplied','success');
        } catch (e) {
          showToast('Profile saved but failed to reapply','warn');
        }
      } else {
        showToast('Profile saved: ' + (currentProfileName || currentProfileId),'success');
      }

      setTimeout(() => {
        const sel = document.getElementById('profilesList');
        if (sel) { sel.value = currentProfileId; updateActivateButton(); }
      }, 50);
    }

    async function showRenameDialog() {
      if (!currentProfileId) { showToast('No profile loaded','error'); return; }
      const entered = await showInputModal('Rename Profile', currentProfileName || '');
      if (!entered || entered === currentProfileName) return;
      const newName = entered;
      try {
        const payload = { setpoints, name: newName, activate: false };
        const resp = await fetch('/api/profile/' + encodeURIComponent(currentProfileId), { method: 'PUT', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload) });
        const data = await resp.json();
        if (!data.ok) { showToast('Rename failed: ' + (data.error || 'unknown'),'error'); return; }

        currentProfileName = newName;
        markSaved();
        await loadProfilesList();
        const sel = document.getElementById('profilesList');
        if (sel) { sel.value = currentProfileId; updateActivateButton(); }
        showToast('Profile renamed to: ' + newName,'success');
      } catch (err) {
        showToast('Error renaming profile: ' + err.message,'error');
      }
    }

    async function autoLoadProfile() {
      const sel = document.getElementById('profilesList');
      const id = sel.value;
      if (!id || isLoading) return;
      isLoading = true;
      const spinnerEl = document.createElement('span');
      spinnerEl.className = 'spinner';
      spinnerEl.id = 'loadSpinner';
      const profileNameEl = document.getElementById('currentProfileName');
      profileNameEl.appendChild(spinnerEl);
      try {
        const resp = await fetch('/api/profile/' + encodeURIComponent(id));
        if (!resp.ok) { showToast('Failed to load profile','error'); return; }
        const data = await resp.json();
        setpoints = data.setpoints || [];
        selectedIndex = -1;
        currentProfileId = data.id;
        currentProfileName = data.name || '';
        markSaved();
        updateRoastTimeFromSetpoints();
        renderGraph();
        updateActivateButton();
      } catch (err) {
        showToast('Error loading profile: ' + err.message,'error');
      } finally {
        const spinner = document.getElementById('loadSpinner');
        if (spinner) spinner.remove();
        isLoading = false;
      }
    }

    async function activateSelected() {
      const sel = document.getElementById('profilesList');
      const id = sel.value;
      const name = (sel.selectedOptions[0] && sel.selectedOptions[0].dataset.name) || '';
      if (!id) { showToast('Please select a profile','warn'); return; }
      try {
        const resp = await fetch('/api/profile/' + encodeURIComponent(id) + '/activate', { method:'POST' });
        if (!resp.ok) {
          const text = await resp.text();
          showToast('Activation failed: ' + resp.status + ' ' + text,'error');
          return;
        }
        const data = await resp.json();
        if (!data.ok) { showToast('Activation failed: ' + (data.error || 'Unknown error'),'error'); return; }
        activeProfileId = data.active || id;
        activeProfileName = data.name || name;
        currentProfileId = activeProfileId;
        currentProfileName = activeProfileName;
        const r = await fetch('/api/profile/' + encodeURIComponent(activeProfileId));
        const d = await r.json(); setpoints = d.setpoints||[]; markSaved(); renderGraph();
        await loadProfilesList();
        showToast('Profile activated: ' + (activeProfileName || activeProfileId),'success');
      } catch (err) {
        showToast('Error activating profile: ' + err.message,'error');
      }
    }

    async function deleteSelected() {
      const sel = document.getElementById('profilesList');
      const id = sel.value; 
      if (!id) return;

      const resp = await fetch('/api/profile/' + encodeURIComponent(id), { method:'DELETE' });
      if (resp.status === 409) { showToast('Cannot delete active profile','error'); return; }
      await loadProfilesList();
      const sel2 = document.getElementById('profilesList');
      if (sel2 && activeProfileId) {
        sel2.value = activeProfileId;
        updateActivateButton();
        await autoLoadProfile();
      }
    }

    async function createNew() {
      const entered = await showInputModal('Create New Profile', 'New Profile');
      if (!entered) return;
      const name = entered;
      try {
        const payload = {
          name: name,
          activate: false,
          setpoints: [
            { time: 0, temp: 200, fanSpeed: 100 },
            { time: 180, temp: 350, fanSpeed: 100 },
            { time: 420, temp: 400, fanSpeed: 100 },
            { time: 600, temp: 444, fanSpeed: 100 }
          ]
        };
        const sel = document.getElementById('profilesList');
        if (sel) sel.value = '';
        
        const resp = await fetch('/api/profiles', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(payload) });
        const data = await resp.json();
        if (data.ok) {
          pushHistory();
          setpoints = data.setpoints || payload.setpoints;
          currentProfileId = data.id;
          currentProfileName = data.name || name;
          selectedIndex = -1;
          markSaved();
          try {
            await loadProfilesList();
          } catch (err) {
            const sel2 = document.getElementById('profilesList');
            if (sel2) {
              const opt = document.createElement('option');
              opt.value = currentProfileId;
              opt.textContent = currentProfileName;
              sel2.appendChild(opt);
              sel2.value = currentProfileId;
              updateActivateButton();
            }
          }
          updateRoastTimeFromSetpoints();
          renderGraph();
          showToast('Profile created: ' + currentProfileName,'success');
        } else {
          showToast('Create failed: ' + (data.error || 'unknown'),'error');
        }
      } catch (err) {
        showToast('Error creating profile: ' + err.message,'error');
      }
    }

    function updateSnapSettings() {
      SNAP.enabled = !!document.getElementById('snapEnabled').checked;
    }

    function exportJSON() {
      const name = currentProfileName || 'unnamed';
      const payload = { name, setpoints };
      const blob = new Blob([JSON.stringify(payload, null, 2)], { type: 'application/json' });
      const a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = 'profile-' + name + '.json';
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
            fanSpeed: Math.max(0, Math.min(100, parseInt(sp.fanSpeed||100, 10)))
        })).filter(sp => Number.isFinite(sp.time) && Number.isFinite(sp.temp) && Number.isFinite(sp.fanSpeed));
        if (cleaned.length === 0 || cleaned.length > MAX_POINTS) throw new Error('Invalid setpoint count');
        pushHistory();
        setpoints = cleaned.sort((a,b)=>a.time-b.time);
        currentProfileId = '';
        if (typeof obj.name === 'string') currentProfileName = obj.name;
        selectedIndex = -1;
        markUnsaved();
        updateRoastTimeFromSetpoints();
        renderGraph();
        e.target.value = '';
      } catch (err) {
        showToast('Import failed: ' + err.message,'error');
      }
    }

    // renameProfile() - use showRenameDialog() instead

    // Debug removed

    // Debug removed

    init();
  </script>
</body>
</html>
)rawliteral";

#endif // PROFILE_WEB_UI_HPP
