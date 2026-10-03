package io.homo.superresolution.shadercompat;

import io.homo.superresolution.common.SuperResolution;
import io.homo.superresolution.common.minecraft.handler.RenderHandlerManager;
import io.homo.superresolution.core.graphics.impl.texture.ITexture;
import org.lwjgl.opengl.GL11;
import org.lwjgl.opengl.GL30;
import org.lwjgl.opengl.GL45;
import org.lwjgl.system.MemoryStack;

import java.util.HashMap;
import java.util.Map;

/** Opt-in, read-only GL diagnostics. No bindings or viewport are changed. */
public final class ShaderPipelineDiagnostics {
    private static final boolean ENABLED = Boolean.getBoolean("sr.baselineDiagnostics");
    private static final Map<String, Long> LAST = new HashMap<>();
    private ShaderPipelineDiagnostics() {}

    private static boolean sample(String key) {
        if (!ENABLED) return false;
        long now = System.nanoTime();
        Long previous = LAST.get(key);
        if (previous != null && now - previous < 10_000_000_000L) return false;
        LAST.put(key, now);
        return true;
    }

    public static void state(String stage) {
        if (!sample("state:" + stage)) return;
        try (MemoryStack stack = MemoryStack.stackPush()) {
            var viewport = stack.callocInt(4);
            var scissor = stack.callocInt(4);
            GL11.glGetIntegerv(GL11.GL_VIEWPORT, viewport);
            GL11.glGetIntegerv(GL11.GL_SCISSOR_BOX, scissor);
            int draw = GL11.glGetInteger(GL30.GL_DRAW_FRAMEBUFFER_BINDING);
            int read = GL11.glGetInteger(GL30.GL_READ_FRAMEBUFFER_BINDING);
            SuperResolution.LOGGER.info("SR_DIAG {} render={} display={} region-1={} region-2={} viewport=[{},{},{},{}] scissor=[{},{},{},{}]/{} drawFbo={} readFbo={} drawBuffer={} readBuffer={} drawStatus={}",
                    stage, RenderHandlerManager.getRenderSize(), RenderHandlerManager.getScreenSize(),
                    RenderHandlerManager.getRenderSize(), RenderHandlerManager.getScreenSize(),
                    viewport.get(0), viewport.get(1), viewport.get(2), viewport.get(3),
                    scissor.get(0), scissor.get(1), scissor.get(2), scissor.get(3), GL11.glIsEnabled(GL11.GL_SCISSOR_TEST),
                    draw, read, GL11.glGetInteger(GL11.GL_DRAW_BUFFER), GL11.glGetInteger(GL11.GL_READ_BUFFER),
                    Integer.toHexString(GL30.glCheckFramebufferStatus(GL30.GL_DRAW_FRAMEBUFFER)));
            attachment(stage, draw, GL30.GL_COLOR_ATTACHMENT0);
            attachment(stage, draw, GL30.GL_DEPTH_ATTACHMENT);
        }
    }

    private static void attachment(String stage, int fbo, int attachment) {
        if (fbo == 0) return; // Default framebuffer has no texture attachment to query.
        int type = GL45.glGetNamedFramebufferAttachmentParameteri(fbo, attachment, GL30.GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE);
        if (type != GL11.GL_TEXTURE) return;
        int texture = GL45.glGetNamedFramebufferAttachmentParameteri(fbo, attachment, GL30.GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME);
        int mip = GL45.glGetNamedFramebufferAttachmentParameteri(fbo, attachment, GL30.GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL);
        SuperResolution.LOGGER.info("SR_DIAG {} fbo={} attachment={} texture={} mip={} size={}x{} internalFormat={}", stage, fbo,
                Integer.toHexString(attachment), texture, mip,
                GL45.glGetTextureLevelParameteri(texture, mip, GL11.GL_TEXTURE_WIDTH),
                GL45.glGetTextureLevelParameteri(texture, mip, GL11.GL_TEXTURE_HEIGHT),
                Integer.toHexString(GL45.glGetTextureLevelParameteri(texture, mip, GL11.GL_TEXTURE_INTERNAL_FORMAT)));
    }

    public static void texture(String name, ITexture texture) {
        if (!sample("texture:" + name)) return;
        if (texture == null) {
            SuperResolution.LOGGER.info("SR_DIAG texture {} MISSING", name);
        } else {
            SuperResolution.LOGGER.info("SR_DIAG texture {} id={} size={}x{} format={} target/type={} glInternalFormat={}", name,
                    texture.handle(), texture.getWidth(), texture.getHeight(), texture.getTextureFormat(), texture.getTextureType(),
                    Integer.toHexString(GL45.glGetTextureLevelParameteri((int) texture.handle(), 0, GL11.GL_TEXTURE_INTERNAL_FORMAT)));
            if (texture.getTextureFormat().isDepth()) {
                SuperResolution.LOGGER.info("SR_DIAG depth {} id={} bits={} componentType={}", name, texture.handle(),
                        GL45.glGetTextureLevelParameteri((int) texture.handle(), 0, org.lwjgl.opengl.GL14.GL_TEXTURE_DEPTH_SIZE),
                        Integer.toHexString(GL45.glGetTextureLevelParameteri((int) texture.handle(), 0, GL30.GL_TEXTURE_DEPTH_TYPE)));
            }
        }
    }
}
