/***********************************************************************
PropertyGridCreator - Class to create grids of properties (inundation,
infiltration, surface roughness, ...) for water flow simulation by
mapping the water simulation bathymetry into color camera space.
Copyright (c) 2023 Oliver Kreylos

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

#include "PropertyGridCreator.h"

#include <string.h>
#include <stdexcept>
#include <Misc/Utility.h>
#include <Misc/MessageLogger.h>
#include <GL/gl.h>
#include <GL/GLMiscTemplates.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBDrawBuffers.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/Extensions/GLEXTFramebufferObject.h>
#include <tiffio.h>
#include <Images/BaseImage.h>
#include <Images/ReadImageFile.h>
#include <Kinect/FrameSource.h>

#include "TextureTracker.h"
#include "ShaderHelper.h"
#include "WaterTable2.h"

/**********************************************
Methods of class PropertyGridCreator::DataItem:
**********************************************/

PropertyGridCreator::DataItem::DataItem(void)
	:createdGridTextureObject(0),
	 colorImageTextureObject(0),colorImageVersion(0),
	 createdGridFramebufferObject(0),
	 loadGridRequest(0),saveGridRequest(0)
	{
	globalParametersVersions[1]=globalParametersVersions[0]=0U;
	}

PropertyGridCreator::DataItem::~DataItem(void)
	{
	/* Delete all allocated textures and framebuffers: */
	glDeleteTextures(1,&createdGridTextureObject);
	glDeleteTextures(1,&colorImageTextureObject);
	glDeleteFramebuffersEXT(1,&createdGridFramebufferObject);
	}

/************************************
Methods of class PropertyGridCreator:
************************************/

PropertyGridCreator::PropertyGridCreator(const WaterTable2& sWaterTable,Kinect::FrameSource& frameSource)
	:waterTable(sWaterTable),gridSize(waterTable.getSize()),
	 roughness(0.01),absorption(0.0),
	 colorIsYuv(frameSource.getColorSpace()==Kinect::FrameSource::YPCBCR),
	 colorImageSize(frameSource.getActualFrameSize(Kinect::FrameSource::COLOR)),
	 colorImageVersion(0),
	 requestState(0U),requestMask(0x0U),requestRoughness(0),requestAbsorption(0),
	 loadGridRequest(0),saveGridRequest(0)
	{
	globalParametersVersions[1]=globalParametersVersions[0]=1U;
	
	/* Calculate the transformation from property grid space to upright elevation map space: */
	const WaterTable2::Box& wtd=waterTable.getDomain();
	PTransform::Matrix& btc=bathymetryToColor.getMatrix();
	btc(0,0)=(wtd.max[0]-wtd.min[0])/Scalar(gridSize[0]);
	btc(0,3)=wtd.min[0];
	btc(1,1)=(wtd.max[1]-wtd.min[1])/Scalar(gridSize[1]);
	btc(1,3)=wtd.min[1];
	
	/* Concatenate the transformation from upright elevation map space to 3D camera space: */
	bathymetryToColor.leftMultiply(Geometry::invert(waterTable.getBaseTransform()));
	
	const Kinect::FrameSource::IntrinsicParameters& ips=frameSource.getIntrinsicParameters();
	
	/* Concatenate the transformation from 3D camera space to depth camera space: */
	bathymetryToColor.leftMultiply(Geometry::invert(ips.depthProjection));
	
	/* Concatenate the transformation from depth camera space to color camera space: */
	bathymetryToColor.leftMultiply(ips.colorProjection);
	
	/* Concatenate the transformation from unit size to color image size: */
	PTransform unitToImageSize=PTransform::identity;
	PTransform::Matrix& utis=unitToImageSize.getMatrix();
	utis(0,0)=Scalar(colorImageSize[0]);
	utis(1,1)=Scalar(colorImageSize[1]);
	bathymetryToColor.leftMultiply(unitToImageSize);
	
	/* Convert the transformation to GLSL-compatible format: */
	GLfloat* btcmPtr=bathymetryToColorMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++btcmPtr)
			*btcmPtr=GLfloat(btc(i,j));
	}

PropertyGridCreator::~PropertyGridCreator(void)
	{
	}

void PropertyGridCreator::initContext(GLContextData& contextData) const
	{
	/* Initialize required OpenGL extensions: */
	GLARBDrawBuffers::initExtension();
	GLARBFragmentShader::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureRg::initExtension();
	GLARBVertexShader::initExtension();
	GLEXTFramebufferObject::initExtension();
	Shader::initExtensions();
	
	/* Create a data item and add it to the context: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Create the color frame texture: */
	glGenTextures(1,&dataItem->colorImageTextureObject);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->colorImageTextureObject);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_RGBA8,colorImageSize[0],colorImageSize[1],0,GL_RGB,GL_UNSIGNED_BYTE,0);
	
	/* Create the property grid texture: */
	glGenTextures(1,&dataItem->createdGridTextureObject);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->createdGridTextureObject);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_RG32F,gridSize[0],gridSize[1],0,GL_RG,GL_FLOAT,0);
	
	/* Protect the generated texture objects: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	/* Save the currently bound framebuffer: */
	GLint currentFramebuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFramebuffer);
	
	/* Create the property grid rendering framebuffer: */
	glGenFramebuffersEXT(1,&dataItem->createdGridFramebufferObject);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->createdGridFramebufferObject);
	
	/* Attach the created grid texture to the property rendering framebuffer: */
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_RECTANGLE_ARB,dataItem->createdGridTextureObject,0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	
	/* Restore the previously bound framebuffer: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFramebuffer);
	
	/* Create the grid reset shader: */
	dataItem->gridResetShader.addShader(glCompileVertexShaderFromString("void main(){gl_Position=gl_Vertex;}"));
	dataItem->gridResetShader.addShader(glCompileFragmentShaderFromString("uniform float roughness,absorption; void main(){gl_FragData[0]=vec4(roughness,absorption,0.0,0.0);}"));
	dataItem->gridResetShader.link();
	dataItem->gridResetShader.setUniformLocation("roughness");
	dataItem->gridResetShader.setUniformLocation("absorption");
	
	/* Create the grid creation shader: */
	dataItem->gridCreatorShader.addShader(glCompileVertexShaderFromString("void main(){gl_Position=gl_Vertex;}"));
	dataItem->gridCreatorShader.addShader(compileFragmentShader(colorIsYuv?"PropertyGridCreatorShaderYpCbCr":"PropertyGridCreatorShaderRGB"));
	dataItem->gridCreatorShader.link();
	dataItem->gridCreatorShader.setUniformLocation("colorImageSampler");
	dataItem->gridCreatorShader.setUniformLocation("bathymetrySampler");
	dataItem->gridCreatorShader.setUniformLocation("bathymetryColorMatrix");
	dataItem->gridCreatorShader.setUniformLocation("roughness");
	dataItem->gridCreatorShader.setUniformLocation("absorption");
	}

void PropertyGridCreator::setRoughness(GLfloat newRoughness)
	{
	/* Update surface roughness: */
	roughness=newRoughness;
	++globalParametersVersions[0];
	}

void PropertyGridCreator::setAbsorption(GLfloat newAbsorption)
	{
	/* Update absorption rate: */
	absorption=newAbsorption;
	++globalParametersVersions[1];
	}

GLint PropertyGridCreator::bindPropertyGridTexture(GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the property grid texture: */
	return textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->createdGridTextureObject);
	}

void PropertyGridCreator::receiveRawFrame(const Kinect::FrameBuffer& frameBuffer)
	{
	/* Store the new color image: */
	colorImage=frameBuffer;
	++colorImageVersion;
	
	/* Count down a current grid creation request: */
	if(requestState!=0U)
		--requestState;
	}

bool PropertyGridCreator::requestRoughnessGrid(GLfloat newRequestRoughness)
	{
	/* Bail out if there is an active request: */
	if(requestState!=0U)
		return false;
	
	/* Set the current request: */
	requestState=15U;
	requestMask=0x1U;
	requestRoughness=newRequestRoughness;
	
	return true;
	}

bool PropertyGridCreator::requestAbsorptionGrid(GLfloat newRequestAbsorption)
	{
	/* Bail out if there is an active request: */
	if(requestState!=0U)
		return false;
	
	/* Set the current request: */
	requestState=15U;
	requestMask=0x2U;
	requestAbsorption=newRequestAbsorption;
	
	return true;
	}

void PropertyGridCreator::loadGrid(const char* gridFileName)
	{
	/* Store the load request: */
	++loadGridRequest;
	loadGridFileName=gridFileName;
	}

void PropertyGridCreator::saveGrid(const char* gridFileName)
	{
	/* Store the save request: */
	++saveGridRequest;
	saveGridFileName=gridFileName;
	}

void PropertyGridCreator::updatePropertyGrid(GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Check whether the property grid texture needs to be updated: */
	bool globalUpdate=dataItem->globalParametersVersions[0]!=globalParametersVersions[0]||dataItem->globalParametersVersions[1]!=globalParametersVersions[1];
	if(requestState==1U||globalUpdate)
		{
		/* Save relevant OpenGL state: */
		glPushAttrib(GL_COLOR_BUFFER_BIT|GL_VIEWPORT_BIT);
		GLint currentFramebuffer;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFramebuffer);
		
		/* Set up the property grid rendering framebuffer: */
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->createdGridFramebufferObject);
		glViewport(gridSize);
		
		/* Check for a global property update: */
		if(globalUpdate)
			{
			/* Select property grid channels to be overwritten: */
			glColorMask(dataItem->globalParametersVersions[0]!=globalParametersVersions[0],dataItem->globalParametersVersions[1]!=globalParametersVersions[1],GL_FALSE,GL_FALSE);
			
			/* Set up the property grid reset shader: */
			dataItem->gridResetShader.use();
			
			/* Set the desired roughness and absorption values: */
			dataItem->gridResetShader.uploadUniform(roughness);
			dataItem->gridResetShader.uploadUniform(absorption);
			
			/* Run the grid creation shader: */
			glBegin(GL_QUADS);
			glVertex2i(-1,-1);
			glVertex2i(1,-1);
			glVertex2i(1,1);
			glVertex2i(-1,1);
			glEnd();
			
			/* Mark the global update as complete: */
			for(int i=0;i<2;++i)
				dataItem->globalParametersVersions[i]=globalParametersVersions[i];
			}
		
		/* Check for a local property update: */
		if(requestState==1U)
			{
			/* Select property grid channels to be overwritten: */
			glColorMask((requestMask&0x1U)!=0x0U,(requestMask&0x2U)!=0x0U,GL_FALSE,GL_FALSE);
			
			/* Set up the property grid creation shader: */
			dataItem->gridCreatorShader.use();
			textureTracker.reset();
			
			/* Bind the color image texture: */
			dataItem->gridCreatorShader.uploadUniform(textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->colorImageTextureObject));
			
			/* Check if the color image texture is out of date: */
			if(dataItem->colorImageVersion!=colorImageVersion)
				{
				/* Upload the current color image into the color image texture: */
				glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,colorImageSize[0],colorImageSize[1],GL_RGB,GL_UNSIGNED_BYTE,colorImage.getData<GLubyte>());
				
				/* Mark the color image texture as up-to-date: */
				dataItem->colorImageVersion=colorImageVersion;
				}
			
			/* Bind the water table's current bathymetry texture: */
			dataItem->gridCreatorShader.uploadUniform(waterTable.bindBathymetryTexture(contextData,textureTracker,false));
			
			/* Upload the bathymetry-to-color transformation matrix: */
			dataItem->gridCreatorShader.uploadUniformMatrix4(1,GL_FALSE,bathymetryToColorMatrix);
			
			/* Set the desired roughness and absorption values: */
			dataItem->gridCreatorShader.uploadUniform(requestRoughness);
			dataItem->gridCreatorShader.uploadUniform(requestAbsorption);
			
			/* Run the grid creation shader: */
			glBegin(GL_QUADS);
			glVertex2i(-1,-1);
			glVertex2i(1,-1);
			glVertex2i(1,1);
			glVertex2i(-1,1);
			glEnd();
			}
		
		/* Restore OpenGL state: */
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFramebuffer);
		glPopAttrib();
		}
	
	/* Check if there is a request to load a grid: */
	if(dataItem->loadGridRequest!=loadGridRequest)
		{
		try
			{
			/* Load the image file of the given name: */
			Images::BaseImage gridImage=Images::readGenericImageFile(loadGridFileName.c_str());
			if(gridImage.getSize()==gridSize&&gridImage.getNumChannels()==2&&gridImage.getChannelSize()==4&&gridImage.getScalarType()==GL_FLOAT)
				{
				/* Upload the grid image into the grid property texture: */
				textureTracker.reset();
				textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->createdGridTextureObject);
				glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,gridSize[0],gridSize[1],GL_RG,GL_FLOAT,gridImage.getPixels());
				}
			}
		catch(const std::runtime_error& err)
			{
			Misc::formattedUserError("PropertyGridCreator: Unable to load property grid file %s due to exception %s",loadGridFileName.c_str(),err.what());
			}
		
		/* Mark the load request as done: */
		dataItem->loadGridRequest=loadGridRequest;
		}
	
	/* Check if there is a request to save a grid: */
	if(dataItem->saveGridRequest!=saveGridRequest)
		{
		try
			{
			/* Download the grid property texture into an image: */
			textureTracker.reset();
			textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->createdGridTextureObject);
			GLfloat* gridImage=new GLfloat[gridSize.volume()*2];
			glGetTexImage(GL_TEXTURE_RECTANGLE_ARB,0,GL_RG,GL_FLOAT,gridImage);
			
			/* Save the grid image to a TIFF file: */
			TIFF* imageFile=TIFFOpen(saveGridFileName.c_str(),"w");
			if(imageFile==0)
				throw std::runtime_error("Unable to create image file");
			bool isOkay=true;
			isOkay=isOkay&&TIFFSetField(imageFile,TIFFTAG_IMAGEWIDTH,gridSize[0])!=0;
			isOkay=isOkay&&TIFFSetField(imageFile,TIFFTAG_IMAGELENGTH,gridSize[1])!=0;
			isOkay=isOkay&&TIFFSetField(imageFile,TIFFTAG_SAMPLESPERPIXEL,2)!=0;
			isOkay=isOkay&&TIFFSetField(imageFile,TIFFTAG_SAMPLEFORMAT,SAMPLEFORMAT_IEEEFP)!=0;
			isOkay=isOkay&&TIFFSetField(imageFile,TIFFTAG_BITSPERSAMPLE,32)!=0;
			isOkay=isOkay&&TIFFSetField(imageFile,TIFFTAG_ORIENTATION,ORIENTATION_TOPLEFT)!=0;
			isOkay=isOkay&&TIFFSetField(imageFile,TIFFTAG_COMPRESSION,COMPRESSION_NONE)!=0;
			isOkay=isOkay&&TIFFSetField(imageFile,TIFFTAG_PLANARCONFIG,PLANARCONFIG_CONTIG)!=0;
			isOkay=isOkay&&TIFFSetField(imageFile,TIFFTAG_PHOTOMETRIC,PHOTOMETRIC_MINISBLACK)!=0;
			size_t rowStride=gridSize[0]*2*sizeof(GLfloat);
			isOkay=isOkay&&TIFFSetField(imageFile,TIFFTAG_ROWSPERSTRIP,TIFFDefaultStripSize(imageFile,rowStride))!=0;
			if(!isOkay)
				throw std::runtime_error("Unable to write image specification");
			
			/* Write the image top to bottom: */
			tdata_t rowBuffer=_TIFFmalloc(Misc::max(rowStride,size_t(TIFFScanlineSize(imageFile))));
			for(uint32_t row=0;row<gridSize[1]&&isOkay;++row)
				{
				memcpy(rowBuffer,gridImage+(gridSize[1]-1-row)*gridSize[0]*2,rowStride);
				isOkay=TIFFWriteScanline(imageFile,rowBuffer,row,0)>=0;
				}
			_TIFFfree(rowBuffer);
			
			TIFFClose(imageFile);
			
			if(!isOkay)
				throw std::runtime_error("Unable to write image data");
			}
		catch(const std::runtime_error& err)
			{
			Misc::formattedUserError("PropertyGridCreator: Unable to save property grid file %s due to exception %s",saveGridFileName.c_str(),err.what());
			}
		
		/* Mark the save request as done: */
		dataItem->saveGridRequest=saveGridRequest;
		}
	}
