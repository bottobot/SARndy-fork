/***********************************************************************
WaterTable2 - Class to simulate water flowing over a surface using
improved water flow simulation based on Saint-Venant system of partial
differenctial equations.
Copyright (c) 2012-2024 Oliver Kreylos

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

#ifndef WATERTABLE2_INCLUDED
#define WATERTABLE2_INCLUDED

#include <vector>
#include <Misc/FunctionCalls.h>
#include <Geometry/Point.h>
#include <Geometry/Box.h>
#include <Geometry/OrthonormalTransformation.h>
#include <GL/gl.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/GLObject.h>
#include <GL/GLContextData.h>

#include "Types.h"
#include "Shader.h"

/* Forward declarations: */
class TextureTracker;
class DepthImageRenderer;
class PropertyGridCreator;

typedef Misc::FunctionCall<GLContextData&> AddWaterFunction; // Type for render functions called to locally add water to the water table

class WaterTable2:public GLObject
	{
	/* Embedded classes: */
	public:
	typedef Geometry::Box<Scalar,3> Box;
	typedef Geometry::OrthonormalTransformation<Scalar,3> ONTransform;
	
	enum Mode // Enumerated type for water simulation modes
		{
		Traditional=0, // Water simulation mode using simple attenuation
		Engineering // Water simulation mode using per-cell roughness coefficients and absorption rates
		};
	
	private:
	struct DataItem:public GLObject::DataItem // Structure holding per-context state
		{
		/* Elements: */
		public:
		GLuint bathymetryTextureObjects[2]; // Double-buffered one-component float color texture object holding the vertex-centered bathymetry grid
		bool bathymetryTextureLinears[2]; // Flags whether the bathymetry textures are currently set up for linear sampling
		int currentBathymetry; // Index of bathymetry texture containing the most recent bathymetry grid
		unsigned int bathymetryVersion; // Version number of the most recent bathymetry grid
		GLuint quantityTextureObjects[3]; // Double-buffered three-component color texture object holding the cell-centered conserved quantity grid (w, hu, hv)
		bool quantityTextureLinears[3]; // Flags whether the quantity textures are currently set up for linear sampling
		int currentQuantity; // Index of quantity texture containing the most recent conserved quantity grid
		GLuint derivativeTextureObject; // Three-component color texture object holding the cell-centered temporal derivative grid
		GLuint maxStepSizeTextureObjects[2]; // Double-buffered one-component color texture objects to gather the maximum step size for Runge-Kutta integration steps
		GLuint waterTextureObject; // One-component color texture object to add or remove water to/from the conserved quantity grid
		GLuint bathymetryFramebufferObject; // Frame buffer used to render the bathymetry surface into the bathymetry grid
		GLuint derivativeFramebufferObject; // Frame buffer used for temporal derivative computation
		GLuint maxStepSizeFramebufferObject; // Frame buffer used to calculate the maximum integration step size
		GLuint integrationFramebufferObject; // Frame buffer used for the Euler and Runge-Kutta integration steps
		GLuint waterFramebufferObject; // Frame buffer used for the water rendering step
		Shader bathymetryShader; // Shader to update cell-centered conserved quantities after a change to the bathymetry grid
		Shader waterAdaptShader; // Shader to adapt a new conserved quantity grid to the current bathymetry grid
		Shader derivativeShaders[2]; // Shaders to compute face-centered partial fluxes and cell-centered temporal derivatives, depending on simulation mode
		Shader maxStepSizeShader; // Shader to compute a maximum step size for a subsequent Runge-Kutta integration step
		Shader boundaryShader; // Shader to enforce boundary conditions on the quantities grid
		Shader eulerStepShaders[2]; // Shaders to compute an Euler integration step, depending on simulation mode
		Shader rungeKuttaStepShaders[2]; // Shaders to compute a Runge-Kutta integration step, depending on simulation mode
		Shader waterAddShader; // Shader to render water adder objects
		Shader waterShader; // Shader to add or remove water from the conserved quantities grid
		
		/* Constructors and destructors: */
		DataItem(void);
		virtual ~DataItem(void);
		
		/* New methods: */
		void bindBathymetry(TextureTracker& textureTracker,Shader& shader,int index); // Binds the bathymetry texture of the given index to the given shader using the given texture tracker and sets it up for nearest-neighbor sampling
		void bindQuantity(TextureTracker& textureTracker,Shader& shader,int index); // Binds the quantity texture of the given index to the given shader using the given texture tracker and sets it up for nearest-neighbor sampling
		};
	
	/* Elements: */
	Size size; // Width and height of water table in pixels
	const DepthImageRenderer* depthImageRenderer; // Renderer object used to update the water table's bathymetry grid
	ONTransform baseTransform; // Transformation from camera space to upright elevation map space
	Box domain; // Domain of elevation map space in rotated camera space
	GLfloat cellSize[2]; // Width and height of water table cells in world coordinate units
	PTransform bathymetryPmv; // Combined projection and modelview matrix to render the current surface into the bathymetry grid
	PTransform waterAddPmv; // Combined projection and modelview matrix to render water-adding geometry into the water grid
	GLfloat waterAddPmvMatrix[16]; // Same, in GLSL-compatible format
	GLfloat theta; // Coefficient for minmod flux-limiting differential operator
	GLfloat g; // Gravitiational acceleration constant
	GLfloat epsilon; // Coefficient for desingularizing division operator
	GLfloat maxPropagationSpeed[2]; // Maximum propagation speeds in x and y to guarantee minimum step size
	Mode mode; // Current water simulation mode
	GLfloat attenuation; // Attenuation factor for partial discharges
	PropertyGridCreator* propertyGridCreator; // Pointer to property grid creation object used in engineering mode
	GLfloat maxStepSize; // Maximum step size for each Runge-Kutta integration step
	PTransform waterTextureTransform; // Projective transformation from camera space to water level texture space
	GLfloat waterTextureTransformMatrix[16]; // Same in GLSL-compatible format
	std::vector<const AddWaterFunction*> renderFunctions; // A list of functions that are called after each water flow simulation step to locally add or remove water from the water table
	GLfloat waterDeposit; // A fixed amount of water added at every iteration of the flow simulation, for evaporation etc.
	bool dryBoundary; // Flag whether to enforce dry boundary conditions at the end of each simulation step
	
	/* Private methods: */
	void calcTransformations(void); // Calculates derived transformations
	GLfloat calcDerivative(GLContextData& contextData,TextureTracker& textureTracker,int quantityTextureIndex,bool calcMaxStepSize) const; // Calculates the temporal derivative of the conserved quantities in the given texture object and returns maximum step size if flag is true
	
	/* Constructors and destructors: */
	public:
	WaterTable2(const Size& sSize,const GLfloat sCellSize[2]); // Creates water table for offline simulation
	WaterTable2(const Size& sSize,const DepthImageRenderer* sDepthImageRenderer,const Point basePlaneCorners[4]); // Creates a water table of the given size in pixels, for the base plane quadrilateral defined by the depth image renderer's plane equation and four corner points
	virtual ~WaterTable2(void);
	
	/* Methods from GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* New methods: */
	const Size& getSize(void) const // Returns the size of the water table
		{
		return size;
		}
	const ONTransform& getBaseTransform(void) const // Returns the transformation from camera space to upright elevation map space
		{
		return baseTransform;
		}
	const Box& getDomain(void) const // Returns the water table's domain in rotated camera space
		{
		return domain;
		}
	const GLfloat* getCellSize(void) const // Returns the water table's cell size
		{
		return cellSize;
		}
	Mode getMode(void) const // Returns the current water simulation mode
		{
		return mode;
		}
	GLfloat getAttenuation(void) const // Returns the attenuation factor for partial discharges
		{
		return attenuation;
		}
	bool getDryBoundary(void) const // Returns true if dry boundaries are enforced after every simulation step
		{
		return dryBoundary;
		}
	void setElevationRange(Scalar newMin,Scalar newMax); // Sets the range of possible elevations in the water table
	void setMode(Mode newMode); // Sets the water simulation mode
	void setAttenuation(GLfloat newAttenuation); // Sets the attenuation factor for partial discharges
	void setPropertyGridCreator(PropertyGridCreator* newPropertyGridCreator); // Sets the property grid creator to be used in engineering mode
	void forceMinStepSize(GLfloat newMinStepSize); // Forces the given minimum step size for all subsequent integration steps by limiting cell fluxes
	void setMaxStepSize(GLfloat newMaxStepSize); // Sets the maximum step size for all subsequent integration steps
	const PTransform& getWaterTextureTransform(void) const // Returns the matrix transforming from camera space into water texture space
		{
		return waterTextureTransform;
		}
	void addRenderFunction(const AddWaterFunction* newRenderFunction); // Adds a render function to the list; object remains owned by caller
	void removeRenderFunction(const AddWaterFunction* removeRenderFunction); // Removes the given render function from the list but does not delete it
	GLfloat getWaterDeposit(void) const // Returns the current amount of water deposited on every simulation step
		{
		return waterDeposit;
		}
	void setWaterDeposit(GLfloat newWaterDeposit); // Sets the amount of deposited water
	void setDryBoundary(bool newDryBoundary); // Enables or disables enforcement of dry boundaries
	void updateBathymetry(GLContextData& contextData,TextureTracker& textureTracker) const; // Prepares the water table for subsequent calls to the runSimulationStep() method
	void updateBathymetry(const GLfloat* bathymetryGrid,GLContextData& contextData,TextureTracker& textureTracker) const; // Updates the bathymetry directly with a vertex-centered elevation grid of grid size minus 1
	void setWaterLevel(const GLfloat* waterGrid,GLContextData& contextData,TextureTracker& textureTracker) const; // Sets the current water level to the given grid, and resets flux components to zero
	GLfloat runSimulationStep(bool forceStepSize,GLContextData& contextData,TextureTracker& textureTracker) const; // Runs a water flow simulation step, always uses maxStepSize if flag is true (may lead to instability); returns step size taken by Runge-Kutta integration step
	void uploadWaterTextureTransform(Shader& shader) const; // Uploads the water texture transformation into the GLSL 4x4 matrix at the next uniform location in the given shader
	GLint bindBathymetryTexture(GLContextData& contextData,TextureTracker& textureTracker,bool linearSampling) const; // Binds the bathymetry texture object to the next available texture unit in the given texture tracker and sets filtering mode to linear if flag is true; returns the used texture unit's index
	GLint bindQuantityTexture(GLContextData& contextData,TextureTracker& textureTracker,bool linearSampling) const; // Binds the most recent conserved quantities texture object to the next available texture unit in the given texture tracker and sets filtering mode to linear if flag is true; returns the used texture unit's index
	void readBathymetryTexture(GLContextData& contextData,TextureTracker& textureTracker,GLfloat* buffer) const; // Reads the current bathymetry texture into the given buffer
	void readQuantityTexture(GLContextData& contextData,TextureTracker& textureTracker,GLenum components,GLfloat* buffer) const; // Reads the given component(s) of the current quantity texture into the given buffer
	Size getBathymetrySize(void) const // Returns the width or height of the bathymetry grid
		{
		return Size(size[0]-1,size[1]-1);
		}
	unsigned int getBathymetrySize(int index) const // Ditto
		{
		return size[index]-1;
		}
	};

#endif
