"use strict";

// ---- Wave analyzer: AnalyserNode + oscilloscope canvases ----------------
// Not a firmware port — the real hardware has no scope; this is a
// browser-only addition for visualizing each oscillator's output.
//
// One AnalyserNode per oscillator, tapped pre-mix (see the worklet's
// extra outputs in app.js) rather than one analyser on the mixed signal —
// summing 3 independent frequencies produces a non-periodic, constantly
// drifting waveform that reads as chaotic even though it's a single trace.
// Per-oscillator traces are clean sines instead.
// Must leave enough room for the trigger search (below) to see a full
// cycle even at the lowest oscillator pitch (80Hz → ~551 samples/cycle at
// 44.1kHz): fftSize - SCOPE_WINDOW_SAMPLES has to exceed that comfortably,
// or the search can come up empty and fall back to an untriggered start.
const SCOPE_FFT_SIZE = 2048;

const scopeCanvases = [
  document.getElementById("scope0"),
  document.getElementById("scope1"),
  document.getElementById("scope2"),
];
const scopeCtxs = scopeCanvases.map((c) => c.getContext("2d"));

// Fixed time window (like a scope's time/div), not a fixed cycle count —
// more cycles show up on screen as pitch rises, same as a real scope.
const SCOPE_WINDOW_SAMPLES = 768;

// getByteTimeDomainData maps the analyser's [-1, 1] float range onto
// [0, TIME_DOMAIN_MAX]: TIME_DOMAIN_ZERO (the midpoint) is silence /
// the zero-crossing threshold, 0 is the low peak, TIME_DOMAIN_MAX the high peak.
const TIME_DOMAIN_MAX = 255;
const TIME_DOMAIN_ZERO = 128;

// Real scopes stabilize the trace by triggering: waiting for a fixed
// reference point (here, a rising zero-crossing) before drawing, so every
// frame starts at the same phase. Without this the start phase drifts
// frame to frame, which reads as a jittery trace even for a clean sine.
function triggeredSlice(timeDomainData) {
  const windowLen = Math.min(timeDomainData.length, SCOPE_WINDOW_SAMPLES);
  const maxSearch = timeDomainData.length - windowLen;
  let start = 0;
  for (let i = 1; i < maxSearch; i++) {
    if (timeDomainData[i - 1] < TIME_DOMAIN_ZERO && timeDomainData[i] >= TIME_DOMAIN_ZERO) { start = i; break; }
  }
  return timeDomainData.subarray(start, start + windowLen);
}

function drawScopeTrace(ctx, canvas, data) {
  const w = canvas.width;
  const h = canvas.height;
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, w, h);
  ctx.strokeStyle = "#6cf0a8";
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  const sliceWidth = w / (data.length - 1);
  for (let i = 0; i < data.length; i++) {
    const y = (data[i] / TIME_DOMAIN_MAX) * h;
    const x = i * sliceWidth;
    if (i === 0) ctx.moveTo(x, y);
    else ctx.lineTo(x, y);
  }
  ctx.stroke();
}

// Flat idle line before "Start Audio" is clicked, so the panels don't
// look broken pre-power-on.
(function drawIdleScopes() {
  const idle = new Uint8Array(2).fill(128);
  for (let i = 0; i < scopeCanvases.length; i++) {
    drawScopeTrace(scopeCtxs[i], scopeCanvases[i], idle);
  }
})();

let analyzerStarted = false;
function setupAnalyzer(workletNode, audioCtx) {
  if (analyzerStarted) return;
  analyzerStarted = true;

  // Analyser nodes only get pulled by the render graph if they're
  // reachable from the destination, so route them here at zero gain
  // rather than routing each oscillator tap to speakers directly.
  const keepAlive = audioCtx.createGain();
  keepAlive.gain.value = 0;
  keepAlive.connect(audioCtx.destination);

  const analysers = scopeCanvases.map((_, i) => {
    const analyser = audioCtx.createAnalyser();
    analyser.fftSize = SCOPE_FFT_SIZE;
    workletNode.connect(analyser, i + 1);
    analyser.connect(keepAlive);
    return analyser;
  });
  // getByteTimeDomainData needs a buffer sized to fftSize, not
  // frequencyBinCount (that's fftSize/2, meant for frequency-domain data).
  const dataBuffers = analysers.map((a) => new Uint8Array(a.fftSize));

  function draw() {
    for (let i = 0; i < analysers.length; i++) {
      analysers[i].getByteTimeDomainData(dataBuffers[i]);
      const slice = triggeredSlice(dataBuffers[i]);
      drawScopeTrace(scopeCtxs[i], scopeCanvases[i], slice);
    }
    requestAnimationFrame(draw);
  }
  draw();
}
