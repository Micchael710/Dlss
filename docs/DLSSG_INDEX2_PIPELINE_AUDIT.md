# Index2 pipeline audit — 2026-10-04

Historical Java25 run closed: 20261004-045826-216. New instrumentation is not
claimed as observed in that run. No root-cause fix is asserted.

COMPLETION_MODEL=GROUP_ORDERED_COMMAND_LIST

`bridge.cpp` records N Evaluate calls and N status copies in one slot command
list, then executes it after GPU Wait(ready) and signals done. The callback
checks completed>=done and rejects UINT64_MAX, maps each independent readback,
and passes an N-element JNI int[] with frameId/done. Java requires exactly that
frameId, group done and count. An index1-only completion cannot authorize
index2: no such independent completion is issued. The group's signal orders all
recorded commands; it does not prove that NGX wrote every optional output.
Vulkan's output wait uses the same done. Its submission follows the inputs on
the borrowed queue, preventing the next ready value from bypassing the previous
output wait. Native retirement verifies done, callback readiness and device reason.

## Twenty requested checks

1. Backend mode count is copied into Pool; C++ loops k=0..count-1. Historical
   external backend log confirms count2/index2, not merely Java intent.
2. opts.multiFrameCount=Pool.generatedCount (historical observed count2).
3. opts.multiFrameIndex=k+1; public helper uploads it; observed external index2.
4. Same feature, BackbufferFrameID, constants and four input images for the loop;
   inputs copied once before group. Prior A is feature history, not a generated image.
5. Slot images[4+k] are independently created; historical VkImages differ.
6. Java slot remains leased through provider/Vulkan/present retirement; no reuse
   before release. Pool exhaustion fails rather than overwriting a live slot.
7. Each image owns independent committed D3D12 resource and imported VkImage.
8. disable[k], disableReadback[k] and flags[k] are independent vectors.
9. No scalar shared status. Shared sentinel upload is read-only initialization,
   not a result buffer; group completion is intentionally shared.
10. API k+1, Java output k, PresentWorker batch generated index k, no offset.
11. GPU sentinel copy precedes its Evaluate, with COPY_DEST/UAV transition.
12. Only one sentinel copy per index; after Evaluate only status COPY_SOURCE
    readback/restore, no clear or reinitialize. Group-end diagnostic never writes it.
13. One ordered group done covers both calls/copies. Callback uses slot done.
14. PresentWorker awaits readiness then calls per-index presentability; native
    reads only after completed>=done. release also checks callback publication.
15. JNI sends the entire vector using SetIntArrayRegion length N; no slot0 scalar.
16. Every vector is sized at count, JNI allocates flags.size().
17. Slot persists through callback; JNI target is a global reference. Local
    helper opts/ep exist through the synchronous Evaluate recording call. Public
    helper puts matrix pointers into parameters; delayed vendor CPU dereference
    after return is not proven or ruled out by closed-source implementation.
18. Result parameters reference Slot-owned GPU buffers, not local host status.
    Callback clones Java values. Matrix-pointer caveat is described in check17.
19. Every generated resource is allocated/imported before binding. New diagnostic
    records both bound IDs and public getters after Evaluate, plus getter results.
20. Helper sets OutputInterpolated and OutputDisableInterpolation for each call;
    CPU mock test verifies count1..4, fresh output/status pointers, same real input.

## Last historical confirmed boundary

Pair2: external backend count2/index2 SUCCESS, GPU group done4 retired,
Java OUTPUT_READY index2 UNKNOWN raw4294967295, discard recorded, normal exit.
Thus LAST_CONFIRMED_BOUNDARY=NGX_EVALUATE_AND_GPU_GROUP_COMPLETION.
Exact reason the sentinel remains is UNKNOWN. No claim about Java/hardware/
external component as the cause. Optional status-write semantics beyond what
public headers specify remain unverified. Pipeline static association checks
pass; vendor-internal lifetime/status-writing behavior cannot be certified.

## Bounded instrumentation and tests

First eight submitted groups only: evaluate_begin resource IDs/array slots;
evaluate_parameters public NGX getters and results; status_trace after retirement
with actual GPU statusBefore snapshot, immediate per-Evaluate status snapshot,
second group-end status snapshot, callback observed fence, read-error detail.
No extra CPU waits, no pixel transport, no rewritten flag. CPU fixtures are
explicitly mocks; they are not GPU output or x3 PASS evidence.
Java tests cover A=(0,0), B=(0,1), C=(0,ffffffff), premature reads, stale/wrong
completion, wrong interval/count, immutable snapshots and out-of-range index.
C++ fixture exercises the actual public helper's setters with mock resources.
JNI CPU smoke loads the rebuilt own DLL and calls its real abi() entry point.

Super Resolution PresentWorker/FrameGenerationWorker/FrameQueue/PresentPacer and
provider interface audited without edits. AMD classes/DLLs checked by packaging.
No DLSS SR, ray tracing, direct Vulkan DLSSG, Dzn, or multiplayer modifications.

Public contract references:
https://github.com/NVIDIA/DLSS/blob/main/include/nvsdk_ngx_params_dlssg.h
https://github.com/NVIDIA/DLSS/blob/main/include/nvsdk_ngx_helpers_dlssg_d3d.h

## New single graphics attempt: 20261004-144431-183

Fresh public getter after loader: max2, result SUCCESS, runtime Java25.0.4.
Own JNI/new candidate packaged with AMD nine classes/two DLLs identical.
Three pairs submitted before failure latch; already submitted work drained.
Pair2 and pair3: native and external backend count2/index2 SUCCESS. Public
getters confirm the distinct bound output/status resources for both indices.
GPU snapshots after each sentinel upload read4294967295. Index1 changes to0;
index2 stays4294967295 both immediately after its Evaluate and at group end.
Callback fence observations are5>=done4 and6>=done6; no read error.
Java propagates UNKNOWN, G2 is discarded. No x3 full presentation sequence.

This excludes Java/JNI slot0 mapping, early host read, shared status/resource,
post-Evaluate host sentinel reset, and a merely late group status update for
these samples. The NGX index2 status-write boundary remains unresolved. The
closed vendor implementation may have group-only optional-status semantics or
other internal behavior; headers alone do not establish that. Neither is
asserted as the cause. No index2 workaround or synthetic valid flag added.

MINECRAFT_DLSSG_X3=FAIL_OUTPUT2_STATUS_NOT_WRITTEN
LAST_CONFIRMED_BOUNDARY=NGX_EVALUATE_AND_GPU_GROUP_COMPLETION
ROOT_CAUSE=INDEX2_STATUS_RESOURCE_UNCHANGED; INTERNAL_REASON_UNDETERMINED
No retries/x4/x5/multiplayer preparation. Pixel hashes/temporal validity cannot
be measured because UNKNOWN stops generation before the bounded sample window.
Full matched evidence: run index2-boundary-review.json.

Closed normally at10:54:12 America/La_Paz; world all dimensions saved, Vulkan
destroyed, BUILD SUCCESSFUL, no crash/device removed (reason0x00000000).
Three completed provider groups, two status-eligible G1 outputs (pixel proof
absent), one G1 presented and zero G2 presented. Global FPS not measured;
startup provider presentation window is in timings.json. No cause of the
reported visual cut is asserted. result-classification.json contains the exact
failure label and all matched native parameter/status/fence evidence.
