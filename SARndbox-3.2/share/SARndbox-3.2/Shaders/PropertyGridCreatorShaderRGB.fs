/***********************************************************************
PropertyGridCreatorShader - Shader to create a water simulation property
grid by sampling a color camera image in RGB color space from a 3D
bathymetry model.
Copyright (c) 2023-2024 Oliver Kreylos

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

#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect colorImageSampler; // Sampler for the current color camera image
uniform sampler2DRect bathymetrySampler; // Sampler for the current bathymetry grid
uniform mat4 bathymetryColorMatrix; // Projection matrix from bathymetry space to color camera image space
uniform float roughness,absorption; // Desired roughness and absorption rate values

void main()
	{
	/* Sample the current vertex-centered bathymetry grid: */
	float bathymetryZ=texture2DRect(bathymetrySampler,vec2(gl_FragCoord.x-0.5,gl_FragCoord.y-0.5)).r;
	
	/* Calculate the current 3D bathymetry vertex: */
	vec4 bathymetryVertex=vec4(gl_FragCoord.x,gl_FragCoord.y,bathymetryZ,1.0);
	
	/* Project the bathymetry vertex to color image space: */
	vec4 colorVertex=bathymetryColorMatrix*bathymetryVertex;
	
	/* Sample the color image: */
	vec3 color=texture2DRectProj(colorImageSampler,colorVertex).rgb;
	
	/* Discard the fragment if it is not selected: */
	if(dot(color,vec3(0.2126,0.7152,0.0722))>0.5)
		discard;
	
	/* Assign the desired property values if the fragment is selected: */
	gl_FragData[0]=vec4(roughness,absorption,0.0,0.0);
	}
