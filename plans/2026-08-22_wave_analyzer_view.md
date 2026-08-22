# Add a wave analyzer (oscilloscope) view to web_sim

## Context

`web_sim/index.html` is a single 420-line file (no build step, opens directly in a browser or via `python3 -m http.server`) that ports the ESP32 synth firmware's DSP/control math to JS 1:1, plus a canvas-drawn OLED emulator. It's untracked/new, not yet committed. There's currently no way to see the actual audio signal — only the OLED's numeric readout of freq/note/volume per oscillator. The user wants a wave analyzer view added, and anticipates the CSS/JS will grow enough to be worth splitting out of the single HTML file.

Decisions confirmed with the user:
- **Content**: oscilloscope only — live time-domain waveform of the final mixed output (not spectrum/FFT bars, not per-oscillator traces).
- **Placement**: an always-visible panel added to the existing single-page layout (no tab/view switcher).
- **File split**: yes — split CSS and JS out of `index.html`, since the new feature pushes both past a size where inlining stays readable.

## File layout after the change

```
web_sim/
  index.html   markup only + <link>/<script src> tags
  style.css    all styles (existing + new scope panel)
  app.js       existing firmware-port logic, moved verbatim (pins mirror, notes.h/controls_math.cpp
               ports, AudioWorkletProcessor source, UI construction, controlsUpdate, drawDisplay,
               audio start/mute)
  analyzer.js  new: AnalyserNode wiring + oscilloscope canvas draw loop — explicitly NOT a firmware
               port (the real hardware has no scope), called out as such in the top-of-file comment
```

Use classic `<script src>` tags (not `type="module"`) and a plain `<link rel="stylesheet">` — both work fine under `file://` with no CORS issues, unlike ES module imports or the existing `audioWorklet.addModule(blobUrl)` call (which already requires the documented `python3 -m http.server` fallback). No change needed to that existing caveat.

Top-level `let`/`const`/`function` declarations in separate classic `<script>` tags share one global lexical scope in the same document, so `analyzer.js` can reference names declared in `app.js` (and vice versa) as long as load order puts `app.js` before `analyzer.js` in `index.html`.

## Integration seam between app.js and analyzer.js

Today `app.js`'s power-button handler (existing lines ~385–417) does:
```js
workletNode.connect(audioCtx.destination);
```

Replace that with a call into `analyzer.js`:
```js
setupAnalyzer(workletNode, audioCtx);
```

`analyzer.js` defines `setupAnalyzer(workletNode, audioCtx)`, which:
1. Creates an `AnalyserNode` (`audioCtx.createAnalyser()`), sized e.g. `fftSize = 1024`.
2. Rewires the graph: `workletNode.connect(analyser); analyser.connect(audioCtx.destination);` — analyzer.js now owns final routing to destination, app.js stops doing it directly.
3. Starts a `requestAnimationFrame` draw loop (first use of rAF in this file — existing loops are all `setInterval`) that reads `analyser.getByteTimeDomainData(...)` each frame and draws a line trace onto a new `<canvas id="scope">`.

This keeps the coupling to a single function call — app.js still owns creating the worklet/context, analyzer.js owns visualization + final destination hookup. Guard `setupAnalyzer` to only run once (first "Start Audio" click), matching the existing lazy-init pattern for `audioCtx`/`workletNode`.

Before the first click, the `#scope` canvas should show a flat idle line (draw once on page load) rather than nothing, so the panel doesn't look broken pre-power-on.

## UI / markup changes (index.html)

Add a new panel after the existing `.rack` div, following the body's existing column-flex layout:
```html
<div class="rack" id="rack"></div>

<div id="scopeWrap">
  <canvas id="scope" width="600" height="150"></canvas>
</div>
```

## Styling (style.css)

Move the existing inline `<style>` block (current lines 30–144) verbatim into `style.css`, then add rules for `#scopeWrap`/`#scope` reusing the existing CSS custom properties (`--panel`, `--border`, `--accent`) so it matches the `#oledWrap` treatment (dark panel, rounded border) rather than introducing new colors.

## Draw loop (analyzer.js)

Follow the file's existing section-banner-comment convention (`// ---- ... ----`). Sketch:
```js
// ---- Wave analyzer: AnalyserNode + oscilloscope canvas -----------------
// Not a firmware port — the real hardware has no scope; this is a
// browser-only addition for visualizing the mixed output.
const SCOPE_FFT_SIZE = 1024;

function setupAnalyzer(workletNode, audioCtx) {
  const analyser = audioCtx.createAnalyser();
  analyser.fftSize = SCOPE_FFT_SIZE;
  workletNode.connect(analyser);
  analyser.connect(audioCtx.destination);

  const data = new Uint8Array(analyser.frequencyBinCount);
  function draw() {
    analyser.getByteTimeDomainData(data);
    // clear canvas, stroke a line mapping data[i] (0-255) across canvas height
    requestAnimationFrame(draw);
  }
  draw();
}
```

## Top-of-file architecture comment

Update the existing lines 2–25 doc comment to:
- Describe the new 4-file layout instead of "single file."
- Add `analyzer.js` to the file-by-file breakdown, explicitly noting it's new browser-only code with no firmware source, preserving the doc's existing "1:1 port, not reimplementation" framing for everything else.

## Verification

1. Serve the folder (`python3 -m http.server` from `web_sim/`) and open it in a browser (also spot-check plain `file://` open, since that's a stated supported mode).
2. Click "Start Audio", raise one or more Volume faders, confirm:
   - Oscilloscope panel shows a live moving waveform reacting to volume (amplitude) and pitch (frequency/shape) changes on all 3 oscillators.
   - OLED readout and rack controls still behave exactly as before (regression check, since `app.js` content is only relocated, not changed).
3. Confirm Mute/Unmute still works and observe scope behavior across suspend/resume.
4. Check browser console for errors on load and during interaction.
