/***********************************************************************
Sandbox - Vrui application to drive an augmented reality sandbox.
Copyright (c) 2012-2025 Oliver Kreylos

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

#ifndef SANDBOX_INCLUDED
#define SANDBOX_INCLUDED

#include <Threads/Mutex.h>
#include <Threads/TripleBuffer.h>
#include <Math/Interval.h>
#include <Geometry/Box.h>
#include <Geometry/Rotation.h>
#include <Geometry/OrthonormalTransformation.h>
#include <Geometry/ProjectiveTransformation.h>
#include <GL/gl.h>
#include <GL/GLColorMap.h>
#include <GL/GLMaterial.h>
#include <GL/GLObject.h>
#include <GL/GLGeometryVertex.h>
#include <GLMotif/ToggleButton.h>
#include <GLMotif/RadioBox.h>
#include <GLMotif/TextFieldSlider.h>
#include <GLMotif/FileSelectionHelper.h>
#include <Vrui/Tool.h>
#include <Vrui/GenericToolFactory.h>
#include <Vrui/TransparentObject.h>
#include <Vrui/Application.h>
#include <Kinect/FrameBuffer.h>
#include <Kinect/FrameSource.h>

#include "Types.h"

/* Forward declarations: */
namespace Misc {
template <class ParameterParam>
class FunctionCall;
}
class GLContextData;
namespace GLMotif {
class PopupMenu;
class PopupWindow;
class TextField;
}
namespace Vrui {
class Lightsource;
}
namespace Kinect {
class Camera;
}
class FrameFilter;
class DepthImageRenderer;
class ElevationColorMap;
class DEM;
class SurfaceRenderer;
class WaterTable2;
class PropertyGridCreator;
class HandExtractor;
typedef Misc::FunctionCall<GLContextData&> AddWaterFunction;
class RemoteServer;
class WaterRenderer;

class Sandbox:public Vrui::Application,public GLObject
	{
	/* Embedded classes: */
	private:
	typedef Geometry::Box<Scalar,3> Box; // Type for bounding boxes
	typedef Geometry::OrthonormalTransformation<Scalar,3> ONTransform; // Type for rigid body transformations
	typedef Kinect::FrameSource::DepthCorrection::PixelCorrection PixelDepthCorrection; // Type for per-pixel depth correction factors
	
	struct DataItem:public GLObject::DataItem
		{
		/* Elements: */
		public:
		double waterTableTime; // Simulation time stamp of the water table in this OpenGL context
		Size shadowBufferSize; // Size of the shadow rendering frame buffer
		GLuint shadowFramebufferObject; // Frame buffer object to render shadow maps
		GLuint shadowDepthTextureObject; // Depth texture for the shadow rendering frame buffer
		
		/* Constructors and destructors: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	struct GridRequest // Structure representing a request to read back bathymetry and/or water level grids from the GPU
		{
		/* Embedded classes: */
		public:
		typedef void (*CallbackFunction)(GLfloat*,GLfloat*,GLfloat*,void*); // Type for callback functions
		
		struct Request // Structure holding a request's parameters
			{
			/* Elements: */
			public:
			GLfloat* bathymetryBuffer; // Pointer to a buffer to hold the requested bathymetry grid if requested
			GLfloat* waterLevelBuffer; // Pointer to a buffer to hold the requested water level grid if requested
			GLfloat* snowHeightBuffer; // Pointer to a buffer to hold the requested snow height grid if requested
			CallbackFunction callback; // Function to call when the grid(s) has/have been read back
			void* callbackData; // Additional data element to pass to callback function
			
			/* Constructors and destructors: */
			Request(void) // Creates an inactive request
				:bathymetryBuffer(0),waterLevelBuffer(0),snowHeightBuffer(0),
				 callback(0),callbackData(0)
				{
				}
			
			/* Methods: */
			bool isActive(void) const // Returns true if there is a pending request
				{
				return callback!=0;
				}
			void complete(void) // Calls the read-back callback
				{
				(*callback)(bathymetryBuffer,waterLevelBuffer,snowHeightBuffer,callbackData);
				}
			};
		
		/* Elements: */
		Threads::Mutex mutex; // Mutex serializing access to the request structure
		Request currentRequest; // The currently pending grid request
		
		/* Constructors and destructors: */
		GridRequest(void) // Creates an inactive grid request
			{
			}
		
		/* Methods: */
		bool requestGrids(GLfloat* newBathymetryBuffer,GLfloat* newWaterLevelBuffer,GLfloat* newSnowHeightBuffer,CallbackFunction newCallback,void* newCallbackData) // Requests a grid read-back; returns true if request has been granted
			{
			Threads::Mutex::Lock lock(mutex);
			if(currentRequest.callback==0)
				{
				currentRequest.bathymetryBuffer=newBathymetryBuffer;
				currentRequest.waterLevelBuffer=newWaterLevelBuffer;
				currentRequest.snowHeightBuffer=newSnowHeightBuffer;
				currentRequest.callback=newCallback;
				currentRequest.callbackData=newCallbackData;
				return true;
				}
			else
				return false;
			}
		Request getRequest(void) // Returns the current grid request and deactivates it
			{
			Threads::Mutex::Lock lock(mutex);
			Request result=currentRequest;
			currentRequest.callback=0;
			return result;
			}
		};
	
	struct RenderSettings // Structure to hold per-window rendering settings
		{
		/* Elements: */
		public:
		bool fixProjectorView; // Flag whether to allow viewpoint navigation or always render from the projector's point of view
		PTransform projectorTransform; // The calibrated projector transformation matrix for fixed-projection rendering
		bool projectorTransformValid; // Flag whether the projector transformation is valid
		bool hillshade; // Flag whether to use augmented reality hill shading
		GLMaterial surfaceMaterial; // Material properties to render the surface in hill shading mode
		bool useShadows; // Flag whether to use shadows in augmented reality hill shading
		ElevationColorMap* elevationColorMap; // Pointer to an elevation color map
		bool useContourLines; // Flag whether to draw elevation contour lines
		GLfloat contourLineSpacing; // Spacing between adjacent contour lines in cm
		bool renderWaterSurface; // Flag whether to render the water surface as a geometric surface
		GLfloat waterOpacity; // Opacity factor for water when rendered as texture
		SurfaceRenderer* surfaceRenderer; // Surface rendering object for this window
		WaterRenderer* waterRenderer; // A renderer to render the water surface as geometry
		
		/* Constructors and destructors: */
		RenderSettings(void); // Creates default rendering settings
		RenderSettings(const RenderSettings& source); // Copy constructor
		~RenderSettings(void); // Destroys rendering settings
		
		/* Methods: */
		void loadProjectorTransform(const char* projectorTransformName); // Loads a projector transformation from the given file
		void loadHeightMap(const char* heightMapName); // Loads the selected height map
		};
	
	friend class GlobalWaterTool;
	friend class LocalWaterTool;
	friend class DEMTool;
	friend class BathymetrySaverTool;
	friend class RemoteServer;
	
	/* Elements: */
	private:
	RemoteServer* remoteServer; // A server to stream bathymetry and water level grids to remote clients
	Kinect::FrameSource* camera; // The Kinect camera device
	Size frameSize; // Width and height of the camera's depth frames
	PixelDepthCorrection* pixelDepthCorrection; // Buffer of per-pixel depth correction coefficients
	Kinect::FrameSource::IntrinsicParameters cameraIps; // Intrinsic parameters of the Kinect camera
	Math::Interval<double> elevationRange; // Range of valid elevations for topography relative to base plane
	FrameFilter* frameFilter; // Processing object to filter raw depth frames from the Kinect camera
	bool pauseUpdates; // Pauses updates of the topography
	Threads::TripleBuffer<Kinect::FrameBuffer> filteredFrames; // Triple buffer for incoming filtered depth frames
	DepthImageRenderer* depthImageRenderer; // Object managing the current filtered depth image
	ONTransform boxTransform; // Transformation from camera space to baseplane space (x along long sandbox axis, z up)
	Scalar boxSize; // Radius of sphere around sandbox area
	Box bbox; // Bounding box around all potential surfaces
	WaterTable2* waterTable; // Water flow simulation object
	double waterSpeed; // Relative speed of water flow simulation
	unsigned int waterMaxSteps; // Maximum number of water simulation steps per frame
	GLfloat rainStrength; // Amount of water deposited by rain tools and objects on each water simulation step
	PropertyGridCreator* propertyGridCreator; // Object to create water simulation property grids from color camera images
	HandExtractor* handExtractor; // Object to detect splayed hands above the sand surface to make rain
	const AddWaterFunction* addWaterFunction; // Render function registered with the water table
	bool addWaterFunctionRegistered; // Flag if the water adding function is currently registered with the water table
	mutable GridRequest gridRequest; // Structure holding pending grid read-back requests
	std::vector<RenderSettings> renderSettings; // List of per-window rendering settings
	Vrui::Lightsource* sun; // An external fixed light source
	DEM* activeDem; // The currently active DEM
	GLMotif::PopupMenu* mainMenu;
	GLMotif::ToggleButton* pauseUpdatesToggle;
	GLMotif::FileSelectionHelper gridPropertyFileHelper; // Helper object to load/save grid property from/to files
	GLMotif::PopupWindow* waterControlDialog;
	GLMotif::TextFieldSlider* snowLineSlider;
	GLMotif::TextFieldSlider* snowMeltSlider;
	GLMotif::TextFieldSlider* waterSpeedSlider;
	GLMotif::TextFieldSlider* waterMaxStepsSlider;
	GLMotif::TextField* frameRateTextField;
	GLMotif::RadioBox* waterModeRadioBox;
	GLMotif::TextFieldSlider* waterAttenuationSlider;
	GLMotif::TextFieldSlider* waterRoughnessSlider;
	GLMotif::TextFieldSlider* waterAbsorptionSlider;
	int controlPipeFd; // File descriptor of an optional named pipe to send control commands to a running AR Sandbox
	
	/* Private methods: */
	void rawDepthFrameDispatcher(const Kinect::FrameBuffer& frameBuffer); // Callback receiving raw depth frames from the Kinect camera; forwards them to the frame filter and rain maker objects
	void receiveFilteredFrame(const Kinect::FrameBuffer& frameBuffer); // Callback receiving filtered depth frames from the filter object
	void toggleDEM(DEM* dem); // Sets or toggles the currently active DEM
	void renderRainDisk(const Point& center,Scalar radius,GLfloat strength) const; // Renders a disk of rain, during rain processing
	void addWater(GLContextData& contextData) const; // Function to render geometry that adds water to the water table
	void pauseUpdatesCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void loadGridPropertyFileCallback(GLMotif::FileSelectionDialog::OKCallbackData* cbData);
	void saveGridPropertyFileCallback(GLMotif::FileSelectionDialog::OKCallbackData* cbData);
	void showWaterControlDialogCallback(Misc::CallbackData* cbData);
	void snowLineSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	void snowMeltSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	void waterSpeedSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	void waterMaxStepsSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	void waterModeRadioBoxCallback(GLMotif::RadioBox::ValueChangedCallbackData* cbData);
	void waterAttenuationSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	void waterRoughnessApplyCallback(Misc::CallbackData* cbData);
	void waterAbsorptionApplyCallback(Misc::CallbackData* cbData);
	GLMotif::PopupMenu* createMainMenu(void);
	GLMotif::PopupWindow* createWaterControlDialog(void);
	
	/* Constructors and destructors: */
	public:
	Sandbox(int& argc,char**& argv);
	virtual ~Sandbox(void);
	
	/* Methods from class Vrui::Application: */
	virtual void toolDestructionCallback(Vrui::ToolManager::ToolDestructionCallbackData* cbData);
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	virtual void resetNavigation(void);
	virtual void eventCallback(EventID eventId,Vrui::InputDevice::ButtonCallbackData* cbData);
	
	/* Methods from class GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	};

#endif
