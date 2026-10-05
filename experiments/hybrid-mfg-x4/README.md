# Hybrid frameset generation prototype

One standalone D3D12 experiment: GPU synthetic A/B fixture; unchanged public
direct NGX count=1/index=1 integration produces G50. Our compute shader takes
only A/B color, depth and current-to-previous motion fields to produce G25/G75.
No G50 input, swapchain, presentation, Minecraft, Vulkan or OpenGL.

Infrastructure copies are adapted from `experiments/dlssg-external-harness/`:
`evidence.inc` (logging/hash-checked component loader), `d3d12_session.inc`
(public D3D12/NGX setup, barriers/fences and metadata readback), `camera.inc`
(fixture camera/NGX options). Full-frame CPU upload/readback methods removed.
GPU-generated fixture matches that harness's f3/f4 pair with 16px positive-X
motion. Four history-building warmups plus one target Evaluate are retained;
all five calls use count=1/index=1. No NVIDIA count=2 invocation.

The synthetic camera is stationary. Motion is RG32F current-to-previous clip
displacement, scaled to pixels by `(W/2,-H/2)`. A's backward flow serves as a
simple inverse-flow approximation for this translating rectangle; no optical
flow estimation or production disocclusion quality is claimed. Depth picks a
foreground flow at the output position and rejects inconsistent warped samples.
Quality remains prototype-only. See the saved motion contract for exact source.

Validation reads 128 bytes of GPU reductions and 16 bytes of NGX status only.
Two-word coordinate-aware fingerprints are diagnostic, noncryptographic;
foreground mass/X centroid verifies strict A<G25<G50<G75<B order. PASS also
requires distinct GPU resources, changed sentinels and distinct fingerprints.
No image bytes are mapped or uploaded by the CPU. Device/fence failures stop.

`build.ps1` enforces two build attempts, including all three HLSL entry points.
`run.ps1` requires the pinned local runtimes, marks its sole run before launch,
and refuses another launch when process-result.json exists. It creates logs in
`logs/research/hybrid-mfg-x4-generation/`. Runtime DLLs/build products remain
local and ignored. Never treat FRAMESET_GENERATION PASS as x4 presentation PASS.
