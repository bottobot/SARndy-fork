/***********************************************************************
WaterRenderingShader - Shader to render the water level surface of a
water table.
Copyright (c) 2014-2019 Oliver Kreylos

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#version 120 // Specify GLSL version for compatibility

uniform sampler2DRect quantitySampler; // Sampler for the water level
uniform mat4 projectionModelviewGridMatrix; // Transformation matrix

varying vec2 vTexCoord; // Pass texture coordinates to the fragment shader

void main() {
    vec4 vertexGc = gl_Vertex; // Get the current vertex position
    vertexGc.z = texture2DRect(quantitySampler, vertexGc.xy).r; // Adjust z-coordinate based on water level

    vTexCoord = vertexGc.xy; // Pass the texture coordinates to the fragment shader

    gl_Position = projectionModelviewGridMatrix * vertexGc; // Transform vertex to clip space
}
