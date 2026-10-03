package org.ireallywanttosleep.wisteria.fsr;

import java.nio.file.*;
import java.security.MessageDigest;
import java.util.HexFormat;
import org.ireallywanttosleep.wisteria.Wisteria;

/** New ABI; exclusively the pinned official 1.1.4 Vulkan API, no swapchain ownership. */
public final class FidelityFxBridge {
    public static final int ABI = 1;
    private static boolean attempted;
    private static volatile boolean available;
    private FidelityFxBridge() {}
    public static synchronized void load() {
        if (attempted) return;
        attempted = true;
        if (!System.getProperty("os.name").startsWith("Windows")
                || !System.getProperty("os.arch").equals("amd64")) return;
        try {
            Path folder = Path.of("config", "wisteria", "fsr-1.1.4").toAbsolutePath();
            Files.createDirectories(folder);
            Path amd = extract(folder, "amd_fidelityfx_vk.dll",
                    "c84ca40421ff5594edfc6553cb89a2d07982b0cadb1a66beb989b2cdedd39197");
            Path bridge = extract(folder, "wisteria_fsr_bridge.dll", null);
            System.load(bridge.toString());
            initializeSdk(amd.toString());
            available = queryBridgeAbi() == ABI && querySdkVersion().startsWith("SDK 1.1.4;")
                    && queryCapabilities() == 1;
            Wisteria.LOGGER.info("FSR_BRIDGE abi={} version={} available={}", queryBridgeAbi(), querySdkVersion(), available);
        } catch (Throwable error) {
            Wisteria.LOGGER.warn("FSR 3.1.4 bridge unavailable: {}", error.toString());
        }
    }
    private static Path extract(Path folder, String name, String expectedHash) throws Exception {
        byte[] bytes;
        try (var input = FidelityFxBridge.class.getResourceAsStream("/natives/windows-x64/" + name)) {
            if (input == null) throw new java.io.FileNotFoundException(name);
            bytes = input.readAllBytes();
        }
        String hash = HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
        if (expectedHash != null && !expectedHash.equals(hash)) throw new SecurityException("Unexpected official AMD DLL hash");
        Path result = folder.resolve(name);
        if (Files.isRegularFile(result) && MessageDigest.isEqual(Files.readAllBytes(result), bytes)) return result;
        Path staged = Files.createTempFile(folder, name, ".tmp");
        try { Files.write(staged, bytes); Files.move(staged, result, StandardCopyOption.REPLACE_EXISTING); }
        finally { Files.deleteIfExists(staged); }
        return result;
    }
    public static boolean isAvailable() { return available; }
    private static native void initializeSdk(String path);
    public static native int queryBridgeAbi();
    public static native String querySdkVersion();
    public static native int queryCapabilities();
    public static native long createSession(long device, long physicalDevice, long getDeviceProcAddr,
                                          int width, int height, int renderWidth, int renderHeight, int outputFormat,
                                          int hudlessFormat);
    public static native void destroySession(long session);
    /** Resource tuple = image, Vulkan format, width, height; params = nine scalars + camera basis. */
    public static native void prepare(long session, long commandBuffer, long frameId,
                                      float[] params, long[] depth, long[] motionVectors, long[] hudless);
    public static native void generate(long session, long commandBuffer, long frameId,
                                       long[] finalColor, long[] output, boolean reset);
}
