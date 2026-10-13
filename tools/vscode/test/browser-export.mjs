import {saveGif, saveMp4} from "../media/savers.js";

const width = 64;
const height = 64;
const pixels = new Uint8ClampedArray(width * height * 4);
const scene = {
    duration: 0.2,
    size: () => [width, height],
    render(time) {
        for (let index = 0; index < width * height; index += 1) {
            pixels[index * 4] = Math.round(time * 255);
            pixels[index * 4 + 1] = index % width * 4;
            pixels[index * 4 + 2] = Math.floor(index / width) * 4;
            pixels[index * 4 + 3] = 255;
        }
        return pixels;
    },
    draw(canvas, time) {
        const context = canvas.getContext("2d");
        context.fillStyle = `rgb(${Math.round(time * 255)}, 96, 180)`;
        context.fillRect(0, 0, width, height);
    },
};

try {
    const gif = await saveGif(scene, 10);
    const mp4 = await saveMp4(scene, 10);
    if (gif.length < 20 || mp4.byteLength < 100) {
        throw new Error("Encoded output is unexpectedly empty");
    }
    document.body.dataset.result = "passed";
    document.body.textContent = `passed gif=${gif.length} mp4=${mp4.byteLength}`;
} catch (error) {
    document.body.dataset.result = "failed";
    document.body.textContent = `failed ${error instanceof Error ? error.message : String(error)}`;
}
