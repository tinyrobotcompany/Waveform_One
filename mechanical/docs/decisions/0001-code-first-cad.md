# 0001 — build123d as the CAD source of truth; Fusion as viewer

Date: 2026-09-26
Status: Accepted (owner approved delegating the choice)

## Context

The mechanical PRD asks for fully parametric, code-first CAD kept in Git, with
Autodesk Fusion as the authoritative environment driven by its Python API.

The model is written and iterated by a coding agent. Fusion scripts only run
inside the Fusion desktop application, so every rebuild would require driving
the GUI, and automated tests cannot inspect the result. Script-generated Fusion
geometry is also largely non-parametric in Fusion's own timeline, so the main
advantage of Fusion (interactive parametric editing) is mostly lost.

## Decision

- Geometry is generated with **build123d** (Python, OpenCascade kernel) under
  `mechanical/cad/`.
- Tests assert real solid geometry: dimensions, datum placement, print-envelope
  fit, and (later) collisions and wall thickness.
- Exports: STEP (archive/exchange, opened in Fusion for review and rendering),
  3MF with one body per colour zone for the CANVAS multi-colour workflow, and STL.
- Fusion remains the visual review and rendering tool.

## Consequences

- Rebuilds, validation and exports run headless and reproducibly.
- PRD references to "Fusion script", "Fusion component" and "Fusion document"
  map to build123d modules, labelled sub-shapes of the root compound, and the
  exported STEP respectively. Milestones and acceptance criteria are otherwise
  unchanged.
- Fusion's default Y-up orientation must be switched to Z-up when viewing.
- Anyone editing geometry by hand in Fusion must port the change back to code,
  or it is lost on the next rebuild.
