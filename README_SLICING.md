# srsRAN Project with Closed-Loop Dynamic Slice Control

This repository is a modified version of the srsRAN Project (since archived and succeeded by OCUDU), accompanying our work on an open-source 5G testbed with dynamic network slicing.

**Base version:** srsRAN Project commit `3ed363dabf2f8176bd60bc1d3bfcba14e5ae2c6f` (upstream `main`).
**Branch:** `slicing-e2sm-rc` | **Release tag:** `srsRAN-dynamic-slicing-final`

## What this fork adds

The base version already provides statically configured slices and an E2SM-RC slice-level PRB quota control action. This fork adds what an xApp needs to close the control loop:

1. **UE-to-slice context reporting (E2SM-RC):** a new RC report service exports the UE-to-slice (S-NSSAI) mapping derived from DRB QoS parameters.
2. **Per-slice PRB reporting (E2SM-KPM):** scheduler metrics are tagged by S-NSSAI, so PRB utilization is reported per slice.
3. **Runtime slice quota control and enforcement:** cell-level slice quota requests from the xApp, and a runtime slice resource manager in the MAC scheduler that applies minimum, maximum and dedicated PRB ratios, clamps slices against a live per-slice RB budget, and rebinds active UEs after a slice reconfiguration.

To see exactly what changed:

    git diff 3ed363dabf2f8176bd60bc1d3bfcba14e5ae2c6f..srsRAN-dynamic-slicing-final --stat

**Note:** every S-NSSAI used by a UE must have a configured slice. UEs whose S-NSSAI has no configured slice are not scheduled in the default slice.

## Companion repository (xApp and testbed)

The graph-based xApp, dashboard and testbed scripts are in: https://github.com/udula27/oran-sc-ric-dynamic-slicing

## Setting up the full closed-loop testbed

This repository contains only the gNB side. A complete setup has four parts, started in this order: 5G core, near-RT RIC, gNB, and the xApp with its dashboard. The physical UEs are attached last.

Base code: srsRAN Project commit 3ed363dabf. Tagged release: srsRAN-dynamic-slicing-final.

### Build

Build as for upstream srsRAN Project (CMake). See https://docs.srsran.com/projects/project for the upstream build instructions. The gNB binary is written to build/apps/gnb/gnb.

### 1. 5G core (Open5GS)

Install Open5GS from upstream (https://github.com/open5gs/open5gs) and run it on bare metal, not in Docker. Configure it to match the RAN:

- PLMN: MCC 001, MNC 01 (PLMN 00101)
- Tracking area code: 7
- Three slices, each with SD 0:
  - SST 1: eMBB, DNN srsapn
  - SST 2: URLLC, DNN emergency
  - SST 3: mMTC, DNN iot
- One UPF and one DNN per slice.
- Add the test subscriber using your own values. Do not record real IMSI, K, or OPc values in this repository.

The AMF address the gNB connects to is 127.0.0.5.

### 2. Near-RT RIC (O-RAN SC)

Clone and start the RIC from https://github.com/udula27/oran-sc-ric-dynamic-slicing:

    cd oran-sc-ric
    docker compose up -d

The E2 termination listens on SCTP port 36421. The gNB's e2 section must point to the RIC address.

### 3. gNB

Create your working configuration from the sample, then replace the placeholders <N310_IP>, <RIC_IP>, and <gNB_IP> with your lab addresses:

    cp N310_example.yml N310.yml

The PLMN, TAC, and SST values must match the core configuration above.

Start the gNB from the repository root, using an absolute path to your configuration:

    sudo ./build/apps/gnb/gnb -c /absolute/path/to/N310.yml

If the shell reports "Permission denied", make the binary executable once with chmod +x build/apps/gnb/gnb.

The sample configuration sets log levels to warning. The output below is what the binary prints at startup. The AMF and E2 confirmation lines are logged at info level and appear only when all_level and e2ap_level are set to info in the configuration:

    --== srsRAN gNB (commit 3ed363dabf) ==--

    Available radio types: uhd.
    Cell pci=1, bw=60 MHz, 1T1R, dl_arfcn=627340 (n78), dl_freq=3410.1 MHz, dl_ssb_arfcn=<value>, ul_freq=<value> MHz

    ==== gNB started ===
    Type <h> to view help

The commit hash, antenna count, and frequency fields depend on your build and may differ from this example. The output is similar, not identical.

Logs are written to /tmp/gnb.log.

Look for the lines shown above in the terminal output. The AMF and E2 confirmation lines appear only at info level.

### 4. xApp and dashboard

Start the slicing controller and its Streamlit dashboard:

    cd oran-sc-ric/xApps/python
    ./run_test_xapp_3_docker_dual_venv.sh

The dashboard is at http://localhost:8503. The alternate Dash dashboard uses run_test_xapp_3_docker_dual_venv_dash.sh on port 8504.

The controller prints the E2 node ID it uses. It must match the node name the RIC reports in rtmgr_sim logs (the ranNamelist value). The default is gnbd_001_001_00019b_0; if your node name differs, pass it with --e2_node_id.

### 5. UEs (physical devices)

Attach the physical UEs last, after the gNB, RIC, and xApp are running.

Configure each UE for 5G SA with network mode set to 5G Only or 5G Preferred. Add each UE's SIM details to the subscriber database used by the core network before attaching it.

### Shutdown

Stop the UEs, then the xApp and dashboard, then the gNB (Ctrl+C). Run docker compose down in oran-sc-ric, then stop the core network.

## License

This is a derivative work and keeps the upstream license. See the `LICENSE` file, which is unchanged, and the original copyright headers.
