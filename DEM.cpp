/***********************************************************************
DEM - Class to represent digital elevation models (DEMs) as float-valued
texture objects.
Copyright (c) 2013-2023 Oliver Kreylos

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

#include "DEM.h"

#include <IO/File.h>
#include <IO/OpenFile.h>
#include <GL/gl.h>
#include <GL/GLMiscTemplates.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <Geometry/Matrix.h>

#include "TextureTracker.h"
#include "Shader.h"

/******************************
Methods of class DEM::DataItem:
******************************/

DEM::DataItem::DataItem(void)
	:textureObjectId(0)
	{
	/* Create the texture object: */
	glGenTextures(1,&textureObjectId);
	}

DEM::DataItem::~DataItem(void)
	{
	/* Destroy the texture object: */
	glDeleteTextures(1,&textureObjectId);
	}

/********************
Methods of class DEM:
********************/

void DEM::calcMatrix(void)
	{
	/* Convert the DEM transformation into a projective transformation matrix: */
	demTransform=PTransform(transform);
	PTransform::Matrix& dtm=demTransform.getMatrix();
	
	/* Pre-multiply the projective transformation matrix with the DEM space to DEM pixel space transformation: */
	PTransform dem;
	dem.getMatrix()(0,0)=Scalar(demSize[0]-1)/(demBox[2]-demBox[0]);
	dem.getMatrix()(0,3)=Scalar(0.5)-Scalar(demSize[0]-1)/(demBox[2]-demBox[0])*demBox[0];
	dem.getMatrix()(1,1)=Scalar(demSize[1]-1)/(demBox[3]-demBox[1]);
	dem.getMatrix()(1,3)=Scalar(0.5)-Scalar(demSize[1]-1)/(demBox[3]-demBox[1])*demBox[1];
	dem.getMatrix()(2,2)=Scalar(1)/verticalScale;
	dem.getMatrix()(2,3)=verticalScaleBase-verticalScaleBase/verticalScale;
	demTransform.leftMultiply(dem);
	
	/* Convert the full transformation to column-major OpenGL format: */
	GLfloat* dtmPtr=demTransformMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++dtmPtr)
			*dtmPtr=GLfloat(dtm(i,j));
	}

DEM::DEM(void)
	:demSize(0,0),
	 dem(0),
	 transform(OGTransform::identity),
	 verticalScale(1),verticalScaleBase(0)
	{
	}

DEM::~DEM(void)
	{
	delete[] dem;
	}

void DEM::initContext(GLContextData& contextData) const
	{
	/* Initialize required OpenGL extensions: */
	GLARBTextureFloat::initExtension();
	GLARBTextureRectangle::initExtension();
	
	/* Create and register a data item: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Upload the DEM array into the texture object: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->textureObjectId);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_LUMINANCE32F_ARB,demSize,0,GL_LUMINANCE,GL_FLOAT,dem);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	}

void DEM::load(const char* demFileName)
	{
	/* Read the DEM file: */
	IO::FilePtr demFile=IO::openFile(demFileName);
	demFile->setEndianness(Misc::LittleEndian);
	demFile->read<int,unsigned int>(demSize.getComponents(),2);
	dem=new float[demSize.volume()];
	demFile->read<float,double>(demBox,4);
	demFile->read(dem,demSize.volume());
	
	/* Update the DEM transformation: */
	calcMatrix();
	}

float DEM::calcAverageElevation(void) const
	{
	/* Sum all elevation measurements: */
	double elevSum=0.0;
	const float* demPtr=dem;
	const float* demEnd=dem+demSize.volume();
	for(;demPtr!=demEnd;++demPtr)
		elevSum+=double(*demPtr);
	
	/* Return the average elevation: */
	return float(elevSum/double(demSize.volume()));
	}

void DEM::setTransform(const OGTransform& newTransform,Scalar newVerticalScale,Scalar newVerticalScaleBase)
	{
	transform=newTransform;
	verticalScale=newVerticalScale;
	verticalScaleBase=newVerticalScaleBase;
	
	/* Update the DEM transformation: */
	calcMatrix();
	}

void DEM::uploadDemTransform(Shader& shader) const
	{
	/* Upload the DEM transform matrix to the given shader: */
	shader.uploadUniformMatrix4(1,GL_FALSE,demTransformMatrix);
	}

GLint DEM::bindTexture(GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the context data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the DEM texture to the next available texture unit: */
	return textureTracker.bindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->textureObjectId);
	}
