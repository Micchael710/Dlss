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

package io.homo.superresolution.shadercompat;

import org.lwjgl.opengl.GL11;

import static org.lwjgl.opengl.GL11.*;

class GlTextureInfoGetter {
    public static int getLevelParameter(int target, int name, int parameter) {
        int previous = glGetInteger(target == GL_TEXTURE_2D ? GL_TEXTURE_BINDING_2D : GL_TEXTURE_BINDING_1D);
        glBindTexture(target, name);
        try {
            return glGetTexLevelParameteri(target, 0, parameter);
        } finally {
            glBindTexture(target, previous);
        }
    }

    /** Resolve legacy unsized depth storage from its actual component precision. */
    public static int getSizedInternalFormat(int target, int name) {
        int format = getInternalFormat(target, name);
        if (format != GL_DEPTH_COMPONENT) return format;
        int bits = getLevelParameter(target, name, org.lwjgl.opengl.GL14.GL_TEXTURE_DEPTH_SIZE);
        int type = getLevelParameter(target, name, org.lwjgl.opengl.GL30.GL_TEXTURE_DEPTH_TYPE);
        int sized = switch (bits) {
            case 16 -> org.lwjgl.opengl.GL14.GL_DEPTH_COMPONENT16;
            case 24 -> org.lwjgl.opengl.GL14.GL_DEPTH_COMPONENT24;
            case 32 -> type == GL_FLOAT ? org.lwjgl.opengl.GL30.GL_DEPTH_COMPONENT32F
                    : org.lwjgl.opengl.GL14.GL_DEPTH_COMPONENT32;
            default -> throw new IllegalArgumentException("Unsupported unsized depth precision: " + bits);
        };
        return sized;
    }

    public static int getInternalFormat(int target, int name) {
        int prevTex = glGetInteger(target == GL11.GL_TEXTURE_2D ? GL_TEXTURE_BINDING_2D : GL_TEXTURE_BINDING_1D);
        glBindTexture(target, name);
        int[] params = new int[1];
        //GL42.glGetInternalformativ()
        GL11.glGetTexLevelParameteriv(target, 0, GL11.GL_TEXTURE_INTERNAL_FORMAT, params);
        
        glBindTexture(target, prevTex);
        return params[0];
    }

    public static int getWidth(int target, int name) {
        int prevTex = glGetInteger(target == GL11.GL_TEXTURE_2D ? GL_TEXTURE_BINDING_2D : GL_TEXTURE_BINDING_1D);
        glBindTexture(target, name);
        int[] params = new int[1];
        GL11.glGetTexLevelParameteriv(target, 0, GL11.GL_TEXTURE_WIDTH, params);
        glBindTexture(target, prevTex);
        return params[0];
    }

    public static int getHeight(int target, int name) {
        int prevTex = glGetInteger(target == GL11.GL_TEXTURE_2D ? GL_TEXTURE_BINDING_2D : GL_TEXTURE_BINDING_1D);
        glBindTexture(target, name);
        int[] params = new int[1];
        GL11.glGetTexLevelParameteriv(target, 0, GL11.GL_TEXTURE_HEIGHT, params);
        glBindTexture(target, prevTex);
        return params[0];
    }
}
