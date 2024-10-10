/***********************************************************************
WaterRenderer - Class to render a water surface defined by regular grids
of vertex-centered bathymetry and cell-centered water level values.
Copyright (c) 2014-2023 Oliver Kreylos

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

#include "WaterRenderer.h"

#include <GL/gl.h>
#include <GL/GLVertexArrayParts.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBVertexBufferObject.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/GLTransformationWrappers.h>

#include "TextureTracker.h"
#include "ShaderHelper.h"
#include "WaterTable2.h"

/****************************************
Methods of class WaterRenderer::DataItem:
****************************************/

WaterRenderer::DataItem::DataItem(void)
	:vertexBuffer(0),indexBuffer(0)
	{
	/* Allocate the buffers: */
	glGenBuffersARB(1,&vertexBuffer);
	glGenBuffersARB(1,&indexBuffer);
	}

WaterRenderer::DataItem::~DataItem(void)
	{
	/* Release all allocated buffers: */
	glDeleteBuffersARB(1,&vertexBuffer);
	glDeleteBuffersARB(1,&indexBuffer);
	}

/******************************
Methods of class WaterRenderer:
******************************/

WaterRenderer::WaterRenderer(const WaterTable2* sWaterTable)
	:waterTable(sWaterTable),
	 waterGridSize(waterTable->getSize()),
	 bathymetryGridSize(waterGridSize[0]-1,waterGridSize[1]-1)
	{
	/* Copy the water table's grid cell size: */
	for(int i=0;i<2;++i)
		cellSize[i]=waterTable->getCellSize()[i];
	
	/* Get the water table's domain: */
	const WaterTable2::Box& wd=waterTable->getDomain();
	
	/* Calculate the transformation from grid space to world space: */
	gridTransform=PTransform::identity;
	PTransform::Matrix& gtm=gridTransform.getMatrix();
	gtm(0,0)=(wd.max[0]-wd.min[0])/Scalar(waterGridSize[0]);
	gtm(0,3)=wd.min[0];
	gtm(1,1)=(wd.max[1]-wd.min[1])/Scalar(waterGridSize[1]);
	gtm(1,3)=wd.min[1];
	gridTransform.leftMultiply(Geometry::invert(waterTable->getBaseTransform()));
	
	/* Calculate the transposed tangent-plane transformation from grid space to world space: */
	tangentGridTransform=PTransform::identity;
	PTransform::Matrix& tgtm=tangentGridTransform.getMatrix();
	tgtm(0,0)=Scalar(waterGridSize[0])/(wd.max[0]-wd.min[0]);
	tgtm(0,3)=-wd.min[0]*tgtm(0,0);
	tgtm(1,1)=Scalar(waterGridSize[1])/(wd.max[1]-wd.min[1]);
	tgtm(1,3)=-wd.min[1]*tgtm(1,1);
	tangentGridTransform*=waterTable->getBaseTransform();
	}

void WaterRenderer::initContext(GLContextData& contextData) const
	{
	/* Initialize required OpenGL extensions: */
	GLARBFragmentShader::initExtension();
	GLARBVertexBufferObject::initExtension();
	GLARBVertexShader::initExtension();
	Shader::initExtensions();
	TextureTracker::initExtensions();
	
	/* Create a data item and add it to the context: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Upload the grid of template vertices into the vertex buffer: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB,waterGridSize[1]*waterGridSize[0]*sizeof(Vertex),0,GL_STATIC_DRAW_ARB);
	Vertex* vPtr=static_cast<Vertex*>(glMapBufferARB(GL_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	for(unsigned int y=0;y<waterGridSize[1];++y)
		for(unsigned int x=0;x<waterGridSize[0];++x,++vPtr)
			{
			/* Set the template vertex' position to the pixel center's position: */
			vPtr->position[0]=GLfloat(x)+0.5f;
			vPtr->position[1]=GLfloat(y)+0.5f;
			}
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	
	/* Upload the surface's triangle indices into the index buffer: */
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,(waterGridSize[1]-1)*waterGridSize[0]*2*sizeof(GLuint),0,GL_STATIC_DRAW_ARB);
	GLuint* iPtr=static_cast<GLuint*>(glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	for(unsigned int y=1;y<waterGridSize[1];++y)
		for(unsigned int x=0;x<waterGridSize[0];++x,iPtr+=2)
			{
			iPtr[0]=GLuint(y*waterGridSize[0]+x);
			iPtr[1]=GLuint((y-1)*waterGridSize[0]+x);
			}
	glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	
	/* Create the water rendering shader: */
	dataItem->waterShader.addShader(compileVertexShader("WaterRenderingShader"));
	dataItem->waterShader.addShader(compileFragmentShader("WaterRenderingShader"));
	dataItem->waterShader.link();
	dataItem->waterShader.setUniformLocation("quantitySampler");
	dataItem->waterShader.setUniformLocation("bathymetrySampler");
	dataItem->waterShader.setUniformLocation("modelviewGridMatrix");
	dataItem->waterShader.setUniformLocation("tangentModelviewGridMatrix");
	dataItem->waterShader.setUniformLocation("projectionModelviewGridMatrix");
	}

void WaterRenderer::render(const PTransform& projection,const OGTransform& modelview,GLContextData& contextData,TextureTracker& textureTracker) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Calculate the required matrices: */
	PTransform projectionModelview=projection;
	projectionModelview*=modelview;
	
	/* Install the water rendering shader: */
	dataItem->waterShader.use();
	textureTracker.reset();
	
	/* Bind the water quantity texture: */
	dataItem->waterShader.uploadUniform(waterTable->bindQuantityTexture(contextData,textureTracker,false));
	
	/* Bind the bathymetry texture: */
	dataItem->waterShader.uploadUniform(waterTable->bindBathymetryTexture(contextData,textureTracker,false));
	
	/* Calculate and upload the vertex transformation from grid space to eye space: */
	PTransform modelviewGridTransform=gridTransform;
	modelviewGridTransform.leftMultiply(modelview);
	dataItem->waterShader.uploadUniform(modelviewGridTransform);
	
	/* Calculate the transposed tangent plane transformation from grid space to eye space: */
	PTransform tangentModelviewGridTransform=tangentGridTransform;
	tangentModelviewGridTransform*=Geometry::invert(modelview);
	
	/* Transpose and upload the transposed tangent plane transformation: */
	const Scalar* tmvgtPtr=tangentModelviewGridTransform.getMatrix().getEntries();
	GLfloat tangentModelviewGridTransformMatrix[16];
	GLfloat* tmvgtmPtr=tangentModelviewGridTransformMatrix;
	for(int i=0;i<16;++i,++tmvgtPtr,++tmvgtmPtr)
		*tmvgtmPtr=GLfloat(*tmvgtPtr);
	dataItem->waterShader.uploadUniformMatrix4(1,GL_FALSE,tangentModelviewGridTransformMatrix);
	
	/* Calculate and upload the vertex transformation from grid space to clip space: */
	PTransform projectionModelviewGridTransform=gridTransform;
	projectionModelviewGridTransform.leftMultiply(modelview);
	projectionModelviewGridTransform.leftMultiply(projection);
	dataItem->waterShader.uploadUniform(projectionModelviewGridTransform);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Draw the surface: */
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<waterGridSize[1];++y,indexPtr+=waterGridSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,waterGridSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind all textures and buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	}
