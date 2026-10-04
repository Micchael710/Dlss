package io.homo.superresolution.common.upscale.algo.dlss;
import io.homo.superresolution.common.upscale.interoplayer.InteropTeardown;
import java.util.ArrayList;
import java.util.List;

public final class DlssSrFailurePathTest {
    public static void main(String[] args) {
        if (!DlssSrPrerequisites.shouldInitializeVulkan(true,"dlss")) throw new AssertionError();
        if (DlssSrPrerequisites.shouldInitializeVulkan(true,"fsr1")) throw new AssertionError();
        if (!DlssSrPrerequisites.shouldInitializeVulkan(false,"fsr1")) throw new AssertionError();
        for (boolean[] state : new boolean[][]{{false,false},{true,false}}) {
            try { DlssSrPrerequisites.requireReady(state[0],state[1]); throw new AssertionError(); }
            catch (IllegalStateException expected) {}
        }
        DlssSrPrerequisites.requireReady(true,true);
        List<String> order=new ArrayList<>();
        InteropTeardown.release(false,()->{throw new AssertionError("No uncreated GPU resource access");},
                ()->order.add("commands"),()->order.add("empty feature"),()->{throw new AssertionError();});
        if (!order.equals(List.of("commands","empty feature"))) throw new AssertionError(order);
        order.clear();
        InteropTeardown.release(true,()->order.add("drain"),()->order.add("commands"),
                ()->order.add("feature"),()->order.add("resources"));
        if (!order.equals(List.of("drain","commands","feature","resources"))) throw new AssertionError(order);
        order.clear();
        try {
            InteropTeardown.release(true,()->order.add("drain"),()->{throw new IllegalStateException("injected cleanup failure");},
                    ()->order.add("feature"),()->order.add("resources"));
            throw new AssertionError();
        } catch (IllegalStateException expected) {}
        if (!order.equals(List.of("drain"))) throw new AssertionError("Owners must stay reachable after cleanup failure");
        System.out.println("PASS CPU prerequisite policy + no-resource teardown + created-resource cleanup order + retained-owner failure; no GPU simulation claimed");
    }
}
