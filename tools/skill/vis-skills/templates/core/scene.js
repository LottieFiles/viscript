import {createVis, vis} from "./client.js";

const definition = vis.scene({camera: {height: 7}});

const figure = definition.group({id: "figure"});
const subject = figure.circle({id: "subject"});
figure.text({text: "viscript", role: "h2", id: "subject-label"});

definition
    .fadeIn(figure, {scale: 0.97, duration: 0.55, curve: "gentle"})
    .indicate(subject, {scale: 1.06, duration: 0.45, curve: "ease_in_out"})
    .wait(0.65);

export const scene = await createVis(definition, "scene.js");
