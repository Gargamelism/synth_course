"use strict";

// ---- pins.h mirror -------------------------------------------------------
const NUM_OSCILLATORS   = 3;
const SAMPLE_RATE_HZ    = 44100;
const SINE_TABLE_SIZE   = 256;
const SINE_TABLE_AMPLITUDE = 9000;
const FREQ_MIN_HZ = 80.0;
const FREQ_MAX_HZ = 1000.0;
const ADC_RESOLUTION_BITS = 12;
const ADC_MAX_COUNT = (1 << ADC_RESOLUTION_BITS) - 1; // 4095
const ADC_EMA_ALPHA = 0.15;
const ADC_HYSTERESIS_COUNTS = 4;
const DISPLAY_REFRESH_MS = 100;
const OLED_WIDTH  = 128;
const OLED_HEIGHT = 64;
const OLED_ROW_HEIGHT_PX = 16;
const PHASE_ACCUMULATOR_RANGE = 4294967296.0; // 2^32, oscillator.h

// ---- notes.h port ---------------------------------------------------------
const NOTE_NAMES = ["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"];
function freqToNoteName(freqHz) {
  const note = Math.round(12.0 * Math.log2(freqHz / 440.0) + 69.0);
  const nameIndex = ((note % 12) + 12) % 12;
  const octave = Math.floor(note / 12) - 1;
  return NOTE_NAMES[nameIndex] + octave;
}

// ---- controls_math.cpp port ------------------------------------------------
// state = { filtered, lastRaw }, mutated in place like the C++ out-params.
function smoothValue(raw, state, hysteresisCounts, emaAlpha) {
  if (Math.abs(raw - state.lastRaw) >= hysteresisCounts) {
    state.filtered = state.filtered * (1.0 - emaAlpha) + raw * emaAlpha;
    state.lastRaw = raw;
  }
  return state.filtered;
}

function mapPitchHz(pitchNorm, freqMinHz, freqMaxHz) {
  return freqMinHz * Math.pow(freqMaxHz / freqMinHz, pitchNorm);
}

function clamp01(x) { return Math.min(1, Math.max(0, x)); }

// A real analogRead() always carries a few counts of ADC noise — pins.h
// sizes ADC_HYSTERESIS_COUNTS specifically to sit "above typical ADC noise
// floor" so that noise gets filtered out, but the exact same noise is what
// lets smoothValue() actually finish converging once a pot stops moving:
// with a perfectly noiseless raw reading (a mouse-driven slider), the
// hysteresis gate can permanently freeze part-way once the input goes
// still, so a fader could visibly get stuck short of its true min/max.
// Reusing ADC_HYSTERESIS_COUNTS as the dither range (rather than a made-up
// constant) reliably re-opens the gate until it settles at the true value.
function adcNoise() {
  return Math.floor(Math.random() * (2 * ADC_HYSTERESIS_COUNTS + 1)) - ADC_HYSTERESIS_COUNTS;
}

// ---- AudioWorkletProcessor source (oscillator.h/.cpp + audio_task.cpp) ----
// Built as a string (constants inlined) so it can be loaded from a blob:
// URL and this stays a single, offline-friendly file.
const workletSource = `
class SynthProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    const SINE_TABLE_SIZE = ${SINE_TABLE_SIZE};
    const SINE_TABLE_AMPLITUDE = ${SINE_TABLE_AMPLITUDE};
    const NUM_OSCILLATORS = ${NUM_OSCILLATORS};
    const SAMPLE_RATE_HZ = ${SAMPLE_RATE_HZ};
    const PHASE_ACCUMULATOR_RANGE = ${PHASE_ACCUMULATOR_RANGE};

    this.NUM_OSCILLATORS = NUM_OSCILLATORS;
    this.SAMPLE_RATE_HZ = SAMPLE_RATE_HZ;
    this.PHASE_ACCUMULATOR_RANGE = PHASE_ACCUMULATOR_RANGE;

    // initSineTable()
    this.sineTable = new Int16Array(SINE_TABLE_SIZE);
    for (let i = 0; i < SINE_TABLE_SIZE; i++) {
      const phase = (2.0 * Math.PI * i) / SINE_TABLE_SIZE;
      this.sineTable[i] = Math.round(Math.sin(phase) * SINE_TABLE_AMPLITUDE);
    }

    // Oscillator phase_/phaseInc_, one per voice.
    this.phase = new Uint32Array(NUM_OSCILLATORS);
    this.phaseInc = new Uint32Array(NUM_OSCILLATORS);
    this.vol = new Float32Array(NUM_OSCILLATORS);

    this.port.onmessage = (e) => {
      const { freqHz, volume } = e.data;
      for (let i = 0; i < NUM_OSCILLATORS; i++) {
        // Oscillator::setFrequency
        this.phaseInc[i] = freqHz[i] * (this.PHASE_ACCUMULATOR_RANGE / this.SAMPLE_RATE_HZ);
        this.vol[i] = volume[i];
      }
    };
  }

  process(inputs, outputs) {
    const output = outputs[0];
    const left = output[0];
    const right = output.length > 1 ? output[1] : null;
    const NUM_OSCILLATORS = this.NUM_OSCILLATORS;
    const samples = new Int16Array(NUM_OSCILLATORS);
    const SINE_TABLE_AMPLITUDE = ${SINE_TABLE_AMPLITUDE};
    const INT16_MAX = 32767;
    const INT16_MIN = -32768;
    const INT16_SCALE = 32768; // normalizes int16 to the [-1, 1] float range

    for (let frame = 0; frame < left.length; frame++) {
      // Oscillator::nextSample() per voice
      for (let i = 0; i < NUM_OSCILLATORS; i++) {
        const index = (this.phase[i] >>> 24) & 0xFF; // top 8 bits, 256-entry table
        samples[i] = this.sineTable[index];
        this.phase[i] = (this.phase[i] + this.phaseInc[i]) >>> 0;
      }

      // mixOscillators()
      let mixed = 0;
      for (let i = 0; i < NUM_OSCILLATORS; i++) {
        mixed += samples[i] * this.vol[i];
      }
      if (mixed > INT16_MAX) mixed = INT16_MAX;
      if (mixed < INT16_MIN) mixed = INT16_MIN;

      const sampleFloat = mixed / INT16_SCALE;
      left[frame] = sampleFloat;
      if (right) right[frame] = sampleFloat;

      // Per-oscillator taps (pre-mix, post-volume) for the scope views.
      for (let i = 0; i < NUM_OSCILLATORS; i++) {
        const tap = outputs[i + 1];
        if (tap && tap[0]) {
          tap[0][frame] = (samples[i] / SINE_TABLE_AMPLITUDE) * this.vol[i];
        }
      }
    }
    return true;
  }
}
registerProcessor('synth-processor', SynthProcessor);
`;

// ---- Per-oscillator UI + control-loop state --------------------------------
const rack = document.getElementById("rack");
const volSliders = [];
const pitchSliders = [];
const volReadouts = [];
const pitchReadouts = [];
const volState = [];
const pitchState = [];
const freqHz = new Array(NUM_OSCILLATORS).fill(FREQ_MIN_HZ);
const volume = new Array(NUM_OSCILLATORS).fill(0);

for (let i = 0; i < NUM_OSCILLATORS; i++) {
  const osc = document.createElement("div");
  osc.className = "osc";
  osc.innerHTML = `
    <h2>OSC ${i + 1}</h2>
    <div class="ctrl">
      <label>Pitch</label>
      <input type="range" min="0" max="${ADC_MAX_COUNT}" value="0" id="pitch${i}">
      <div class="readout" id="pitchOut${i}"></div>
    </div>
    <div class="ctrl">
      <label>Volume</label>
      <input type="range" min="0" max="${ADC_MAX_COUNT}" value="0" id="vol${i}">
      <div class="readout" id="volOut${i}"></div>
    </div>
  `;
  rack.appendChild(osc);

  const pitchInput = osc.querySelector(`#pitch${i}`);
  const volInput = osc.querySelector(`#vol${i}`);
  pitchSliders.push(pitchInput);
  volSliders.push(volInput);
  pitchReadouts.push(osc.querySelector(`#pitchOut${i}`));
  volReadouts.push(osc.querySelector(`#volOut${i}`));

  // controlsBegin(): seed filtered/lastRaw from the initial reading.
  const initialRaw = parseInt(volInput.value, 10);
  const initialPitchRaw = parseInt(pitchInput.value, 10);
  volState.push({ filtered: initialRaw, lastRaw: initialRaw });
  pitchState.push({ filtered: initialPitchRaw, lastRaw: initialPitchRaw });
}

// ---- controlsUpdate() port, ticking on the main thread ---------------------
function controlsUpdate() {
  for (let i = 0; i < NUM_OSCILLATORS; i++) {
    const volRaw = parseInt(volSliders[i].value, 10) + adcNoise();
    const pitchRaw = parseInt(pitchSliders[i].value, 10) + adcNoise();

    const volFiltered = smoothValue(volRaw, volState[i], ADC_HYSTERESIS_COUNTS, ADC_EMA_ALPHA);
    const pitchFiltered = smoothValue(pitchRaw, pitchState[i], ADC_HYSTERESIS_COUNTS, ADC_EMA_ALPHA);

    volume[i] = clamp01(volFiltered / ADC_MAX_COUNT);

    const pitchNorm = clamp01(pitchFiltered / ADC_MAX_COUNT);
    freqHz[i] = mapPitchHz(pitchNorm, FREQ_MIN_HZ, FREQ_MAX_HZ);

    volReadouts[i].textContent = `${Math.round(volume[i] * 100)}%`;
    pitchReadouts[i].textContent = `${Math.round(freqHz[i])}Hz`;
  }

  if (workletNode) {
    workletNode.port.postMessage({ freqHz: freqHz.slice(), volume: volume.slice() });
  }
}
setInterval(controlsUpdate, 16); // ~Core-1 loop() cadence

// ---- display.cpp port: draw the OLED panel every DISPLAY_REFRESH_MS -------
const canvas = document.getElementById("oled");
const ctx = canvas.getContext("2d");

function drawDisplay() {
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, OLED_WIDTH, OLED_HEIGHT);
  ctx.fillStyle = "#f5f5f5";
  ctx.font = "8px monospace";
  ctx.textBaseline = "top";
  ctx.fillText("3-OSC SYNTH", 0, 0);

  for (let i = 0; i < NUM_OSCILLATORS; i++) {
    const note = freqToNoteName(freqHz[i]);
    const freqStr = String(Math.trunc(freqHz[i])).padStart(4, " ");
    const noteStr = note.padEnd(3, " ");
    const volStr = String(Math.trunc(volume[i] * 100)).padStart(3, " ");
    const line = `O${i + 1} ${freqStr}Hz ${noteStr} V:${volStr}%`;
    ctx.fillText(line, 0, OLED_ROW_HEIGHT_PX * (i + 1));
  }
}
setInterval(drawDisplay, DISPLAY_REFRESH_MS);
drawDisplay();

// ---- Audio start/mute (AudioContext requires a user gesture) --------------
let audioCtx = null;
let workletNode = null;
const powerBtn = document.getElementById("power");
const statusEl = document.getElementById("status");

powerBtn.addEventListener("click", async () => {
  try {
    if (!audioCtx) {
      audioCtx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: SAMPLE_RATE_HZ });
      const blobUrl = URL.createObjectURL(new Blob([workletSource], { type: "application/javascript" }));
      await audioCtx.audioWorklet.addModule(blobUrl);
      workletNode = new AudioWorkletNode(audioCtx, "synth-processor", {
        numberOfInputs: 0,
        numberOfOutputs: NUM_OSCILLATORS + 1,
        outputChannelCount: [2, 1, 1, 1],
      });
      workletNode.connect(audioCtx.destination, 0);
      setupAnalyzer(workletNode, audioCtx);
      workletNode.port.postMessage({ freqHz: freqHz.slice(), volume: volume.slice() });
      powerBtn.textContent = "Mute";
      powerBtn.classList.add("on");
      statusEl.textContent = "Audio running. All 3 oscillators boot at Volume 0 — raise a Volume fader to hear a tone.";
      return;
    }

    if (audioCtx.state === "running") {
      await audioCtx.suspend();
      powerBtn.textContent = "Unmute";
      powerBtn.classList.remove("on");
    } else {
      await audioCtx.resume();
      powerBtn.textContent = "Mute";
      powerBtn.classList.add("on");
    }
  } catch (err) {
    statusEl.textContent = `Audio failed to start: ${err.message || err}. If you opened this file directly (file://), try serving it instead: python3 -m http.server, then open http://localhost:8000/.`;
    console.error(err);
  }
});
