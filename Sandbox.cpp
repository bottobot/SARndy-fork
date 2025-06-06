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

#include "Sandbox.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <Misc/SizedTypes.h>
#include <Misc/SelfDestructPointer.h>
#include <Misc/FixedArray.h>
#include <Misc/FunctionCalls.h>
#include <Misc/MessageLogger.h>
#include <Misc/FileNameExtensions.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/ArrayValueCoders.h>
#include <Misc/ConfigurationFile.h>
#include <IO/File.h>
#include <IO/ValueSource.h>
#include <IO/OpenFile.h>
#include <Comm/OpenPipe.h>
#include <Math/Math.h>
#include <Math/Constants.h>
#include <Math/Interval.h>
#include <Math/MathValueCoders.h>
#include <Geometry/Point.h>
#include <Geometry/AffineCombiner.h>
#include <Geometry/HVector.h>
#include <Geometry/Plane.h>
#include <Geometry/LinearUnit.h>
#include <Geometry/GeometryValueCoders.h>
#include <Geometry/OutputOperators.h>
#include <GL/gl.h>
#include <GL/GLMaterialTemplates.h>
#include <GL/GLLightTracker.h>
#include <GL/Extensions/GLARBDepthTexture.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBVertexProgram.h>
#include <GL/Extensions/GLEXTFramebufferObject.h>
#include <GL/GLContextData.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>
#include <GLMotif/StyleSheet.h>
#include <GLMotif/WidgetManager.h>
#include <GLMotif/PopupMenu.h>
#include <GLMotif/Menu.h>
#include <GLMotif/PopupWindow.h>
#include <GLMotif/Margin.h>
#include <GLMotif/Label.h>
#include <GLMotif/TextField.h>
#include <Vrui/Vrui.h>
#include <Vrui/CoordinateManager.h>
#include <Vrui/Lightsource.h>
#include <Vrui/LightsourceManager.h>
#include <Vrui/Viewer.h>
#include <Vrui/ToolManager.h>
#include <Vrui/DisplayState.h>
#include <Kinect/FileFrameSource.h>
#include <Kinect/MultiplexedFrameSource.h>
#include <Kinect/DirectFrameSource.h>
#include <Kinect/OpenDirectFrameSource.h>

#define SAVEDEPTH 0

#if SAVEDEPTH
#include <Images/RGBImage.h>
#include <Images/WriteImageFile.h>
#endif

#include "FrameFilter.h"
#include "TextureTracker.h"
#include "ElevationColorMap.h"
#include "DEM.h"
#include "DepthImageRenderer.h"
#include "WaterTable2.h"
#include "SurfaceRenderer.h"
#include "WaterRenderer.h"
#include "PropertyGridCreator.h"
#include "HandExtractor.h"
#include "RemoteServer.h"
#include "GlobalWaterTool.h"
#include "LocalWaterTool.h"
#include "DEMTool.h"
#include "BathymetrySaverTool.h"

#include "Config.h"

// DEBUGGING
// #include <Realtime/Time.h>

/**********************************
Methods of class Sandbox::DataItem:
**********************************/

Sandbox::DataItem::DataItem(void)
	:waterTableTime(0.0),
	 shadowFramebufferObject(0),shadowDepthTextureObject(0)
	{
	/* Initialize all required extensions, will throw exceptions if any are unsupported: */
	GLARBDepthTexture::initExtension();
	GLARBShaderObjects::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBVertexProgram::initExtension();
	GLEXTFramebufferObject::initExtension();
	TextureTracker::initExtensions();
	}

Sandbox::DataItem::~DataItem(void)
	{
	/* Delete all buffers and texture objects: */
	glDeleteFramebuffersEXT(1,&shadowFramebufferObject);
	glDeleteTextures(1,&shadowDepthTextureObject);
	}

/****************************************
Methods of class Sandbox::RenderSettings:
****************************************/

Sandbox::RenderSettings::RenderSettings(void)
	:fixProjectorView(false),projectorTransform(PTransform::identity),projectorTransformValid(false),
	 hillshade(false),surfaceMaterial(GLMaterial::Color(1.0f,1.0f,1.0f)),
	 useShadows(false),
	 elevationColorMap(0),
	 useContourLines(true),contourLineSpacing(0.75f),
	 renderWaterSurface(false),waterOpacity(2.0f),
	 surfaceRenderer(0),waterRenderer(0)
	{
	/* Load the default projector transformation: */
	loadProjectorTransform(CONFIG_DEFAULTPROJECTIONMATRIXFILENAME);
	}

Sandbox::RenderSettings::RenderSettings(const Sandbox::RenderSettings& source)
	:fixProjectorView(source.fixProjectorView),projectorTransform(source.projectorTransform),projectorTransformValid(source.projectorTransformValid),
	 hillshade(source.hillshade),surfaceMaterial(source.surfaceMaterial),
	 useShadows(source.useShadows),
	 elevationColorMap(source.elevationColorMap!=0?new ElevationColorMap(*source.elevationColorMap):0),
	 useContourLines(source.useContourLines),contourLineSpacing(source.contourLineSpacing),
	 renderWaterSurface(source.renderWaterSurface),waterOpacity(source.waterOpacity),
	 surfaceRenderer(0),waterRenderer(0)
	{
	}

Sandbox::RenderSettings::~RenderSettings(void)
	{
	delete surfaceRenderer;
	delete waterRenderer;
	delete elevationColorMap;
	}

void Sandbox::RenderSettings::loadProjectorTransform(const char* projectorTransformName)
	{
	std::string fullProjectorTransformName;
	try
		{
		/* Open the projector transformation file: */
		if(projectorTransformName[0]=='/')
			{
			/* Use the absolute file name directly: */
			fullProjectorTransformName=projectorTransformName;
			}
		else
			{
			/* Assemble a file name relative to the configuration file directory: */
			fullProjectorTransformName=CONFIG_CONFIGDIR;
			fullProjectorTransformName.push_back('/');
			fullProjectorTransformName.append(projectorTransformName);
			}
		IO::FilePtr projectorTransformFile=IO::openFile(fullProjectorTransformName.c_str(),IO::File::ReadOnly);
		projectorTransformFile->setEndianness(Misc::LittleEndian);
		
		/* Read the projector transformation matrix from the binary file: */
		Misc::Float64 pt[16];
		projectorTransformFile->read(pt,16);
		projectorTransform=PTransform::fromRowMajor(pt);
		
		projectorTransformValid=true;
		}
	catch(const std::runtime_error& err)
		{
		/* Print an error message and disable calibrated projections: */
		std::cerr<<"Unable to load projector transformation from file "<<fullProjectorTransformName<<" due to exception "<<err.what()<<std::endl;
		projectorTransformValid=false;
		}
	}

void Sandbox::RenderSettings::loadHeightMap(const char* heightMapName)
	{
	try
		{
		/* Load the elevation color map of the given name: */
		ElevationColorMap* newElevationColorMap=new ElevationColorMap(heightMapName);
		
		/* Delete the previous elevation color map and assign the new one: */
		delete elevationColorMap;
		elevationColorMap=newElevationColorMap;
		}
	catch(const std::runtime_error& err)
		{
		std::cerr<<"Ignoring height map due to exception "<<err.what()<<std::endl;
		}
	}

/************************
Methods of class Sandbox:
************************/

void Sandbox::rawDepthFrameDispatcher(const Kinect::FrameBuffer& frameBuffer)
	{
	/* Pass the received frame to the frame filter and the hand extractor: */
	if(frameFilter!=0&&!pauseUpdates)
		frameFilter->receiveRawFrame(frameBuffer);
	if(handExtractor!=0)
		handExtractor->receiveRawFrame(frameBuffer);
	}

void Sandbox::receiveFilteredFrame(const Kinect::FrameBuffer& frameBuffer)
	{
	/* Put the new frame into the frame input buffer: */
	filteredFrames.postNewValue(frameBuffer);
	
	/* Wake up the foreground thread: */
	Vrui::requestUpdate();
	}

void Sandbox::toggleDEM(DEM* dem)
	{
	/* Check if this is the active DEM: */
	if(activeDem==dem)
		{
		/* Deactivate the currently active DEM: */
		activeDem=0;
		}
	else
		{
		/* Activate this DEM: */
		activeDem=dem;
		}
	
	/* Enable DEM matching in all surface renderers that use a fixed projector matrix, i.e., in all physical sandboxes: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->fixProjectorView)
			rsIt->surfaceRenderer->setDem(activeDem);
	}

void Sandbox::renderRainDisk(const Point& center,Scalar radius,GLfloat strength) const
	{
	/* Create a local coordinate frame to render rain disks: */
	Vector x=waterTable->getBaseTransform().inverseTransform(Vector(1,0,0));
	Vector y=waterTable->getBaseTransform().inverseTransform(Vector(0,1,0));
	
	/* Set up a disk with smooth decay around the edge: */
	int numSegments=32;
	Scalar fudge=Math::sqrt(Math::sqr(waterTable->getCellSize()[0])+Math::sqr(waterTable->getCellSize()[1]))*Scalar(2);
	Scalar inner=Math::max(radius-fudge*Scalar(0.5),Scalar(0));
	Scalar outer=radius+fudge*Scalar(0.5);
	
	/* Render the inner disk: */
	glBegin(GL_POLYGON);
	glVertexAttrib1fARB(1,strength);
	for(int i=0;i<numSegments;++i)
		{
		Scalar angle=Scalar(2)*Math::Constants<Scalar>::pi*Scalar(i)/Scalar(numSegments);
		glVertex(center+x*(Math::cos(angle)*inner)+y*(Math::sin(angle)*inner));
		}
	glEnd();
	
	/* Render the smooth edge: */
	glBegin(GL_QUAD_STRIP);
	glVertexAttrib1fARB(1,0.0f);
	glVertex(center+x*outer);
	glVertexAttrib1fARB(1,strength);
	glVertex(center+x*inner);
	for(int i=1;i<numSegments;++i)
		{
		Scalar angle=Scalar(2)*Math::Constants<Scalar>::pi*Scalar(i)/Scalar(numSegments);
		Scalar c=Math::cos(angle);
		Scalar s=Math::sin(angle);
		glVertexAttrib1fARB(1,0.0f);
		glVertex(center+x*(c*outer)+y*(s*outer));
		glVertexAttrib1fARB(1,strength);
		glVertex(center+x*(c*inner)+y*(s*inner));
		}
	glVertexAttrib1fARB(1,0.0f);
	glVertex(center+x*outer);
	glVertexAttrib1fARB(1,strength);
	glVertex(center+x*inner);
	glEnd();
	}

void Sandbox::addWater(GLContextData& contextData) const
	{
	/* Check if the most recent rain object list is not empty: */
	if(handExtractor!=0&&!handExtractor->getLockedExtractedHands().empty())
		{
		/* Render all rain objects into the water table: */
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_CULL_FACE);
		
		/* Create a local coordinate frame to render rain disks: */
		Vector z=waterTable->getBaseTransform().inverseTransform(Vector(0,0,1));
		Vector x=Geometry::normal(z);
		Vector y=Geometry::cross(z,x);
		x.normalize();
		y.normalize();
		
		GLfloat rain=rainStrength/waterSpeed;
		glVertexAttrib1fARB(1,rain);
		
		for(HandExtractor::HandList::const_iterator hIt=handExtractor->getLockedExtractedHands().begin();hIt!=handExtractor->getLockedExtractedHands().end();++hIt)
			{
			/* Render a rain disk approximating the hand: */
			renderRainDisk(hIt->center,hIt->radius*Scalar(0.75),rainStrength/waterSpeed);
			}
		
		glPopAttrib();
		}
	}

void Sandbox::pauseUpdatesCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	pauseUpdates=cbData->set;
	}

void Sandbox::loadGridPropertyFileCallback(GLMotif::FileSelectionDialog::OKCallbackData* cbData)
	{
	propertyGridCreator->loadGrid(cbData->getSelectedPath().c_str());
	}

void Sandbox::saveGridPropertyFileCallback(GLMotif::FileSelectionDialog::OKCallbackData* cbData)
	{
	propertyGridCreator->saveGrid(cbData->getSelectedPath().c_str());
	}

void Sandbox::showWaterControlDialogCallback(Misc::CallbackData* cbData)
	{
	Vrui::popupPrimaryWidget(waterControlDialog);
	}

void Sandbox::snowLineSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
	waterTable->setSnowLine(GLfloat(cbData->value));
	}

void Sandbox::snowMeltSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
	waterTable->setSnowMelt(GLfloat(cbData->value));
	}

void Sandbox::waterSpeedSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
	waterSpeed=cbData->value;
	}

void Sandbox::waterMaxStepsSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
	waterMaxSteps=int(Math::floor(cbData->value+0.5));
	}

void Sandbox::waterModeRadioBoxCallback(GLMotif::RadioBox::ValueChangedCallbackData* cbData)
	{
	switch(cbData->radioBox->getChildIndex(cbData->newSelectedToggle))
		{
		case 0:
			waterTable->setMode(WaterTable2::Traditional);
			break;
		
		case 1:
			waterTable->setMode(WaterTable2::Engineering);
			break;
		}
	}

void Sandbox::waterAttenuationSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
	waterTable->setAttenuation(GLfloat(1.0-cbData->value));
	}

void Sandbox::waterRoughnessApplyCallback(Misc::CallbackData* cbData)
	{
	propertyGridCreator->setRoughness(GLfloat(waterRoughnessSlider->getValue()));
	}

void Sandbox::waterAbsorptionApplyCallback(Misc::CallbackData* cbData)
	{
	propertyGridCreator->setAbsorption(GLfloat(waterAbsorptionSlider->getValue()));
	}

GLMotif::PopupMenu* Sandbox::createMainMenu(void)
	{
	/* Create a popup shell to hold the main menu: */
	GLMotif::PopupMenu* mainMenuPopup=new GLMotif::PopupMenu("MainMenuPopup",Vrui::getWidgetManager());
	mainMenuPopup->setTitle("AR Sandbox");
	
	/* Create the main menu itself: */
	GLMotif::Menu* mainMenu=new GLMotif::Menu("MainMenu",mainMenuPopup,false);
	
	/* Create a button to pause topography updates: */
	pauseUpdatesToggle=new GLMotif::ToggleButton("PauseUpdatesToggle",mainMenu,"Pause Topography");
	pauseUpdatesToggle->setToggle(false);
	pauseUpdatesToggle->getValueChangedCallbacks().add(this,&Sandbox::pauseUpdatesCallback);
	
	if(waterTable!=0)
		{
		/* Create a button to show the water control dialog: */
		GLMotif::Button* showWaterControlDialogButton=new GLMotif::Button("ShowWaterControlDialogButton",mainMenu,"Show Water Simulation Control");
		showWaterControlDialogButton->getSelectCallbacks().add(this,&Sandbox::showWaterControlDialogCallback);
		
		/* Create buttons to load and save water simulation property grids: */
		GLMotif::Button* loadGridFileButton=new GLMotif::Button("LoadGridFileButton",mainMenu,"Load Grid Properties...");
		gridPropertyFileHelper.addLoadCallback(loadGridFileButton,Misc::createFunctionCall(this,&Sandbox::loadGridPropertyFileCallback));
		GLMotif::Button* saveGridFileButton=new GLMotif::Button("SaveGridFileButton",mainMenu,"Save Grid Properties...");
		gridPropertyFileHelper.addSaveCallback(saveGridFileButton,Misc::createFunctionCall(this,&Sandbox::saveGridPropertyFileCallback));
		}
	
	/* Finish building the main menu: */
	mainMenu->manageChild();
	
	return mainMenuPopup;
	}

GLMotif::PopupWindow* Sandbox::createWaterControlDialog(void)
	{
	const GLMotif::StyleSheet& ss=*Vrui::getUiStyleSheet();
	
	/* Create a popup window shell: */
	GLMotif::PopupWindow* waterControlDialogPopup=new GLMotif::PopupWindow("WaterControlDialogPopup",Vrui::getWidgetManager(),"Water Simulation Control");
	waterControlDialogPopup->setCloseButton(true);
	waterControlDialogPopup->setResizableFlags(true,false);
	waterControlDialogPopup->popDownOnClose();
	
	GLMotif::RowColumn* waterControlDialog=new GLMotif::RowColumn("WaterControlDialog",waterControlDialogPopup,false);
	waterControlDialog->setOrientation(GLMotif::RowColumn::VERTICAL);
	waterControlDialog->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	waterControlDialog->setNumMinorWidgets(2);
	
	new GLMotif::Label("SnowLineLabel",waterControlDialog,"Snow Line");
	
	snowLineSlider=new GLMotif::TextFieldSlider("SnowLineSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	snowLineSlider->getTextField()->setFieldWidth(7);
	snowLineSlider->getTextField()->setPrecision(2);
	snowLineSlider->getTextField()->setFloatFormat(GLMotif::TextField::FIXED);
	snowLineSlider->setValueRange(elevationRange.getMin(),elevationRange.getMax(),0.01);
	snowLineSlider->setValue(waterTable->getSnowLine());
	snowLineSlider->getValueChangedCallbacks().add(this,&Sandbox::snowLineSliderCallback);
	
	new GLMotif::Label("SnowMeltLabel",waterControlDialog,"Snow Melt");
	
	snowMeltSlider=new GLMotif::TextFieldSlider("SnowMeltSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	snowMeltSlider->getTextField()->setFieldWidth(7);
	snowMeltSlider->getTextField()->setPrecision(2);
	snowMeltSlider->getTextField()->setFloatFormat(GLMotif::TextField::FIXED);
	
	double maxSnowMelt=Math::pow(10.0,Math::ceil(Math::log10(double(waterTable->getSnowMelt())))+1.0);
	snowMeltSlider->setValueRange(0.0,maxSnowMelt,maxSnowMelt/100.0);
	snowMeltSlider->getSlider()->addNotch(waterTable->getSnowMelt());
	snowMeltSlider->setValue(waterTable->getSnowMelt());
	snowMeltSlider->getValueChangedCallbacks().add(this,&Sandbox::snowMeltSliderCallback);
	
	new GLMotif::Label("WaterSpeedLabel",waterControlDialog,"Speed");
	
	waterSpeedSlider=new GLMotif::TextFieldSlider("WaterSpeedSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	waterSpeedSlider->getTextField()->setFieldWidth(7);
	waterSpeedSlider->getTextField()->setPrecision(4);
	waterSpeedSlider->getTextField()->setFloatFormat(GLMotif::TextField::SMART);
	waterSpeedSlider->setSliderMapping(GLMotif::TextFieldSlider::EXP10);
	waterSpeedSlider->setValueRange(0.001,10.0,0.05);
	waterSpeedSlider->getSlider()->addNotch(0.0f);
	waterSpeedSlider->setValue(waterSpeed);
	waterSpeedSlider->getValueChangedCallbacks().add(this,&Sandbox::waterSpeedSliderCallback);
	
	new GLMotif::Label("WaterMaxStepsLabel",waterControlDialog,"Max Steps");
	
	waterMaxStepsSlider=new GLMotif::TextFieldSlider("WaterMaxStepsSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	waterMaxStepsSlider->getTextField()->setFieldWidth(7);
	waterMaxStepsSlider->getTextField()->setPrecision(0);
	waterMaxStepsSlider->getTextField()->setFloatFormat(GLMotif::TextField::FIXED);
	waterMaxStepsSlider->setSliderMapping(GLMotif::TextFieldSlider::LINEAR);
	waterMaxStepsSlider->setValueType(GLMotif::TextFieldSlider::UINT);
	waterMaxStepsSlider->setValueRange(0,200,1);
	waterMaxStepsSlider->setValue(waterMaxSteps);
	waterMaxStepsSlider->getValueChangedCallbacks().add(this,&Sandbox::waterMaxStepsSliderCallback);
	
	new GLMotif::Label("FrameRateLabel",waterControlDialog,"Frame Rate");
	
	GLMotif::Margin* frameRateMargin=new GLMotif::Margin("FrameRateMargin",waterControlDialog,false);
	frameRateMargin->setAlignment(GLMotif::Alignment::LEFT);
	
	frameRateTextField=new GLMotif::TextField("FrameRateTextField",frameRateMargin,8);
	frameRateTextField->setFieldWidth(7);
	frameRateTextField->setPrecision(2);
	frameRateTextField->setFloatFormat(GLMotif::TextField::FIXED);
	frameRateTextField->setValue(0.0);
	
	frameRateMargin->manageChild();
	
	new GLMotif::Label("WaterModeLabel",waterControlDialog,"Water Mode");
	
	GLMotif::Margin* waterModeMargin=new GLMotif::Margin("WaterModeMargin",waterControlDialog,false);
	waterModeMargin->setAlignment(GLMotif::Alignment::LEFT);
	
	waterModeRadioBox=new GLMotif::RadioBox("WaterModeBox",waterModeMargin,false);
	waterModeRadioBox->setOrientation(GLMotif::RowColumn::HORIZONTAL);
	waterModeRadioBox->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	waterModeRadioBox->setSelectionMode(GLMotif::RadioBox::ALWAYS_ONE);
	
	waterModeRadioBox->addToggle("Traditional");
	waterModeRadioBox->addToggle("Engineering");
	waterModeRadioBox->setSelectedToggle(waterTable->getMode()==WaterTable2::Engineering?1:0);
	
	waterModeRadioBox->getValueChangedCallbacks().add(this,&Sandbox::waterModeRadioBoxCallback);
	
	waterModeRadioBox->manageChild();
	
	waterModeMargin->manageChild();
	
	new GLMotif::Label("WaterAttenuationLabel",waterControlDialog,"Attenuation");
	
	waterAttenuationSlider=new GLMotif::TextFieldSlider("WaterAttenuationSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	waterAttenuationSlider->getTextField()->setFieldWidth(7);
	waterAttenuationSlider->getTextField()->setPrecision(5);
	waterAttenuationSlider->getTextField()->setFloatFormat(GLMotif::TextField::SMART);
	waterAttenuationSlider->setSliderMapping(GLMotif::TextFieldSlider::EXP10);
	waterAttenuationSlider->setValueRange(0.001,1.0,0.01);
	waterAttenuationSlider->getSlider()->addNotch(Math::log10(1.0-double(waterTable->getAttenuation())));
	waterAttenuationSlider->setValue(1.0-double(waterTable->getAttenuation()));
	waterAttenuationSlider->getValueChangedCallbacks().add(this,&Sandbox::waterAttenuationSliderCallback);
	
	new GLMotif::Label("WaterRoughnessLabel",waterControlDialog,"Roughness");
	
	GLMotif::RowColumn* waterRoughnessBox=new GLMotif::RowColumn("WaterRoughnessBox",waterControlDialog,false);
	waterRoughnessBox->setOrientation(GLMotif::RowColumn::HORIZONTAL);
	waterRoughnessBox->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	
	waterRoughnessSlider=new GLMotif::TextFieldSlider("WaterRoughnessSlider",waterRoughnessBox,8,ss.fontHeight*10.0f);
	waterRoughnessSlider->getTextField()->setFieldWidth(7);
	waterRoughnessSlider->getTextField()->setPrecision(3);
	waterRoughnessSlider->getTextField()->setFloatFormat(GLMotif::TextField::FIXED);
	waterRoughnessSlider->setSliderMapping(GLMotif::TextFieldSlider::LINEAR);
	waterRoughnessSlider->setValueRange(0.001,0.1,0.001);
	waterRoughnessSlider->setValue(double(propertyGridCreator->getRoughness()));
	
	waterRoughnessBox->setColumnWeight(0,1.0f);
	
	GLMotif::Button* waterRoughnessApplyButton=new GLMotif::Button("WaterRoughnessApplyButton",waterRoughnessBox,"Apply");
	waterRoughnessApplyButton->getSelectCallbacks().add(this,&Sandbox::waterRoughnessApplyCallback);
	
	waterRoughnessBox->setColumnWeight(1,0.0f);
	
	waterRoughnessBox->manageChild();
	
	new GLMotif::Label("WaterAbsorptionLabel",waterControlDialog,"Absorption");
	
	GLMotif::RowColumn* waterAbsorptionBox=new GLMotif::RowColumn("WaterAbsorptionBox",waterControlDialog,false);
	waterAbsorptionBox->setOrientation(GLMotif::RowColumn::HORIZONTAL);
	waterAbsorptionBox->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	
	waterAbsorptionSlider=new GLMotif::TextFieldSlider("WaterAbsorptionSlider",waterAbsorptionBox,8,ss.fontHeight*10.0f);
	waterAbsorptionSlider->getTextField()->setFieldWidth(7);
	waterAbsorptionSlider->getTextField()->setPrecision(2);
	waterAbsorptionSlider->getTextField()->setFloatFormat(GLMotif::TextField::FIXED);
	waterAbsorptionSlider->setSliderMapping(GLMotif::TextFieldSlider::LINEAR);
	waterAbsorptionSlider->setValueRange(-1.0,1.0,0.01);
	waterAbsorptionSlider->getSlider()->addNotch(0.0);
	waterAbsorptionSlider->setValue(double(propertyGridCreator->getAbsorption()));
	
	waterAbsorptionBox->setColumnWeight(0,1.0f);
	
	GLMotif::Button* waterAbsorptionApplyButton=new GLMotif::Button("WaterAbsorptionApplyButton",waterAbsorptionBox,"Apply");
	waterAbsorptionApplyButton->getSelectCallbacks().add(this,&Sandbox::waterAbsorptionApplyCallback);
	
	waterAbsorptionBox->setColumnWeight(1,0.0f);
	
	waterAbsorptionBox->manageChild();
	
	waterControlDialog->manageChild();
	
	return waterControlDialogPopup;
	}

namespace {

/****************
Helper functions:
****************/

void printUsage(void)
	{
	std::cout<<"Usage: SARndbox [option 1] ... [option n]"<<std::endl;
	std::cout<<"  Options:"<<std::endl;
	std::cout<<"  -h"<<std::endl;
	std::cout<<"     Prints this help message"<<std::endl;
	std::cout<<"  -remote [<listening port ID>]"<<std::endl;
	std::cout<<"     Creates a data streaming server listening on TCP port <listening port ID>"<<std::endl;
	std::cout<<"     Default listening port ID: 26000"<<std::endl;
	std::cout<<"  -c <camera index>"<<std::endl;
	std::cout<<"     Selects the local 3D camera of the given index (0: first camera on USB bus)"<<std::endl;
	std::cout<<"     Default: 0"<<std::endl;
	std::cout<<"  -f <frame file name prefix>"<<std::endl;
	std::cout<<"     Reads a pre-recorded 3D video stream from a pair of color/depth files of"<<std::endl;
	std::cout<<"     the given file name prefix"<<std::endl;
	std::cout<<"  -s <scale factor>"<<std::endl;
	std::cout<<"     Scale factor from real sandbox to simulated terrain"<<std::endl;
	std::cout<<"     Default: 100.0 (1:100 scale, 1cm in sandbox is 1m in terrain"<<std::endl;
	std::cout<<"  -slf <sandbox layout file name>"<<std::endl;
	std::cout<<"     Loads the sandbox layout file of the given name"<<std::endl;
	std::cout<<"     Default: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTBOXLAYOUTFILENAME<<std::endl;
	std::cout<<"  -er <min elevation> <max elevation>"<<std::endl;
	std::cout<<"     Sets the range of valid sand surface elevations relative to the ground"<<std::endl;
	std::cout<<"     plane in cm"<<std::endl;
	std::cout<<"     Default: Range of elevation color map"<<std::endl;
	std::cout<<"  -hmp <x> <y> <z> <offset>"<<std::endl;
	std::cout<<"     Sets an explicit base plane equation to use for height color mapping"<<std::endl;
	std::cout<<"  -nas <num averaging slots>"<<std::endl;
	std::cout<<"     Sets the number of averaging slots in the frame filter; latency is"<<std::endl;
	std::cout<<"     <num averaging slots> * 1/30 s"<<std::endl;
	std::cout<<"     Default: 30"<<std::endl;
	std::cout<<"  -sp <min num samples> <max variance>"<<std::endl;
	std::cout<<"     Sets the frame filter parameters minimum number of valid samples and"<<std::endl;
	std::cout<<"     maximum sample variance before convergence"<<std::endl;
	std::cout<<"     Default: 10 2"<<std::endl;
	std::cout<<"  -he <hysteresis envelope>"<<std::endl;
	std::cout<<"     Sets the size of the hysteresis envelope used for jitter removal"<<std::endl;
	std::cout<<"     Default: 0.1"<<std::endl;
	std::cout<<"  -wts <water grid width> <water grid height>"<<std::endl;
	std::cout<<"     Sets the width and height of the water flow simulation grid"<<std::endl;
	std::cout<<"     Default: 640 480"<<std::endl;
	std::cout<<"  -ws <water speed> <water max steps>"<<std::endl;
	std::cout<<"     Sets the relative speed of the water simulation and the maximum number of"<<std::endl;
	std::cout<<"     simulation steps per frame"<<std::endl;
	std::cout<<"     Default: 1.0 30"<<std::endl;
	std::cout<<"  -weng"<<std::endl;
	std::cout<<"     Sets the water simulation to engineering mode"<<std::endl;
	std::cout<<"  -wmts <water table minimum time step>"<<std::endl;
	std::cout<<"     Sets the minimum time step for water simulation to ensure frame rates at"<<std::endl;
	std::cout<<"     the cost of water simulation accuracy in high-flow regions"<<std::endl;
	std::cout<<"  -sl <snow line>"<<std::endl;
	std::cout<<"     Sets the elevation above which precipitation lands as snow instead of rain"<<std::endl;
	std::cout<<"     in cm"<<std::endl;
	std::cout<<"     Default: Top range of elevation color map"<<std::endl;
	std::cout<<"  -sm <snow melt rate>"<<std::endl;
	std::cout<<"     Sets the rate at which snow melts in cm/s"<<std::endl;
	std::cout<<"     Default: 0.0625 cm/s"<<std::endl;
	std::cout<<"  -rer <min rain elevation> <max rain elevation>"<<std::endl;
	std::cout<<"     Sets the elevation range of the rain cloud level relative to the ground"<<std::endl;
	std::cout<<"     plane in cm"<<std::endl;
	std::cout<<"     Default: Above range of elevation color map"<<std::endl;
	std::cout<<"  -rs <rain strength>"<<std::endl;
	std::cout<<"     Sets the strength of global or local rainfall in cm/s"<<std::endl;
	std::cout<<"     Default: 0.25"<<std::endl;
	std::cout<<"  -evr <evaporation rate>"<<std::endl;
	std::cout<<"     Water evaporation rate in cm/s"<<std::endl;
	std::cout<<"     Default: 0.0"<<std::endl;
	std::cout<<"  -dds <DEM distance scale>"<<std::endl;
	std::cout<<"     DEM matching distance scale factor in cm"<<std::endl;
	std::cout<<"     Default: 1.0"<<std::endl;
	std::cout<<"  -wi <window index>"<<std::endl;
	std::cout<<"     Sets the zero-based index of the display window to which the following"<<std::endl;
	std::cout<<"     rendering settings are applied"<<std::endl;
	std::cout<<"     Default: 0"<<std::endl;
	std::cout<<"  -fpv [projector transform file name]"<<std::endl;
	std::cout<<"     Fixes the navigation transformation so that the 3D camera and projector are"<<std::endl;
	std::cout<<"     aligned, as defined by the projector transform file of the given name"<<std::endl;
	std::cout<<"     Default projector transform file name: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTPROJECTIONMATRIXFILENAME<<std::endl;
	std::cout<<"  -nhs"<<std::endl;
	std::cout<<"     Disables hill shading"<<std::endl;
	std::cout<<"  -uhs"<<std::endl;
	std::cout<<"     Enables hill shading"<<std::endl;
	std::cout<<"  -ns"<<std::endl;
	std::cout<<"     Disables shadows"<<std::endl;
	std::cout<<"  -us"<<std::endl;
	std::cout<<"     Enables shadows"<<std::endl;
	std::cout<<"  -nhm"<<std::endl;
	std::cout<<"     Disables elevation color mapping"<<std::endl;
	std::cout<<"  -uhm [elevation color map file name]"<<std::endl;
	std::cout<<"     Enables elevation color mapping and loads the elevation color map from the"<<std::endl;
	std::cout<<"     file of the given name"<<std::endl;
	std::cout<<"     Default elevation color map file name: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTHEIGHTCOLORMAPFILENAME<<std::endl;
	std::cout<<"  -ncl"<<std::endl;
	std::cout<<"     Disables topographic contour lines"<<std::endl;
	std::cout<<"  -ucl [contour line spacing]"<<std::endl;
	std::cout<<"     Enables topographic contour lines and sets the elevation distance between"<<std::endl;
	std::cout<<"     adjacent contour lines to the given value in cm"<<std::endl;
	std::cout<<"     Default contour line spacing: 0.75"<<std::endl;
	std::cout<<"  -rws"<<std::endl;
	std::cout<<"     Renders water surface as geometric surface"<<std::endl;
	std::cout<<"  -rwt"<<std::endl;
	std::cout<<"     Renders water surface as texture"<<std::endl;
	std::cout<<"  -wo <water opacity>"<<std::endl;
	std::cout<<"     Sets the water depth at which water appears opaque in cm"<<std::endl;
	std::cout<<"     Default: 2.0"<<std::endl;
	std::cout<<"  -cp <control pipe name>"<<std::endl;
	std::cout<<"     Sets the name of a named POSIX pipe from which to read control commands"<<std::endl;
	std::cout<<std::endl;
	std::cout<<"  Units: All input parameters specified in cm apply to physical space, meaning"<<std::endl;
	std::cout<<"    they are unaffected by the overall sand box scale factor."<<std::endl;
	}

}

Sandbox::Sandbox(int& argc,char**& argv)
	:Vrui::Application(argc,argv),
	 remoteServer(0),
	 camera(0),pixelDepthCorrection(0),
	 frameFilter(0),pauseUpdates(false),
	 depthImageRenderer(0),
	 waterTable(0),
	 propertyGridCreator(0),
	 handExtractor(0),addWaterFunction(0),addWaterFunctionRegistered(false),
	 sun(0),
	 activeDem(0),
	 mainMenu(0),pauseUpdatesToggle(0),
	 gridPropertyFileHelper(Vrui::getWidgetManager(),"GridProperty.tiff",".tif;.tiff"),
	 waterControlDialog(0),
	 snowLineSlider(0),waterSpeedSlider(0),waterMaxStepsSlider(0),frameRateTextField(0),waterAttenuationSlider(0),
	 controlPipeFd(-1)
	{
	/* Read the sandbox's default configuration parameters: */
	std::string sandboxConfigFileName=CONFIG_CONFIGDIR;
	sandboxConfigFileName.push_back('/');
	sandboxConfigFileName.append(CONFIG_DEFAULTCONFIGFILENAME);
	Misc::ConfigurationFile sandboxConfigFile(sandboxConfigFileName.c_str());
	Misc::ConfigurationFileSection cfg=sandboxConfigFile.getSection("/SARndbox");
	unsigned int cameraIndex=cfg.retrieveValue<int>("./cameraIndex",0);
	std::string cameraConfiguration=cfg.retrieveString("./cameraConfiguration","Camera");
	double scale=cfg.retrieveValue<double>("./scaleFactor",100.0);
	std::string sandboxLayoutFileName=CONFIG_CONFIGDIR;
	sandboxLayoutFileName.push_back('/');
	sandboxLayoutFileName.append(CONFIG_DEFAULTBOXLAYOUTFILENAME);
	sandboxLayoutFileName=cfg.retrieveString("./sandboxLayoutFileName",sandboxLayoutFileName);
	elevationRange=cfg.retrieveValue<Math::Interval<double> >("./elevationRange",Math::Interval<double>(-1000.0,1000.0));
	bool haveHeightMapPlane=cfg.hasTag("./heightMapPlane");
	Plane heightMapPlane;
	if(haveHeightMapPlane)
		heightMapPlane=cfg.retrieveValue<Plane>("./heightMapPlane");
	unsigned int numAveragingSlots=cfg.retrieveValue<unsigned int>("./numAveragingSlots",30);
	unsigned int minNumSamples=cfg.retrieveValue<unsigned int>("./minNumSamples",10);
	unsigned int maxVariance=cfg.retrieveValue<unsigned int>("./maxVariance",2);
	float hysteresis=cfg.retrieveValue<float>("./hysteresis",0.1f);
	Size wtSize(640,480);
	cfg.updateValue("./waterTableSize",wtSize);
	waterSpeed=cfg.retrieveValue<double>("./waterSpeed",1.0);
	waterMaxSteps=cfg.retrieveValue<unsigned int>("./waterMaxSteps",30U);
	float waterMinTimeStep=cfg.retrieveValue<float>("./waterMinTimeStep",0.0f);
	Math::Interval<double> rainElevationRange=cfg.retrieveValue<Math::Interval<double> >("./rainElevationRange",Math::Interval<double>(-1000.0,1000.0));
	rainStrength=cfg.retrieveValue<GLfloat>("./rainStrength",0.25f);
	double snowLine=cfg.retrieveValue<double>("./snowLine",1000.0);
	double snowMelt=cfg.retrieveValue<double>("./snowMelt",0.0625);
	double evaporationRate=cfg.retrieveValue<double>("./evaporationRate",0.0);
	float demDistScale=cfg.retrieveValue<float>("./demDistScale",1.0f);
	std::string controlPipeName=cfg.retrieveString("./controlPipeName","pipe.fifo"); /*#added cp filename*/
	
	/* Process command line parameters: */
	bool printHelp=false;
	const char* frameFilePrefix=0;
	const char* kinectServerName=0;
	bool useRemoteServer=false;
	int remoteServerPortId=26000;
	bool engineering=false;
	int windowIndex=0;
	renderSettings.push_back(RenderSettings());
	for(int i=1;i<argc;++i)
		{
		if(argv[i][0]=='-')
			{
			if(strcasecmp(argv[i]+1,"h")==0)
				printHelp=true;
			else if(strcasecmp(argv[i]+1,"remote")==0)
				{
				/* Check if there is an optional port number: */
				if(i+1<argc&&argv[i+1][0]>='0'&&argv[i+1][0]<='9')
					{
					++i;
					remoteServerPortId=atoi(argv[i]);
					}
				
				useRemoteServer=true;
				}
			else if(strcasecmp(argv[i]+1,"c")==0)
				{
				++i;
				cameraIndex=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"f")==0)
				{
				++i;
				frameFilePrefix=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"p")==0)
				{
				++i;
				kinectServerName=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"s")==0)
				{
				++i;
				scale=atof(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"slf")==0)
				{
				++i;
				sandboxLayoutFileName=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"er")==0)
				{
				++i;
				double elevationMin=atof(argv[i]);
				++i;
				double elevationMax=atof(argv[i]);
				elevationRange=Math::Interval<double>(elevationMin,elevationMax);
				}
			else if(strcasecmp(argv[i]+1,"hmp")==0)
				{
				/* Read height mapping plane coefficients: */
				haveHeightMapPlane=true;
				double hmp[4];
				for(int j=0;j<4;++j)
					{
					++i;
					hmp[j]=atof(argv[i]);
					}
				heightMapPlane=Plane(Plane::Vector(hmp),hmp[3]);
				heightMapPlane.normalize();
				}
			else if(strcasecmp(argv[i]+1,"nas")==0)
				{
				++i;
				numAveragingSlots=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"sp")==0)
				{
				++i;
				minNumSamples=atoi(argv[i]);
				++i;
				maxVariance=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"he")==0)
				{
				++i;
				hysteresis=float(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"wts")==0)
				{
				for(int j=0;j<2;++j)
					{
					++i;
					wtSize[j]=(unsigned int)(atoi(argv[i]));
					}
				}
			else if(strcasecmp(argv[i]+1,"ws")==0)
				{
				++i;
				waterSpeed=atof(argv[i]);
				++i;
				waterMaxSteps=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"wmts")==0)
				{
				++i;
				waterMinTimeStep=atof(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"weng")==0)
				{
				engineering=true;
				}
			else if(strcasecmp(argv[i]+1,"rer")==0)
				{
				++i;
				double rainElevationMin=atof(argv[i]);
				++i;
				double rainElevationMax=atof(argv[i]);
				rainElevationRange=Math::Interval<double>(rainElevationMin,rainElevationMax);
				}
			else if(strcasecmp(argv[i]+1,"rs")==0)
				{
				++i;
				rainStrength=GLfloat(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"sl")==0)
				{
				++i;
				snowLine=atof(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"sm")==0)
				{
				++i;
				snowMelt=atof(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"evr")==0)
				{
				++i;
				evaporationRate=atof(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"dds")==0)
				{
				++i;
				demDistScale=float(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"wi")==0)
				{
				++i;
				windowIndex=atoi(argv[i]);
				
				/* Extend the list of render settings if an index beyond the end is selected: */
				while(int(renderSettings.size())<=windowIndex)
					renderSettings.push_back(renderSettings.back());
				
				/* Disable fixed projector view on the new render settings: */
				renderSettings.back().fixProjectorView=false;
				}
			else if(strcasecmp(argv[i]+1,"fpv")==0)
				{
				renderSettings.back().fixProjectorView=true;
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Load the projector transformation file specified in the next argument: */
					++i;
					renderSettings.back().loadProjectorTransform(argv[i]);
					}
				}
			else if(strcasecmp(argv[i]+1,"nhs")==0)
				renderSettings.back().hillshade=false;
			else if(strcasecmp(argv[i]+1,"uhs")==0)
				renderSettings.back().hillshade=true;
			else if(strcasecmp(argv[i]+1,"ns")==0)
				renderSettings.back().useShadows=false;
			else if(strcasecmp(argv[i]+1,"us")==0)
				renderSettings.back().useShadows=true;
			else if(strcasecmp(argv[i]+1,"nhm")==0)
				{
				delete renderSettings.back().elevationColorMap;
				renderSettings.back().elevationColorMap=0;
				}
			else if(strcasecmp(argv[i]+1,"uhm")==0)
				{
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Load the height color map file specified in the next argument: */
					++i;
					renderSettings.back().loadHeightMap(argv[i]);
					}
				else
					{
					/* Load the default height color map: */
					renderSettings.back().loadHeightMap(CONFIG_DEFAULTHEIGHTCOLORMAPFILENAME);
					}
				}
			else if(strcasecmp(argv[i]+1,"ncl")==0)
				renderSettings.back().useContourLines=false;
			else if(strcasecmp(argv[i]+1,"ucl")==0)
				{
				renderSettings.back().useContourLines=true;
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Read the contour line spacing: */
					++i;
					renderSettings.back().contourLineSpacing=GLfloat(atof(argv[i]));
					}
				}
			else if(strcasecmp(argv[i]+1,"rws")==0)
				renderSettings.back().renderWaterSurface=true;
			else if(strcasecmp(argv[i]+1,"rwt")==0)
				renderSettings.back().renderWaterSurface=false;
			else if(strcasecmp(argv[i]+1,"wo")==0)
				{
				++i;
				renderSettings.back().waterOpacity=GLfloat(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"cp")==0)
				{
				++i;
				controlPipeName=argv[i];
				}
			else
				std::cerr<<"Ignoring unrecognized command line switch "<<argv[i]<<std::endl;
			}
		}
	
	/* Print usage help if requested: */
	if(printHelp)
		printUsage();
	
	if(frameFilePrefix!=0)
		{
		/* Open the selected pre-recorded 3D video files: */
		std::string colorFileName=frameFilePrefix;
		colorFileName.append(".color");
		std::string depthFileName=frameFilePrefix;
		depthFileName.append(".depth");
		camera=new Kinect::FileFrameSource(IO::openFile(colorFileName.c_str()),IO::openFile(depthFileName.c_str()));
		}
	else if(kinectServerName!=0)
		{
		/* Split the server name into host name and port: */
		const char* colonPtr=0;
		for(const char* snPtr=kinectServerName;*snPtr!='\0';++snPtr)
			if(*snPtr==':')
				colonPtr=snPtr;
		std::string hostName;
		int port;
		if(colonPtr!=0)
			{
			/* Extract host name and port: */
			hostName=std::string(kinectServerName,colonPtr);
			port=atoi(colonPtr+1);
			}
		else
			{
			/* Use complete host name and default port: */
			hostName=kinectServerName;
			port=26000;
			}
		
		/* Open a multiplexed frame source for the given server host name and port number: */
		Kinect::MultiplexedFrameSource* source=Kinect::MultiplexedFrameSource::create(Comm::openTCPPipe(hostName.c_str(),port));
		
		/* Use the server's first component stream as the camera device: */
		camera=source->getStream(0);
		}
	else
		{
		/* Open the 3D camera device of the selected index: */
		Kinect::DirectFrameSource* realCamera=Kinect::openDirectFrameSource(cameraIndex,false);
		Misc::ConfigurationFileSection cameraConfigurationSection=cfg.getSection(cameraConfiguration.c_str());
		realCamera->configure(cameraConfigurationSection);
		camera=realCamera;
		}
	frameSize=camera->getActualFrameSize(Kinect::FrameSource::DEPTH);
	
	/* Get the camera's per-pixel depth correction parameters and evaluate it on the depth frame's pixel grid: */
	Kinect::FrameSource::DepthCorrection* depthCorrection=camera->getDepthCorrectionParameters();
	if(depthCorrection!=0)
		{
		pixelDepthCorrection=depthCorrection->getPixelCorrection(frameSize);
		delete depthCorrection;
		}
	else
		{
		/* Create dummy per-pixel depth correction parameters: */
		pixelDepthCorrection=new PixelDepthCorrection[frameSize[1]*frameSize[0]];
		PixelDepthCorrection* pdcPtr=pixelDepthCorrection;
		for(unsigned int y=0;y<frameSize[1];++y)
			for(unsigned int x=0;x<frameSize[0];++x,++pdcPtr)
				{
				pdcPtr->scale=1.0f;
				pdcPtr->offset=0.0f;
				}
		}
	
	/* Get the camera's intrinsic parameters: */
	cameraIps=camera->getIntrinsicParameters();
	
	/* Read the sandbox layout file: */
	Geometry::Plane<double,3> basePlane;
	Geometry::Point<double,3> basePlaneCorners[4];
	{
	IO::ValueSource layoutSource(IO::openFile(sandboxLayoutFileName.c_str()));
	layoutSource.skipWs();
	
	/* Read the base plane equation: */
	std::string s=layoutSource.readLine();
	basePlane=Misc::ValueCoder<Geometry::Plane<double,3> >::decode(s.c_str(),s.c_str()+s.length());
	basePlane.normalize();
	
	/* Read the corners of the base quadrilateral and project them into the base plane: */
	for(int i=0;i<4;++i)
		{
		layoutSource.skipWs();
		s=layoutSource.readLine();
		basePlaneCorners[i]=basePlane.project(Misc::ValueCoder<Geometry::Point<double,3> >::decode(s.c_str(),s.c_str()+s.length()));
		}
	}
	
	/* Limit the valid elevation range to the intersection of the extents of all height color maps: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->elevationColorMap!=0)
			{
			Math::Interval<double> mapRange(rsIt->elevationColorMap->getScalarRangeMin(),rsIt->elevationColorMap->getScalarRangeMax());
			elevationRange.intersectInterval(mapRange);
			}
	
	/* Scale all sizes by the given scale factor: */
	double sf=scale/100.0; // Scale factor from cm to final units
	for(int i=0;i<3;++i)
		for(int j=0;j<4;++j)
			cameraIps.depthProjection.getMatrix()(i,j)*=sf;
	basePlane=Geometry::Plane<double,3>(basePlane.getNormal(),basePlane.getOffset()*sf);
	for(int i=0;i<4;++i)
		for(int j=0;j<3;++j)
			basePlaneCorners[i][j]*=sf;
	if(elevationRange!=Math::Interval<double>::full)
		elevationRange*=sf;
	if(rainElevationRange!=Math::Interval<double>::full)
		rainElevationRange*=sf;
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		{
		if(rsIt->elevationColorMap!=0)
			rsIt->elevationColorMap->setScalarRange(rsIt->elevationColorMap->getScalarRangeMin()*sf,rsIt->elevationColorMap->getScalarRangeMax()*sf);
		rsIt->contourLineSpacing*=sf;
		rsIt->waterOpacity/=sf;
		for(int i=0;i<4;++i)
			rsIt->projectorTransform.getMatrix()(i,3)*=sf;
		}
	rainStrength*=sf;
	snowLine*=sf;
	snowMelt*=sf;
	evaporationRate*=sf;
	demDistScale*=sf;
	
	/* Create the frame filter object: */
	frameFilter=new FrameFilter(frameSize,numAveragingSlots,pixelDepthCorrection,cameraIps.depthProjection,basePlane);
	frameFilter->setValidElevationInterval(cameraIps.depthProjection,basePlane,elevationRange.getMin(),elevationRange.getMax());
	frameFilter->setStableParameters(minNumSamples,maxVariance);
	frameFilter->setHysteresis(hysteresis);
	frameFilter->setSpatialFilter(true);
	frameFilter->setOutputFrameFunction(Misc::createFunctionCall(this,&Sandbox::receiveFilteredFrame));
	
	/* Create the depth image renderer: */
	depthImageRenderer=new DepthImageRenderer(frameSize);
	depthImageRenderer->setIntrinsics(cameraIps);
	depthImageRenderer->setBasePlane(basePlane);
	
	{
	/* Calculate the transformation from camera space to sandbox space: */
	ONTransform::Vector z=basePlane.getNormal();
	ONTransform::Vector x=(basePlaneCorners[1]-basePlaneCorners[0])+(basePlaneCorners[3]-basePlaneCorners[2]);
	ONTransform::Vector y=z^x;
	boxTransform=ONTransform::rotate(Geometry::invert(ONTransform::Rotation::fromBaseVectors(x,y)));
	ONTransform::Point center=Geometry::mid(Geometry::mid(basePlaneCorners[0],basePlaneCorners[1]),Geometry::mid(basePlaneCorners[2],basePlaneCorners[3]));
	boxTransform*=ONTransform::translateToOriginFrom(center);
	
	/* Calculate the size of the sandbox area: */
	boxSize=Geometry::dist(center,basePlaneCorners[0]);
	for(int i=1;i<4;++i)
		boxSize=Math::max(boxSize,Geometry::dist(center,basePlaneCorners[i]));
	}
	
	/* Calculate a bounding box around all potential surfaces: */
	bbox=Box::empty;
	for(int i=0;i<4;++i)
		{
		bbox.addPoint(basePlaneCorners[i]+basePlane.getNormal()*elevationRange.getMin());
		bbox.addPoint(basePlaneCorners[i]+basePlane.getNormal()*elevationRange.getMax());
		}
	
	if(waterSpeed>0.0)
		{
		/* Initialize the water flow simulator: */
		waterTable=new WaterTable2(wtSize,depthImageRenderer,basePlaneCorners);
		waterTable->setElevationRange(elevationRange.getMin(),rainElevationRange.getMax());
		if(engineering)
			waterTable->setMode(WaterTable2::Engineering);
		if(waterMinTimeStep>0.0f)
			waterTable->forceMinStepSize(waterMinTimeStep);
		snowLine=Math::clamp(snowLine,elevationRange.getMin(),elevationRange.getMax());
		waterTable->setSnowLine(snowLine);
		waterTable->setSnowMelt(snowMelt);
		waterTable->setWaterDeposit(evaporationRate);
		
		/* Create the property grid creator object: */
		propertyGridCreator=new PropertyGridCreator(*waterTable,*camera);
		waterTable->setPropertyGridCreator(propertyGridCreator);
		
		/* Create the hand extractor object: */
		handExtractor=new HandExtractor(frameSize,pixelDepthCorrection,cameraIps.depthProjection);
		
		/* Register a render function with the water table: */
		addWaterFunction=Misc::createFunctionCall(this,&Sandbox::addWater);
		waterTable->addRenderFunction(addWaterFunction);
		addWaterFunctionRegistered=true;
		}
	
	/* Start streaming color and depth frames: */
	if(propertyGridCreator!=0)
		camera->startStreaming(Misc::createFunctionCall(propertyGridCreator,&PropertyGridCreator::receiveRawFrame),Misc::createFunctionCall(this,&Sandbox::rawDepthFrameDispatcher));
	else
		camera->startStreaming(0,Misc::createFunctionCall(this,&Sandbox::rawDepthFrameDispatcher));
	
	if(useRemoteServer)
		{
		/* Create a remote server: */
		try
			{
			remoteServer=new RemoteServer(this,remoteServerPortId,1.0/30.0);
			}
		catch(const std::runtime_error& err)
			{
			Misc::formattedConsoleError("Sandbox: Unable to create remote server on port %d due to exception %s",remoteServerPortId,err.what());
			}
		}
	
	/* Initialize all surface renderers: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		{
		/* Calculate the texture mapping plane for this renderer's height map: */
		if(rsIt->elevationColorMap!=0)
			{
			if(haveHeightMapPlane)
				rsIt->elevationColorMap->calcTexturePlane(heightMapPlane);
			else
				rsIt->elevationColorMap->calcTexturePlane(depthImageRenderer);
			}
		
		/* Initialize the surface renderer: */
		rsIt->surfaceRenderer=new SurfaceRenderer(depthImageRenderer);
		rsIt->surfaceRenderer->setDrawContourLines(rsIt->useContourLines);
		rsIt->surfaceRenderer->setContourLineDistance(rsIt->contourLineSpacing);
		rsIt->surfaceRenderer->setElevationColorMap(rsIt->elevationColorMap);
		rsIt->surfaceRenderer->setIlluminate(rsIt->hillshade);
		if(waterTable!=0)
			{
			if(rsIt->renderWaterSurface)
				{
				/* Create a water renderer: */
				rsIt->waterRenderer=new WaterRenderer(waterTable);
				}
			else
				{
				rsIt->surfaceRenderer->setWaterTable(waterTable);
				rsIt->surfaceRenderer->setAdvectWaterTexture(true);
				rsIt->surfaceRenderer->setWaterOpacity(rsIt->waterOpacity);
				}
			}
		rsIt->surfaceRenderer->setDemDistScale(demDistScale);
		}
	
	#if 0
	/* Create a fixed-position light source: */
	sun=Vrui::getLightsourceManager()->createLightsource(true);
	for(int i=0;i<Vrui::getNumViewers();++i)
		Vrui::getViewer(i)->setHeadlightState(false);
	sun->enable();
	sun->getLight().position=GLLight::Position(1,0,1,0);
	#endif
	
	/* Create the GUI: */
	mainMenu=createMainMenu();
	Vrui::setMainMenu(mainMenu);
	if(waterTable!=0)
		waterControlDialog=createWaterControlDialog();
	
	/* Initialize the custom tool classes: */
	GlobalWaterTool::initClass(*Vrui::getToolManager());
	LocalWaterTool::initClass(*Vrui::getToolManager());
	DEMTool::initClass(*Vrui::getToolManager());
	if(waterTable!=0)
		BathymetrySaverTool::initClass(waterTable,*Vrui::getToolManager());
	addEventTool("Pause Topography",0,0);
	addEventTool("Set Roughness",0,1);
	addEventTool("Set Absorption",0,2);
	
	if(!controlPipeName.empty())
		{
		/* Open the control pipe in non-blocking mode: */
		controlPipeFd=open(controlPipeName.c_str(),O_RDONLY|O_NONBLOCK);
		if(controlPipeFd<0)
			std::cerr<<"Unable to open control pipe "<<controlPipeName<<"; ignoring"<<std::endl;
		}
	
	/* Inhibit the screen saver: */
	Vrui::inhibitScreenSaver();
	
	/* Set the linear unit to support proper scaling: */
	Vrui::getCoordinateManager()->setUnit(Geometry::LinearUnit(Geometry::LinearUnit::METER,1.0));
	}

Sandbox::~Sandbox(void)
	{
	/* Stop streaming color and depth frames: */
	camera->stopStreaming();
	delete camera;
	delete frameFilter;
	
	/* Delete helper objects: */
	delete handExtractor;
	delete propertyGridCreator;
	delete waterTable;
	delete depthImageRenderer;
	delete addWaterFunction;
	delete[] pixelDepthCorrection;
	delete remoteServer;
	
	delete mainMenu;
	delete waterControlDialog;
	
	close(controlPipeFd);
	}

void Sandbox::toolDestructionCallback(Vrui::ToolManager::ToolDestructionCallbackData* cbData)
	{
	/* Check if the destroyed tool is the active DEM tool: */
	if(activeDem==dynamic_cast<DEM*>(cbData->tool))
		{
		/* Deactivate the active DEM tool: */
		activeDem=0;
		}
	}

namespace {

/****************
Helper functions:
****************/

std::vector<std::string> tokenizeLine(const char*& buffer)
	{
	std::vector<std::string> result;
	
	/* Skip initial whitespace but not end-of-line: */
	const char* bPtr=buffer;
	while(*bPtr!='\0'&&*bPtr!='\n'&&isspace(*bPtr))
		++bPtr;
	
	/* Extract white-space separated tokens until a newline or end-of-string are encountered: */
	while(*bPtr!='\0'&&*bPtr!='\n')
		{
		/* Remember the start of the current token: */
		const char* tokenStart=bPtr;
		
		/* Find the end of the current token: */
		while(*bPtr!='\0'&&!isspace(*bPtr))
			++bPtr;
		
		/* Extract the token: */
		result.push_back(std::string(tokenStart,bPtr));
		
		/* Skip whitespace but not end-of-line: */
		while(*bPtr!='\0'&&*bPtr!='\n'&&isspace(*bPtr))
			++bPtr;
		}
	
	/* Skip end-of-line: */
	if(*bPtr=='\n')
		++bPtr;
	
	/* Set the start of the next line and return the token list: */
	buffer=bPtr;
	return result;
	}

bool isToken(const std::string& token,const char* pattern)
	{
	return strcasecmp(token.c_str(),pattern)==0;
	}

}

void Sandbox::frame(void)
	{
	/* Call the remote server's frame method: */
	if(remoteServer!=0)
		remoteServer->frame(Vrui::getApplicationTime());
	
	/* Check if the filtered frame has been updated: */
	if(filteredFrames.lockNewValue())
		{
		/* Update the depth image renderer's depth image: */
		depthImageRenderer->setDepthImage(filteredFrames.getLockedValue());
		}
	
	if(handExtractor!=0)
		{
		/* Lock the most recent extracted hand list: */
		handExtractor->lockNewExtractedHands();
		
		#if 0
		
		/* Register/unregister the rain rendering function based on whether hands have been detected: */
		bool registerWaterFunction=!handExtractor->getLockedExtractedHands().empty();
		if(addWaterFunctionRegistered!=registerWaterFunction)
			{
			if(registerWaterFunction)
				waterTable->addRenderFunction(addWaterFunction);
			else
				waterTable->removeRenderFunction(addWaterFunction);
			addWaterFunctionRegistered=registerWaterFunction;
			}
		
		#endif
		}
	
	/* Update all surface renderers: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		rsIt->surfaceRenderer->setAnimationTime(Vrui::getApplicationTime());
	
	/* Check if there is a control command on the control pipe: */
	if(controlPipeFd>=0)
		{
		/* Try reading a chunk of data (will fail with EAGAIN if no data due to non-blocking access): */
		char commandBuffer[1024];
		ssize_t readResult=read(controlPipeFd,commandBuffer,sizeof(commandBuffer)-1);
		if(readResult>0)
			{
			commandBuffer[readResult]='\0';
			
			/* Extract commands line-by-line: */
			const char* cPtr=commandBuffer;
			while(*cPtr!='\0')
				{
				/* Split the current line into tokens and skip empty lines: */
				std::vector<std::string> tokens=tokenizeLine(cPtr);
				if(tokens.empty())
					continue;
				
				/* Parse the command: */
				if(isToken(tokens[0],"snowLine"))
					{
					if(tokens.size()==2)
						{
						double snowLine=atof(tokens[1].c_str());
						if(snowLineSlider!=0)
							{
							/* Set the new value in the slider first to clamp it to the valid range: */
							snowLineSlider->setValue(snowLine);
							snowLine=snowLineSlider->getValue();
							}
						if(waterTable!=0)
							waterTable->setSnowLine(GLfloat(snowLine));
						}
					else
						std::cerr<<"Wrong number of arguments for snowLine control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"snowMelt"))
					{
					if(tokens.size()==2)
						{
						double snowMelt=atof(tokens[1].c_str());
						if(snowMeltSlider!=0)
							{
							/* Set the new value in the slider first to clamp it to the valid range: */
							snowMeltSlider->setValue(snowMelt);
							snowMelt=snowMeltSlider->getValue();
							}
						if(waterTable!=0)
							waterTable->setSnowMelt(GLfloat(snowMelt));
						}
					else
						std::cerr<<"Wrong number of arguments for snowMelt control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterSpeed"))
					{
					if(tokens.size()==2)
						{
						waterSpeed=atof(tokens[1].c_str());
						if(waterSpeedSlider!=0)
							waterSpeedSlider->setValue(waterSpeed);
						}
					else
						std::cerr<<"Wrong number of arguments for waterSpeed control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterMaxSteps"))
					{
					if(tokens.size()==2)
						{
						waterMaxSteps=atoi(tokens[1].c_str());
						if(waterMaxStepsSlider!=0)
							waterMaxStepsSlider->setValue(waterMaxSteps);
						}
					else
						std::cerr<<"Wrong number of arguments for waterMaxSteps control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterMode"))
					{
					if(tokens.size()==2)
						{
						if(isToken(tokens[1],"traditional"))
							{
							if(waterTable!=0)
								{
								waterTable->setMode(WaterTable2::Traditional);
								waterModeRadioBox->setSelectedToggle(0);
								}
							}
						else if(isToken(tokens[1],"engineering"))
							{
							if(waterTable!=0)
								{
								waterTable->setMode(WaterTable2::Engineering);
								waterModeRadioBox->setSelectedToggle(1);
								}
							}
						else
							std::cerr<<"Unknown water mode "<<tokens[1]<<" in waterMode control pipe command"<<std::endl;
						}
					else
						std::cerr<<"Wrong number of arguments for waterMode control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterAttenuation"))
					{
					if(tokens.size()==2)
						{
						double attenuation=atof(tokens[1].c_str());
						if(waterTable!=0)
							waterTable->setAttenuation(GLfloat(1.0-attenuation));
						if(waterAttenuationSlider!=0)
							waterAttenuationSlider->setValue(attenuation);
						}
					else
						std::cerr<<"Wrong number of arguments for waterAttenuation control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterRoughness"))
					{
					if(tokens.size()==2)
						{
						double roughness=atof(tokens[1].c_str());
						if(propertyGridCreator!=0)
							propertyGridCreator->setRoughness(roughness);
						if(waterRoughnessSlider!=0)
							waterRoughnessSlider->setValue(roughness);
						}
					else
						std::cerr<<"Wrong number of arguments for waterRoughness control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"rainStrength"))
					{
					if(tokens.size()==2)
						{
						rainStrength=GLfloat(atof(tokens[1].c_str()));
						}
					else
						std::cerr<<"Wrong number of arguments for rainStrength control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterAbsorption"))
					{
					if(tokens.size()==2)
						{
						double absorption=atof(tokens[1].c_str());
						if(propertyGridCreator!=0)
							propertyGridCreator->setAbsorption(absorption);
						if(waterAbsorptionSlider!=0)
							waterAbsorptionSlider->setValue(absorption);
						}
					else
						std::cerr<<"Wrong number of arguments for waterAbsorption control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"evaporationRate"))
					{
					if(tokens.size()==2)
						{
						double evaporationRate=atof(tokens[1].c_str());
						if(waterTable!=0)
							waterTable->setWaterDeposit(evaporationRate);
						}
					else
						std::cerr<<"Wrong number of arguments for evaporationRate control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterColor"))
					{
					if(tokens.size()==4)
						{
						/* Parse RGB color values: */
						GLfloat waterColor[3];
						waterColor[0]=GLfloat(atof(tokens[1].c_str()));
						waterColor[1]=GLfloat(atof(tokens[2].c_str()));
						waterColor[2]=GLfloat(atof(tokens[3].c_str()));
						
						/* Set the water color on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->surfaceRenderer!=0)
								rsIt->surfaceRenderer->setWaterColor(waterColor);
						}
					else
						std::cerr<<"Wrong number of arguments for waterColor control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterReflectionColor"))
					{
					if(tokens.size()==4)
						{
						/* Parse RGB color values: */
						GLfloat waterReflectionColor[3];
						waterReflectionColor[0]=GLfloat(atof(tokens[1].c_str()));
						waterReflectionColor[1]=GLfloat(atof(tokens[2].c_str()));
						waterReflectionColor[2]=GLfloat(atof(tokens[3].c_str()));
						
						/* Set the water reflection color on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->surfaceRenderer!=0)
								rsIt->surfaceRenderer->setWaterReflectionColor(waterReflectionColor);
						}
					else
						std::cerr<<"Wrong number of arguments for waterReflectionColor control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"colorCycle"))
					{
					if(tokens.size()>=2)
						{
						/* Parse the enable/disable parameter: */
						int enable=atoi(tokens[1].c_str());
						float speed=1.0f;
						
						/* Check if there's an optional speed parameter: */
						if(tokens.size()>=3)
							speed=float(atof(tokens[2].c_str()));
						
						/* Apply color cycling to all elevation color maps: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->elevationColorMap!=0)
								rsIt->elevationColorMap->setColorCycling(enable!=0,speed);
						}
					else
						std::cerr<<"Wrong number of arguments for colorCycle control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"colorMap"))
					{
					if(tokens.size()==2)
						{
						try
							{
							/* Update all height color maps: */
							for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
								if(rsIt->elevationColorMap!=0)
									rsIt->elevationColorMap->load(tokens[1].c_str());
							}
						catch(const std::runtime_error& err)
							{
							std::cerr<<"Cannot read height color map "<<tokens[1]<<" due to exception "<<err.what()<<std::endl;
							}
						}
					else
						std::cerr<<"Wrong number of arguments for colorMap control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"heightMapPlane"))
					{
					if(tokens.size()==5)
						{
						/* Read the height map plane equation: */
						double hmp[4];
						for(int i=0;i<4;++i)
							hmp[i]=atof(tokens[1+i].c_str());
						Plane heightMapPlane=Plane(Plane::Vector(hmp),hmp[3]);
						heightMapPlane.normalize();
						
						/* Override the height mapping planes of all elevation color maps: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->elevationColorMap!=0)
								rsIt->elevationColorMap->calcTexturePlane(heightMapPlane);
						}
					else
						std::cerr<<"Wrong number of arguments for heightMapPlane control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"useContourLines"))
					{
					if(tokens.size()==2)
						{
						/* Parse the command parameter: */
						if(isToken(tokens[1],"on")||isToken(tokens[1],"off"))
							{
							/* Enable or disable contour lines on all surface renderers: */
							bool useContourLines=isToken(tokens[1],"on");
							for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
								rsIt->surfaceRenderer->setDrawContourLines(useContourLines);
							}
						else
							std::cerr<<"Invalid parameter "<<tokens[1]<<" for useContourLines control pipe command"<<std::endl;
						}
					else
						std::cerr<<"Wrong number of arguments for contourLineSpacing control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"contourLineSpacing"))
					{
					if(tokens.size()==2)
						{
						/* Parse the contour line distance: */
						GLfloat contourLineSpacing=GLfloat(atof(tokens[1].c_str()));
						
						/* Check if the requested spacing is valid: */
						if(contourLineSpacing>0.0f)
							{
							/* Override the contour line spacing of all surface renderers: */
							for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
								rsIt->surfaceRenderer->setContourLineDistance(contourLineSpacing);
							}
						else
							std::cerr<<"Invalid parameter "<<contourLineSpacing<<" for contourLineSpacing control pipe command"<<std::endl;
						}
					else
						std::cerr<<"Wrong number of arguments for contourLineSpacing control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"dippingBed"))
					{
					if(tokens.size()==2&&isToken(tokens[1],"off"))
						{
						/* Disable dipping bed rendering on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							rsIt->surfaceRenderer->setDrawDippingBed(false);
						}
					else if(tokens.size()==5)
						{
						/* Read the dipping bed plane equation: */
						GLfloat dbp[4];
						for(int i=0;i<4;++i)
							dbp[i]=GLfloat(atof(tokens[1+i].c_str()));
						SurfaceRenderer::Plane dippingBedPlane=SurfaceRenderer::Plane(SurfaceRenderer::Plane::Vector(dbp),dbp[3]);
						dippingBedPlane.normalize();
						
						/* Enable dipping bed rendering and set the dipping bed plane equation on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							{
							rsIt->surfaceRenderer->setDrawDippingBed(true);
							rsIt->surfaceRenderer->setDippingBedPlane(dippingBedPlane);
							}
						}
					else
						std::cerr<<"Wrong number of arguments for dippingBed control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"foldedDippingBed"))
					{
					if(tokens.size()==6)
						{
						/* Read the dipping bed coefficients: */
						GLfloat dbc[5];
						for(int i=0;i<5;++i)
							dbc[i]=GLfloat(atof(tokens[1+i].c_str()));
						
						/* Enable dipping bed rendering and set the dipping bed coefficients on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							{
							rsIt->surfaceRenderer->setDrawDippingBed(true);
							rsIt->surfaceRenderer->setDippingBedCoeffs(dbc);
							}
						}
					else
						std::cerr<<"Wrong number of arguments for foldedDippingBed control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"dippingBedThickness"))
					{
					if(tokens.size()==2)
						{
						/* Read the dipping bed thickness: */
						float dippingBedThickness=float(atof(tokens[1].c_str()));
						
						/* Set the dipping bed thickness on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							rsIt->surfaceRenderer->setDippingBedThickness(dippingBedThickness);
						}
					else
						std::cerr<<"Wrong number of arguments for dippingBedThickness control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"loadWarpTexture"))
					{
					if(tokens.size()==2)
						{
						/* Load the warp texture on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->surfaceRenderer!=nullptr)
								rsIt->surfaceRenderer->loadWarpTexture(tokens[1].c_str());
						}
					else
						std::cerr<<"Wrong number of arguments for loadWarpTexture control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"useWarpTexture"))
					{
					if(tokens.size()==2)
						{
						/* Parse the command parameter: */
						if(isToken(tokens[1],"on")||isToken(tokens[1],"off"))
							{
							/* Enable or disable texture warping on all surface renderers: */
							bool enable=isToken(tokens[1],"on");
							for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
								if(rsIt->surfaceRenderer!=nullptr)
									rsIt->surfaceRenderer->setUseWarpTexture(enable);
							}
						else
							std::cerr<<"Invalid parameter "<<tokens[1]<<" for useWarpTexture control pipe command"<<std::endl;
						}
					else
						std::cerr<<"Wrong number of arguments for useWarpTexture control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"warpIntensity"))
					{
					if(tokens.size()==2)
						{
						float intensity=atof(tokens[1].c_str());
						/* Set the warp intensity on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->surfaceRenderer!=nullptr)
								rsIt->surfaceRenderer->setWarpIntensity(intensity);
						}
					else
						std::cerr<<"Wrong number of arguments for warpIntensity control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"textureScale"))
					{
					if(tokens.size()==2)
						{
						float scale=atof(tokens[1].c_str());
						/* Set the texture scale on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->surfaceRenderer!=nullptr)
								rsIt->surfaceRenderer->setTextureScale(scale);
						}
					else
						std::cerr<<"Wrong number of arguments for textureScale control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"gradientThreshold"))
					{
					if(tokens.size()==2)
						{
						float threshold=atof(tokens[1].c_str());
						/* Set the gradient threshold on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->surfaceRenderer!=nullptr)
								rsIt->surfaceRenderer->setGradientThreshold(threshold);
						}
					else
						std::cerr<<"Wrong number of arguments for gradientThreshold control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"warpMode"))
					{
					if(tokens.size()==2)
						{
						int mode=atoi(tokens[1].c_str());
						/* Set the warp mode on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->surfaceRenderer!=nullptr)
								rsIt->surfaceRenderer->setWarpMode(mode);
						}
					else
						std::cerr<<"Wrong number of arguments for warpMode control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"textureBlendMode"))
					{
					if(tokens.size()==2)
						{
						int mode=atoi(tokens[1].c_str());
						/* Set the texture blend mode on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->surfaceRenderer!=nullptr)
								rsIt->surfaceRenderer->setTextureBlendMode(mode);
						}
					else
						std::cerr<<"Wrong number of arguments for textureBlendMode control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"textureOpacity"))
					{
					if(tokens.size()==2)
						{
						float opacity=atof(tokens[1].c_str());
						/* Set the texture opacity on all surface renderers: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->surfaceRenderer!=nullptr)
								rsIt->surfaceRenderer->setTextureOpacity(opacity);
						}
					else
						std::cerr<<"Wrong number of arguments for textureOpacity control pipe command"<<std::endl;
					}
				else
					std::cerr<<"Unrecognized control pipe command "<<tokens[0]<<std::endl;
				}
			}
		}
	
	if(frameRateTextField!=0&&Vrui::getWidgetManager()->isVisible(waterControlDialog))
		{
		/* Update the frame rate display: */
		frameRateTextField->setValue(1.0/Vrui::getCurrentFrameTime());
		}
	
	if(pauseUpdates)
		Vrui::scheduleUpdate(Vrui::getApplicationTime()+1.0/30.0);
	}

void Sandbox::display(GLContextData& contextData) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Create a texture tracker: */
	TextureTracker textureTracker;
	
	/* Get the rendering settings for this window: */
	const Vrui::DisplayState& ds=Vrui::getDisplayState(contextData);
	const Vrui::VRWindow* window=ds.window;
	int windowIndex;
	for(windowIndex=0;windowIndex<Vrui::getNumWindows()&&window!=Vrui::getWindow(windowIndex);++windowIndex)
		;
	const RenderSettings& rs=windowIndex<int(renderSettings.size())?renderSettings[windowIndex]:renderSettings.back();
	
	/* Check if the water simulation state needs to be updated: */
	if(waterTable!=0&&dataItem->waterTableTime!=Vrui::getApplicationTime())
		{
		/* Retrieve a potential pending grid read-back request: */
		GridRequest::Request request=gridRequest.getRequest();
		
		/* Update the water table's bathymetry grid: */
		waterTable->updateBathymetry(contextData,textureTracker);
		
		/* Check if the grid request is active and wants bathymetry data: */
		if(request.isActive()&&request.bathymetryBuffer!=0)
			{
			/* Read back the current bathymetry grid: */
			waterTable->readBathymetryTexture(contextData,textureTracker,request.bathymetryBuffer);
			}
		
		/* Update the water simulation property grid: */
		propertyGridCreator->updatePropertyGrid(contextData,textureTracker);
		
		/* Run the water flow simulation's main pass: */
		GLfloat totalTimeStep=GLfloat(Vrui::getFrameTime()*waterSpeed);
		
		// DEBUGGING
		// std::cout<<totalTimeStep<<',';
		// Realtime::TimePointMonotonic timer;
		
		unsigned int numSteps=0;
		while(numSteps<waterMaxSteps&&totalTimeStep>1.0e-8f)
			{
			/* Run with a self-determined time step to maintain stability: */
			waterTable->setMaxStepSize(totalTimeStep);
			GLfloat timeStep=waterTable->runSimulationStep(false,contextData,textureTracker);
			totalTimeStep-=timeStep;
			++numSteps;
			}
		
		// DEBUGGING
		// double elapsed(timer.setAndDiff());
		// std::cout<<numSteps<<','<<elapsed<<','<<elapsed/numSteps<<std::endl;
		
		#if 0
		if(totalTimeStep>1.0e-8f)
			{
			std::cout<<'.'<<std::flush;
			/* Force the final step to avoid simulation slow-down: */
			waterTable->setMaxStepSize(totalTimeStep);
			GLfloat timeStep=waterTable->runSimulationStep(true,contextData,textureTracker);
			totalTimeStep-=timeStep;
			++numSteps;
			}
		#elif 0
		if(totalTimeStep>1.0e-8f)
			std::cout<<"Ran out of time by "<<totalTimeStep<<std::endl;
		#endif
		
		/* Check if the grid request is active and wants water level data: */
		if(request.isActive()&&request.waterLevelBuffer!=0)
			{
			/* Read back the current water level grid: */
			waterTable->readQuantityTexture(contextData,textureTracker,GL_RED,request.waterLevelBuffer);
			}
		
		/* Check if the grid request is active and wants snow height data: */
		if(request.isActive()&&request.snowHeightBuffer!=0)
			{
			/* Read back the current snow height grid: */
			waterTable->readSnowTexture(contextData,textureTracker,request.snowHeightBuffer);
			}
		
		/* Finish an active grid request: */
		if(request.isActive())
			request.complete();
		
		/* Mark the water simulation state as up-to-date for this frame: */
		dataItem->waterTableTime=Vrui::getApplicationTime();
		}
	
	/* Check if rendering is suspended due to a property grid creation request: */
	if(propertyGridCreator==0||!propertyGridCreator->isRequestActive())
		{
		/* Calculate the projection matrix: */
		PTransform projection=ds.projection;
		if(rs.fixProjectorView&&rs.projectorTransformValid)
			{
			/* Use the projector transformation instead: */
			projection=rs.projectorTransform;
			
			/* Multiply with the inverse modelview transformation so that lighting still works as usual: */
			projection*=Geometry::invert(ds.modelviewNavigational);
			}
		
		if(rs.hillshade)
			{
			/* Set the surface material: */
			glMaterial(GLMaterialEnums::FRONT,rs.surfaceMaterial);
			}
		
		#if 0
		if(rs.hillshade&&rs.useShadows)
			{
			/* Set up OpenGL state: */
			glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_POLYGON_BIT);
			
			GLLightTracker& lt=*contextData.getLightTracker();
			
			/* Save the currently-bound frame buffer and viewport: */
			GLint currentFrameBuffer;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
			GLint currentViewport[4];
			glGetIntegerv(GL_VIEWPORT,currentViewport);
			
			/*******************************************************************
			First rendering pass: Global ambient illumination only
			*******************************************************************/
			
			/* Draw the surface mesh: */
			surfaceRenderer->glRenderGlobalAmbientHeightMap(dataItem->heightColorMapObject,contextData,textureTracker);
			
			/*******************************************************************
			Second rendering pass: Add local illumination for every light source
			*******************************************************************/
			
			/* Enable additive rendering: */
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE,GL_ONE);
			glDepthFunc(GL_LEQUAL);
			glDepthMask(GL_FALSE);
			
			for(int lightSourceIndex=0;lightSourceIndex<lt.getMaxNumLights();++lightSourceIndex)
				if(lt.getLightState(lightSourceIndex).isEnabled())
					{
					/***************************************************************
					First step: Render to the light source's shadow map
					***************************************************************/
					
					/* Set up OpenGL state to render to the shadow map: */
					glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->shadowFramebufferObject);
					glViewport(0,0,dataItem->shadowBufferSize[0],dataItem->shadowBufferSize[1]);
					glDepthMask(GL_TRUE);
					glClear(GL_DEPTH_BUFFER_BIT);
					glCullFace(GL_FRONT);
					
					/*************************************************************
					Calculate the shadow projection matrix:
					*************************************************************/
					
					/* Get the light source position in eye space: */
					Geometry::HVector<float,3> lightPosEc;
					glGetLightfv(GL_LIGHT0+lightSourceIndex,GL_POSITION,lightPosEc.getComponents());
					
					/* Transform the light source position to camera space: */
					Vrui::ONTransform::HVector lightPosCc=Vrui::getDisplayState(contextData).modelviewNavigational.inverseTransform(Vrui::ONTransform::HVector(lightPosEc));
					
					/* Calculate the direction vector from the center of the bounding box to the light source: */
					Point bboxCenter=Geometry::mid(bbox.min,bbox.max);
					Vrui::Vector lightDirCc=Vrui::Vector(lightPosCc.getComponents())-Vrui::Vector(bboxCenter.getComponents())*lightPosCc[3];
					
					/* Build a transformation that aligns the light direction with the positive z axis: */
					Vrui::ONTransform shadowModelview=Vrui::ONTransform::rotate(Vrui::Rotation::rotateFromTo(lightDirCc,Vrui::Vector(0,0,1)));
					shadowModelview*=Vrui::ONTransform::translateToOriginFrom(bboxCenter);
					
					/* Create a projection matrix, based on whether the light is positional or directional: */
					PTransform shadowProjection(0.0);
					if(lightPosEc[3]!=0.0f)
						{
						/* Modify the modelview transformation such that the light source is at the origin: */
						shadowModelview.leftMultiply(Vrui::ONTransform::translate(Vrui::Vector(0,0,-lightDirCc.mag())));
						
						/***********************************************************
						Create a perspective projection:
						***********************************************************/
						
						/* Calculate the perspective bounding box of the surface bounding box in eye space: */
						Box pBox=Box::empty;
						for(int i=0;i<8;++i)
							{
							Point bc=shadowModelview.transform(bbox.getVertex(i));
							pBox.addPoint(Point(-bc[0]/bc[2],-bc[1]/bc[2],-bc[2]));
							}
						
						/* Upload the frustum matrix: */
						double l=pBox.min[0]*pBox.min[2];
						double r=pBox.max[0]*pBox.min[2];
						double b=pBox.min[1]*pBox.min[2];
						double t=pBox.max[1]*pBox.min[2];
						double n=pBox.min[2];
						double f=pBox.max[2];
						shadowProjection.getMatrix()(0,0)=2.0*n/(r-l);
						shadowProjection.getMatrix()(0,2)=(r+l)/(r-l);
						shadowProjection.getMatrix()(1,1)=2.0*n/(t-b);
						shadowProjection.getMatrix()(1,2)=(t+b)/(t-b);
						shadowProjection.getMatrix()(2,2)=-(f+n)/(f-n);
						shadowProjection.getMatrix()(2,3)=-2.0*f*n/(f-n);
						shadowProjection.getMatrix()(3,2)=-1.0;
						}
					else
						{
						/***********************************************************
						Create a perspective projection:
						***********************************************************/
						
						/* Transform the bounding box with the modelview transformation: */
						Box bboxEc=bbox;
						bboxEc.transform(shadowModelview);
						
						/* Upload the ortho matrix: */
						double l=bboxEc.min[0];
						double r=bboxEc.max[0];
						double b=bboxEc.min[1];
						double t=bboxEc.max[1];
						double n=-bboxEc.max[2];
						double f=-bboxEc.min[2];
						shadowProjection.getMatrix()(0,0)=2.0/(r-l);
						shadowProjection.getMatrix()(0,3)=-(r+l)/(r-l);
						shadowProjection.getMatrix()(1,1)=2.0/(t-b);
						shadowProjection.getMatrix()(1,3)=-(t+b)/(t-b);
						shadowProjection.getMatrix()(2,2)=-2.0/(f-n);
						shadowProjection.getMatrix()(2,3)=-(f+n)/(f-n);
						shadowProjection.getMatrix()(3,3)=1.0;
						}
					
					/* Multiply the shadow modelview matrix onto the shadow projection matrix: */
					shadowProjection*=shadowModelview;
					
					/* Draw the surface into the shadow buffer: */
					surfaceRenderer->glRenderDepthOnly(shadowProjection,contextData,textureTracker);
					
					/* Reset OpenGL state: */
					glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
					glViewport(currentViewport[0],currentViewport[1],currentViewport[2],currentViewport[3]);
					glCullFace(GL_BACK);
					glDepthMask(GL_FALSE);
					
					#if SAVEDEPTH
					/* Save the depth image: */
					{
					glBindTexture(GL_TEXTURE_2D,dataItem->shadowDepthTextureObject);
					GLfloat* depthTextureImage=new GLfloat[dataItem->shadowBufferSize[1]*dataItem->shadowBufferSize[0]];
					glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,depthTextureImage);
					glBindTexture(GL_TEXTURE_2D,0);
					Images::RGBImage dti(dataItem->shadowBufferSize[0],dataItem->shadowBufferSize[1]);
					GLfloat* dtiPtr=depthTextureImage;
					Images::RGBImage::Color* ciPtr=dti.modifyPixels();
					for(int y=0;y<dataItem->shadowBufferSize[1];++y)
						for(int x=0;x<dataItem->shadowBufferSize[0];++x,++dtiPtr,++ciPtr)
							{
							GLColor<GLfloat,3> tc(*dtiPtr,*dtiPtr,*dtiPtr);
							*ciPtr=tc;
							}
					delete[] depthTextureImage;
					Images::writeImageFile(dti,"DepthImage.png");
					}
					#endif
					
					/* Draw the surface using the shadow texture: */
					rs.surfaceRenderer->glRenderShadowedIlluminatedHeightMap(dataItem->heightColorMapObject,dataItem->shadowDepthTextureObject,shadowProjection,contextData,textureTracker);
					}
			
			/* Reset OpenGL state: */
			glPopAttrib();
			}
		else
		#endif
			{
			/* Render the surface in a single pass: */
			rs.surfaceRenderer->renderSinglePass(ds.viewport,projection,ds.modelviewNavigational,contextData,textureTracker);
			}
		
		if(rs.waterRenderer!=0)
			{
			/* Draw the water surface: */
			glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(0.0f,0.5f,0.8f));
			glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,1.0f,1.0f));
			glMaterialShininess(GLMaterialEnums::FRONT,64.0f);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
			rs.waterRenderer->render(projection,ds.modelviewNavigational,contextData,textureTracker);
			glDisable(GL_BLEND);
			}
		
		/* Uninstall any remaining shader programs: */
		glUseProgramObjectARB(0);
		
		/* Call the remote server's render method: */
		if(remoteServer!=0)
			remoteServer->glRenderAction(projection,ds.modelviewNavigational,contextData);
		}
	else
		{
		/* Draw a white rectangle overlaying the entire viewport: */
		glPushAttrib(GL_ENABLE_BIT|GL_POLYGON_BIT);
		glDisable(GL_LIGHTING);
		glFrontFace(GL_CCW);
		glCullFace(GL_BACK);
		
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		
		/* Disable any shader programs: */
		glUseProgramObjectARB(0);
		
		glBegin(GL_QUADS);
		glColor3f(1.0f,1.0f,1.0f);
		glVertex3i(-1,-1,-1);
		glVertex3i( 1,-1,-1);
		glVertex3i( 1, 1,-1);
		glVertex3i(-1, 1,-1);
		glEnd();
		
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		
		glPopAttrib();
		}
	}

void Sandbox::resetNavigation(void)
	{
	/* Construct a navigation transformation to center the sandbox area in the display, facing the viewer, with the long sandbox axis facing to the right: */
	Vrui::NavTransform nav=Vrui::NavTransform::translateFromOriginTo(Vrui::getDisplayCenter());
	nav*=Vrui::NavTransform::scale(Vrui::getDisplaySize()/boxSize);
	Vrui::Vector y=Vrui::getUpDirection();
	Vrui::Vector z=Vrui::getForwardDirection();
	Vrui::Vector x=z^y;
	nav*=Vrui::NavTransform::rotate(Vrui::Rotation::fromBaseVectors(x,y));
	nav*=boxTransform;
	Vrui::setNavigationTransformation(nav);
	}

void Sandbox::eventCallback(Vrui::Application::EventID eventId,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	if(cbData->newButtonState)
		{
		switch(eventId)
			{
			case 0:
				/* Invert the current pause setting: */
				pauseUpdates=!pauseUpdates;
				
				/* Update the main menu toggle: */
				pauseUpdatesToggle->setToggle(pauseUpdates);
				
				break;
			
			case 1:
				/* Update roughness: */
				if(propertyGridCreator!=0)
					propertyGridCreator->requestRoughnessGrid(GLfloat(waterRoughnessSlider->getValue()));
				
				break;
			
			case 2:
				/* Update absorption rate: */
				if(propertyGridCreator!=0)
					propertyGridCreator->requestAbsorptionGrid(GLfloat(waterAbsorptionSlider->getValue()));
				
				break;
			}
		}
	}

void Sandbox::initContext(GLContextData& contextData) const
	{
	/* Create a data item and add it to the context: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	{
	/* Save the currently bound frame buffer: */
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	
	/* Set the default shadow buffer size: */
	dataItem->shadowBufferSize[0]=1024;
	dataItem->shadowBufferSize[1]=1024;
	
	/* Generate the shadow rendering frame buffer: */
	glGenFramebuffersEXT(1,&dataItem->shadowFramebufferObject);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->shadowFramebufferObject);
	
	/* Generate a depth texture for shadow rendering: */
	glGenTextures(1,&dataItem->shadowDepthTextureObject);
	glBindTexture(GL_TEXTURE_2D,dataItem->shadowDepthTextureObject);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE_ARB,GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC_ARB,GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D,GL_DEPTH_TEXTURE_MODE_ARB,GL_INTENSITY);
	glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24_ARB,dataItem->shadowBufferSize[0],dataItem->shadowBufferSize[1],0,GL_DEPTH_COMPONENT,GL_UNSIGNED_BYTE,0);
	glBindTexture(GL_TEXTURE_2D,0);
	
	/* Attach the depth texture to the frame buffer object: */
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,dataItem->shadowDepthTextureObject,0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	}
	}

VRUI_APPLICATION_RUN(Sandbox)
