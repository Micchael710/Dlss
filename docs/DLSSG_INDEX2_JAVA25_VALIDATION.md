# Index2 status and Java25 continuation

Start HEAD ed7a4d42cdfa887f94103872834ceaf72d512b7e, main/origin user repository.
Old run20261004-042043-949 remains FAIL_JAVA21_RUN; no result overwritten.
AMD/shadow/x2 gates are reused. Exactly one new x3 graphics attempt is authorized.

## Static status audit

Native Slot owns N distinct disable UAV buffers and N distinct readback buffers.
Every iteration k binds generated image[4+k], disable[k], and copies disable[k]
to readback[k]. Index passed to public NGX is k+1; vector/lease index is k.
No shared single status buffer, JNI scalar result, or output VkImage is reused
between indices. Pool slots cannot be acquired again while leased.

All N Evaluate calls and their status copies are recorded in the **same D3D12
command list**. Queue Wait(ready) -> ExecuteCommandLists -> Signal(done).
The completion event is armed for that group done value. Its callback checks
GetCompletedValue>=done and reads N buffers; it passes the matching realFrameId,
array length N and done value to an immutable Java snapshot. This is group
completion after **both** Evaluate/copies, not an inference from index1's fence.
Vulkan waits on the imported group timeline. Each output command buffer's
handoff semaphore is consumed by its corresponding presentation blit; skipped
outputs still drain their unused handoffs during normal batch retirement.

FrameGenerationWorker and PresentWorker already preserve candidate indices.
PresentWorker checks per-index validity after metadata readiness and before
acquiring the target. It retains the generated resources until all GPU and
presentation work is retired. No AMD defaults or GPU synchronization modified.

## Changes

`DlssgOutputStatus`: 0=ENABLED,1=DISABLED,0xffffffff and other values=UNKNOWN.
UNKNOWN completes the metadata notification but remains non-presentable.
An unknown index no longer masks an independently valid index. Wrong interval,
completion value or array length remains an exceptional contract violation.
Out-of-range indices are rejected. Caller arrays are cloned and duplicate
notifications do not overwrite the snapshot. Reset suppresses every candidate.

Adapter logs each index/state/raw unsigned status/completion/resource, then
latches generation unavailable on UNKNOWN. Already submitted groups drain;
real-only fallback remains possible. No unknown output becomes a valid count.
No forced Available/max, private ABI, binary patch or altered kernel mathematics.
Java event log and JSONL flush each observation so a short failed run is retained.
Per-index Evaluate logging distinguishes group reset from evaluateReset.
Observed in-flight leases/exhaustion are logged, rather than inferred from capacity.

Minecraft writes its actual java.version/java.home/PID/process command before
external-loader init and enforces25.0.4. Mod bytecode target stays21. runClient's
launcher is audited separately from Gradle's JVM. Preflight uses a new **own JNI**
entry point to public NGX D3D12 init/GetCapabilityParameters only, no CreateFeature,
Evaluate or Vulkan-direct retest. Fresh reported max must permit requested count
before Minecraft starts; embedded init queries it again. Both queries may be
hooked and are not generation proof. The external module stays resident until
its isolated preflight process exits; Minecraft loads it independently afterward.

## Validation status

Build/contracts/preflight/runtime evidence will be recorded in the new run.
No x3 graphics PASS follows from these static assertions or CPU tests.
Sentinel still observed in the new run => stop, no x4/x5. No Java causality assumed.

## Closed Java25 run

Run `20261004-045826-216` is now CLOSED_FAIL; the offline review preserves the
original events and proves both Evaluate calls succeeded, group done=4 retired,
index1=0 and index2=4294967295 (UNKNOWN). One G1 was presented, G2 discarded.
No pixel samples, no x3 content/temporal PASS, normal shutdown and device reason
0x00000000. AMD nine classes and two DLLs match baseline byte-for-byte.
The exact cause of the missing status remains unknown. See the run result.json.
