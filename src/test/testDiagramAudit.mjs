import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { resolve } from "node:path";

const [cliArg, fontArg, goodArg, failureArg, pathsArg, autoRouteArg, sampledArg] = process.argv.slice(2);
if (!cliArg || !fontArg || !goodArg || !failureArg || !pathsArg || !autoRouteArg || !sampledArg) {
    throw new Error(
        "Usage: testDiagramAudit.mjs <viscript> <font> <good> <failure> <paths> <auto-route> <sampled>",
    );
}

const cli = resolve(cliArg);
const font = resolve(fontArg);
const good = resolve(goodArg);
const failure = resolve(failureArg);
const paths = resolve(pathsArg);
const autoRoute = resolve(autoRouteArg);
const sampled = resolve(sampledArg);

function run(scene, extra = [], expected = 0) {
    const result = spawnSync(cli, ["audit", scene, "--font", font, ...extra], {
        encoding: "utf8",
        maxBuffer: 32 * 1024 * 1024,
    });
    assert.equal(result.status, expected, result.stderr || result.stdout);
    return result;
}

const passed = JSON.parse(run(good).stdout);
assert.equal(passed.valid, true);
assert.equal(passed.diagramIR.length, 1);
const receipt = passed.diagramIR[0];
assert.equal(receipt.kind, "vis.diagram.compile-receipt");
assert.equal(receipt.version, 1);
assert.equal(receipt.layoutAlgorithm, "vis.diagram.layout/v2");
assert.equal(receipt.scenePath, "root");
assert.equal(receipt.root.id, "request-flow:root");
assert.ok(receipt.byteSize > 0);
assert.equal(receipt.config.id, "request-flow");
assert.deepEqual(receipt.nodes.map((node) => node.id), ["input", "transform", "output"]);
assert.deepEqual(receipt.edges.map((edge) => edge.id), ["decode", "encode"]);
assert.equal(receipt.edges[0].fromNodeId, "input");
assert.equal(receipt.edges[0].toNodeId, "transform");
assert.equal(receipt.edges[0].placement.fromPort, "right");
assert.equal(receipt.edges[0].placement.toPort, "left");
assert.equal(receipt.constraints.valid, true);
assert.deepEqual(receipt.constraints.issues, []);

const generated = passed.policy.containmentLedger.relationships.filter((relationship) => {
    return relationship.provenance?.kind === "diagram-ir";
});
assert.deepEqual(generated.map((relationship) => relationship.textId), [
    "request-flow:node:input:label",
    "request-flow:node:transform:label",
    "request-flow:node:output:label",
]);
assert.deepEqual(generated[0].provenance, {
    kind: "diagram-ir",
    scenePath: "root",
    diagramId: "request-flow",
    entityKind: "node",
    entityId: "input",
    role: "label",
});

const overridden = JSON.parse(run(good, [
    "--require-containment-ledger",
    "--allow-uncontained", "request-flow:node:input:label",
]).stdout);
assert.equal(overridden.valid, true);
assert.ok(!overridden.policy.containmentLedger.relationships.some((relationship) => {
    return relationship.textId === "request-flow:node:input:label";
}));
assert.deepEqual(overridden.policy.containmentLedger.uncontained, [{
    id: "request-flow:node:input:label",
    matched: true,
}]);

const failedFirst = run(failure, ["--panel-inset", "0"], 1);
const failedSecond = run(failure, ["--panel-inset", "0"], 1);
assert.equal(failedFirst.stdout, failedSecond.stdout, "Diagram audit JSON must be deterministic");
const failed = JSON.parse(failedFirst.stdout);
assert.equal(failed.valid, false);
assert.equal(failed.diagramIR.length, 1);
const failedReceipt = failed.diagramIR[0];
assert.equal(failedReceipt.config.id, "layout-failure");
assert.equal(failedReceipt.constraints.valid, false);
assert.deepEqual(failedReceipt.constraints.issues.map((issue) => issue.code), [
    "diagram_node_overlap",
    "diagram_route_crosses_node",
    "diagram_edge_endpoint_direction",
    "diagram_edge_endpoint_direction",
]);
assert.deepEqual(failedReceipt.constraints.issues[0], {
    code: "diagram_node_overlap",
    nodeId: "first",
    relatedNodeId: "second",
    edgeId: null,
    zoneId: null,
    endpoint: "none",
    port: "auto",
    repair: "separate-diagram-nodes",
});
assert.equal(failedReceipt.constraints.issues[1].edgeId, "crossing");
assert.equal(failedReceipt.constraints.issues[1].nodeId, "blocker");
assert.deepEqual(failedReceipt.constraints.issues.slice(2).map((issue) => ({
    edgeId: issue.edgeId,
    nodeId: issue.nodeId,
    endpoint: issue.endpoint,
    port: issue.port,
    repair: issue.repair,
})), [
    {
        edgeId: "wrong-direction",
        nodeId: "direction-source",
        endpoint: "from",
        port: "top",
        repair: "align-diagram-route-with-port",
    },
    {
        edgeId: "wrong-direction",
        nodeId: "direction-target",
        endpoint: "to",
        port: "top",
        repair: "align-diagram-route-with-port",
    },
]);

const staticIssues = failed.issues.filter((issue) => issue.provenance?.kind === "diagram-ir");
assert.deepEqual(staticIssues.map((issue) => issue.code), [
    "diagram_node_overlap",
    "diagram_route_crosses_node",
    "diagram_edge_endpoint_direction",
    "diagram_edge_endpoint_direction",
]);
assert.ok(staticIssues.every((issue) => issue.firstSample === null));
assert.equal(staticIssues[1].subject.id, "layout-failure:edge:crossing");
assert.equal(staticIssues[1].related.id, "layout-failure:node:blocker");
assert.deepEqual(staticIssues[1].provenance, {
    kind: "diagram-ir",
    diagramId: "layout-failure",
    subject: {kind: "edge", id: "crossing"},
    related: {kind: "node", id: "blocker"},
});
assert.equal(staticIssues[1].repair, "reroute-or-move-diagram-node");

const scoped = JSON.parse(run(paths).stdout);
assert.equal(scoped.valid, true);
assert.deepEqual(scoped.diagramIR.map((item) => [item.scenePath, item.config.id]), [
    ["root", "shared-diagram"],
    ["root/viewport:0", "shared-diagram"],
]);
assert.equal(scoped.diagramIR[0].root.id, scoped.diagramIR[1].root.id,
    "scenePath must disambiguate intentionally repeated semantic IDs");
assert.deepEqual(scoped.policy.containmentLedger.relationships.map((relationship) => {
    return [relationship.provenance.scenePath, relationship.provenance.entityId];
}), [
    ["root", "node"],
    ["root/viewport:0", "node"],
]);

const sampledFirst = run(sampled, [], 1);
const sampledSecond = run(sampled, [], 1);
assert.equal(sampledFirst.stdout, sampledSecond.stdout,
    "Sampled Diagram geometry JSON must be deterministic");
const sampledReport = JSON.parse(sampledFirst.stdout);
assert.equal(sampledReport.valid, false);
assert.equal(sampledReport.sampling.encodedFrames, 60);
assert.equal(sampledReport.sampling.samples, 61);
assert.equal(sampledReport.thresholds.diagramRouteGap, 2);
assert.equal(sampledReport.diagramIR[0].constraints.valid, true,
    "Static Diagram IR must remain separate from rendered geometry failures");
const physicalIssues = sampledReport.issues.filter((issue) => {
    return issue.provenance?.kind === "sampled-rendered-geometry";
});
assert.deepEqual(new Set(physicalIssues.map((issue) => issue.code)), new Set([
    "diagram_sampled_route_node_collision",
    "diagram_sampled_route_label_collision",
    "diagram_sampled_route_route_collision",
    "diagram_sampled_route_canvas_overflow",
]));
const nodeCollision = physicalIssues.find((issue) => {
    return issue.code === "diagram_sampled_route_node_collision" &&
        issue.provenance.related?.id === "blocker";
});
assert.ok(nodeCollision);
assert.equal(nodeCollision.firstSample.kind, "frame");
assert.ok(nodeCollision.firstSample.index > 0 && nodeCollision.lastSample.index < 20,
    "A transient collision must not be confused with the settled endpoints");
assert.equal(nodeCollision.provenance.diagramId, "sampled-failure");
assert.deepEqual(nodeCollision.provenance.subject, {kind: "edge", id: "horizontal"});
assert.deepEqual(nodeCollision.provenance.related, {kind: "node-body", id: "blocker"});
assert.equal(nodeCollision.related.type, "rectangle");
assert.ok(nodeCollision.measurement.actual < nodeCollision.measurement.required);
assert.ok(nodeCollision.bounds.subject.width > 0 && nodeCollision.bounds.related.width > 0);
const labelCollision = physicalIssues.find((issue) => {
    return issue.code === "diagram_sampled_route_label_collision";
});
assert.ok(labelCollision);
assert.equal(labelCollision.provenance.related.kind, "node-label");
assert.equal(labelCollision.related.type, "text");
const routeCollision = physicalIssues.find((issue) => {
    return issue.code === "diagram_sampled_route_route_collision";
});
assert.ok(routeCollision);
assert.deepEqual(routeCollision.provenance.related, {kind: "edge-route", id: "vertical"});
const canvasOverflow = physicalIssues.find((issue) => {
    return issue.code === "diagram_sampled_route_canvas_overflow";
});
assert.ok(canvasOverflow);
assert.equal(canvasOverflow.related, null);
assert.equal(canvasOverflow.provenance.related, null);
assert.equal(canvasOverflow.repair, "move-diagram-route-inside-canvas");
assert.equal(JSON.parse(run(sampled, ["--diagram-route-gap", "0"], 1).stdout)
    .thresholds.diagramRouteGap, 0);

const routedFirst = run(autoRoute);
const routedSecond = run(autoRoute);
assert.equal(routedFirst.stdout, routedSecond.stdout,
    "Automatic Diagram routing must produce deterministic audit JSON");
const routed = JSON.parse(routedFirst.stdout);
assert.equal(routed.valid, true);
assert.equal(routed.diagramIR.length, 1);
const routedReceipt = routed.diagramIR[0];
assert.equal(routedReceipt.layoutAlgorithm, "vis.diagram.layout/v2");
assert.equal(routedReceipt.constraints.valid, true);
assert.deepEqual(routedReceipt.constraints.issues, []);
assert.equal(routedReceipt.edges[0].route, "auto");
assert.equal(routedReceipt.edges[0].placement.route, "orthogonal");
assert.ok(routedReceipt.edges[0].placement.centerline.length >= 4);
const routeClearance = 1 + Math.max(
    routedReceipt.config.routeWidth,
    routedReceipt.config.arrowWidth,
) / 2 + routedReceipt.config.routeWidth;
assert.ok(routedReceipt.edges[0].placement.centerline.some((point) => {
    return Math.abs(point.y) > routeClearance;
}));
const routedPoints = routedReceipt.edges[0].placement.centerline;
const routedLast = routedPoints.length - 1;
assert.ok(Math.hypot(
    routedPoints[routedLast].x - routedPoints[routedLast - 1].x,
    routedPoints[routedLast].y - routedPoints[routedLast - 1].y,
) > routedReceipt.config.arrowLength);

console.log("viscript Diagram audit tests passed");
