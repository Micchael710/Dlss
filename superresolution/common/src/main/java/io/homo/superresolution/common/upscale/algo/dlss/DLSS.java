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

package io.homo.superresolution.common.upscale.algo.dlss;

import io.homo.superresolution.api.InitializationDescription;
import io.homo.superresolution.common.SuperResolution;
import io.homo.superresolution.common.config.SuperResolutionConfig;
import io.homo.superresolution.common.minecraft.handler.RenderHandlerManager;
import io.homo.superresolution.common.perf.PerformanceTracker;
import io.homo.superresolution.common.upscale.interoplayer.GlVulkanInteropAlgorithm;
import io.homo.superresolution.core.RenderSystems;
import io.homo.superresolution.core.graphics.vulkan.VulkanCommandBuffer;
import io.homo.superresolution.core.graphics.vulkan.VulkanDevice;
import io.homo.superresolution.core.graphics.vulkan.VulkanTexture;
import io.homo.superresolution.core.graphics.vulkan.VulkanTimestampProfiler;
import io.homo.superresolution.core.ngx.*;

import java.util.Objects;

import static io.homo.superresolution.api.interop.InteropResourceType.*;

public class DLSS extends GlVulkanInteropAlgorithm {
    private NgxDispatchResources ngxDispatchResource;
    private NgxFeature ngxDlssFeature;
    private NgxParameters ngxParameters;
    private final java.util.Map<VulkanCommandBuffer,Integer> diagnosticPending=new java.util.IdentityHashMap<>();
    private int evaluated,diagnosticSamples;

    private static void closeResource(NgxResourceVK resource) {
        if (resource != null) {
            resource.close();
        }
    }

    private static void requireNgxSuccess(String operation, int result) {
        if (!NgxConstants.succeeded(result)) {
            throw new IllegalStateException(operation + " failed. NGX result: " + result);
        }
    }

    @Override
    protected void dispatchVulkanUpscale(
            VulkanCommandBuffer commandBuffer,
            FrameResourcesSet frameResourcesSet
    ) {
        dispatchNgxContext(commandBuffer, frameResourcesSet);
    }

    @Override
    protected boolean isVulkanInteropReady() {
        return ngxDlssFeature != null
                && ngxDlssFeature.isValid()
                && ngxParameters != null
                && ngxParameters.isValid();
    }

    @Override
    protected void onInteropResourcesCreated() {
        recreateNgxContext(initDesc);
        createNgxDispatchResources();
    }

    @Override
    protected void onBeforeInteropResourcesDestroyed() {
        destroyNgxDispatchResources();
        destroyNgxContext();
    }

    private void recreateNgxContext(InitializationDescription desc) {
        VulkanDevice vulkanDevice = RenderSystems.vulkan().device();
        if (!NgxInitializer.initializeIfSupported()) {
            throw new IllegalStateException("NGX is unavailable for the current GPU; no automatic retry");
        }

        NgxParameters parameters = new NgxParameters();
        int parametersResult = NgxVulkan.getCapabilityParameters(parameters);
        requireNgxSuccess("NVSDK_NGX_VULKAN_GetCapabilityParameters", parametersResult);
        int[] available={0},initResult={0};
        int availableQuery=parameters.getInt("SuperSampling.Available",available);
        int initQuery=parameters.getInt("SuperSampling.FeatureInitResult",initResult);
        DlssSrDiagnostics.event("CAPABILITY","queryResult",Integer.toUnsignedString(parametersResult),"availableQuery",Integer.toUnsignedString(availableQuery),"available",available[0],"initQuery",Integer.toUnsignedString(initQuery),"initResult",Integer.toUnsignedString(initResult[0]));
        requireNgxSuccess("SuperSampling.Available",availableQuery);
        if(available[0]==0){parameters.close();throw new IllegalStateException("NGX SuperSampling is unavailable");}

        NgxFeature feature = new NgxFeature();
        VulkanCommandBuffer commandBuffer = vulkanDevice.createCommandBuffer();
        try {
            configureNgxRenderPreset(parameters);

            NgxDLSSCreateParams createParams = new NgxDLSSCreateParams();
            createParams.feature.width = RenderHandlerManager.getRenderWidth();
            createParams.feature.height = RenderHandlerManager.getRenderHeight();
            createParams.feature.targetWidth = RenderHandlerManager.getScreenWidth();
            createParams.feature.targetHeight = RenderHandlerManager.getScreenHeight();
            createParams.feature.perfQualityValue=DlssSrContract.perfQuality(createParams.feature.width,createParams.feature.targetWidth);
            createParams.featureCreateFlags = createNgxFeatureFlags(desc);
            DlssSrDiagnostics.event("CREATE_INPUTS","renderWidth",createParams.feature.width,"renderHeight",createParams.feature.height,"outputWidth",createParams.feature.targetWidth,"outputHeight",createParams.feature.targetHeight,"flags",createParams.featureCreateFlags,"perfQualityValue",createParams.feature.perfQualityValue,"hdr",desc.isHdrInput(),"autoExposure",desc.isAutoExposure(),"motionJittered",desc.isMotionJittered(),"depthInverted",desc.isDepthInverted());

            commandBuffer.begin();
            int createResult = NgxVulkan.createDLSS(
                    commandBuffer.getNativeCommandBuffer().address(),
                    1,
                    1,
                    feature,
                    parameters,
                    createParams
            );
            commandBuffer.end();
            requireNgxSuccess("NGX_VULKAN_CREATE_DLSS_EXT", createResult);
            DlssSrDiagnostics.event("CREATE_FEATURE","result",Integer.toUnsignedString(createResult),"handleValid",feature.isValid());
            if(!feature.isValid())throw new IllegalStateException("NGX CreateFeature returned no valid DLSS handle");

            vulkanDevice.submitCommandBuffer(commandBuffer);
            commandBuffer.waitForFence();
            DlssSrDiagnostics.event("CREATE_COMPLETION","completed",true);

            ngxParameters = parameters;
            ngxDlssFeature = feature;
        } catch (RuntimeException | Error e) {
            feature.close();
            parameters.close();
            throw e;
        } finally {
            commandBuffer.destroy();
        }
    }

    private int createNgxFeatureFlags(InitializationDescription desc) {
        int flags = NgxConstants.DLSS_FLAG_MV_LOW_RES;
        if (desc.isAutoExposure()) {
            flags |= NgxConstants.DLSS_FLAG_AUTO_EXPOSURE;
        }
        if (desc.isHdrInput()) {
            flags |= NgxConstants.DLSS_FLAG_HDR;
        }
        if (desc.isMotionJittered()) {
            flags |= NgxConstants.DLSS_FLAG_MV_JITTERED;
        }
        if (desc.isDepthInverted()) {
            flags |= NgxConstants.DLSS_FLAG_DEPTH_INVERTED;
        }
        return flags;
    }

    private void configureNgxRenderPreset(NgxParameters parameters) {
        int preset = SuperResolutionConfig.SPECIAL.DLSS.RENDER_PRESET.get().getCode();
        parameters.setInt("DLSS.Hint.Render.Preset.DLAA", preset);
        parameters.setInt("DLSS.Hint.Render.Preset.Quality", preset);
        parameters.setInt("DLSS.Hint.Render.Preset.Balanced", preset);
        parameters.setInt("DLSS.Hint.Render.Preset.Performance", preset);
        parameters.setInt("DLSS.Hint.Render.Preset.UltraPerformance", preset);
        parameters.setInt("DLSS.Hint.Render.Preset.UltraQuality", preset);
    }

    private void destroyNgxContext() {
        // Base destroy/rebuild has drained resource users before this hook.
        for(var pending:diagnosticPending.entrySet())DlssSrDiagnostics.event("GPU_COMPLETION","evaluation",pending.getValue(),"source","resource drain before feature destruction");
        diagnosticPending.clear();
        if (ngxDlssFeature != null) {
            int result = ngxDlssFeature.release();
            if (!NgxConstants.succeeded(result)) {
                SuperResolution.LOGGER.error("Failed to release the DLSS NGX feature. Result: {}", result);
            }
            ngxDlssFeature = null;
        }
        if (ngxParameters != null) {
            int result = ngxParameters.destroy();
            if (!NgxConstants.succeeded(result)) {
                SuperResolution.LOGGER.error("Failed to destroy the DLSS NGX parameters. Result: {}", result);
            }
            ngxParameters = null;
        }
    }

    private void dispatchNgxContext(
            VulkanCommandBuffer commandBuffer,
            FrameResourcesSet frameResourcesSet
    ) {
        // acquire() waited this reusable command buffer's previous fence before recording.
        Integer completed=diagnosticPending.remove(commandBuffer);
        if(completed!=null)DlssSrDiagnostics.event("GPU_COMPLETION","evaluation",completed,"source","command ring acquire fence wait");
        if (ngxDlssFeature == null || ngxParameters == null) {
            throw new IllegalStateException("DLSS context is unavailable; output not produced");
        }

        NgxDispatchResources dispatchResources = ngxDispatchResource;
        if (dispatchResources == null) {
            throw new IllegalStateException("DLSS resources are unavailable; output not produced");
        }

        NgxVKDLSSEvalParams evalParams = dispatchResources.evalParams;
        evalParams.feature.sharpness = SuperResolutionConfig.getSharpness();
        evalParams.jitterOffsetX = frameResourcesSet.frameData.jitterOffset().x;
        evalParams.jitterOffsetY = frameResourcesSet.frameData.jitterOffset().y;
        evalParams.renderSubrectDimensions.width = frameResourcesSet.frameData.renderWidth();
        evalParams.renderSubrectDimensions.height = frameResourcesSet.frameData.renderHeight();
        evalParams.motionVectorScaleX = frameResourcesSet.frameData.renderSize().x;
        evalParams.motionVectorScaleY = frameResourcesSet.frameData.renderSize().y;
        evalParams.reset = consumeHistoryReset() ? 1 : 0;
        evalParams.preExposure = frameResourcesSet.frameData.preExposure();
        evalParams.exposureScale = 1.0f;
        evalParams.frameTimeDeltaInMsec = frameResourcesSet.frameData.frameTimeDelta();

        VulkanTimestampProfiler profiler =
                RenderSystems.vulkan().device().timestampProfiler();
        int timestampSlot = profiler == null
                ? -1
                : profiler.beginRegion(
                commandBuffer.getNativeCommandBuffer(),
                PerformanceTracker.VK_UPSCALE
        );
        int evaluateResult = NgxVulkan.evaluateDLSS(
                commandBuffer.getNativeCommandBuffer().address(),
                ngxDlssFeature,
                ngxParameters,
                evalParams
        );
        if (timestampSlot >= 0) {
            profiler.endRegion(commandBuffer.getNativeCommandBuffer(), timestampSlot);
        }
        if (!NgxConstants.succeeded(evaluateResult)) {
            SuperResolution.LOGGER.error("NGX DLSS evaluation failed. Result: {}", evaluateResult);
            DlssSrDiagnostics.event("EVALUATE_FAILURE","result",Integer.toUnsignedString(evaluateResult));
            throw new IllegalStateException("NGX DLSS evaluation failed: "+Integer.toUnsignedString(evaluateResult));
        }
        evaluated++;
        if(DlssSrDiagnostics.enabled())diagnosticPending.put(commandBuffer,evaluated);
        DlssSrDiagnostics.event("EVALUATE","evaluation",evaluated,"result",Integer.toUnsignedString(evaluateResult),"outputVkImage",frameResourcesSet.vulkan(OutputColor).handle(),"inputVkImage",frameResourcesSet.vulkan(Color).handle(),"renderWidth",evalParams.renderSubrectDimensions.width,"renderHeight",evalParams.renderSubrectDimensions.height,"outputWidth",frameResourcesSet.vulkan(OutputColor).getWidth(),"outputHeight",frameResourcesSet.vulkan(OutputColor).getHeight(),"jitterX",evalParams.jitterOffsetX,"jitterY",evalParams.jitterOffsetY,"mvScaleX",evalParams.motionVectorScaleX,"mvScaleY",evalParams.motionVectorScaleY,"reset",evalParams.reset,"preExposure",evalParams.preExposure);
    }

    @Override
    protected void onUpscaleOutputQueued(FrameResourcesSet resources){
        DlssSrDiagnostics.event("OUTPUT_QUEUED","evaluation",evaluated,"outputVkImage",resources.vulkan(OutputColor).handle(),"outputGlTexture",getOutputTextureId(),"transport","Vulkan signal -> OpenGL GPU semaphore wait -> existing output framebuffer");
        if(DlssSrDiagnostics.enabled()&&evaluated>=30&&diagnosticSamples<3){
            diagnosticSamples++;
            for(var type:new io.homo.superresolution.api.interop.InteropResourceType[]{Color,OutputColor}){
                var texture=resources.openGl(type);int bytes=Math.multiplyExact(Math.multiplyExact(texture.getWidth(),texture.getHeight()),4);
                var buffer=org.lwjgl.system.MemoryUtil.memAlloc(bytes);
                try{org.lwjgl.opengl.GL45.glGetTextureImage((int)texture.handle(),0,org.lwjgl.opengl.GL11.GL_RGBA,org.lwjgl.opengl.GL11.GL_UNSIGNED_BYTE,buffer);
                    byte[] data=new byte[bytes];buffer.get(data);String name="sr-sample-"+evaluated+"-"+type+".bin";DlssSrDiagnostics.save(name,data);
                    DlssSrDiagnostics.event("IMAGE_SAMPLE","evaluation",evaluated,"type",type,"width",texture.getWidth(),"height",texture.getHeight(),"file",name,"readback","bounded diagnostic only; GL wait ordered after NGX Vulkan submission");
                }finally{org.lwjgl.system.MemoryUtil.memFree(buffer);}
            }
        }
    }

    private void createNgxDispatchResources() {
        destroyNgxDispatchResources();
        try {
            ngxDispatchResource = new NgxDispatchResources(Objects.requireNonNull(frameResourcesSet));
        } catch (RuntimeException | Error e) {
            destroyNgxDispatchResources();
            throw e;
        }
    }

    private void destroyNgxDispatchResources() {
        if (ngxDispatchResource != null){
            ngxDispatchResource.close();
        }
    }

    private NgxResourceVK createNgxTextureResource(VulkanTexture texture, boolean readWrite) {
        if (texture == null) {
            return null;
        }
        NgxImageSubresourceRange subresourceRange = new NgxImageSubresourceRange();
        subresourceRange.aspectMask = texture.getAspectMask();
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = texture.getMipmapSettings().getLevels();
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;
        return NgxVulkan.createImageViewResourceVK(
                texture.getImageView(),
                texture.handle(),
                subresourceRange,
                texture.getTextureFormat().vk(),
                texture.getWidth(),
                texture.getHeight(),
                readWrite
        );
    }

    private final class NgxDispatchResources implements AutoCloseable {
        private final NgxVKDLSSEvalParams evalParams = new NgxVKDLSSEvalParams();
        private NgxResourceVK color;
        private NgxResourceVK depth;
        private NgxResourceVK motionVectors;
        private NgxResourceVK exposure;
        private NgxResourceVK output;

        private NgxDispatchResources(FrameResourcesSet frameResourcesSet) {
            try {
                color = createNgxTextureResource(frameResourcesSet.vulkan(Color), true);
                depth = createNgxTextureResource(frameResourcesSet.vulkan(Depth), false);
                motionVectors = createNgxTextureResource(frameResourcesSet.vulkan(MotionVectors), true);
                exposure = createNgxTextureResource(frameResourcesSet.vulkan(Exposure), false);
                output = createNgxTextureResource(frameResourcesSet.vulkan(OutputColor), true);

                evalParams.feature.inputColor = color;
                evalParams.feature.output = output;
                evalParams.depth = depth;
                evalParams.motionVectors = motionVectors;
                evalParams.exposureTexture = exposure;
            } catch (RuntimeException | Error e) {
                close();
                throw e;
            }
        }

        @Override
        public void close() {
            closeResource(output);
            closeResource(exposure);
            closeResource(motionVectors);
            closeResource(depth);
            closeResource(color);
            output = null;
            exposure = null;
            motionVectors = null;
            depth = null;
            color = null;
        }
    }
}
