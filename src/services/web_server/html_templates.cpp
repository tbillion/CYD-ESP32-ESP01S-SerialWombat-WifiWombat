#include "html_templates.h"

// ===================================================================================
// --- HTML: Dashboard (wifiwombat06 - PRESERVED) ---
// ===================================================================================
const char INDEX_HTML_HEAD[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Wombat Manager</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; text-align: center; background: #222; color: #eee; margin:0; padding:10px; }
    .card { background: #333; padding: 15px; margin: 10px auto; max-width: 450px; border-radius: 8px; border: 1px solid #444; }
    h2 { color: #00d2ff; margin: 0 0 10px 0; }
    h3 { border-bottom: 1px solid #555; padding-bottom: 5px; margin: 10px 0; font-size: 1.1em; color: #ccc;}
    button { background: #007acc; color: white; padding: 12px; border: none; border-radius: 4px; cursor: pointer; margin: 5px; width: 100%; font-size:1rem; }
    button.scan { background: #28a745; }
    button.deep { background: #6f42c1; }
    button.flash { background: #d35400; font-weight:bold; }
    button.danger { background: #c0392b; }
    button.warn { background: #e67e22; color: #fff; }
    select, input[type=text], textarea { padding: 10px; margin: 5px; width: 95%; background: #ddd; border: none; border-radius: 4px; box-sizing: border-box; }
    input[type=radio] { transform: scale(1.5); margin: 10px; }
    .status { font-family: monospace; color: #0f0; }
    .warn-text { color: #e74c3c; font-size: 0.8rem; font-weight: bold; }
    .slot-label { display: block; text-align: left; padding: 5px; background: #444; margin-bottom: 2px; border-radius: 4px; cursor: pointer;}
    .slot-label:hover { background: #555; }
  </style>
</head>
<body>
  <h2>Wombat Wifi Bridge</h2>
  
  <div class="card">
    <div style="text-align:left; font-size:0.9em;">
      <strong>Bridge IP:</strong> <span class="status">%IP%</span> (Port 3000)<br>
      <strong>Target:</strong> <span class="status">0x%ADDR%</span>
    </div>
  </div>

  <div class="card">
    <h3>Scanner Tools</h3>
    <button class="scan" onclick="location.href='/scanner'">Fast I2C Scanner</button>
    <button class="deep" onclick="location.href='/deepscan'">Deep Chip Analysis</button>
  </div>

  <div class="card" style="border:1px solid #00d2ff;">
    <h3>Firmware Manager</h3>
    <button onclick="document.getElementById('uploadUi').style.display='block';">Update a Firmware Slot</button>
    
    <div id="uploadUi" style="display:none; text-align:left; background:#222; padding:10px; margin-top:10px;">
      
      <h4>1. Select Slot to Update:</h4>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Default_FW" checked> Default Firmware</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Brushed_Motor"> Brushed Motor</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Front_Panel"> Front Panel</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Keypad"> Keypad</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Motor_Control"> Motor Control</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="TM1637"> TM1637 Display</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Ultrasonic"> Ultrasonic</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Comms"> Communications</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Custom1"> Custom Slot 1</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Custom2"> Custom Slot 2</label>

      <h4>2. Enter Version:</h4>
      <input type="text" id="fwVer" placeholder="e.g. 2.1.2">

      <h4>3. Input Method:</h4>
      <div style="display:flex; gap:18px; align-items:center; flex-wrap:wrap;">
        <label style="display:inline-flex; align-items:center; gap:8px;">
          <input type="radio" name="fwInputMode" value="blob" checked onchange="setFwInputMode('blob')"> Blob
        </label>
        <label style="display:inline-flex; align-items:center; gap:8px;">
          <input type="radio" name="fwInputMode" value="hex" onchange="setFwInputMode('hex')"> IntelHEX
        </label>
        %SD_FW_OPTION%
      </div>

      <div id="fwBlobArea" style="margin-top:10px;">
        <h4>4. Paste Array Code:</h4>
        <textarea id="fwContent" rows="5" placeholder="0x306F, 0x37A0, ..."></textarea>
      </div>

      <div id="fwHexArea" style="display:none; margin-top:10px;">
        <h4>4. Upload Intel HEX (.hex):</h4>
        <input type="file" id="fwHexFile" accept=".hex" style="display:none" />
        <button class="scan" type="button" onclick="document.getElementById('fwHexFile').click();">Choose .hex File</button>
        <div id="fwHexName" class="status" style="margin-top:6px; font-size:0.9em; word-break:break-all;"></div>
      </div>

      %SD_FW_AREA%

      <button class="scan" onclick="uploadFW()">Save & Overwrite Slot</button>
      <div id="uploadStatus"></div>
    </div>
    
    <script>
      function setFwInputMode(mode) {
        var blobArea = document.getElementById('fwBlobArea');
        var hexArea  = document.getElementById('fwHexArea');
        var sdArea   = document.getElementById('fwSdArea');

        if (blobArea) blobArea.style.display = 'none';
        if (hexArea)  hexArea.style.display  = 'none';
        if (sdArea)   sdArea.style.display   = 'none';

        if (mode === 'hex') {
          if (hexArea) hexArea.style.display = 'block';
        } else if (mode === 'sd') {
          if (sdArea) sdArea.style.display = 'block';
          try { if (typeof refreshSdFwList === 'function') refreshSdFwList(); } catch(e) {}
        } else {
          if (blobArea) blobArea.style.display = 'block';
        }
      }

      // Keep filename display in HEX mode
      (function(){
        var f = document.getElementById('fwHexFile');
        if (f) {
          f.addEventListener('change', function(){
            var n = (f.files && f.files.length) ? f.files[0].name : '';
            var lbl = document.getElementById('fwHexName');
            if (lbl) lbl.textContent = n ? ('Selected: ' + n) : '';
          });
        }
      })();

      function getSelectedSlot() {
        var slots = document.getElementsByName('fwSlot');
        for (var i=0; i<slots.length; i++) {
          if (slots[i].checked) return slots[i].value;
        }
        return '';
      }

      function getInputMode() {
        var modes = document.getElementsByName('fwInputMode');
        for (var i=0; i<modes.length; i++) {
          if (modes[i].checked) return modes[i].value;
        }
        return 'blob';
      }

      function uploadFW() {
        var mode = getInputMode();
        if (mode === 'hex') return uploadFW_hex();
        if (mode === 'sd') return uploadFW_sd();
        return uploadFW_blob();
      }

      function uploadFW_blob() {
        var slotName = getSelectedSlot();
        var ver = document.getElementById('fwVer').value.trim();
        var text = document.getElementById('fwContent').value;

        if(!ver || !text) { alert("Please enter Version and Paste Code."); return; }

        var finalName = slotName + "_" + ver;
        var status = document.getElementById('uploadStatus');
        status.innerHTML = "Cleaning old files...";

        // 1. Clean Slot First
        fetch('/clean_slot?prefix=' + encodeURIComponent(slotName))
        .then(() => {
            status.innerHTML = "Parsing Hex...";
            // 2. Parse Hex
            var hexValues = text.match(/0x[0-9A-Fa-f]{1,4}/g);
            if(!hexValues) { status.innerHTML = "Error: No hex found."; return; }

            var bytes = new Uint8Array(hexValues.length * 2);
            for(var i=0; i<hexValues.length; i++) {
               var val = parseInt(hexValues[i], 16);
               bytes[i*2] = val & 0xFF;
               bytes[i*2+1] = (val >> 8) & 0xFF;
            }

            status.innerHTML = "Uploading " + bytes.length + " bytes...";

            // 3. Upload
            var blob = new Blob([bytes], {type: "application/octet-stream"});
            var fd = new FormData();
            fd.append("file", blob, finalName + ".bin");

            return fetch('/upload_fw', {method:'POST', body:fd});
        })
        .then(r => r.text())
        .then(() => {
             status.innerHTML = "Success! Reloading...";
             setTimeout(() => location.reload(), 1500);
        })
        .catch(e => status.innerHTML = "Error: " + e);
      }

      function uploadFW_hex() {
        var slotName = getSelectedSlot();
        var ver = document.getElementById('fwVer').value.trim();
        var f = document.getElementById('fwHexFile');
        var file = (f && f.files && f.files.length) ? f.files[0] : null;

        if(!ver) { alert("Please enter Version."); return; }
        if(!file) { alert("Please choose a .hex file."); return; }

        var status = document.getElementById('uploadStatus');
        status.innerHTML = "Cleaning old files...";

        fetch('/clean_slot?prefix=' + encodeURIComponent(slotName))
        .then(() => {
          status.innerHTML = "Uploading HEX...";
          var fd = new FormData();
          fd.append('file', file, file.name);
          var url = '/upload_hex?prefix=' + encodeURIComponent(slotName) + '&ver=' + encodeURIComponent(ver);
          return fetch(url, {method:'POST', body:fd});
        })
        .then(r => r.text())
        .then(t => {
          status.innerHTML = t + "<br>Reloading...";
          setTimeout(() => location.reload(), 1500);
        })
        .catch(e => status.innerHTML = "Error: " + e);
      }

      function uploadFW_sd() {
        var slotName = getSelectedSlot();
        var ver = document.getElementById('fwVer').value.trim();
        var sel = document.getElementById('fwSdSelect');
        var sdPath = sel ? sel.value : '';

        if (!ver) { alert('Please enter Version.'); return; }
        if (!sdPath) { alert('Please select a file from SD.'); return; }

        var status = document.getElementById('uploadStatus');
        status.innerHTML = 'Cleaning old files...';

        fetch('/clean_slot?prefix=' + encodeURIComponent(slotName))
        .then(() => {
          status.innerHTML = 'Importing from SD...';
          return fetch('/api/sd/import_fw', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ path: sdPath, slot: slotName, ver: ver })
          });
        })
        .then(r => r.text())
        .then(t => {
          status.innerHTML = t + '<br>Reloading...';
          setTimeout(() => location.reload(), 1500);
        })
        .catch(e => status.innerHTML = 'Error: ' + e);
      }
    </script>
  </div>

  <div class="card" style="border:1px solid #d35400;">
    <h3 style="color:#e67e22;">Firmware Flasher</h3>
    <p class="warn-text">WARNING: Do not power off during update.</p>
    <form action="/flashfw" method="POST">
      <label>Select Firmware from Storage:</label>
      <select name="fw_name">
)rawliteral";

const char INDEX_HTML_TAIL[] PROGMEM = R"rawliteral(
      </select>
      <button class="flash" type="submit" onclick="return confirm('Ready to Flash SW8B? This takes about 30 seconds.');">Start Flash Update</button>
    </form>
  </div>

  <div class="card">
    <h3>Settings</h3>
    <form action="/connect" method="GET">
      <input type="text" name="addr" placeholder="Hex (e.g. 0x6C)"><br>
      <button type="submit">Connect to Address</button>
    </form>
    
    <button class="warn" onclick="if(confirm('Reset Wombat at Target Address?')) location.href='/resetwombat'">Reset Target Wombat</button>
    
    <hr style="border-color:#555;">
    <form action="/changeaddr" method="GET">
      <input type="text" name="newaddr" placeholder="Change Hardware Addr"><br>
      <button class="danger" type="submit">Flash New Address</button>
    </form>
    <br>
    <button onclick="location.href='/resetwifi'">Reset WiFi</button>
    <button onclick="if(confirm('Delete ALL firmwares?')) location.href='/formatfs'" style="background:#555;font-size:0.7em;">Format Storage</button>
  </div>

  %SD_TILE%
</body>
</html>
)rawliteral";



#if SD_SUPPORT_ENABLED
// ===================================================================================
// --- SD Card Management UI Fragments (injected conditionally) ---
// ===================================================================================
const char SD_FW_OPTION_HTML[] PROGMEM = R"rawliteral(
        <label style="display:inline-flex; align-items:center; gap:8px;">
          <input type="radio" name="fwInputMode" value="sd" onchange="setFwInputMode('sd')"> SD_Custom
        </label>
)rawliteral";

const char SD_FW_AREA_HTML[] PROGMEM = R"rawliteral(
      <div id="fwSdArea" style="display:none; margin-top:10px;">
        <h4>4. Pick from SD Card:</h4>
        <div style="display:flex; gap:10px; align-items:center; flex-wrap:wrap;">
          <select id="fwSdSelect" style="flex:1; min-width:220px;"></select>
          <button class="scan" type="button" onclick="refreshSdFwList()">Refresh</button>
        </div>
        <small style="color:#aaa; display:block; margin-top:6px;">Select a file from the SD card root. Supports .bin (direct import), .txt (FW array text), or .hex (convert).</small>
      </div>

      <script>
      async function refreshSdFwList() {
        const sel = document.getElementById('fwSdSelect');
        if (!sel) return;
        sel.innerHTML = '';
        try {
          const r = await fetch('/api/sd/list?path=/');
          if (!r.ok) { sel.innerHTML = '<option value="">SD not available</option>'; return; }
          const j = await r.json();
          const files = (j && j.entries) ? j.entries : [];
          const keep = files.filter(e => !e.isDir && (e.name.endsWith('.bin') || e.name.endsWith('.hex') || e.name.endsWith('.txt')));
          if (!keep.length) { sel.innerHTML = '<option value="">No .bin/.hex/.txt found</option>'; return; }
          keep.forEach(e => {
            const opt = document.createElement('option');
            opt.value = '/' + e.name;
            opt.textContent = e.name + (e.size ? (' (' + e.size + ' bytes)') : '');
            sel.appendChild(opt);
          });
        } catch (e) {
          sel.innerHTML = '<option value="">SD error</option>';
        }
      }
      </script>
)rawliteral";

const char SD_TILE_HTML[] PROGMEM = R"rawliteral(
  <div class="card" id="sdCardTile">
    <h3>SD Card Management</h3>
    <div id="sdStatus" class="status">Loading...</div>

    <div style="margin-top:10px; display:flex; gap:10px; align-items:center; flex-wrap:wrap;">
      <button class="scan" type="button" onclick="sdUp()">Up</button>
      <button class="scan" type="button" onclick="sdOpenSelected()">Open</button>
      <button class="scan" type="button" onclick="sdRefresh()">Refresh</button>
      <button class="warn" type="button" onclick="sdEject()">Safe Eject</button>
    </div>

    <div style="margin-top:10px; text-align:left;">
      <div style="font-size:0.9em; color:#aaa;">Path: <span id="sdPath">/</span></div>
      <select id="sdList" size="10" style="width:100%; margin-top:6px; background:#1e1e1e; color:#eee; border:1px solid #555; border-radius:6px; padding:6px;"></select>
    </div>

    <div style="margin-top:10px; display:flex; gap:10px; flex-wrap:wrap; align-items:center;">
      <button class="danger" type="button" onclick="sdDelete()">Delete</button>
      <button class="scan" type="button" onclick="sdRename()">Rename</button>
      <button class="scan" type="button" onclick="sdDownload()">Download</button>

      <input type="file" id="sdUploadFile" style="display:none" />
      <button class="scan" type="button" onclick="document.getElementById('sdUploadFile').click()">Upload</button>
    </div>

    <hr style="border-color:#555; margin:12px 0;">

    <h4 style="margin:0 0 6px 0;">Convert .hex on SD into Firmware Slot</h4>
    <div style="display:flex; gap:10px; flex-wrap:wrap; align-items:center;">
      <select id="sdFwSlot" style="flex:1; min-width:180px;">
        <option value="Keypad">Keypad</option>
        <option value="Front_Panel">Front Panel</option>
        <option value="Motor_Control">Motor Control</option>
        <option value="TM1637">TM1637 Display</option>
        <option value="Ultrasonic">Ultrasonic</option>
        <option value="Comms">Communications</option>
        <option value="Custom1">Custom Slot 1</option>
        <option value="Custom2">Custom Slot 2</option>
      </select>
      <input type="text" id="sdFwVer" placeholder="Version" style="flex:1; min-width:120px;" />
      <button class="scan" type="button" onclick="sdConvertToFw()">Convert to FW</button>
    </div>
    <div id="sdFwMsg" class="status" style="margin-top:6px; word-break:break-word;"></div>
  </div>

  <script>
    let sdCurPath = '/';

    async function sdFetchJson(url, opts) {
      const r = await fetch(url, opts || {});
      if (!r.ok) throw new Error(await r.text());
      return await r.json();
    }

    async function sdStatus() {
      try {
        const j = await sdFetchJson('/api/sd/status');
        const el = document.getElementById('sdStatus');
        if (el) el.textContent = (j.mounted ? 'Mounted' : 'Not mounted') + (j.msg ? (' - ' + j.msg) : '');
      } catch (e) {
        const el = document.getElementById('sdStatus');
        if (el) el.textContent = 'SD not available';
      }
    }

    async function sdRefresh() {
      await sdStatus();
      const pathEl = document.getElementById('sdPath');
      if (pathEl) pathEl.textContent = sdCurPath;

      const sel = document.getElementById('sdList');
      if (!sel) return;
      sel.innerHTML = '';

      try {
        const j = await sdFetchJson('/api/sd/list?path=' + encodeURIComponent(sdCurPath));
        const entries = j.entries || [];
        // Dirs first
        entries.sort((a,b)=> (b.isDir - a.isDir) || a.name.localeCompare(b.name));
        entries.forEach(e => {
          const opt = document.createElement('option');
          opt.value = e.name;
          opt.textContent = (e.isDir ? '[DIR] ' : '      ') + e.name + (!e.isDir ? ('  ('+e.size+' bytes)') : '');
          opt.dataset.isdir = e.isDir ? '1' : '0';
          sel.appendChild(opt);
        });
        if (!entries.length) {
          const opt = document.createElement('option');
          opt.value=''; opt.textContent='(empty)';
          sel.appendChild(opt);
        }
      } catch (e) {
        const opt = document.createElement('option');
        opt.value=''; opt.textContent='(SD error)';
        sel.appendChild(opt);
      }
    }

    function sdJoin(path, name) {
      if (!path.endsWith('/')) path += '/';
      return path + name;
    }

    function sdGetSelected() {
      const sel = document.getElementById('sdList');
      if (!sel || sel.selectedIndex < 0) return null;
      const opt = sel.options[sel.selectedIndex];
      if (!opt || !opt.value) return null;
      return { name: opt.value, isDir: opt.dataset.isdir === '1' };
    }

    async function sdOpenSelected() {
      const it = sdGetSelected();
      if (!it || !it.isDir) return;
      sdCurPath = sdJoin(sdCurPath, it.name);
      await sdRefresh();
    }

    async function sdUp() {
      if (sdCurPath === '/' ) return;
      let p = sdCurPath;
      if (p.endsWith('/')) p = p.slice(0, -1);
      const i = p.lastIndexOf('/');
      sdCurPath = (i <= 0) ? '/' : p.substring(0, i);
      await sdRefresh();
    }

    async function sdDelete() {
      const it = sdGetSelected();
      if (!it) return;
      if (!confirm('Delete ' + it.name + (it.isDir ? ' (dir)?' : '?'))) return;
      try {
        await sdFetchJson('/api/sd/delete', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ path: sdJoin(sdCurPath, it.name) }) });
        await sdRefresh();
      } catch (e) {
        alert('Delete failed: ' + e.message);
      }
    }

    async function sdRename() {
      const it = sdGetSelected();
      if (!it) return;
      const newName = prompt('Rename to:', it.name);
      if (!newName) return;
      try {
        await sdFetchJson('/api/sd/rename', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ from: sdJoin(sdCurPath, it.name), to: sdJoin(sdCurPath, newName) }) });
        await sdRefresh();
      } catch (e) {
        alert('Rename failed: ' + e.message);
      }
    }

    function sdDownload() {
      const it = sdGetSelected();
      if (!it || it.isDir) return;
      const p = sdJoin(sdCurPath, it.name);
      window.open('/sd/download?path=' + encodeURIComponent(p), '_blank');
    }

    async function sdEject() {
      if (!confirm('Unmount SD card?')) return;
      try {
        await sdFetchJson('/api/sd/eject', {method:'POST'});
      } catch (e) {
        // ignore
      }
      await sdRefresh();
    }

    async function sdConvertToFw() {
      const msg = document.getElementById('sdFwMsg');
      if (msg) msg.textContent = '';
      const it = sdGetSelected();
      if (!it || it.isDir) { if (msg) msg.textContent='Select a .hex file'; return; }
      const full = sdJoin(sdCurPath, it.name);
      if (!full.toLowerCase().endsWith('.hex')) { if (msg) msg.textContent='Selected file is not .hex'; return; }
      const slot = document.getElementById('sdFwSlot') ? document.getElementById('sdFwSlot').value : '';
      const ver  = document.getElementById('sdFwVer') ? document.getElementById('sdFwVer').value : '';
      if (!slot || !ver) { if (msg) msg.textContent='Pick slot and version'; return; }

      try {
        const r = await fetch('/api/sd/convert_fw', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ path: full, prefix: slot, ver: ver }) });
        const t = await r.text();
        if (msg) msg.textContent = r.ok ? t : ('Error: ' + t);
      } catch (e) {
        if (msg) msg.textContent = 'Error: ' + e.message;
      }
    }

    // Upload
    (function(){
      const f = document.getElementById('sdUploadFile');
      if (!f) return;
      f.addEventListener('change', async function(){
        if (!f.files || !f.files.length) return;
        const fd = new FormData();
        fd.append('file', f.files[0]);
        try {
          const r = await fetch('/api/sd/upload?dir=' + encodeURIComponent(sdCurPath), { method:'POST', body: fd });
          const t = await r.text();
          alert(r.ok ? t : ('Upload failed: ' + t));
        } catch (e) {
          alert('Upload error: ' + e.message);
        }
        f.value = '';
        await sdRefresh();
      });
    })();

    // Initial load
    (function(){
      sdRefresh();
    })();
  </script>
)rawliteral";
#else
const char SD_FW_OPTION_HTML[] PROGMEM = "";
const char SD_FW_AREA_HTML[] PROGMEM = "";
const char SD_TILE_HTML[] PROGMEM = "";
#endif

const char SCANNER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>I2C Scanner</title>
  <style>
    body { font-family: monospace; background: #000; color: #0f0; padding: 20px; text-align:center; }
    .box { border: 2px solid #0f0; padding: 20px; max-width: 400px; margin: 0 auto; min-height: 150px;}
    button { background: #333; color: white; border: 1px solid white; padding: 10px; width: 100%; margin-top:20px;}
  </style>
  <script>
    setInterval(function() {
      fetch('/scan-data').then(res => res.text()).then(data => {
        document.getElementById("res").innerHTML = data;
      });
    }, 1500); 
  </script>
</head>
<body>
  <h2>I2C SCANNER</h2>
  <div class="box">
    <p>Scanning Bus...</p>
    <div id="res">Waiting...</div>
  </div>
  <button onclick="location.href='/'">RETURN TO DASHBOARD</button>
</body>
</html>
)rawliteral";

// ===================================================================================
// --- HTML: Configurator (secondary tab) ---
// ===================================================================================
const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SW8B Configurator</title>
<style>
  body { font-family: 'Segoe UI', sans-serif; background: #222; color: #eee; text-align: center; margin: 0; padding: 10px; }
  .nav { background: #333; padding: 10px; margin-bottom: 20px; border-bottom: 1px solid #444; }
  .nav a { color: #00d2ff; text-decoration: none; margin: 0 10px; font-weight: bold; }
  .card { background: #333; padding: 15px; margin: 10px auto; max-width: 600px; border-radius: 8px; border: 1px solid #444; text-align: left; }
  h2, h3 { color: #00d2ff; margin-top: 0; }
  .btn { padding: 8px 15px; border: none; border-radius: 4px; cursor: pointer; color: white; margin: 2px; }
  .btn-green { background: #28a745; } .btn-blue { background: #007acc; } .btn-red { background: #c0392b; }
  input, select { padding: 8px; background: #ddd; border: none; border-radius: 4px; width: 100%; box-sizing: border-box; margin-bottom: 5px;}
  .row { display: flex; gap: 10px; align-items: center; margin-bottom: 5px; padding: 5px; background: #2a2a2a; border-radius: 4px;}
  .col { flex: 1; }
  .pin-dot { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 5px; background: #555; }
  .pin-used { background: #c0392b; } .pin-free { background: #28a745; }
  label { font-size: 0.8em; color: #aaa; display: block;}
  .error-banner { background: #c0392b; color: white; padding: 10px; margin: 10px 0; border-radius: 4px; display:none; }
</style>
<script>
let config = { device_mode: [], pin_mode: {} };
let variant = "Unknown";
let caps = [];
const DEV_REQS = {"MOTOR_SIMPLE_HBRIDGE": 6, "SERVO_RAMPED": 3, "QUAD_ENC": 5, "ULTRASONIC": 27, "TM1637": 11, "PWM_DIMMER": 16};

function init() {
  fetch('/api/variant').then(r=>r.json()).then(d => {
    variant = d.variant; caps = d.capabilities || [];
    document.getElementById('variantLabel').innerText = variant;
    filterDropdown(); render();
  });
  loadList();
}

function filterDropdown() {
  const sel = document.getElementById('newDevType');
  for (let i = 0; i < sel.options.length; i++) {
    let opt = sel.options[i]; let req = DEV_REQS[opt.value];
    if (req && !caps.includes(req)) { opt.disabled = true; if (!opt.text.includes("(Not supported)")) opt.text += " (Not supported)"; }
    else { opt.disabled = false; opt.text = opt.text.replace(" (Not supported)", ""); }
  }
}

function getUsedPins() {
  let u = new Set();
  config.device_mode.forEach(d => Object.values(d.pins).forEach(p => u.add(parseInt(p))));
  Object.keys(config.pin_mode).forEach(p => u.add(parseInt(p)));
  return u;
}

function getNextFreePin(excludeSet) { for(let i=0; i<8; i++) if(!excludeSet.has(i)) return i; return -1; }

function render() {
  const devList = document.getElementById('devList'); devList.innerHTML = '';
  let usedPins = new Set();
  config.device_mode.forEach(d => Object.values(d.pins).forEach(p => usedPins.add(parseInt(p))));

  config.device_mode.forEach((dev, idx) => {
    let pinInputs = "";
    Object.keys(dev.pins).forEach(k => {
      pinInputs += `<label>${k}: <select style="width:50px" onchange="updDevPin(${idx}, '${k}', this.value)">`;
      for(let i=0; i<8; i++) pinInputs += `<option value="${i}" ${dev.pins[k]==i?'selected':''}>${i}</option>`;
      pinInputs += `</select></label> `;
    });
    devList.innerHTML += `<div class="row" style="border-left: 3px solid #00d2ff;"><div class="col"><b>${dev.type}</b><br><small>${dev.id}</small></div><div class="col">${pinInputs}</div><button class="btn btn-red" onclick="remDev(${idx})">X</button></div>`;
  });

  const pinTable = document.getElementById('pinTable'); pinTable.innerHTML = '';
  for(let i=0; i<8; i++) {
    if(usedPins.has(i)) continue;
    let pConf = config.pin_mode[i] || { mode: "DIGITAL_IN" };
    let opts = `<option value="DIGITAL_IN" ${pConf.mode=='DIGITAL_IN'?'selected':''}>Digital Input</option><option value="DIGITAL_OUT" ${pConf.mode=='DIGITAL_OUT'?'selected':''}>Digital Output</option><option value="INPUT_PULLUP" ${pConf.mode=='INPUT_PULLUP'?'selected':''}>Input Pullup</option>`;
    if(caps.includes(2)) opts += `<option value="ANALOG_IN" ${pConf.mode=='ANALOG_IN'?'selected':''}>Analog Input</option>`;
    if(caps.includes(3)) opts += `<option value="SERVO" ${pConf.mode=='SERVO'?'selected':''}>Servo</option>`;
    if(caps.includes(16)) opts += `<option value="PWM" ${pConf.mode=='PWM'?'selected':''}>PWM</option>`;
    pinTable.innerHTML += `<div class="row"><div class="pin-dot pin-free"></div> WP${i}<div class="col"><select onchange="updPin(${i}, 'mode', this.value)">${opts}</select></div></div>`;
  }
}

function addDevice() {
  const type = document.getElementById('newDevType').value;
  const id = document.getElementById('newDevId').value || ("Dev"+Math.floor(Math.random()*1000));
  let used = getUsedPins(); let p1 = getNextFreePin(used); used.add(p1); let p2 = getNextFreePin(used);
  let newDev = { type: type, id: id, pins: {}, settings: {} };
  if(type.includes("MOTOR_SIMPLE")) newDev.pins = { pwm: (p1>-1?p1:0), dir: (p2>-1?p2:1) };
  else if(type.includes("ULTRASONIC")) newDev.pins = { trig: (p1>-1?p1:0), echo: (p2>-1?p2:1) };
  else if(type.includes("QUAD_ENC")) newDev.pins = { A: (p1>-1?p1:0), B: (p2>-1?p2:1) };
  else if(type.includes("TM1637")) newDev.pins = { clk: (p1>-1?p1:0), dio: (p2>-1?p2:1) };
  else newDev.pins = { pin: (p1>-1?p1:0) };
  config.device_mode.push(newDev); render();
}

function updDevPin(idx, k, v) { config.device_mode[idx].pins[k] = parseInt(v); render(); }
function remDev(idx) { config.device_mode.splice(idx, 1); render(); }
function updPin(p, k, v) { if(!config.pin_mode[p]) config.pin_mode[p] = {}; config.pin_mode[p][k] = v; if(v === "DIGITAL_IN") delete config.pin_mode[p]; }

function validate() {
  let usage = {}; let errors = [];
  config.device_mode.forEach(d => Object.entries(d.pins).forEach(([role, p]) => {
    let pin = parseInt(p); if(pin < 0 || pin > 7) errors.push(`${d.id}: Pin ${pin} OOR`);
    if(usage[pin]) errors.push(`Conflict: ${pin}`); usage[pin] = d.id;
  }));
  const banner = document.getElementById('errBanner');
  if(errors.length > 0) { banner.style.display = 'block'; banner.innerHTML = errors.join("<br>"); return false; }
  banner.style.display = 'none'; return true;
}

function applyConfig() {
  if(!validate()) return;
  config.meta = { variant: variant, timestamp: Date.now() };
  config.schema_version = 1;
  fetch('/api/apply', { method: 'POST', body: JSON.stringify(config) }).then(r => r.text()).then(alert);
}

function saveConfig() {
  if(!validate()) return;
  let name = prompt("Name:"); if(!name) return;
  fetch('/api/config/exists?name='+encodeURIComponent(name)).then(r => r.json()).then(d => {
    if(d.exists && !confirm("Overwrite?")) return;
    fetch('/api/config/save?name='+encodeURIComponent(name), { method: 'POST', body: JSON.stringify(config) }).then(() => loadList());
  });
}

function loadList() {
  fetch('/api/config/list').then(r=>r.json()).then(lst => {
    const sel = document.getElementById('loadSel');
    sel.innerHTML = '<option value="">-- Select --</option>';
    lst.forEach(f => sel.innerHTML += `<option value="${f}">${f}</option>`);
  });
}

function loadConfig() {
  const name = document.getElementById('loadSel').value; if(!name) return;
  fetch('/api/config/load?name='+encodeURIComponent(name)).then(r=>r.json()).then(c => { config = c; render(); });
}
function deleteConfig() {
  const name = document.getElementById('loadSel').value; if(!name) return;
  if(confirm("Del?")) fetch('/api/config/delete?name='+encodeURIComponent(name)).then(() => loadList());
}
</script>
</head><body onload="init()">
<div class="nav"><a href="/">Dashboard</a><a href="/configure" style="color:white; border-bottom:2px solid white;">Configure</a><a href="/settings">System Settings</a></div>
<h2>System Configurator</h2>
<div class="card"><small>Variant: <span id="variantLabel">...</span></small><div id="errBanner" class="error-banner"></div></div>
<div class="card"><h3>1. Device Mode</h3><div id="devList"></div><hr>
<div class="row"><div class="col"><select id="newDevType"><option value="MOTOR_SIMPLE_HBRIDGE">Motor</option><option value="SERVO">Servo</option><option value="QUAD_ENC">Encoder</option><option value="ULTRASONIC">Ultrasonic</option><option value="TM1637">TM1637</option><option value="PWM_DIMMER">PWM</option></select></div>
<div class="col"><input type="text" id="newDevId" placeholder="ID"></div><button class="btn btn-blue" onclick="addDevice()">+</button></div></div>
<div class="card"><h3>2. Pin Mode</h3><div id="pinTable"></div></div>
<div class="card"><h3>Files</h3><div class="row"><div class="col"><select id="loadSel"></select></div><button class="btn btn-green" onclick="loadConfig()">L</button><button class="btn btn-red" onclick="deleteConfig()">D</button></div>
<button class="btn btn-blue" onclick="saveConfig()">Save As</button></div>
<br><button class="btn btn-green" onclick="applyConfig()">APPLY CONFIG</button>
</body></html>
)rawliteral";


const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>System Settings</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { ... }
  </style>
</head>
<body>
  <div style='background:#333;padding:10px;margin:0 0 10px 0;border-bottom:1px solid #444;'>
    <a href='/' style='color:#00d2ff;font-weight:bold;margin:0 10px;text-decoration:none;'>Dashboard</a>
    <a href='/configure' style='color:#00d2ff;font-weight:bold;margin:0 10px;text-decoration:none;'>Configurator</a>
    <a href='/settings' style='color:white;font-weight:bold;margin:0 10px;text-decoration:none;border-bottom:2px solid white;'>System Settings</a>
  </div>

  <div style='max-width:900px;margin:0 auto;padding:0 10px;'>
    <div style='background:#222;border:1px solid #444;border-radius:10px;padding:15px;margin-bottom:15px;'>
      <h3 style='margin:0 0 10px 0;'>ESP32 Diagnostics</h3>
      <div id='diagBox' style='font-family:monospace;white-space:pre-wrap;color:#ddd;'>Loading...</div>
    </div>

    <div style='background:#222;border:1px solid #444;border-radius:10px;padding:15px;margin-bottom:15px;'>
      <h3 style='margin:0 0 10px 0;'>Memory & Storage</h3>
      <div id='memBox' style='font-family:monospace;white-space:pre-wrap;color:#ddd;'>Loading...</div>
    </div>
  </div>

  <script>
    function fmtBytes(n) {
      if (n === null || n === undefined) return 'n/a';
      const u = ['B','KB','MB','GB','TB'];
      let i = 0; let v = Number(n);
      while (v >= 1024 && i < u.length-1) { v /= 1024; i++; }
      return (i >= 2 ? v.toFixed(2) : v.toFixed(0)) + ' ' + u[i];
    }

    async function refreshSystem() {
      try {
        const r = await fetch('/api/system');
        if (!r.ok) throw new Error(await r.text());
        const j = await r.json();

        const diag = [
          'CPU Frequency : ' + j.cpu_mhz + ' MHz',
          'Flash Speed   : ' + (j.flash_speed_hz ? (j.flash_speed_hz/1000000).toFixed(0) + ' MHz' : 'n/a'),
          'SDK Version   : ' + (j.sdk || 'n/a'),
          'Chip Revision : ' + (j.chip_rev ?? 'n/a'),
          'MAC Address   : ' + (j.mac || 'n/a'),
        ].join('\n');
        document.getElementById('diagBox').textContent = diag;

        const mem = [
          'Heap Total    : ' + fmtBytes(j.heap_total),
          'Heap Free     : ' + fmtBytes(j.heap_free),
          'Min Free Heap : ' + fmtBytes(j.heap_min_free),
          '',
          'LittleFS Total: ' + fmtBytes(j.fs_total),
          'LittleFS Used : ' + fmtBytes(j.fs_used),
          'LittleFS Free : ' + fmtBytes(j.fs_free),
          '',
          'SD Status     : ' + (j.sd_enabled ? (j.sd_mounted ? 'Mounted' : 'Not mounted') : 'Disabled'),
          'SD Total      : ' + (j.sd_enabled ? fmtBytes(j.sd_total) : 'n/a'),
          'SD Used       : ' + (j.sd_enabled ? fmtBytes(j.sd_used) : 'n/a'),
          'SD Free       : ' + (j.sd_enabled ? fmtBytes(j.sd_free) : 'n/a'),
        ].join('\n');
        document.getElementById('memBox').textContent = mem;
      } catch (e) {
        document.getElementById('diagBox').textContent = 'Error: ' + e;
        document.getElementById('memBox').textContent = 'Error: ' + e;
      }
    }

    refreshSystem();
    setInterval(refreshSystem, 2000);
  </script>
</body></html>
)rawliteral";
