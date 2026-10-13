# Orbital Warden audio

These WAV files are deterministic, repository-generated 8-bit-style game audio.
They contain no third-party samples or external runtime dependencies.

Regenerate every asset with:

```sh
node generate-game-audio.mjs
```

The looping track is 8 bars at 128 BPM. Effects are mono, 22.05 kHz, 16-bit PCM
so the complete template remains small and decodes in both miniaudio and WebAudio.
