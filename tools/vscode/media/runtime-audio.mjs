// The scene's audio in the preview. A browser keeps audio locked until a user gesture, so the
// webview starts it from one; from then on it follows playback. Audio is optional: a runtime may
// have none, and its failures never stop the scene.

// Sounds go to the runtime's audio, the other assets are images.
export function loadSceneAsset(runtime, asset, bytes) {
    if (asset.mime.startsWith("audio/")) runtime.audio?.load(asset.name, bytes);
    else runtime.asset(asset.name, bytes, asset.mime);
}

// runtime() gives the current runtime, time() its playback time, rate() the playback speed and
// playing() whether it plays.
export function createRuntimeAudio({runtime, time, rate, playing}) {
    let started = false;
    const sync = (seek) => {
        const audio = runtime()?.audio;
        if (!started || !audio) return;
        try {
            audio.transport({time: time(), rate: rate(), playing: playing(), seek});
        } catch {
            started = false;
        }
    };
    return {
        get started() {
            return started;
        },
        start() {
            const audio = runtime()?.audio;
            if (started || !audio) return;
            try {
                audio.start();
                started = true;
                sync(true);
            } catch {
                started = false;
            }
        },
        // A new runtime's audio has not started.
        reset() {
            started = false;
        },
        sync,
        // Plays the sounds a retained frame emitted.
        play(step) {
            const audio = runtime()?.audio;
            if (!started || !audio || !step?.sounds?.length) return;
            for (const sound of step.sounds) {
                try {
                    audio.play(sound.asset, {bus: sound.bus, gain: sound.gain, rate: sound.rate, loop: sound.loop});
                } catch {
                    // A saturated optional voice pool may drop sound, never simulation.
                }
            }
        },
    };
}
