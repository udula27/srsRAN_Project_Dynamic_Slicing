# srsRAN Project with Closed-Loop Dynamic Slice Control (CCNC 2027)

This repository is a modified version of the srsRAN Project (since archived and succeeded by OCUDU), accompanying the paper:

> *An Open-Source 5G Testbed with Dynamic Network Slicing: Design, Implementation, and Performance Evaluation*, IEEE CCNC 2027.

**Base version:** srsRAN Project commit `3ed363dabf2f8176bd60bc1d3bfcba14e5ae2c6f` (upstream `main`).
**Branch:** `slicing-e2sm-rc` | **Release tag:** `ccnc2027-camera-ready`

## What this fork adds

The base version already provides statically configured slices and an E2SM-RC slice-level PRB quota control action. This fork adds what an xApp needs to close the control loop:

1. **UE-to-slice context reporting (E2SM-RC):** a new RC report service exports the UE-to-slice (S-NSSAI) mapping derived from DRB QoS parameters.
2. **Per-slice PRB reporting (E2SM-KPM):** scheduler metrics are tagged by S-NSSAI, so PRB utilization is reported per slice.
3. **Runtime slice quota control and enforcement:** cell-level slice quota requests from the xApp, and a runtime slice resource manager in the MAC scheduler that applies minimum, maximum and dedicated PRB ratios, clamps slices against a live per-slice RB budget, and rebinds active UEs after a slice reconfiguration.

To see exactly what changed:

    git diff 3ed363dabf2f8176bd60bc1d3bfcba14e5ae2c6f..ccnc2027-camera-ready --stat

**Note:** every S-NSSAI used by a UE must have a configured slice. UEs whose S-NSSAI has no configured slice are not scheduled in the default slice.

## Companion repository (xApp and testbed)

The graph-based xApp, dashboard and testbed scripts are in: https://github.com/udula27/oran-sc-ric-dynamic-slicing

## Build and run

Build as for upstream srsRAN Project (CMake). See the upstream documentation: https://docs.srsran.com/projects/project

## License

This is a derivative work and keeps the upstream license. See the `LICENSE` file, which is unchanged, and the original copyright headers.
