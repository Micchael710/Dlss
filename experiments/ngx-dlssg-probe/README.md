# NGX stock Vulkan capability probe

Standalone Windows x64 capability measurement. This executable is isolated from
Minecraft, the mods, their native bridges and PresentWorker. No surfaces, swapchains,
images, Evaluate, frame generation or presentation are implemented.

Uses public NGX API with a CUSTOM engine and a project UUID assigned to this probe.
Selects the NVIDIA RTX 3050 Ti explicitly, records device UUID/LUID/driver, initializes
NGX on the created Vulkan device, queries the capability map, and saves each getter's
return code independently of its value. An absent parameter remains null; no hardcoded
maximum or silent numeric fallback. Available=0 is a completed measurement, not a probe failure.

The runtime is the signed stock NVIDIA DLL at pinned NVIDIA/DLSS commit
374959484e79a640feaba44c93ac8cfb0a03f5b5. vendor/manifest.json records provenance/hash.
The runner verifies the staged DLL hash/signature before execution. Library/DLL assets
are local experiment dependencies, excluded from versioning, and not packaged into mods.
NVIDIA license accompanies the local SDK assets. No third-party proxy is loaded.

Reproduce from D:/ProjectDllsss using the existing installed Python/Visual Studio:

1. `tools/stage_ngx_stock_probe.py` stages the pinned official assets.
2. `tools/build_ngx_stock_probe.ps1` builds only this standalone executable.
3. `tools/run_ngx_stock_probe.ps1` verifies provenance and runs with a new timestamped
   output directory under logs/runtime/ngx-stock-probe.
4. `tools/summarize_ngx_stock_probe.py OUTPUT_DIRECTORY` checks the real result and
   produces result.json separating stock, adapted and hook observations.

First actual run: logs/runtime/ngx-stock-probe/20261003-141624.
Query completed; stock Available=0, NeedsUpdatedDriver=0, max getter UnsupportedParameter.
No CreateFeature: availability gate failed. No multipliers validated.

The SM86 scenario is NOT RUN. A public external loading contract is now documented;
private backend ABI investigation is closed. The isolated adapted-harness design is in
docs/DLSSG_EXTERNAL_HARNESS_DESIGN.md. Do not change this stock runner to accept a community
DLL. Adapted queries must remain labeled reported/potentially hooked; if the original
adapted value cannot be observed, preserve null rather than claim an unhooked measurement.
The stock gate and AMD x2 remain unchanged; no generation was added to this probe.

NGX can inspect its existing driver/cache fallback paths; the callback log and module
snapshot document the actually attempted/chosen runtime paths rather than assuming a
candidate filename implies the runtime used. No cache or global profile is manually edited.
