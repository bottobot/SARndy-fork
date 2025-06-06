/***********************************************************************
SurfaceRenderer - Class to render a surface defined by a regular grid in
depth image space.
Copyright (c) 2012-2023 Oliver Kreylos

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

#ifndef SURFACERENDERER_INCLUDED
#define SURFACERENDERER_INCLUDED

#include <Misc/Rect.h>
#include <IO/FileMonitor.h>
#include <Geometry/ProjectiveTransformation.h>
#include <Geometry/Plane.h>
#include <GL/gl.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/GLObject.h>
#include <Kinect/Types.h>
#include <Kinect/FrameBuffer.h>

#include "Types.h"
#include "Shader.h"

/* Forward declarations: */
class TextureTracker;
class DepthImageRenderer;
class ElevationColorMap;
class GLLightTracker;
class DEM;
class WaterTable2;

class SurfaceRenderer:public GLObject
	{
	/* Embedded classes: */
	public:
	typedef Kinect::Size Size;
	typedef Misc::Rect<2> Rect;
	typedef Geometry::Plane<GLfloat,3> Plane; // Type for plane equations
	
	private:
	struct DataItem:public GLObject::DataItem
		{
		/* Elements: */
		public:
		Size contourLineFramebufferSize; // Current width and height of contour line rendering frame buffer
		GLuint contourLineFramebufferObject; // Frame buffer object used to render topographic contour lines
		GLuint contourLineDepthBufferObject; // Depth render buffer for topographic contour line frame buffer
		GLuint contourLineColorTextureObject; // Color texture object for topographic contour line frame buffer
		unsigned int contourLineVersion; // Version number of depth image used for contour line generation
		Shader heightMapShader; // Shader program to render the surface using a height color map
		unsigned int surfaceSettingsVersion; // Version number of surface settings for which the height map shader was built
		unsigned int lightTrackerVersion; // Version number of light tracker state for which the height map shader was built
		Shader globalAmbientHeightMapShader; // Shader program to render the global ambient component of the surface using a height color map
		Shader shadowedIlluminatedHeightMapShader; // Shader program to render the surface using illumination with shadows and a height color map
		
		/* Constructors and destructors: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	/* Elements: */
	const DepthImageRenderer* depthImageRenderer; // Renderer for low-level surface rendering
	Size depthImageSize; // Size of depth image texture
	PTransform tangentDepthProjection; // Transposed depth projection matrix for tangent planes, i.e., homogeneous normal vectors
	IO::FileMonitor fileMonitor; // Monitor to watch the renderer's external shader source files
	
	bool drawContourLines; // Flag if topographic contour lines are enabled
	GLfloat contourLineFactor; // Inverse elevation distance between adjacent topographic contour lines
	
	ElevationColorMap* elevationColorMap; // Pointer to a color map for topographic elevation map coloring
	
	bool drawDippingBed; // Flag to draw a potentially dipping bedding plane
	bool dippingBedFolded; // Flag whether the dipping bed is folded or planar
	Plane dippingBedPlane; // Plane equation of the planar dipping bed
	GLfloat dippingBedCoeffs[5]; // Coefficients of folded dipping bed
	GLfloat dippingBedThickness; // Thickness of dipping bed in camera-space units
	
	DEM* dem; // Pointer to a pre-made digital elevation model to create a zero-surface for height color mapping
	GLfloat demDistScale; // Maximum deviation from surface to DEM in camera-space units
	
	bool illuminate; // Flag whether the surface shall be illuminated
	
	WaterTable2* waterTable; // Pointer to the water table object; if NULL, water is ignored
	bool advectWaterTexture; // Flag whether water texture coordinates are advected to visualize water flow
	GLfloat waterOpacity; // Scaling factor for water opacity
	GLfloat waterColor[3]; // RGB color for water rendering
	GLfloat waterReflectionColor[3]; // RGB color for water reflections
	
	unsigned int surfaceSettingsVersion; // Version number of surface settings to invalidate surface rendering shader on changes
	double animationTime; // Time value for water animation
	
	/* Texture warping system elements: */
	GLuint warpTextureObject; // Texture object for the warping texture
	bool useWarpTexture; // Flag to enable/disable texture warping
	float warpIntensity; // How much the texture warps (0.0 - 1.0)
	float textureScale; // Base scale of the texture
	float gradientThreshold; // Minimum gradient to start warping
	int warpMode; // 0=contour follow, 1=radial, 2=flow
	int textureBlendMode; // 0=multiply, 1=overlay, 2=add, 3=replace
	float textureOpacity; // Overall texture strength
	
	/* Private methods: */
	void shaderSourceFileChanged(const IO::FileMonitor::Event& event); // Callback called when one of the external shader source files is changed
	void updateSinglePassSurfaceShader(const GLLightTracker& lt,DataItem* dataItem) const; // Updates the given single-pass surface rendering shader based on current renderer settings
	void renderPixelCornerElevations(const Rect& viewport,const PTransform& projectionModelview,GLContextData& contextData,TextureTracker& textureTracker,DataItem* dataItem) const; // Creates texture containing pixel-corner elevations based on the current depth image
	
	/* Constructors and destructors: */
	public:
	SurfaceRenderer(const DepthImageRenderer* sDepthImageRenderer); // Creates a renderer for the given depth image renderer
	
	/* Methods from class GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* New methods: */
	void setDrawContourLines(bool newDrawContourLines); // Enables or disables topographic contour lines
	void setContourLineDistance(GLfloat newContourLineDistance); // Sets the elevation distance between adjacent topographic contour lines
	void setElevationColorMap(ElevationColorMap* newElevationColorMap); // Sets an elevation color map
	void setDrawDippingBed(bool newDrawDippingBed); // Sets the dipping bed flag
	void setDippingBedPlane(const Plane& newDippingBedPlane); // Sets the dipping bed plane equation
	void setDippingBedCoeffs(const GLfloat newDippingBedCoeffs[5]); // Sets folding dipping bed's coefficients
	void setDippingBedThickness(GLfloat newDippingBedThickness); // Sets the thickness of the dipping bed in camera-space units
	void setDem(DEM* newDem); // Sets a pre-made digital elevation model to create a zero surface for height color mapping
	void setDemDistScale(GLfloat newDemDistScale); // Sets the deviation from DEM to surface to saturate the deviation color map
	void setIlluminate(bool newIlluminate); // Sets the illumination flag
	void setWaterTable(WaterTable2* newWaterTable); // Sets the pointer to the water table; NULL disables water handling
	void setAdvectWaterTexture(bool newAdvectWaterTexture); // Sets the water texture coordinate advection flag
	void setWaterOpacity(GLfloat newWaterOpacity); // Sets the water opacity factor
	void setWaterColor(const GLfloat newWaterColor[3]); // Sets the water color (RGB values 0.0-1.0)
	void setWaterReflectionColor(const GLfloat newWaterReflectionColor[3]); // Sets the water reflection color (RGB values 0.0-1.0)
	void setAnimationTime(double newAnimationTime); // Sets the time for water animation in seconds
	
	/* Texture warping system methods: */
	void setUseWarpTexture(bool enable); // Enable/disable texture warping
	void loadWarpTexture(const char* filename); // Load a texture image file
	void setWarpIntensity(float intensity); // Set warping strength (0.0-1.0)
	void setTextureScale(float scale); // Set base texture scale
	void setGradientThreshold(float threshold); // Set minimum gradient to start warping
	void setWarpMode(int mode); // Set warping mode (0=contour follow, 1=radial, 2=flow)
	void setTextureBlendMode(int mode); // Set texture blend mode (0=multiply, 1=overlay, 2=add, 3=replace)
	void setTextureOpacity(float opacity); // Set overall texture visibility (0.0-1.0)
	
	void renderSinglePass(const Rect& viewport,const PTransform& projection,const OGTransform& modelview,GLContextData& contextData,TextureTracker& textureTracker) const; // Renders the surface in a single pass using the current surface settings
	#if 0
	void renderGlobalAmbientHeightMap(GLuint heightColorMapTexture,GLContextData& contextData) const; // Renders the global ambient component of the surface as an illuminated height map in the current OpenGL context using the given pixel-corner elevation texture and 1D height color map
	void renderShadowedIlluminatedHeightMap(GLuint heightColorMapTexture,GLuint shadowTexture,const PTransform& shadowProjection,GLContextData& contextData) const; // Renders the surface as an illuminated height map in the current OpenGL context using the given pixel-corner elevation texture and 1D height color map
	#endif
	};

#endif
