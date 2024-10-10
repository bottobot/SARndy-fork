/***********************************************************************
PropertyGridCreator - Class to create grids of properties (inundation,
infiltration, surface roughness, ...) for water flow simulation by
mapping the water simulation bathymetry into color camera space.
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

#ifndef PROPERTYGRIDCREATOR_INCLUDED
#define PROPERTYGRIDCREATOR_INCLUDED

#include <string>
#include <GL/gl.h>
#include <GL/GLObject.h>
#include <Kinect/FrameBuffer.h>

#include "Types.h"
#include "Shader.h"

/* Forward declarations: */
namespace Kinect {
class FrameSource;
}
class TextureTracker;
class WaterTable2;

class PropertyGridCreator:public GLObject
	{
	/* Embedded classes: */
	private:
	struct DataItem:public GLObject::DataItem
		{
		/* Elements: */
		public:
		GLuint createdGridTextureObject; // Texture object holding the most recently created property grid
		unsigned int globalParametersVersions[2]; // Version number of global parameters represented in current property grid texture
		GLuint colorImageTextureObject; // Texture object holding the current color image
		unsigned int colorImageVersion; // Version of color image currently in the color image texture
		GLuint createdGridFramebufferObject; // Framebuffer used to render into the created property grid
		Shader gridResetShader; // Shader to reset the property grid to current global properties
		Shader gridCreatorShader; // Shader to calculate a property grid based on the current bathymetry and color frame
		unsigned int loadGridRequest,saveGridRequest; // Counter for grid loading/saving requests
		
		/* Constructors and destructors: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	/* Elements: */
	const WaterTable2& waterTable; // Reference to the water simulation object
	Size gridSize; // Size of generated property grids, equal to and aligned with water simulation object's quantity grid
	PTransform bathymetryToColor; // Projection from bathymetry space into color camera space
	GLfloat bathymetryToColorMatrix[16]; // Ditto, in GLSL-compatible format
	GLfloat roughness,absorption; // Most recently set global parameters
	unsigned int globalParametersVersions[2]; // Version numbers of global parameters
	bool colorIsYuv; // Flag whether incoming color frames are in Y'CbCr color space
	Size colorImageSize; // Size of incoming color frames
	Kinect::FrameBuffer colorImage; // The most recent color image
	unsigned int colorImageVersion; // Version of color image
	unsigned int requestState; // State of a current property grid creation request; active if value >0
	unsigned int requestMask; // Bit mask of requested properties; 0x1 - roughness, 0x2 - absorption rate
	GLfloat requestRoughness,requestAbsorption; // Requested roughness and absorption rate
	unsigned int loadGridRequest; // Counter for grid loading requests
	std::string loadGridFileName; // Name of file from which to load a grid
	unsigned int saveGridRequest; // Counter for grid saving requests
	std::string saveGridFileName; // Name of file to which to save a grid
	
	/* Constructors and destructors: */
	public:
	PropertyGridCreator(const WaterTable2& sWaterTable,Kinect::FrameSource& frameSource); // Creates a property grid creator for the given water simulation object and color/depth frame source
	virtual ~PropertyGridCreator(void);
	
	/* Methods from class GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* New methods: */
	GLfloat getRoughness(void) const // Returns the current global surface roughness
		{
		return roughness;
		}
	GLfloat getAbsorption(void) const // Returns the current global surface absorption rate
		{
		return absorption;
		}
	void setRoughness(GLfloat newRoughness); // Globally resets the grid property to the given roughness
	void setAbsorption(GLfloat newAbsorption); // Globally resets the grid property to the given absorption rate
	GLint bindPropertyGridTexture(GLContextData& contextData,TextureTracker& textureTracker) const; // Binds the property grid texture object to the next available texture unit in the given texture tracker; returns the used texture unit's index
	void receiveRawFrame(const Kinect::FrameBuffer& frameBuffer); // Receives a new color frame from the camera
	bool requestRoughnessGrid(GLfloat newRequestRoughness); // Requests creation of a roughness property grid; returns true if request was granted
	bool requestAbsorptionGrid(GLfloat newRequestAbsorption); // Requests creation of an absorption rate property grid; returns true if request was granted
	bool isRequestActive(void) const // Returns true if there is an active property grid request
		{
		return requestState!=0U;
		}
	void loadGrid(const char* gridFileName); // Requests to load a property grid from the given image file
	void saveGrid(const char* gridFileName); // Requests to save the property grid to the given image file
	void updatePropertyGrid(GLContextData& contextData,TextureTracker& textureTracker) const; // Updates the property grid based on global and/or local requests
	};

#endif
