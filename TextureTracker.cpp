/***********************************************************************
TextureTracker - Helper class to track which texture objects are
currently bound to which texture targets on the OpenGL context's texture
units.
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

#include "TextureTracker.h"

#include <stdexcept>
#include <GL/gl.h>
#include <GL/Extensions/GLARBMultitexture.h>
#include <GL/Extensions/GLARBVertexProgram.h>

/*******************************
Methods of class TextureTracker:
*******************************/

void TextureTracker::initExtensions(void)
	{
	GLARBMultitexture::initExtension();
	GLARBVertexProgram::initExtension();
	}

TextureTracker::TextureTracker(void)
	:numUnits(0),bindings(0),
	 numActiveUnits(0),nextUnit(0)
	{
	/* Query the number of available texture units in the current OpenGL context: */
	glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS_ARB,&numUnits);
	
	/* Allocate the bindings array: */
	bindings=new Binding[numUnits];
	}

TextureTracker::~TextureTracker(void)
	{
	/* Unbind all bound textures: */
	for(GLint i=0;i<numActiveUnits;++i)
		{
		glActiveTextureARB(GL_TEXTURE0_ARB+i);
		glBindTexture(bindings[i].target,0U);
		}
	
	/* Reset the active texture unit: */
	glActiveTextureARB(GL_TEXTURE0_ARB);
	
	/* Release the bindings array: */
	delete[] bindings;
	}

void TextureTracker::reset(void)
	{
	/* Start binding from the first texture unit again: */
	nextUnit=0;
	}

GLint TextureTracker::bindTexture(GLenum target,GLuint texture)
	{
	GLint result=nextUnit;
	
	/* Check if there is another texture unit available: */
	if(nextUnit>=numUnits)
		throw std::runtime_error("TextureTracker::bindTexture: No more available texture units");
	
	/* Bind the texture to the next texture unit: */
	glActiveTextureARB(GL_TEXTURE0_ARB+nextUnit);
	glBindTexture(target,texture);
	bindings[nextUnit].target=target;
	bindings[nextUnit].texture=texture;
	++nextUnit;
	
	/* Update the number of active texture units: */
	if(numActiveUnits<nextUnit)
		numActiveUnits=nextUnit;
	
	/* Return the used texture unit and move to the next one: */
	return result;
	}
