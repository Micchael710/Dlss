/*
 * Super Resolution
 * Copyright (c) 2026. 187J3X1-114514
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

package io.homo.superresolution.common.presentation.capture;

import java.util.concurrent.atomic.AtomicReference;

final class FrameResourceLifecycle {
    private final Object submissionMonitor = new Object();

    /** Producer backpressure: ownership is not released until an actual submission is published. */
    void awaitSubmissionForReuse(java.util.function.BooleanSupplier failed) {
        long deadline = System.nanoTime() + java.util.concurrent.TimeUnit.SECONDS.toNanos(5);
        synchronized (submissionMonitor) {
            while (state() != FrameResourceState.SUBMITTED && state() != FrameResourceState.REUSABLE) {
                if (failed.getAsBoolean()) throw new IllegalStateException("Capture ownership recovery failed");
                var current = state();
                if (current != FrameResourceState.QUEUED && current != FrameResourceState.DISPATCHING
                        && current != FrameResourceState.SEALED) throw invalidTransition(current, FrameResourceState.REUSABLE);
                long remaining = deadline - System.nanoTime();
                if (remaining <= 0) throw new IllegalStateException("Timed out awaiting capture submission, state=" + current);
                try { submissionMonitor.wait(Math.max(1, Math.min(50, remaining / 1_000_000))); }
                catch (InterruptedException interrupted) {
                    Thread.currentThread().interrupt(); throw new IllegalStateException("Capture producer interrupted", interrupted);
                }
            }
        }
    }
    private final AtomicReference<FrameResourceState> state =
            new AtomicReference<>(FrameResourceState.REUSABLE);

    FrameResourceState state() {
        return state.get();
    }

    void beginRecording() {
        transition(FrameResourceState.REUSABLE, FrameResourceState.RECORDING);
    }

    void seal() {
        transition(FrameResourceState.RECORDING, FrameResourceState.SEALED);
    }

    void discardEmptyRecording() {
        transition(FrameResourceState.RECORDING, FrameResourceState.REUSABLE);
    }

    void markQueued() {
        transition(FrameResourceState.SEALED, FrameResourceState.QUEUED);
    }

    void markDispatching() {
        transition(FrameResourceState.QUEUED, FrameResourceState.DISPATCHING);
    }

    void requireSubmittable() {
        FrameResourceState current = state.get();
        if (current != FrameResourceState.SEALED
                && current != FrameResourceState.DISPATCHING) {
            throw invalidTransition(current, FrameResourceState.SUBMITTED);
        }
    }

    void markSubmitted() {
        while (true) {
            FrameResourceState current = state.get();
            if (current != FrameResourceState.SEALED
                    && current != FrameResourceState.DISPATCHING) {
                throw invalidTransition(current, FrameResourceState.SUBMITTED);
            }
            if (state.compareAndSet(current, FrameResourceState.SUBMITTED)) {
                synchronized (submissionMonitor) { submissionMonitor.notifyAll(); }
                return;
            }
        }
    }

    void markReusable() {
        transition(FrameResourceState.SUBMITTED, FrameResourceState.REUSABLE);
    }

    private void transition(FrameResourceState expected, FrameResourceState target) {
        if (!state.compareAndSet(expected, target)) {
            throw invalidTransition(state.get(), target);
        }
    }

    private static IllegalStateException invalidTransition(
            FrameResourceState current,
            FrameResourceState target
    ) {
        return new IllegalStateException(
                "Invalid frame-resource transition " + current + " -> " + target
        );
    }
}
