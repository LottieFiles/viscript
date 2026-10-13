import fs from "node:fs";
import path from "node:path";
import {fileURLToPath} from "node:url";

const SAMPLE_RATE = 22050;
const OUTPUT = path.dirname(fileURLToPath(import.meta.url));
const TAU = Math.PI * 2;

const clamp = (value, minimum = -1, maximum = 1) =>
    Math.max(minimum, Math.min(maximum, value));
const square = (phase, duty = 0.5) => phase - Math.floor(phase) < duty ? 1 : -1;
const triangle = (phase) => 1 - 4 * Math.abs((phase - Math.floor(phase)) - 0.5);
const midi = (note) => 440 * 2 ** ((note - 69) / 12);
const decay = (time, speed) => Math.exp(-time * speed);

function wav(name, duration, sample) {
    const count = Math.ceil(duration * SAMPLE_RATE);
    const output = Buffer.alloc(44 + count * 2);
    output.write("RIFF", 0);
    output.writeUInt32LE(36 + count * 2, 4);
    output.write("WAVEfmt ", 8);
    output.writeUInt32LE(16, 16);
    output.writeUInt16LE(1, 20);
    output.writeUInt16LE(1, 22);
    output.writeUInt32LE(SAMPLE_RATE, 24);
    output.writeUInt32LE(SAMPLE_RATE * 2, 28);
    output.writeUInt16LE(2, 32);
    output.writeUInt16LE(16, 34);
    output.write("data", 36);
    output.writeUInt32LE(count * 2, 40);
    for (let index = 0; index < count; index += 1) {
        const time = index / SAMPLE_RATE;
        const value = Math.tanh(sample(time, duration) * 1.35) * 0.86;
        output.writeInt16LE(Math.round(clamp(value) * 32767), 44 + index * 2);
    }
    fs.writeFileSync(path.join(OUTPUT, name), output);
}

function noiseGenerator(seed) {
    let state = seed >>> 0;
    return () => {
        state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
        return state / 0x80000000 - 1;
    };
}

function music() {
    const bpm = 128;
    const beat = 60 / bpm;
    const bar = beat * 4;
    const bars = 8;
    const duration = bar * bars;
    const roots = [40, 36, 43, 38, 40, 36, 43, 38];
    const chords = [[0, 3, 7], [0, 4, 7], [0, 4, 7], [0, 3, 7]];
    const lead = [12, 15, 19, 22, 19, 15, 14, 17, 21, 24, 21, 17, 12, 14, 15, 19];
    const noise = noiseGenerator(0x544d4154);
    wav("orbital-warden-loop.wav", duration, (time) => {
        const barIndex = Math.min(bars - 1, Math.floor(time / bar));
        const barTime = time % bar;
        const beatIndex = Math.floor(barTime / beat);
        const beatTime = barTime % beat;
        const eighth = beat / 2;
        const eighthTime = time % eighth;
        const step = beat / 4;
        const stepIndex = Math.floor(time / step);
        const stepTime = time % step;
        const root = roots[barIndex];
        const chord = chords[barIndex % chords.length];

        const bassEnvelope = decay(eighthTime, 5.5) * Math.min(1, eighthTime * 80);
        const bassPhase = time * midi(root - 12);
        let value = triangle(bassPhase) * bassEnvelope * 0.20;
        value += square(bassPhase, 0.46) * bassEnvelope * 0.055;

        const arpNote = root + 12 + chord[stepIndex % chord.length];
        const arpEnvelope = decay(stepTime, 13) * Math.min(1, stepTime * 120);
        value += square(time * midi(arpNote), 0.28) * arpEnvelope * 0.105;

        if (barIndex >= 2) {
            const leadNote = root + lead[stepIndex % lead.length];
            const leadEnvelope = decay(stepTime, 7.2) * Math.min(1, stepTime * 90);
            const vibrato = Math.sin(time * TAU * 5.2) * 0.004;
            value += square(time * midi(leadNote) + vibrato, 0.18)
                * leadEnvelope * (barIndex >= 6 ? 0.115 : 0.085);
        }

        const kickFrequency = 47 + 95 * decay(beatTime, 18);
        value += Math.sin(TAU * kickFrequency * beatTime)
            * decay(beatTime, 15) * (beatIndex === 0 || beatIndex === 2 ? 0.34 : 0.13);
        if (beatIndex === 1 || beatIndex === 3) {
            value += noise() * decay(beatTime, 18) * 0.15;
            value += Math.sin(TAU * 176 * beatTime) * decay(beatTime, 22) * 0.055;
        }
        value += noise() * decay(eighthTime, 48) * 0.052;
        return value;
    });
}

music();

wav("short-laser.wav", 0.16, (time) => {
    const frequency = 1180 - time * 3600;
    return (square(time * frequency, 0.28) * 0.36
        + Math.sin(TAU * time * frequency * 1.5) * 0.20) * decay(time, 18);
});

wav("homing-missile.wav", 0.30, (time) => {
    const frequency = 180 + time * 1500;
    return (square(time * frequency, 0.36) * 0.20
        + Math.sin(TAU * time * frequency * 0.5) * 0.16) * decay(time, 6);
});

wav("ricochet-ball.wav", 0.22, (time) => {
    const frequency = 520 + Math.sin(time * TAU * 18) * 90;
    return (triangle(time * frequency) * 0.30
        + Math.sin(TAU * time * frequency * 2) * 0.14) * decay(time, 10);
});

{
    const noise = noiseGenerator(0x4c415345);
    wav("long-laser.wav", 0.62, (time, duration) => {
        const ignition = decay(time, 34);
        const attack = Math.min(1, time / 0.018);
        const release = Math.min(1, (duration - time) / 0.13);
        const sustain = attack * release;
        const sweep = 238 - 92 * Math.min(1, time / 0.22);
        const pulse = 0.78 + 0.22 * square(time * 31, 0.58);
        const body = square(time * sweep, 0.38) * 0.22
            + triangle(time * sweep * 0.5) * 0.19
            + Math.sin(TAU * time * sweep * 2.01) * 0.12;
        const snap = Math.sin(TAU * time * (1320 - time * 8200)) * ignition * 0.38
            + noise() * ignition * 0.20;
        const tail = Math.sin(TAU * time * 74) * decay(time, 2.4) * 0.12;
        return snap + (body * pulse + tail) * sustain;
    });
}

wav("pulse-bullet.wav", 0.11, (time) => {
    const frequency = 310 + 680 * decay(time, 16);
    return (square(time * frequency, 0.22) * 0.26
        + triangle(time * frequency * 0.5) * 0.12) * decay(time, 24);
});

{
    const noise = noiseGenerator(0x4558504c);
    wav("explosion.wav", 0.58, (time) => {
        const body = noise() * decay(time, 5.8);
        const rumble = Math.sin(TAU * time * (84 - time * 58)) * decay(time, 6.5);
        return body * 0.42 + rumble * 0.30;
    });
}

wav("level-up.wav", 0.92, (time) => {
    const notes = [64, 67, 71, 76, 79];
    const index = Math.min(notes.length - 1, Math.floor(time / 0.14));
    const local = time - index * 0.14;
    const envelope = local >= 0 ? decay(local, 5.2) * Math.min(1, local * 70) : 0;
    return (square(time * midi(notes[index]), 0.25) * 0.20
        + triangle(time * midi(notes[index]) * 0.5) * 0.13) * envelope;
});

{
    const noise = noiseGenerator(0x48495421);
    wav("player-hit.wav", 0.34, (time) => {
        const frequency = 190 - time * 330;
        return (square(time * frequency, 0.5) * 0.26
            + noise() * 0.24) * decay(time, 9);
    });
}

wav("card-select.wav", 0.28, (time) => {
    const envelope = decay(time, 8) * Math.min(1, time * 80);
    return (square(time * midi(72), 0.25)
        + square(time * midi(76), 0.25)
        + square(time * midi(79), 0.25)) * envelope * 0.10;
});
