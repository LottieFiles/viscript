# Source grounding for diagrams

Read this when a diagram makes claims about source code, revisions, runtime behavior, papers, AI systems, accelerator execution, distributed/HPC topology, or performance.

## Build an evidence ledger

Fix the inspected revision, input or scenario, configuration, and reader question before layout. For each claim record:

- the source anchor or measurement that supports it;
- whether it is verified statically, observed at runtime, derived, illustrative, possible, or unresolved;
- the entities and relation it introduces;
- the view or beat in which it appears.

Keep source facts, runtime observations, stated intent, and inference in separate lanes. A diagram may simplify evidence, but it must not silently promote a possible path to an observed path, a derived quantity to a measured value, or an illustrative placement to physical topology.

## Ground code and revision views

Inspect the entry point, construction or registration, decisive calls and dispatch, state owners, error and cleanup paths, thread or queue boundaries, and the tests or traces that constrain the selected behavior. Choose one representative scenario and follow its causal handoffs. Use stable identifiers so the same object, queue, handle, or state remains recognizable across views.

Distinguish types from runtime instances; ownership from borrowing or sharing; synchronous calls from asynchronous messages; requests, returns, errors, events, and state transitions; and total order from partial order. Anchor every node, relation, and motion beat to evidence or label it as derived, illustrative, possible, or unresolved.

For a diff, inspect exact before and after revisions plus the unchanged surrounding code that gives the change meaning. Compare the same scenario on both sides, identify the first behavioral divergence, and preserve uncertainty about intent unless a commit message, issue, test, comment, or review statement supports it. Do not infer deleted code from the current file or turn patch order into runtime order.

## Keep AI and HPC scales explicit

Select one primary scale for each view:

```text
model or algorithm -> operator or tensor -> kernel -> device -> process or rank -> node -> cluster
```

Bridge adjacent scales only when evidence defines the mapping. Do not mix model semantics, kernel scheduling, device memory, and cluster placement in one unlabeled plane.

For distributed placement, keep separate layers for model/data decomposition, logical tasks or ranks, runtime processes, devices, physical nodes and interconnect, and storage. A rank is not automatically a process, device, or node. Show sharding, replication, ownership, lifetime, and residency only when supported.

Name a collective by its verified semantics—broadcast, reduce, all-reduce, reduce-scatter, all-gather, all-to-all, or point-to-point. Do not draw a ring, tree, routing algorithm, or physical fabric unless the source establishes it. Label logical communication separately from physical links.

For schedules, use one time direction and show owner, operation, dependency, and synchronization. Mark overlap as observed only when a trace proves it; otherwise describe it as possible. Identify a critical path only from supported dependencies and durations.

## Handle quantitative evidence

For every value retain its unit, scope, aggregation, workload, hardware, software revision, and measurement conditions. Classify it as theoretical, specified, derived, profiled, or benchmarked. Align comparable values to a shared scale and do not imply comparability when conditions differ. Show missing or unresolved values honestly rather than filling the composition with invented estimates.

## Review the claim-to-mark mapping

Before delivery, trace each visible node, edge, label, state, placement, and measurement back to the ledger. Verify that relation style matches its declared meaning, concurrency is not serialized for convenience, placement does not collapse logical and physical layers, and the final still preserves the supported conclusion and its uncertainty. Put revision and evidence caveats in the delivery summary when they would overload the diagram itself.
