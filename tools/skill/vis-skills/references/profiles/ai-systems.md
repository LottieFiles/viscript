# AI systems evidence

Use this profile when model semantics, tensor meaning, operator execution, accelerator mapping, distributed placement, or performance quantities must remain technically exact. Preserve the named paper, implementation, configuration, platform, and measurement source before styling.

## Ground every claim

Establish a source hierarchy appropriate to the request: primary paper and supplement, official implementation or configuration, framework or platform documentation, and measured trace or benchmark. Classify each visible claim as sourced, derived, illustrative, or unknown. Do not promote a familiar architecture, typical dimension, or plausible device behavior into a source fact.

Name the exact variant and the semantic scale being explained. Use the smallest connected span of:

```text
model -> stage/block -> operator/tensor -> kernel/tile -> device -> node/cluster
```

Bridge adjacent levels through an explicit mapping. Split overview and detail for a larger jump; do not place model blocks, CUDA thread blocks, and physical devices in one untyped flow.

Build only the ledgers needed by the claim:

- tensor shape, axis meaning, dtype, layout, residency, and identity;
- operator inputs, outputs, parameter sharing, normalization, masking, and dependencies;
- kernel work ownership, tile mapping, memory movement, synchronization, and schedule;
- ranks, devices, shards, collectives, topology, and physical placement;
- assumptions and derivation for parameters, FLOPs, bytes, communication, time, utilization, or scaling;
- provenance for each visible value and relation.

## Preserve model and execution semantics

Retain layer order, normalization placement, residual or recurrent paths, masking, grouping, sharing, cache behavior, and training-versus-inference differences that affect the claim. Show repeated depth with exact multiplicity or state that it varies. Ellipses and folded stacks may compress repetition but may not conceal a necessary branch or transition.

Every tensor-changing edge must declare enough shape, axis, layout, placement, or operation detail to answer the question. Distinguish activation, weight, gradient, optimizer state, token, cache, collective, control, and synchronization when they carry different semantics.

Distinguish a learned convolution filter, a framework operator implementation, and a launched accelerator kernel. Keep CUDA programming abstractions separate from hardware scheduling: grids contain blocks and blocks contain threads, while warp or wave issue, SM residency, cache behavior, Tensor Core use, and interconnect routing require named implementation or platform evidence. Do not universalize warp width, capacity, bandwidth, topology, limits, or collective algorithms.

Distinguish mathematical parallelism, executable concurrency, and physical placement. Parallel-looking lanes do not prove temporal overlap, and colocated boxes do not prove shared memory or synchronization scope. Type communication edges and show the dependency or schedule that supports an overlap claim.

## Quantitative and learned evidence

Derive shapes, parameter counts, FLOPs, byte traffic, communication volume, and timing from displayed assumptions. Label theoretical, specified, derived, profiled, benchmarked, estimated, and illustrative values accurately. Occupancy is not utilization, peak throughput is not achieved throughput, and arithmetic intensity alone is not a measured speedup. Use shared axes and exact-value labels for comparisons that support a quantitative conclusion.

Attention or another learned relation requires actual weights, activations, or source-supported illustrative values clearly labeled as such. A decorative connection pattern is not learned evidence. Keep token, head, channel, shard, stage, rank, and memory-space distinctions readable without color alone.

## Motion and acceptance

Animate only sequence, causality, reuse, reduction, synchronization, generation, communication, or overlap. Move the actual tensor slice, token, tile, shard, cache entry, gradient, message, or schedule interval named by the explanation. Keep topology stable when topology change is not the subject; avoid generic data rain or glowing particles.

- The represented variant and every visible model path match the source.
- Tensor and operator identities remain consistent across scale changes.
- Kernel, device, rank, memory, and communication mappings are typed and sourced.
- Every quantitative result is recomputable and carries a provenance category.
- The final still exposes the architecture or mechanism, important identities, assumptions, and conclusion without replay.
- Unverified platform behavior or unavailable measurements are reported explicitly.
