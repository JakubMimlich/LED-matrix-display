#pragma once

// Separate, self-contained web interface stored in flash.
// No external CSS or JavaScript libraries are required.
static const char WEB_UI_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>LED Matrix Control</title>
  <style>
    :root{color-scheme:dark;--bg:#08101d;--panel:#121d30;--panel2:#0b1525;--line:#2a3b58;--text:#f3f7ff;--muted:#9eabc3;--accent:#67e8f9;--good:#86efac;--bad:#fda4af;--warn:#fde68a}
    *{box-sizing:border-box}body{margin:0;background:radial-gradient(circle at top,#1a2b4b 0,#08101d 50%);font:16px/1.45 system-ui,-apple-system,Segoe UI,sans-serif;color:var(--text)}
    main{width:min(1040px,calc(100% - 24px));margin:22px auto 48px}.hero{display:flex;gap:16px;align-items:flex-start;justify-content:space-between;margin-bottom:14px}.hero h1{font-size:clamp(1.55rem,5vw,2.25rem);margin:0}.hero p{margin:5px 0 0;color:var(--muted)}
    .status{display:inline-flex;align-items:center;gap:8px;border:1px solid var(--line);border-radius:999px;padding:8px 12px;background:#0b1526;white-space:nowrap}.dot{width:9px;height:9px;border-radius:50%;background:var(--bad)}.dot.ok{background:var(--good);box-shadow:0 0 12px var(--good)}
    nav{display:flex;gap:8px;overflow:auto;padding:5px 0 14px;scrollbar-width:thin}.nav-button{width:auto;min-width:max-content;margin:0;padding:9px 14px;border-radius:999px;background:#142139;color:var(--muted);border:1px solid var(--line);font-weight:700}.nav-button.active{background:var(--accent);color:#05212a;border-color:var(--accent)}
    .page{display:none}.page.active{display:block}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.card{background:linear-gradient(180deg,#15223a,#0e182a);border:1px solid var(--line);border-radius:18px;padding:20px}.wide{grid-column:1/-1}h2{font-size:1.08rem;margin:0 0 15px}h3{font-size:.98rem;margin:0 0 10px}.hint,.small{color:var(--muted);font-size:.88rem}.small{text-align:center;margin-top:18px}
    label{display:block;color:var(--muted);font-size:.9rem;margin:12px 0 6px}input,select,button{width:100%;font:inherit;border-radius:11px;border:1px solid var(--line)}input,select{padding:11px 12px;background:#081323;color:var(--text);outline:none}input:focus,select:focus{border-color:var(--accent);box-shadow:0 0 0 3px #67e8f922}button{margin-top:12px;padding:11px 14px;background:var(--accent);color:#05212a;font-weight:760;cursor:pointer;touch-action:manipulation}button:hover{filter:brightness(1.08)}button:disabled{opacity:.55;cursor:wait}.secondary{background:#273a59;color:var(--text)}.danger{background:#7f1d2d;color:#fff}
    .row{display:grid;grid-template-columns:1fr 1fr;gap:12px}.actions{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.actions.three{grid-template-columns:repeat(3,minmax(0,1fr))}.actions button{margin-top:10px}.check{display:flex;align-items:center;gap:9px;color:var(--text)}.check input{width:auto}.toast{min-height:24px;margin-top:10px;color:var(--good)}.toast.error{color:var(--bad)}
    .overview{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px}.metric{border:1px solid var(--line);border-radius:14px;background:#0a1424;padding:13px}.metric span{display:block;color:var(--muted);font-size:.78rem}.metric strong{display:block;margin-top:4px;font-size:1.02rem;overflow-wrap:anywhere}.mode-note{margin:12px 0 0;color:var(--accent)}
    .preview{min-height:62px;display:flex;align-items:center;padding:12px 14px;border:1px dashed #526581;border-radius:12px;background:#050b14;font:700 1.25rem/1 ui-monospace,SFMono-Regular,Consolas,monospace;overflow:hidden;white-space:nowrap}.protocol{font:12px/1.5 ui-monospace,SFMono-Regular,Consolas,monospace;background:#07101d;border:1px solid var(--line);border-radius:10px;padding:10px;overflow-wrap:anywhere}
    .clock-face{font:700 clamp(2rem,8vw,4rem)/1 ui-monospace,SFMono-Regular,Consolas,monospace;letter-spacing:.04em;text-align:center;color:var(--accent);padding:18px 8px;border:1px solid var(--line);border-radius:14px;background:#060d18;font-variant-numeric:tabular-nums}.time-state{text-align:center;color:var(--muted);margin:9px 0 0}.field-with-button{display:grid;grid-template-columns:1fr auto;gap:8px;align-items:end}.field-with-button button{width:auto;margin-bottom:0}.timer-inputs{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:9px}.chrono-actions{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}.chrono-actions button{margin-top:9px}.timer-bar-preview{height:26px;border:1px solid var(--line);border-radius:10px;padding:3px;background:#020712;margin:10px 0}.timer-bar-fill{height:100%;width:100%;border-radius:6px;background:var(--accent);transition:width .2s linear}.scoreboard-face{font:700 clamp(2rem,9vw,4.5rem)/1 ui-monospace,SFMono-Regular,Consolas,monospace;letter-spacing:.06em;text-align:center;color:var(--accent);padding:18px 8px;border:1px solid var(--line);border-radius:14px;background:#060d18;font-variant-numeric:tabular-nums}.score-buttons{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}.score-side{display:grid;grid-template-columns:1fr 1fr;gap:7px}.animation-demo{height:84px;border:1px solid var(--line);border-radius:14px;background:#020712;position:relative;overflow:hidden;margin-bottom:12px}.dolphin-symbol{position:absolute;font-size:2rem;animation:swim 2.2s ease-in-out infinite}.dolphin-symbol.one{left:18%;top:16px}.dolphin-symbol.two{right:18%;top:34px;transform:scaleX(-1);animation-delay:-1.1s}@keyframes swim{0%,100%{translate:0 0}50%{translate:10px -8px}}
    .game-list{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.game-card{border:1px solid var(--line);border-radius:17px;background:#0b1525;padding:17px}.game-card.active-game{border-color:var(--accent);box-shadow:0 0 0 2px #67e8f922}.game-screen{position:relative;aspect-ratio:8/1;border:2px solid #526581;border-radius:10px;background:#020712;overflow:hidden;image-rendering:pixelated;margin:10px 0 14px}.stars{position:absolute;inset:0;background-image:radial-gradient(circle,#64748b 1px,transparent 1px);background-size:18px 13px;opacity:.35}.ship-icon{position:absolute;left:6%;top:50%;transform:translateY(-50%);font-size:clamp(17px,4vw,28px);color:var(--accent)}.rock{position:absolute;right:8%;top:19%;font-size:clamp(15px,3.5vw,25px);color:var(--warn)}
    .trex-ground{position:absolute;left:0;right:0;bottom:8%;height:3px;background:#64748b}.trex-player{position:absolute;left:8%;bottom:calc(8% + 6px);width:8px;height:8px;border-radius:50%;background:var(--accent)}.trex-enemy{position:absolute;right:12%;bottom:calc(8% + 5px);display:flex;flex-direction:column;gap:2px}.trex-enemy span{display:block;width:7px;height:7px;border-radius:50%;background:var(--warn)}
    .gravity-route{position:absolute;left:0;right:0;height:3px;background:#64748b}.gravity-route.top{top:7%}.gravity-route.bottom{bottom:7%}.gravity-guy{position:absolute;left:9%;bottom:calc(7% + 7px);width:8px;height:8px;border-radius:50%;background:var(--accent)}.gravity-pillar{position:absolute;width:10px;background:var(--warn)}.gravity-pillar.bottom{right:13%;bottom:calc(7% + 3px);height:24px}.gravity-pillar.top{right:38%;top:calc(7% + 3px);height:17px}.gravity-platform{position:absolute;right:52%;top:34%;width:55px;height:7px;background:var(--warn)}.gravity-stairs{position:absolute;right:25%;bottom:calc(7% + 3px);display:flex;align-items:flex-end;gap:2px}.gravity-stairs span{display:block;width:7px;background:var(--warn)}.gravity-stairs span:nth-child(1){height:7px}.gravity-stairs span:nth-child(2){height:14px}.gravity-stairs span:nth-child(3){height:21px}.gravity-stairs span:nth-child(4){height:28px}
    .tetris-grid{position:absolute;inset:0;background:repeating-linear-gradient(90deg,transparent 0,transparent calc(1.5625% - 1px),#24344d calc(1.5625% - 1px),#24344d 1.5625%)}.tetris-block{position:absolute;width:1.5625%;height:12.5%;background:var(--accent);border:1px solid #07111f}.tetris-block.b1{left:8%;bottom:0}.tetris-block.b2{left:9.5625%;bottom:0}.tetris-block.b3{left:9.5625%;bottom:12.5%}.tetris-block.b4{left:11.125%;bottom:0}.tetris-active{position:absolute;right:10%;top:25%;display:grid;grid-template-columns:repeat(2,10px);grid-template-rows:repeat(2,10px)}.tetris-active span{background:var(--warn);border:1px solid #07111f}
    .game-stats{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}.stat{border:1px solid var(--line);border-radius:12px;background:#081222;padding:9px;text-align:center}.stat strong{display:block;font-size:1.08rem}.stat span{color:var(--muted);font-size:.75rem}.range-line{display:flex;align-items:center;gap:10px}.range-line input{padding:0}.range-value{min-width:64px;text-align:right;color:var(--accent);font-variant-numeric:tabular-nums}.game-control-row{display:grid;grid-template-columns:1fr 1fr;gap:10px}.jump-button{font-size:1.08rem}.game-help{min-height:40px}
    .paint-wrap{border:1px solid var(--line);border-radius:14px;background:#020712;padding:10px;overflow:hidden}.paint-canvas{display:block;width:100%;aspect-ratio:8/1;background:#020712;image-rendering:pixelated;touch-action:none;cursor:crosshair}.paint-toolbar{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:9px}.paint-tool.active{background:var(--accent);color:#05212a}.paint-control-grid{display:grid;grid-template-columns:minmax(180px,.7fr) 1.3fr;gap:16px;align-items:start;margin-top:14px}.paint-dpad{display:grid;grid-template-columns:repeat(3,1fr);gap:7px;max-width:260px;margin:auto}.paint-dpad button{margin-top:0;min-height:48px}.paint-dpad .up{grid-column:2}.paint-dpad .left{grid-column:1;grid-row:2}.paint-dpad .center{grid-column:2;grid-row:2}.paint-dpad .right{grid-column:3;grid-row:2}.paint-dpad .down{grid-column:2;grid-row:3}.paint-position{text-align:center;color:var(--muted);margin:9px 0 0;font-variant-numeric:tabular-nums}.paint-actions{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:9px}.paint-actions button{margin-top:9px}
    dl.meta{display:grid;grid-template-columns:minmax(115px,.75fr) 1.25fr;gap:8px 12px;margin:0}dt{color:var(--muted)}dd{margin:0;overflow-wrap:anywhere}
    @media(max-width:800px){.grid,.game-list,.paint-control-grid{grid-template-columns:1fr}.wide{grid-column:auto}.overview{grid-template-columns:repeat(2,minmax(0,1fr))}.hero{flex-direction:column}.status{align-self:flex-start}}
    @media(max-width:520px){.row,.actions.three,.game-control-row,.paint-toolbar,.paint-actions{grid-template-columns:1fr}.overview{grid-template-columns:1fr}main{width:min(100% - 16px,1040px)}.card{padding:16px}}
  </style>
</head>
<body>
<main>
  <header class="hero">
    <div><h1>64×8 LED Matrix</h1><p>Display, text, paint, time, games, animations, Wi-Fi, and ESP-NOW control</p></div>
    <div class="status"><span id="dot" class="dot"></span><span id="connection">Connecting…</span></div>
  </header>

  <nav aria-label="Control pages">
    <button class="nav-button active" data-page="display" type="button">Display</button>
    <button class="nav-button" data-page="text" type="button">Text</button>
    <button class="nav-button" data-page="paint" type="button">Paint</button>
    <button class="nav-button" data-page="time" type="button">Time</button>
    <button class="nav-button" data-page="games" type="button">Games</button>
    <button class="nav-button" data-page="animation" type="button">Animation</button>
    <button class="nav-button" data-page="settings" type="button">Settings</button>
  </nav>

  <section id="page-display" class="page active">
    <div class="grid">
      <article class="card wide">
        <h2>Current display</h2>
        <div id="currentDisplay" class="preview">Loading…</div>
        <p id="activeMode" class="mode-note">Display mode: loading</p>
      </article>
      <article class="card wide">
        <h2>Controller overview</h2>
        <div class="overview">
          <div class="metric"><span>Mode</span><strong id="overviewMode">—</strong></div>
          <div class="metric"><span>Message stack</span><strong><span id="queueDepth">0</span> item(s)</strong></div>
          <div class="metric"><span>STA address</span><strong id="overviewStaIp">—</strong></div>
          <div class="metric"><span>Clock</span><strong id="overviewClock">--:--:--</strong></div>
        </div>
      </article>
      <article class="card"><h2>Choose a function</h2><div class="actions"><button class="page-link" data-page="text" type="button">Show text</button><button class="page-link" data-page="paint" type="button">Paint pixels</button><button class="page-link" data-page="time" type="button">Show time</button><button class="page-link" data-page="games" type="button">Open games</button><button class="page-link" data-page="animation" type="button">Animations</button><button class="page-link secondary" data-page="settings" type="button">Settings</button></div></article>
      <article class="card"><h2>Network summary</h2><dl class="meta"><dt>Home Wi-Fi</dt><dd id="summarySta">—</dd><dt>Setup AP</dt><dd id="summaryAp">—</dd><dt>Signal</dt><dd id="summaryRssi">—</dd></dl></article>
    </div>
  </section>

  <section id="page-text" class="page">
    <div class="grid">
      <article class="card wide">
        <h2>Custom text and message stack</h2>
        <label for="displayText">Text</label><input id="displayText" maxlength="127" autocomplete="off" placeholder="Type text for the matrix">
        <div id="preview" class="preview">Hello world</div>
        <div class="row">
          <div><label for="displayMode">Presentation</label><select id="displayMode"><option value="scroll">Running text</option><option value="static">Static text</option></select></div>
          <div><label for="speed">Scroll step (ms)</label><input id="speed" type="number" min="15" max="500" value="60"></div>
        </div>
        <label class="check"><input id="queueText" type="checkbox"> Add this message to the display stack</label>
        <div class="row">
          <div><label for="holdMs">Static hold (ms)</label><input id="holdMs" type="number" min="250" max="60000" value="3000"></div>
          <div><label for="repeatCount">Running passes</label><input id="repeatCount" type="number" min="1" max="10" value="1"></div>
        </div>
        <div class="actions"><button id="sendText" type="button">Show text now</button><button id="clearStack" class="danger" type="button">Clear stack</button></div>
        <div id="textMessage" class="toast"></div>
      </article>
    </div>
  </section>

  <section id="page-paint" class="page">
    <div class="grid">
      <article class="card wide">
        <h2>Paint pixels</h2>
        <p class="hint">Click or drag on the 64×8 canvas. Arrow keys move the cursor; Draw and Erase apply while the cursor moves.</p>
        <div class="paint-wrap"><canvas id="paintCanvas" class="paint-canvas" width="640" height="80" aria-label="64 by 8 pixel paint canvas"></canvas></div>
        <div class="paint-control-grid">
          <div>
            <h3>Cursor</h3>
            <div class="paint-dpad">
              <button class="paint-move up secondary" data-dx="0" data-dy="-1" type="button">▲</button>
              <button class="paint-move left secondary" data-dx="-1" data-dy="0" type="button">◀</button>
              <button id="paintToggle" class="center" type="button">Toggle</button>
              <button class="paint-move right secondary" data-dx="1" data-dy="0" type="button">▶</button>
              <button class="paint-move down secondary" data-dx="0" data-dy="1" type="button">▼</button>
            </div>
            <p id="paintPosition" class="paint-position">X 0 · Y 0</p>
          </div>
          <div>
            <h3>Drawing tool</h3>
            <div class="paint-toolbar"><button class="paint-tool active" data-tool="draw" type="button">Draw ON</button><button class="paint-tool secondary" data-tool="erase" type="button">Draw OFF</button><button class="paint-tool secondary" data-tool="move" type="button">Move only</button></div>
            <div class="paint-actions"><button id="paintPixelOn" type="button">Pixel ON</button><button id="paintPixelOff" class="secondary" type="button">Pixel OFF</button><button id="paintShow" class="secondary" type="button">Show paint</button><button id="paintClear" class="danger" type="button">Clear screen</button><button id="paintInvert" class="secondary" type="button">Inverse pixels</button><button id="paintFill" class="secondary" type="button">Fill screen</button></div>
            <div id="paintMessage" class="toast"></div>
            <p class="hint">Keyboard: arrows move, Space toggles, D draws, E erases, M moves only, C clears, and I inverses all pixels.</p>
          </div>
        </div>
      </article>
    </div>
  </section>

  <section id="page-time" class="page">
    <div class="grid">
      <article class="card">
        <h2>Current time</h2>
        <div id="clockFace" class="clock-face">--:--:--</div><p id="timeState" class="time-state">Checking time…</p>
        <button id="showTime" type="button">Show clock on matrix</button>
      </article>
      <article class="card">
        <h2>Calendar</h2>
        <div id="calendarFace" class="clock-face">--.--.--</div><p class="time-state">Local date · DD.MM.YY</p>
        <button id="showCalendar" type="button">Show calendar on matrix</button>
      </article>
      <article class="card">
        <h2>Clock settings</h2>
        <div class="field-with-button"><div><label for="timezoneOffset">UTC offset (minutes)</label><input id="timezoneOffset" type="number" min="-720" max="840" value="0"></div><button id="browserZone" class="secondary" type="button">Use browser</button></div>
        <label for="ntpServer">NTP server</label><input id="ntpServer" maxlength="63" value="pool.ntp.org" autocomplete="off">
        <div class="actions"><button id="saveTime" class="secondary" type="button">Save settings</button><button id="syncNtp" type="button">Sync with server</button></div>
        <p class="hint">While STA has internet access, the controller requests NTP synchronization automatically every 12 hours.</p>
      </article>
      <article class="card wide">
        <h2>Set controller time</h2>
        <div class="row"><div><label for="manualTime">Manual local date and time</label><input id="manualTime" type="datetime-local" step="1"></div><div><label>&nbsp;</label><button id="setManualTime" class="secondary" type="button">Set manual time</button></div></div>
        <button id="syncBrowser" type="button">Synchronize from this browser</button><div id="timeMessage" class="toast"></div>
      </article>
      <article class="card">
        <h2>Countdown timer</h2>
        <div id="timerFace" class="clock-face">00:05:00</div><p id="timerState" class="time-state">Ready</p>
        <div id="timerBarPreview" class="timer-bar-preview"><div id="timerBarFill" class="timer-bar-fill"></div></div>
        <label for="timerPresentation">Matrix presentation</label><select id="timerPresentation"><option value="time">Time · HH:MM:SS</option><option value="bar">Progress bar</option></select>
        <div class="timer-inputs">
          <div><label for="timerHours">Hours</label><input id="timerHours" type="number" min="0" max="99" value="0"></div>
          <div><label for="timerMinutes">Minutes</label><input id="timerMinutes" type="number" min="0" max="59" value="5"></div>
          <div><label for="timerSeconds">Seconds</label><input id="timerSeconds" type="number" min="0" max="59" value="0"></div>
        </div>
        <div class="chrono-actions"><button id="timerSet" class="secondary" type="button">Set & show</button><button id="timerStart" type="button">Start</button><button id="timerPause" class="secondary" type="button">Pause</button><button id="timerReset" class="danger" type="button">Reset</button></div>
        <div id="timerMessage" class="toast"></div><p class="hint">When the countdown reaches zero, the selected timer display blinks until the timer is reset or started again.</p>
      </article>
      <article class="card">
        <h2>Stopwatch</h2>
        <div id="stopwatchFace" class="clock-face">00:00:00</div><p id="stopwatchState" class="time-state">Ready</p>
        <button id="stopwatchShow" class="secondary" type="button">Show stopwatch on matrix</button>
        <div class="actions three"><button id="stopwatchStart" type="button">Start</button><button id="stopwatchPause" class="secondary" type="button">Pause</button><button id="stopwatchReset" class="danger" type="button">Reset</button></div>
        <div id="stopwatchMessage" class="toast"></div>
      </article>
    </div>
  </section>

  <section id="page-games" class="page">
    <div class="game-list">
      <article id="spaceCard" class="game-card">
        <h2>Space Runner</h2>
        <div class="game-screen" aria-label="Ship avoiding an asteroid"><div class="stars"></div><div class="ship-icon">▶</div><div class="rock">◆</div></div>
        <div class="game-stats"><div class="stat"><strong id="spaceState">Ready</strong><span>State</span></div><div class="stat"><strong id="spaceScore">0</strong><span>Score</span></div><div class="stat"><strong id="shipPosition">4</strong><span>Ship Y</span></div></div>
        <label for="spaceSpeed">Starting step: <span id="spaceSpeedValue">120</span> ms</label><div class="range-line"><input id="spaceSpeed" type="range" min="60" max="500" value="120"><span id="spaceRangeSpeed" class="range-value">120 ms</span></div>
        <div class="game-control-row"><button class="space-action secondary" data-action="up" type="button">▲ Up</button><button class="space-action secondary" data-action="down" type="button">▼ Down</button></div>
        <div class="actions three"><button class="space-action" data-action="start" type="button">Start</button><button id="spacePause" class="space-action secondary" data-action="pause" type="button">Pause</button><button class="space-action danger" data-action="reset" type="button">Reset</button></div>
        <div id="spaceMessage" class="toast game-help"></div><p class="hint">Keyboard: W/S or arrows. Space pauses.</p>
      </article>

      <article id="trexCard" class="game-card">
        <h2>T‑Rex Runner</h2>
        <div class="game-screen" aria-label="Single-pixel player jumping over variable-height dot obstacles"><div class="trex-ground"></div><div class="trex-player"></div><div class="trex-enemy"><span></span><span></span><span></span><span></span></div></div>
        <div class="game-stats"><div class="stat"><strong id="trexState">Ready</strong><span>State</span></div><div class="stat"><strong id="trexScore">0</strong><span>Score</span></div><div class="stat"><strong id="trexJumpState">Ground</strong><span>Player</span></div></div>
        <label for="trexSpeed">Starting step: <span id="trexSpeedValue">110</span> ms</label><div class="range-line"><input id="trexSpeed" type="range" min="60" max="500" value="110"><span id="trexRangeSpeed" class="range-value">110 ms</span></div>
        <button class="trex-action jump-button secondary" data-action="jump" type="button">▲ Jump</button>
        <div class="actions three"><button class="trex-action" data-action="start" type="button">Start</button><button id="trexPause" class="trex-action secondary" data-action="pause" type="button">Pause</button><button class="trex-action danger" data-action="reset" type="button">Reset</button></div>
        <div id="trexMessage" class="toast game-help"></div><p class="hint">W, Up, or Space jumps; P pauses.</p>
      </article>

      <article id="gravityCard" class="game-card">
        <h2>Gravity Guy</h2>
        <div class="game-screen" aria-label="One-pixel player flipping between ceiling and floor while avoiding walls, platforms, and stairs"><div class="gravity-route top"></div><div class="gravity-route bottom"></div><div class="gravity-guy"></div><div class="gravity-pillar top"></div><div class="gravity-platform"></div><div class="gravity-stairs"><span></span><span></span><span></span><span></span></div></div>
        <div class="game-stats"><div class="stat"><strong id="gravityState">Ready</strong><span>State</span></div><div class="stat"><strong id="gravityScore">0</strong><span>Score</span></div><div class="stat"><strong id="gravitySurface">Bottom</strong><span>Gravity</span></div></div>
        <label for="gravitySpeed">Starting step: <span id="gravitySpeedValue">90</span> ms</label><div class="range-line"><input id="gravitySpeed" type="range" min="45" max="350" value="90"><span id="gravityRangeSpeed" class="range-value">90 ms</span></div>
        <button class="gravity-action jump-button secondary" data-action="flip" type="button">↕ Flip gravity</button>
        <div class="actions three"><button class="gravity-action" data-action="start" type="button">Start</button><button id="gravityPause" class="gravity-action secondary" data-action="pause" type="button">Pause</button><button class="gravity-action danger" data-action="reset" type="button">Reset</button></div>
        <div id="gravityMessage" class="toast game-help"></div><p class="hint">The player is one LED. Every wall, stair, and platform pixel is fatal. Flip before contact. Space, F, W, Up, or Down flips; P pauses.</p>
      </article>

      <article id="racingCard" class="game-card">
        <h2>Pixel Racing</h2>
        <div class="game-screen" aria-label="Pixel car avoiding traffic on a road"><div class="gravity-route top"></div><div class="gravity-route bottom"></div><div class="ship-icon" style="left:12%;top:47%">▰</div><div class="rock" style="right:24%;top:30%">▮</div></div>
        <div class="game-stats"><div class="stat"><strong id="racingState">Ready</strong><span>State</span></div><div class="stat"><strong id="racingScore">0</strong><span>Score</span></div><div class="stat"><strong id="racingPosition">4</strong><span>Lane Y</span></div></div>
        <label for="racingSpeed">Starting step: <span id="racingSpeedValue">120</span> ms</label><div class="range-line"><input id="racingSpeed" type="range" min="60" max="500" value="120"><span id="racingRangeSpeed" class="range-value">120 ms</span></div>
        <div class="game-control-row"><button class="racing-action secondary" data-action="up" type="button">▲ Up</button><button class="racing-action secondary" data-action="down" type="button">▼ Down</button></div>
        <div class="actions three"><button class="racing-action" data-action="start" type="button">Start</button><button id="racingPause" class="racing-action secondary" data-action="pause" type="button">Pause</button><button class="racing-action danger" data-action="reset" type="button">Reset</button></div>
        <div id="racingMessage" class="toast game-help"></div><p class="hint">The road speeds up as traffic is passed. W/S or Up/Down changes lane; P pauses.</p>
      </article>

      <article id="scoreboardCard" class="game-card">
        <h2>Scoreboard</h2>
        <div id="scoreboardFace" class="scoreboard-face">00:00</div>
        <div class="score-buttons">
          <div><h3>Left team</h3><div class="score-side"><button class="score-action secondary" data-action="left_dec" type="button">−1</button><button class="score-action" data-action="left_inc" type="button">+1</button></div></div>
          <div><h3>Right team</h3><div class="score-side"><button class="score-action secondary" data-action="right_dec" type="button">−1</button><button class="score-action" data-action="right_inc" type="button">+1</button></div></div>
        </div>
        <div class="actions three"><button class="score-action" data-action="show" type="button">Show</button><button class="score-action secondary" data-action="swap" type="button">Swap</button><button class="score-action danger" data-action="reset" type="button">Reset</button></div>
        <div id="scoreboardMessage" class="toast game-help"></div><p class="hint">Scores are 0–99 and render as LEFT:RIGHT on the matrix.</p>
      </article>

      <article id="tetrisCard" class="game-card">
        <h2>Tetris</h2>
        <div class="game-screen" aria-label="Rotated Tetris board with pieces moving from right to left"><div class="tetris-grid"></div><div class="tetris-block b1"></div><div class="tetris-block b2"></div><div class="tetris-block b3"></div><div class="tetris-block b4"></div><div class="tetris-active"><span></span><span></span><span></span><span></span></div></div>
        <div class="game-stats"><div class="stat"><strong id="tetrisState">Ready</strong><span>State</span></div><div class="stat"><strong id="tetrisScore">0</strong><span>Score</span></div><div class="stat"><strong id="tetrisLines">0</strong><span>Lines</span></div></div>
        <label for="tetrisSpeed">Fall step: <span id="tetrisSpeedValue">450</span> ms</label><div class="range-line"><input id="tetrisSpeed" type="range" min="120" max="1200" value="450"><span id="tetrisRangeSpeed" class="range-value">450 ms</span></div>
        <div class="game-control-row"><button class="tetris-action secondary" data-action="up" type="button">▲ Up</button><button class="tetris-action secondary" data-action="down" type="button">▼ Down</button></div>
        <div class="game-control-row"><button class="tetris-action secondary" data-action="rotate" type="button">↻ Rotate</button><button class="tetris-action secondary" data-action="drop" type="button">◀ Drop</button></div>
        <div class="actions three"><button class="tetris-action" data-action="start" type="button">Start</button><button id="tetrisPause" class="tetris-action secondary" data-action="pause" type="button">Pause</button><button class="tetris-action danger" data-action="reset" type="button">Reset</button></div>
        <div id="tetrisMessage" class="toast game-help"></div><p class="hint">The native 64×8 board uses one LED per Tetris block, preserving the original four-cell tetromino proportions. Pieces fall right-to-left and complete vertical columns clear. Up/Down moves, R rotates, Space or Left drops, and P pauses.</p>
      </article>
    </div>
  </section>

  <section id="page-animation" class="page">
    <div class="grid">
      <article class="card wide">
        <h2>Dolphins animation</h2>
        <div class="animation-demo" aria-label="Dolphins animation preview"><span class="dolphin-symbol one">🐬</span><span class="dolphin-symbol two">🐬</span></div>
        <label for="animationSelect">Animation</label><select id="animationSelect"><option value="dolphins">Dolphins</option></select>
        <label for="animationSpeed">Frame interval: <span id="animationSpeedValue">220</span> ms</label><div class="range-line"><input id="animationSpeed" type="range" min="80" max="1000" value="220"><span id="animationRangeSpeed" class="range-value">220 ms</span></div>
        <div class="actions three"><button id="animationPlay" type="button">Play on matrix</button><button id="animationPause" class="secondary" type="button">Pause</button><button id="animationReset" class="danger" type="button">Restart</button></div>
        <div id="animationMessage" class="toast"></div><p class="hint">The built-in animation draws two small pixel dolphins and bubbles directly on the 64×8 matrix.</p>
      </article>
    </div>
  </section>

  <section id="page-settings" class="page">
    <div class="grid">
      <article class="card">
        <h2>Network status</h2><dl class="meta"><dt>Setup AP</dt><dd id="apName">—</dd><dt>AP address</dt><dd id="apIp">—</dd><dt>AP clients</dt><dd id="apClients">0</dd><dt>Home Wi-Fi</dt><dd id="staName">—</dd><dt>Home address</dt><dd id="staIp">—</dd><dt>Signal</dt><dd id="rssi">—</dd></dl><p class="hint">The setup AP remains active while STA mode connects to the home router.</p>
      </article>
      <article class="card">
        <h2>ESP-NOW receiver</h2><dl class="meta"><dt>Status</dt><dd id="espNowStatus">—</dd><dt>Receiver MAC</dt><dd id="espNowMac">—</dd><dt>Wi-Fi channel</dt><dd id="wifiChannel">—</dd><dt>Received</dt><dd id="espNowReceived">0</dd><dt>Dropped</dt><dd id="espNowDropped">0</dd><dt>Last sender</dt><dd id="espNowLastSender">—</dd></dl><div class="protocol">Magic 0x4C4D4553 · Version 1 · Commands: 1 static, 2 running, 3 time, 4 clear stack · Flag bit 0 queues text</div>
      </article>
      <article class="card">
        <h2>Display orientation</h2>
        <label for="displayRotation">Screen rotation</label>
        <select id="displayRotation"><option value="0">0° · normal</option><option value="180">180° · upside down</option></select>
        <button id="saveRotation" type="button">Apply rotation</button><div id="rotationMessage" class="toast"></div>
        <p class="hint">The 64×8 geometry supports 0° and 180° rotation without cropping.</p>
      </article>
      <article class="card wide">
        <h2>STA + AP settings</h2>
        <div class="row"><div><label for="staSsid">Home Wi-Fi name (SSID)</label><input id="staSsid" maxlength="32" autocomplete="off" placeholder="Your router SSID"></div><div><label for="staPassword">Home Wi-Fi password</label><input id="staPassword" maxlength="63" type="password" autocomplete="new-password" placeholder="Leave blank to keep current"></div></div>
        <div class="row"><div><label for="apSsid">Setup AP name</label><input id="apSsid" maxlength="32" autocomplete="off" placeholder="LED-Matrix-Setup"></div><div><label for="apPassword">Setup AP password</label><input id="apPassword" maxlength="63" type="password" autocomplete="new-password" placeholder="Leave blank to keep current"></div></div>
        <label class="check"><input id="openAp" type="checkbox"> Use an open setup AP</label><button id="saveWifi" type="button">Save and restart Wi-Fi</button><div id="wifiMessage" class="toast"></div><p class="hint">A protected AP password must contain 8–63 characters.</p>
      </article>
    </div>
  </section>

  <p class="small">Open at 192.168.4.1 from the setup AP, or use the home-network address shown in Settings.</p>
</main>
<script>
const $=id=>document.getElementById(id);
const formEncode=data=>new URLSearchParams(data).toString();
let gameBusy=false,statusBusy=false,selectedGame='space';
let paintBits=new Uint8Array(512),paintCursorX=0,paintCursorY=0,paintTool='draw',paintDragging=false,paintUploadTimer=null,paintIgnoreStatusUntil=0,paintRequest=Promise.resolve();
const dirtyFields=new Set();
const trackedFields=['displayText','displayMode','speed','spaceSpeed','trexSpeed','gravitySpeed','racingSpeed','tetrisSpeed','animationSpeed','timezoneOffset','ntpServer','timerHours','timerMinutes','timerSeconds','timerPresentation','staSsid','apSsid','openAp','displayRotation'];

function showPage(name){document.querySelectorAll('.page').forEach(p=>p.classList.toggle('active',p.id==='page-'+name));document.querySelectorAll('.nav-button').forEach(b=>b.classList.toggle('active',b.dataset.page===name));}
document.querySelectorAll('[data-page]').forEach(b=>b.addEventListener('click',()=>showPage(b.dataset.page)));
function message(id,text,error=false){const el=$(id);el.textContent=text;el.classList.toggle('error',error)}
function updatePreview(){$('preview').textContent=($('displayText').value||' ').toUpperCase()}
function prettyGameState(state){return ({ready:'Ready',running:'Running',paused:'Paused',gameover:'Game over'})[state]||state||'—'}
function prettyChronoState(state){return ({ready:'Ready',running:'Running',paused:'Paused',finished:'Finished'})[state]||state||'—'}
function prettyMode(mode){return ({scroll:'Running text',static:'Static text',paint:'Paint',space:'Space Runner',trex:'T-Rex Runner',gravity:'Gravity Guy',racing:'Pixel Racing',tetris:'Tetris',scoreboard:'Scoreboard',animation:'Dolphins animation',time:'Clock',timer:'Timer',stopwatch:'Stopwatch',calendar:'Calendar'})[mode]||mode}
function timerDuration(){const h=Math.max(0,Math.min(99,Number($('timerHours').value)||0)),m=Math.max(0,Math.min(59,Number($('timerMinutes').value)||0)),sec=Math.max(0,Math.min(59,Number($('timerSeconds').value)||0));return h*3600+m*60+sec}
function syncTimerDuration(total){total=Math.max(0,Number(total)||0);syncValue('timerHours',Math.floor(total/3600));syncValue('timerMinutes',Math.floor(total/60)%60);syncValue('timerSeconds',total%60)}
function timePayload(action,extra={}){return {action,timezoneOffset:$('timezoneOffset').value,ntpServer:$('ntpServer').value,...extra}}
function syncValue(id,value){const el=$(id);if(el&&!dirtyFields.has(id)&&document.activeElement!==el)el.value=value}
function syncChecked(id,value){const el=$(id);if(el&&!dirtyFields.has(id)&&document.activeElement!==el)el.checked=!!value}
function markClean(...ids){ids.forEach(id=>dirtyFields.delete(id))}
function selectGame(game){selectedGame=game;$('spaceCard').classList.toggle('active-game',game==='space');$('trexCard').classList.toggle('active-game',game==='trex');$('gravityCard').classList.toggle('active-game',game==='gravity');$('racingCard').classList.toggle('active-game',game==='racing');$('tetrisCard').classList.toggle('active-game',game==='tetris')}
function paintIndex(x,y){return y*64+x}
function paintHex(){let out='';for(let y=0;y<8;y++){for(let b=0;b<8;b++){let value=0;for(let bit=0;bit<8;bit++){if(paintBits[paintIndex(b*8+bit,y)])value|=1<<bit}out+=value.toString(16).padStart(2,'0')}}return out.toUpperCase()}
function loadPaintHex(hex){if(typeof hex!=='string'||hex.length!==128)return false;const next=new Uint8Array(512);for(let y=0;y<8;y++){for(let b=0;b<8;b++){const value=parseInt(hex.slice((y*8+b)*2,(y*8+b)*2+2),16);if(Number.isNaN(value))return false;for(let bit=0;bit<8;bit++)next[paintIndex(b*8+bit,y)]=(value>>bit)&1}}paintBits=next;return true}
function drawPaintCanvas(){const canvas=$('paintCanvas'),ctx=canvas.getContext('2d'),cellW=canvas.width/64,cellH=canvas.height/8;ctx.fillStyle='#020712';ctx.fillRect(0,0,canvas.width,canvas.height);ctx.fillStyle='#67e8f9';for(let y=0;y<8;y++)for(let x=0;x<64;x++)if(paintBits[paintIndex(x,y)])ctx.fillRect(x*cellW+1,y*cellH+1,cellW-2,cellH-2);ctx.strokeStyle='#273a59';ctx.lineWidth=1;for(let x=0;x<=64;x++){ctx.beginPath();ctx.moveTo(x*cellW+.5,0);ctx.lineTo(x*cellW+.5,canvas.height);ctx.stroke()}for(let y=0;y<=8;y++){ctx.beginPath();ctx.moveTo(0,y*cellH+.5);ctx.lineTo(canvas.width,y*cellH+.5);ctx.stroke()}ctx.strokeStyle='#fde68a';ctx.lineWidth=2;ctx.strokeRect(paintCursorX*cellW+1,paintCursorY*cellH+1,cellW-2,cellH-2);$('paintPosition').textContent='X '+paintCursorX+' · Y '+paintCursorY}
function setPaintTool(tool){paintTool=tool;document.querySelectorAll('.paint-tool').forEach(b=>{const active=b.dataset.tool===tool;b.classList.toggle('active',active);b.classList.toggle('secondary',!active)})}
function applyPaintTool(){const index=paintIndex(paintCursorX,paintCursorY);if(paintTool==='draw')paintBits[index]=1;else if(paintTool==='erase')paintBits[index]=0}
function setPaintCursor(x,y,apply=true){paintCursorX=Math.max(0,Math.min(63,x));paintCursorY=Math.max(0,Math.min(7,y));if(apply)applyPaintTool();drawPaintCanvas();schedulePaintUpload()}
function movePaintCursor(dx,dy){setPaintCursor(paintCursorX+dx,paintCursorY+dy,true)}
function setCurrentPaintPixel(value){paintBits[paintIndex(paintCursorX,paintCursorY)]=value;drawPaintCanvas();schedulePaintUpload()}
function invertPaint(){for(let i=0;i<paintBits.length;i++)paintBits[i]=paintBits[i]?0:1;drawPaintCanvas();schedulePaintUpload()}
function clearPaint(value=0){paintBits.fill(value);drawPaintCanvas();schedulePaintUpload()}
async function postPaint(action,extra={},announce=false){paintIgnoreStatusUntil=Date.now()+2500;paintRequest=paintRequest.then(async()=>{const r=await fetch('/api/paint',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formEncode({action,x:paintCursorX,y:paintCursorY,...extra})});const j=await r.json();if(!r.ok||!j.ok)throw new Error(j.error||'Paint command failed');if(Number.isInteger(j.cursorX))paintCursorX=j.cursorX;if(Number.isInteger(j.cursorY))paintCursorY=j.cursorY;drawPaintCanvas();if(announce)message('paintMessage','Paint display updated.');return j}).catch(e=>message('paintMessage',e.message,true));return paintRequest}
function schedulePaintUpload(){paintIgnoreStatusUntil=Date.now()+2500;clearTimeout(paintUploadTimer);paintUploadTimer=setTimeout(()=>postPaint('bitmap',{data:paintHex()}),120)}
function paintPointFromEvent(event){const rect=$('paintCanvas').getBoundingClientRect();return {x:Math.max(0,Math.min(63,Math.floor((event.clientX-rect.left)*64/rect.width))),y:Math.max(0,Math.min(7,Math.floor((event.clientY-rect.top)*8/rect.height)))}}

trackedFields.forEach(id=>{const el=$(id);if(!el)return;el.addEventListener('input',()=>dirtyFields.add(id));el.addEventListener('change',()=>dirtyFields.add(id));});
$('displayText').addEventListener('input',updatePreview);
$('queueText').addEventListener('change',()=>{$('sendText').textContent=$('queueText').checked?'Add to stack':'Show text now'});
$('spaceCard').addEventListener('pointerdown',()=>selectGame('space'));
$('trexCard').addEventListener('pointerdown',()=>selectGame('trex'));
$('gravityCard').addEventListener('pointerdown',()=>selectGame('gravity'));
$('racingCard').addEventListener('pointerdown',()=>selectGame('racing'));
$('tetrisCard').addEventListener('pointerdown',()=>selectGame('tetris'));
document.querySelectorAll('.paint-tool').forEach(b=>b.addEventListener('click',()=>setPaintTool(b.dataset.tool)));
document.querySelectorAll('.paint-move').forEach(b=>b.addEventListener('click',()=>movePaintCursor(Number(b.dataset.dx),Number(b.dataset.dy))));
$('paintToggle').addEventListener('click',()=>setCurrentPaintPixel(paintBits[paintIndex(paintCursorX,paintCursorY)]?0:1));
$('paintPixelOn').addEventListener('click',()=>setCurrentPaintPixel(1));
$('paintPixelOff').addEventListener('click',()=>setCurrentPaintPixel(0));
$('paintShow').addEventListener('click',()=>{clearTimeout(paintUploadTimer);postPaint('bitmap',{data:paintHex()},true)});
$('paintClear').addEventListener('click',()=>clearPaint(0));
$('paintFill').addEventListener('click',()=>clearPaint(1));
$('paintInvert').addEventListener('click',invertPaint);
$('paintCanvas').addEventListener('pointerdown',event=>{paintDragging=true;$('paintCanvas').setPointerCapture(event.pointerId);const p=paintPointFromEvent(event);setPaintCursor(p.x,p.y,true)});
$('paintCanvas').addEventListener('pointermove',event=>{if(!paintDragging)return;const p=paintPointFromEvent(event);if(p.x!==paintCursorX||p.y!==paintCursorY)setPaintCursor(p.x,p.y,true)});
$('paintCanvas').addEventListener('pointerup',()=>{paintDragging=false;schedulePaintUpload()});
$('paintCanvas').addEventListener('pointercancel',()=>{paintDragging=false});

async function readStatus(){
  if(statusBusy)return;statusBusy=true;
  try{
    const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw new Error('Status unavailable');const s=await r.json();
    $('dot').classList.toggle('ok',s.staConnected);$('connection').textContent=s.staConnected?'Home Wi-Fi connected':'Setup AP active';
    $('apName').textContent=s.apSsid||'—';$('apIp').textContent=s.apIp||'—';$('apClients').textContent=s.apClients??0;$('staName').textContent=s.staSsid||'Not configured';$('staIp').textContent=s.staIp||'Not connected';$('rssi').textContent=s.staConnected?(s.rssi+' dBm'):'—';
    $('summarySta').textContent=s.staConnected?(s.staSsid+' · '+s.staIp):(s.staSsid||'Not configured');$('summaryAp').textContent=(s.apSsid||'—')+' · '+(s.apIp||'—');$('summaryRssi').textContent=s.staConnected?(s.rssi+' dBm'):'—';
    syncValue('displayText',s.text||'');syncValue('speed',s.speed||60);if(s.mode==='scroll'||s.mode==='static')syncValue('displayMode',s.mode);syncValue('staSsid',s.configuredStaSsid||'');syncValue('apSsid',s.apSsid||'');syncChecked('openAp',s.apOpen);syncValue('spaceSpeed',s.spaceSpeed||120);syncValue('trexSpeed',s.trexSpeed||110);syncValue('gravitySpeed',s.gravitySpeed||90);syncValue('racingSpeed',s.racingSpeed||120);syncValue('tetrisSpeed',s.tetrisSpeed||450);syncValue('animationSpeed',s.animationSpeed||220);syncValue('timerPresentation',s.timerPresentation||'time');syncValue('timezoneOffset',s.timezoneOffset??0);syncValue('ntpServer',s.ntpServer||'pool.ntp.org');syncValue('displayRotation',String(s.rotation??0));syncTimerDuration(s.timerDuration||300);
    const ss=dirtyFields.has('spaceSpeed')?$('spaceSpeed').value:(s.spaceSpeed||120),ts=dirtyFields.has('trexSpeed')?$('trexSpeed').value:(s.trexSpeed||110),gs=dirtyFields.has('gravitySpeed')?$('gravitySpeed').value:(s.gravitySpeed||90),rs=dirtyFields.has('racingSpeed')?$('racingSpeed').value:(s.racingSpeed||120),zs=dirtyFields.has('tetrisSpeed')?$('tetrisSpeed').value:(s.tetrisSpeed||450),as=dirtyFields.has('animationSpeed')?$('animationSpeed').value:(s.animationSpeed||220);$('spaceSpeedValue').textContent=ss;$('spaceRangeSpeed').textContent='Live '+(s.spaceCurrentSpeed||ss)+' ms';$('trexSpeedValue').textContent=ts;$('trexRangeSpeed').textContent='Live '+(s.trexCurrentSpeed||ts)+' ms';$('gravitySpeedValue').textContent=gs;$('gravityRangeSpeed').textContent='Live '+(s.gravityCurrentSpeed||gs)+' ms';$('racingSpeedValue').textContent=rs;$('racingRangeSpeed').textContent='Live '+(s.racingCurrentSpeed||rs)+' ms';$('tetrisSpeedValue').textContent=zs;$('tetrisRangeSpeed').textContent=zs+' ms';$('animationSpeedValue').textContent=as;$('animationRangeSpeed').textContent=as+' ms';
    if(Date.now()>=paintIgnoreStatusUntil&&!paintDragging&&s.paintData){loadPaintHex(s.paintData);paintCursorX=Math.max(0,Math.min(63,Number(s.paintCursorX)||0));paintCursorY=Math.max(0,Math.min(7,Number(s.paintCursorY)||0));drawPaintCanvas()}
    $('spaceState').textContent=prettyGameState(s.spaceState);$('spaceScore').textContent=s.spaceScore??0;$('shipPosition').textContent=s.shipY??4;$('spacePause').textContent=s.spaceState==='paused'?'Resume':'Pause';
    $('trexState').textContent=prettyGameState(s.trexState);$('trexScore').textContent=s.trexScore??0;$('trexJumpState').textContent=s.trexJumping?'Jumping':'Ground';$('trexPause').textContent=s.trexState==='paused'?'Resume':'Pause';
    $('gravityState').textContent=prettyGameState(s.gravityState);$('gravityScore').textContent=s.gravityScore??0;$('gravitySurface').textContent=(s.gravitySurface==='top'?'Top':'Bottom')+' · Y '+(s.gravityPlayerY??6);$('gravityPause').textContent=s.gravityState==='paused'?'Resume':'Pause';
    $('racingState').textContent=prettyGameState(s.racingState);$('racingScore').textContent=s.racingScore??0;$('racingPosition').textContent=s.racingPlayerY??4;$('racingPause').textContent=s.racingState==='paused'?'Resume':'Pause';
    $('tetrisState').textContent=prettyGameState(s.tetrisState);$('tetrisScore').textContent=s.tetrisScore??0;$('tetrisLines').textContent=s.tetrisLines??0;$('tetrisPause').textContent=s.tetrisState==='paused'?'Resume':'Pause';
    $('scoreboardFace').textContent=String(s.scoreboardLeft??0).padStart(2,'0')+':'+String(s.scoreboardRight??0).padStart(2,'0');$('animationPause').textContent=s.animationState==='paused'?'Resume':'Pause';
    if(['space','trex','gravity','racing','tetris'].includes(s.mode))selectGame(s.mode);
    const modeName=prettyMode(s.mode);$('activeMode').textContent='Display mode: '+modeName;$('overviewMode').textContent=modeName;$('currentDisplay').textContent=s.activeText||s.text||' ';$('queueDepth').textContent=s.queueDepth??0;$('overviewStaIp').textContent=s.staIp||'Not connected';
    $('clockFace').textContent=s.clockTime||'--:--:--';$('calendarFace').textContent=s.calendarDate||'--.--.--';$('overviewClock').textContent=s.clockTime||'--:--:--';const source=({ntp:'NTP synchronized',manual:'Manual/browser time',syncing:'Waiting for NTP response',unset:'Time is not synchronized'})[s.timeSource]||s.timeSource;$('timeState').textContent=source;
    $('timerFace').textContent=s.timerText||'00:05:00';$('timerState').textContent=prettyChronoState(s.timerState)+(s.timerBlinking?' · blinking':'');$('timerPause').textContent=s.timerState==='running'?'Pause':(s.timerState==='ready'?'Start':'Resume');$('timerBarFill').style.width=Math.max(0,Math.min(100,Number(s.timerProgress)||0))+'%';
    $('stopwatchFace').textContent=s.stopwatchText||'00:00:00';$('stopwatchState').textContent=prettyChronoState(s.stopwatchState);$('stopwatchPause').textContent=s.stopwatchState==='running'?'Pause':(s.stopwatchState==='ready'?'Start':'Resume');
    $('espNowStatus').textContent=s.espNowReady?'Listening':'Initialization failed';$('espNowMac').textContent=s.espNowMac||'—';$('wifiChannel').textContent=s.wifiChannel??'—';$('espNowReceived').textContent=s.espNowReceived??0;$('espNowDropped').textContent=s.espNowDropped??0;$('espNowLastSender').textContent=s.espNowLastSender||'—';updatePreview();
  }catch(e){$('connection').textContent='Controller unavailable';$('dot').classList.remove('ok')}finally{statusBusy=false}
}

$('sendText').addEventListener('click',async()=>{const b=$('sendText'),queued=$('queueText').checked;b.disabled=true;message('textMessage',queued?'Adding to stack…':'Sending…');try{const r=await fetch('/api/text',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formEncode({text:$('displayText').value,mode:$('displayMode').value,speed:$('speed').value,queue:queued?'1':'0',hold:$('holdMs').value,repeats:$('repeatCount').value})});const j=await r.json();if(!r.ok||!j.ok)throw new Error(j.error||'Unable to update display');if(!queued)markClean('displayText','displayMode','speed');message('textMessage',queued?'Message added to stack.':'Display updated.');await readStatus()}catch(e){message('textMessage',e.message,true)}finally{b.disabled=false}});
$('clearStack').addEventListener('click',async()=>{const b=$('clearStack');b.disabled=true;message('textMessage','Clearing stack…');try{const r=await fetch('/api/queue',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formEncode({action:'clear'})});const j=await r.json();if(!r.ok||!j.ok)throw new Error(j.error||'Unable to clear stack');message('textMessage','Display stack cleared.');await readStatus()}catch(e){message('textMessage',e.message,true)}finally{b.disabled=false}});

async function sendGame(game,action){const instant=['up','down','jump','flip','rotate','drop'].includes(action);selectGame(game);if(!instant&&gameBusy)return;if(!instant)gameBusy=true;const speedId=game==='trex'?'trexSpeed':game==='gravity'?'gravitySpeed':game==='racing'?'racingSpeed':game==='tetris'?'tetrisSpeed':'spaceSpeed';const msgId=game==='trex'?'trexMessage':game==='gravity'?'gravityMessage':game==='racing'?'racingMessage':game==='tetris'?'tetrisMessage':'spaceMessage';try{const r=await fetch('/api/game',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formEncode({game,action,speed:$(speedId).value})});const j=await r.json();if(!r.ok||!j.ok)throw new Error(j.error||'Game command failed');if(action==='speed')markClean(speedId);if(!instant)message(msgId,action==='reset'?'Game reset.':'');await readStatus()}catch(e){message(msgId,e.message,true)}finally{if(!instant)gameBusy=false}}
document.querySelectorAll('.space-action').forEach(b=>b.addEventListener('click',()=>sendGame('space',b.dataset.action)));document.querySelectorAll('.trex-action').forEach(b=>b.addEventListener('click',()=>sendGame('trex',b.dataset.action)));document.querySelectorAll('.gravity-action').forEach(b=>b.addEventListener('click',()=>sendGame('gravity',b.dataset.action)));document.querySelectorAll('.racing-action').forEach(b=>b.addEventListener('click',()=>sendGame('racing',b.dataset.action)));document.querySelectorAll('.tetris-action').forEach(b=>b.addEventListener('click',()=>sendGame('tetris',b.dataset.action)));
$('spaceSpeed').addEventListener('input',()=>{$('spaceSpeedValue').textContent=$('spaceSpeed').value;$('spaceRangeSpeed').textContent='Start '+$('spaceSpeed').value+' ms'});$('spaceSpeed').addEventListener('change',()=>sendGame('space','speed'));
$('trexSpeed').addEventListener('input',()=>{$('trexSpeedValue').textContent=$('trexSpeed').value;$('trexRangeSpeed').textContent='Start '+$('trexSpeed').value+' ms'});$('trexSpeed').addEventListener('change',()=>sendGame('trex','speed'));
$('gravitySpeed').addEventListener('input',()=>{$('gravitySpeedValue').textContent=$('gravitySpeed').value;$('gravityRangeSpeed').textContent='Start '+$('gravitySpeed').value+' ms'});$('gravitySpeed').addEventListener('change',()=>sendGame('gravity','speed'));
$('racingSpeed').addEventListener('input',()=>{$('racingSpeedValue').textContent=$('racingSpeed').value;$('racingRangeSpeed').textContent='Start '+$('racingSpeed').value+' ms'});$('racingSpeed').addEventListener('change',()=>sendGame('racing','speed'));
$('tetrisSpeed').addEventListener('input',()=>{$('tetrisSpeedValue').textContent=$('tetrisSpeed').value;$('tetrisRangeSpeed').textContent=$('tetrisSpeed').value+' ms'});$('tetrisSpeed').addEventListener('change',()=>sendGame('tetris','speed'));
document.addEventListener('keydown',event=>{
  const tag=document.activeElement&&document.activeElement.tagName;if(tag==='INPUT'||tag==='SELECT'||tag==='TEXTAREA')return;
  if($('page-paint').classList.contains('active')){
    const movement={ArrowLeft:[-1,0],ArrowRight:[1,0],ArrowUp:[0,-1],ArrowDown:[0,1]}[event.key];
    if(movement){event.preventDefault();movePaintCursor(movement[0],movement[1]);return}
    if(event.key===' '){event.preventDefault();setCurrentPaintPixel(paintBits[paintIndex(paintCursorX,paintCursorY)]?0:1);return}
    if(event.key==='d'||event.key==='D'){setPaintTool('draw');return}
    if(event.key==='e'||event.key==='E'){setPaintTool('erase');return}
    if(event.key==='m'||event.key==='M'){setPaintTool('move');return}
    if(event.key==='c'||event.key==='C'){clearPaint(0);return}
    if(event.key==='i'||event.key==='I'){invertPaint();return}
  }
  let action=null;if(selectedGame==='gravity'){if(['ArrowUp','ArrowDown','w','W','f','F',' '].includes(event.key))action='flip';else if(event.key==='p'||event.key==='P')action='pause'}else if(selectedGame==='racing'){if(event.key==='ArrowUp'||event.key==='w'||event.key==='W')action='up';else if(event.key==='ArrowDown'||event.key==='s'||event.key==='S')action='down';else if(event.key==='p'||event.key==='P')action='pause'}else if(selectedGame==='tetris'){if(event.key==='ArrowUp'||event.key==='w'||event.key==='W')action='up';else if(event.key==='ArrowDown'||event.key==='s'||event.key==='S')action='down';else if(event.key==='r'||event.key==='R')action='rotate';else if(event.key==='ArrowLeft'||event.key===' ')action='drop';else if(event.key==='p'||event.key==='P')action='pause'}else if(selectedGame==='trex'){if(['ArrowUp','w','W',' '].includes(event.key))action='jump';else if(event.key==='p'||event.key==='P')action='pause'}else{action=({ArrowUp:'up',w:'up',W:'up',ArrowDown:'down',s:'down',S:'down',' ':'pause'})[event.key]}if(!action)return;event.preventDefault();sendGame(selectedGame,action)
});

async function sendScoreboard(action){message('scoreboardMessage','Updating…');try{const r=await fetch('/api/scoreboard',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formEncode({action})});const j=await r.json();if(!r.ok||!j.ok)throw new Error(j.error||'Scoreboard command failed');message('scoreboardMessage','');await readStatus()}catch(e){message('scoreboardMessage',e.message,true)}}
document.querySelectorAll('.score-action').forEach(b=>b.addEventListener('click',()=>sendScoreboard(b.dataset.action)));

async function sendAnimation(action){message('animationMessage','Updating…');try{const r=await fetch('/api/animation',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formEncode({action,speed:$('animationSpeed').value})});const j=await r.json();if(!r.ok||!j.ok)throw new Error(j.error||'Animation command failed');if(action==='speed')markClean('animationSpeed');message('animationMessage','');await readStatus()}catch(e){message('animationMessage',e.message,true)}}
$('animationPlay').addEventListener('click',()=>sendAnimation('play'));$('animationPause').addEventListener('click',()=>sendAnimation('pause'));$('animationReset').addEventListener('click',()=>sendAnimation('reset'));$('animationSpeed').addEventListener('input',()=>{$('animationSpeedValue').textContent=$('animationSpeed').value;$('animationRangeSpeed').textContent=$('animationSpeed').value+' ms'});$('animationSpeed').addEventListener('change',()=>sendAnimation('speed'));

async function sendTime(action,extra={},success='Time settings updated.'){message('timeMessage',action==='ntp'?'Requesting NTP sync…':'Updating…');try{const r=await fetch('/api/time',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formEncode(timePayload(action,extra))});const j=await r.json();if(!r.ok||!j.ok)throw new Error(j.error||'Time command failed');markClean('timezoneOffset','ntpServer');message('timeMessage',success);await readStatus()}catch(e){message('timeMessage',e.message,true)}}
$('browserZone').addEventListener('click',()=>{$('timezoneOffset').value=-new Date().getTimezoneOffset();dirtyFields.add('timezoneOffset');message('timeMessage','Browser UTC offset copied.')});$('showTime').addEventListener('click',()=>sendTime('show',{},'Clock mode selected.'));$('showCalendar').addEventListener('click',()=>sendTime('calendar_show',{},'Calendar mode selected.'));$('saveTime').addEventListener('click',()=>sendTime('save',{},'Time settings saved.'));$('syncNtp').addEventListener('click',()=>sendTime('ntp',{},'NTP sync requested.'));$('syncBrowser').addEventListener('click',()=>sendTime('set',{epoch:Math.floor(Date.now()/1000)},'Controller synchronized with browser time.'));
$('setManualTime').addEventListener('click',()=>{const value=$('manualTime').value;if(!value){message('timeMessage','Select a manual date and time.',true);return}const parts=value.split('T'),date=parts[0].split('-').map(Number),time=parts[1].split(':').map(Number),offset=Number($('timezoneOffset').value||0);const epoch=Math.floor(Date.UTC(date[0],date[1]-1,date[2],time[0],time[1],time[2]||0)/1000-offset*60);sendTime('set',{epoch},'Manual time set.')});

async function sendChrono(action,extra,messageId,success){message(messageId,'Updating…');try{const r=await fetch('/api/time',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formEncode({action,...extra})});const j=await r.json();if(!r.ok||!j.ok)throw new Error(j.error||'Timer command failed');message(messageId,success);await readStatus();return true}catch(e){message(messageId,e.message,true);return false}}
async function timerCommand(action,success){const duration=timerDuration();if((action==='timer_set'||action==='timer_start')&&duration<1){message('timerMessage','Set a duration of at least one second.',true);return}if(await sendChrono(action,{duration,presentation:$('timerPresentation').value},'timerMessage',success))markClean('timerHours','timerMinutes','timerSeconds','timerPresentation')}
$('timerSet').addEventListener('click',()=>timerCommand('timer_set','Timer set and selected.'));
$('timerStart').addEventListener('click',()=>timerCommand('timer_start','Timer started.'));
$('timerPause').addEventListener('click',()=>sendChrono('timer_pause',{presentation:$('timerPresentation').value},'timerMessage','Timer state changed.'));
$('timerReset').addEventListener('click',()=>sendChrono('timer_reset',{presentation:$('timerPresentation').value},'timerMessage','Timer reset.'));
$('timerPresentation').addEventListener('change',async()=>{if(await sendChrono('timer_show',{presentation:$('timerPresentation').value},'timerMessage','Timer presentation updated.'))markClean('timerPresentation')});
$('stopwatchShow').addEventListener('click',()=>sendChrono('stopwatch_show',{},'stopwatchMessage','Stopwatch selected.'));
$('stopwatchStart').addEventListener('click',()=>sendChrono('stopwatch_start',{},'stopwatchMessage','Stopwatch started.'));
$('stopwatchPause').addEventListener('click',()=>sendChrono('stopwatch_pause',{},'stopwatchMessage','Stopwatch state changed.'));
$('stopwatchReset').addEventListener('click',()=>sendChrono('stopwatch_reset',{},'stopwatchMessage','Stopwatch reset.'));

$('saveRotation').addEventListener('click',async()=>{const b=$('saveRotation');b.disabled=true;message('rotationMessage','Applying…');try{const r=await fetch('/api/device',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formEncode({rotation:$('displayRotation').value})});const j=await r.json();if(!r.ok||!j.ok)throw new Error(j.error||'Unable to apply rotation');markClean('displayRotation');message('rotationMessage','Rotation applied.');await readStatus()}catch(e){message('rotationMessage',e.message,true)}finally{b.disabled=false}});

$('saveWifi').addEventListener('click',async()=>{const b=$('saveWifi');b.disabled=true;message('wifiMessage','Saving…');try{const r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:formEncode({staSsid:$('staSsid').value,staPassword:$('staPassword').value,apSsid:$('apSsid').value,apPassword:$('apPassword').value,apOpen:$('openAp').checked?'1':'0'})});const j=await r.json();if(!r.ok||!j.ok)throw new Error(j.error||'Unable to save Wi-Fi settings');message('wifiMessage','Saved. The controller is restarting; reconnect shortly.');$('staPassword').value='';$('apPassword').value=''}catch(e){message('wifiMessage',e.message,true);b.disabled=false}});

(function setManualDefault(){const d=new Date(),pad=n=>String(n).padStart(2,'0');$('manualTime').value=d.getFullYear()+'-'+pad(d.getMonth()+1)+'-'+pad(d.getDate())+'T'+pad(d.getHours())+':'+pad(d.getMinutes())+':'+pad(d.getSeconds())})();
selectGame('space');setPaintTool('draw');drawPaintCanvas();readStatus();setInterval(readStatus,1000);
</script>
</body>
</html>
)HTML";
