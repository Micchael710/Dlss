/*
 * Super Resolution
 * Copyright (c) 2025-2026. 187J3X1-114514
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

package io.homo.superresolution.core.graphics.vulkan;

import org.lwjgl.opengl.GL20;
import org.lwjgl.system.MemoryStack;
import org.lwjgl.vulkan.VkSemaphoreCreateInfo;

import static io.homo.superresolution.core.graphics.vulkan.VulkanUtils.VK_CHECK;
import static org.lwjgl.opengl.EXTSemaphore.*;
import static org.lwjgl.vulkan.VK11.*;

public class VkGlInteropSemaphore {
    private final long vkSemaphoreHandle;
    private final long glSemaphoreHandle;
    private final long semaphoreHandle;
    private final VulkanDevice device;
    private static final java.util.concurrent.atomic.AtomicLong GENERATIONS=new java.util.concurrent.atomic.AtomicLong();
    private final long generation=GENERATIONS.incrementAndGet();
    private boolean destroyed;

    private VkGlInteropSemaphore(long vkSemaphoreHandle, long glSemaphoreHandle, long semaphoreHandle, VulkanDevice device) {
        this.vkSemaphoreHandle = vkSemaphoreHandle;
        this.glSemaphoreHandle = glSemaphoreHandle;
        this.semaphoreHandle = semaphoreHandle;
        this.device = device;
    }

    public static VkGlInteropSemaphore create(VulkanDevice vulkanDevice) {
        try (MemoryStack stack = MemoryStack.stackPush()) {
            long[] pVkSemaphore = new long[]{0};
            int pGlSemaphore = 0;
            long exportedSemaphoreHandle = -1;
            boolean imported = false;
            try {
                VkSemaphoreCreateInfo semaphoreCreateInfo = VkSemaphoreCreateInfo.calloc(stack);
                semaphoreCreateInfo.sType(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
                semaphoreCreateInfo.pNext(VulkanInterop.IMPL.createVkExportSemaphoreCreateInfo(stack).address());
                VK_CHECK(vkCreateSemaphore(
                        vulkanDevice.getVkDevice(),
                        semaphoreCreateInfo,
                        null,
                        pVkSemaphore
                ));
                vulkanDevice.setDebugName(VK_OBJECT_TYPE_SEMAPHORE, pVkSemaphore[0], "VkGlInteropSemaphore");
                exportedSemaphoreHandle = VulkanInterop.IMPL.vkGetSemaphoreHandleKHR(
                        stack,
                        vulkanDevice.getVkDevice(),
                        VulkanInterop.IMPL.createVkSemaphoreGetHandleInfoKHR(stack, pVkSemaphore[0])
                );

                pGlSemaphore = glGenSemaphoresEXT();
                VulkanInterop.IMPL.glImportSemaphoreHandleEXT(stack, pGlSemaphore, exportedSemaphoreHandle);
                imported = true;
                return new VkGlInteropSemaphore(
                        pVkSemaphore[0],
                        pGlSemaphore,
                        exportedSemaphoreHandle,
                        vulkanDevice
                );
            } catch (Throwable throwable) {
                try {
                    if (pGlSemaphore != 0) {
                        glDeleteSemaphoresEXT(pGlSemaphore);
                    }
                } finally {
                    try {
                        if (pVkSemaphore[0] != 0L) {
                            vkDestroySemaphore(vulkanDevice.getVkDevice(), pVkSemaphore[0], null);
                        }
                    } finally {
                        if (imported) {
                            VulkanInterop.closeImportedExportedHandle(exportedSemaphoreHandle);
                        } else {
                            VulkanInterop.closeUnimportedExportedHandle(exportedSemaphoreHandle);
                        }
                    }
                }
                throw throwable;
            }
        }
    }

    public long getVkSemaphoreHandle() {
        return vkSemaphoreHandle;
    }

    public long getGlSemaphoreHandle() {
        return glSemaphoreHandle;
    }

    public long getSemaphoreHandle() {
        return semaphoreHandle;
    }

    public void destroy() {
        if(destroyed)return;
        destroyed=true;
        InteropReleaseDiagnostics.stage("SEMAPHORE_VK_DESTROY_BEGIN vk="+vkSemaphoreHandle+" gl="+glSemaphoreHandle+" generation="+generation);
        try {
            vkDestroySemaphore(
                    device.getVkDevice(),
                    vkSemaphoreHandle,
                    null
            );
            InteropReleaseDiagnostics.stage("SEMAPHORE_VK_DESTROY_END vk="+vkSemaphoreHandle);
        } finally {
            try {
                InteropReleaseDiagnostics.stage("SEMAPHORE_GL_DELETE_BEGIN gl="+glSemaphoreHandle);
                glDeleteSemaphoresEXT((int) glSemaphoreHandle);
                InteropReleaseDiagnostics.stage("SEMAPHORE_GL_DELETE_END gl="+glSemaphoreHandle);
            } finally {
                InteropReleaseDiagnostics.stage("SEMAPHORE_HANDLE_CLOSE_BEGIN handle="+semaphoreHandle);
                VulkanInterop.closeImportedExportedHandle(semaphoreHandle);
                InteropReleaseDiagnostics.stage("SEMAPHORE_HANDLE_CLOSE_END handle="+semaphoreHandle);
            }
        }
    }

    public void signalVulkan(int[] textures, int[] buffers, int[] dstLayouts) {
        glSignalSemaphoreEXT(
                (int) glSemaphoreHandle,
                buffers == null ? new int[]{} : buffers,
                textures == null ? new int[]{} : textures,
                dstLayouts == null ? new int[]{} : dstLayouts
        );
        GL20.glFlush();
    }

    public void waitVulkanSignal(int[] textures, int[] buffers, int[] srcLayouts) {
        glWaitSemaphoreEXT(
                (int) glSemaphoreHandle,
                buffers == null ? new int[]{} : buffers,
                textures == null ? new int[]{} : textures,
                srcLayouts == null ? new int[]{} : srcLayouts
        );
    }

    public int waitBorrowedRelease(GlReleaseContract.Submission submitted,String resource,
                                   GlReleaseContract cycle,int texture,int layout) {
        if(destroyed)throw new IllegalStateException("Release semaphore already destroyed");
        int[] textures={texture},layouts={layout};cycle.validate(submitted,vkSemaphoreHandle,textures,layouts);
        long expected=resource.equals("depth")?submitted.depthSemaphore():submitted.motionSemaphore();
        if(vkSemaphoreHandle!=expected)throw new IllegalStateException("Release resource/direction mismatch");
        boolean diagnostic=InteropReleaseDiagnostics.enabled();
        boolean validSemaphore=!diagnostic||glIsSemaphoreEXT((int)glSemaphoreHandle);
        boolean validTexture=!diagnostic||org.lwjgl.opengl.GL11.glIsTexture(texture);
        long context=diagnostic?org.lwjgl.glfw.GLFW.glfwGetCurrentContext():0;
        if(diagnostic&&(context==0||!validSemaphore||!validTexture||Thread.currentThread()!=io.homo.superresolution.common.SuperResolution.renderThread))throw new IllegalStateException("Invalid GL release context/object/thread");
        int preError=org.lwjgl.opengl.GL11.glGetError();
        String label="logicalFrame="+submitted.logicalFrame()+" realFrameId="+submitted.realFrameId()+" captureGeneration="+submitted.generation()+" slot="+submitted.slot()+" resource="+resource+" glSemaphore="+glSemaphoreHandle+" vkSemaphore="+vkSemaphoreHandle;
        InteropReleaseDiagnostics.active(label);
        int error;
        try{glWaitSemaphoreEXT((int)glSemaphoreHandle,new int[0],textures,layouts);
            error=org.lwjgl.opengl.GL11.glGetError(); // Must be the immediate next GL call.
        }finally{InteropReleaseDiagnostics.clear();}
        InteropReleaseDiagnostics.waitResult(submitted,resource,glSemaphoreHandle,vkSemaphoreHandle,generation,texture,layout,error,preError,cycle.previousWaitFrame(),validSemaphore,validTexture,context);
        if(error==0)cycle.consumed(submitted);
        return error;
    }

    public void signalVulkan() {
        signalVulkan(null, null, null);
    }

    public void waitVulkanSignal() {
        waitVulkanSignal(null, null, null);
    }
}
