package org.ireallywanttosleep.wisteria.fsr;

import io.homo.superresolution.core.graphics.vulkan.*;
import io.homo.superresolution.core.graphics.impl.texture.*;
import java.util.*;

/** FG-worker-owned slots, with leases spanning submission and PresentWorker completion. */
public final class FsrOutputPool implements AutoCloseable {
    private final List<Slot> slots = new ArrayList<>();
    private final Thread owner = Thread.currentThread();
    public Slot acquire(VulkanDevice device, int width, int height, int format, int count) {
        checkOwner();
        if (count < 0 || count > 5) throw new IllegalArgumentException("Output count outside 0..5");
        for (Slot slot : slots) if (!slot.leased && slot.width == width && slot.height == height
                && slot.format == format && slot.outputs.size() >= count) { slot.leased = true; return slot; }
        if (slots.size() >= 12) throw new IllegalStateException("All FSR output slots still leased");
        TextureFormat textureFormat = Arrays.stream(TextureFormat.values()).filter(f -> f.vk() == format)
                .findFirst().orElseThrow(() -> new IllegalArgumentException("Unsupported output format"));
        Slot slot = new Slot(width, height, format);
        try {
            for (int i = 0; i < count; i++) slot.outputs.add((VulkanTexture) device.createTexture(TextureDescription.create()
                    .type(TextureType.Texture2D).format(textureFormat).size(width, height)
                    .usages(TextureUsages.create().storage().sampler().transferSource().transferDestination())
                    .label("WisteriaFSR-output-" + slots.size() + "-" + i).build()));
            slot.real = (VulkanTexture) device.createTexture(TextureDescription.create().type(TextureType.Texture2D)
                    .format(textureFormat).size(width, height)
                    .usages(TextureUsages.create().sampler().transferSource().transferDestination())
                    .label("WisteriaFSR-real-" + slots.size()).build());
        } catch (Throwable error) { slot.outputs.forEach(VulkanTexture::destroy); if (slot.real != null) slot.real.destroy(); throw error; }
        slots.add(slot); slot.leased = true; return slot;
    }
    public void release(Slot slot) { checkOwner(); slot.leased = false; }
    public int indexOf(Slot slot) { checkOwner(); return slots.indexOf(slot); }
    public void close() {
        checkOwner();
        if (slots.stream().anyMatch(s -> s.leased)) throw new IllegalStateException("FSR outputs still in flight");
        slots.forEach(s -> { s.outputs.forEach(VulkanTexture::destroy); s.real.destroy(); }); slots.clear();
    }
    private void checkOwner() { if (Thread.currentThread() != owner) throw new IllegalStateException("FSR pool accessed off worker"); }
    public static final class Slot {
        final int width, height, format;
        final List<VulkanTexture> outputs = new ArrayList<>();
        VulkanTexture real;
        boolean leased;
        Slot(int width, int height, int format) { this.width = width; this.height = height; this.format = format; }
        public List<VulkanTexture> outputs(int count) { return List.copyOf(outputs.subList(0, count)); }
        public VulkanTexture realOutput() { return real; }
    }
}
