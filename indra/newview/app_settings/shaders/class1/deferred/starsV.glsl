/**
 * @file starsV.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
 * Copyright (C) 2025, William Weaver (paperwork) @ Second Life
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

uniform mat4 texture_matrix0;
uniform mat4 modelview_projection_matrix;
uniform float time;

// Inputs
in vec3 position;
in vec4 diffuse_color;
in vec2 texcoord0;
in float weight;         // Star intensity input

// Outputs
out vec4 vertex_color;
out vec2 vary_texcoord0;
out vec2 screenpos;
out float vary_intensity;
out vec3 vary_worldDir;  // World direction vector for horizon dimming

void main()
{
    //transform vertex
    vec4 pos = modelview_projection_matrix * vec4(position, 1.0);

    // smash to far clip plane to
    // avoid rendering on top of moon (do NOT write to gl_FragDepth, it's slow)
    pos.z = pos.w;

    gl_Position = pos;

    // Stable position derived from vertex position (used for noise hash in FS)
    screenpos = position.xy;

    // Calculate texture coordinates
    vary_texcoord0 = (texture_matrix0 * vec4(texcoord0,0,1)).xy;

    // Pass vertex color and intensity
    vertex_color = diffuse_color;
    vary_intensity = weight;

    // World direction (normalized object position) for horizon effects
    vary_worldDir = normalize(position);
}

