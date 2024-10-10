/***********************************************************************
Shader - Helper class to represent a GLSL shader program and its uniform
variable locations.
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

#include "Shader.h"

#include <utility>
#include <stdexcept>
#include <Misc/StdError.h>
#include <Geometry/TranslationTransformation.h>
#include <Geometry/RotationTransformation.h>
#include <Geometry/OrthonormalTransformation.h>
#include <Geometry/OrthogonalTransformation.h>
#include <Geometry/AffineTransformation.h>
#include <Geometry/ProjectiveTransformation.h>

/***********************
Methods of class Shader:
***********************/

void Shader::initExtensions(void)
	{
	GLARBShaderObjects::initExtension();
	}

Shader::Shader(void)
	:shaderProgram(0),
	 numUniforms(0),uniformLocations(0),
	 nextUniformIndex(0)
	{
	}

Shader::~Shader(void)
	{
	/* Clear the link list in case the shader program was never linked: */
	clearLinkList();
	
	/* Release the array of uniform locations: */
	delete[] uniformLocations;
	
	/* Destroy the shader program object: */
	if(shaderProgram!=0)
		glDeleteObjectARB(shaderProgram);
	}

void Shader::addShader(GLhandleARB shader,bool deleteAfterLink)
	{
	/* Add the compiled shader object to the link list: */
	linkList.push_back(LinkListItem(shader,deleteAfterLink));
	}

void Shader::clearLinkList(void)
	{
	/* Delete all flagged shaders in the link list: */
	for(std::vector<LinkListItem>::iterator llIt=linkList.begin();llIt!=linkList.end();++llIt)
		if(llIt->deleteAfterLink)
			glDeleteObjectARB(llIt->shader);
	
	/* Release the link list: */
	std::vector<LinkListItem> emptyLinkList;
	std::swap(linkList,emptyLinkList);
	}

void Shader::link(void)
	{
	try
		{
		/* Link the shader program: */
		std::vector<GLhandleARB> shaders;
		shaders.reserve(linkList.size());
		for(std::vector<LinkListItem>::iterator llIt=linkList.begin();llIt!=linkList.end();++llIt)
			shaders.push_back(llIt->shader);
		GLhandleARB newShaderProgram=glLinkShader(shaders);
		
		/* Clear the link list: */
		clearLinkList();
		
		/* Delete a previous and install the new shader program: */
		if(shaderProgram!=0)
			glDeleteObjectARB(shaderProgram);
		shaderProgram=newShaderProgram;
		
		/* Query and set the number of uniform variables used by the shader: */
		GLint uniformCount;
		glGetObjectParameterivARB(shaderProgram,GL_OBJECT_ACTIVE_UNIFORMS_ARB,&uniformCount);
		setNumUniforms(uniformCount);
		}
	catch(...)
		{
		/* Clear the link list: */
		clearLinkList();
		
		/* Re-throw the exception: */
		throw;
		}
	}

void Shader::setNumUniforms(unsigned int newNumUniforms)
	{
	/* Re-allocate the uniform location array if the number of uniform variables changed: */
	if(numUniforms!=newNumUniforms)
		{
		delete[] uniformLocations;
		numUniforms=newNumUniforms;
		uniformLocations=numUniforms>0?new GLint[numUniforms]:0;
		}
	
	/* Reset all uniform locations: */
	for(unsigned int i=0;i<numUniforms;++i)
		uniformLocations[i]=-1;
	
	/* Reset the uniform index to start assigning locations from the top: */
	nextUniformIndex=0;
	}

unsigned int Shader::setUniformLocation(const char* uniformName)
	{
	unsigned int result=nextUniformIndex;
	
	/* Ensure that the uniform array is big enough (this can be considered a run-time error): */
	if(nextUniformIndex>=numUniforms)
		throw Misc::makeStdErr(__PRETTY_FUNCTION__,"Attempt to set more uniform variables than are used by shader");
	
	/* Query the uniform location from the linked shader program object: */
	GLint location=glGetUniformLocationARB(shaderProgram,uniformName);
	#if 0
	if(location<0)
		throw Misc::makeStdErr(__PRETTY_FUNCTION__,"Uniform variable %s not used in shader",uniformName);
	#endif
	uniformLocations[nextUniformIndex]=location;
	++nextUniformIndex;
	
	return result;
	}

namespace {

/*************************************************************
Helper functions to upload transformations into GLSL matrices:
*************************************************************/

template <class TransformParam>
inline
void
glUniformMatrix(
	GLint location,
	const TransformParam& transform)
	{
	/* Write the transformation to a 4x4 matrix: */
	Geometry::Matrix<GLfloat,4,4> matrix(1.0f);
	transform.writeMatrix(matrix);
	
	/* Upload the matrix to the uniform variable at the given location: */
	glUniformMatrix4fvARB(location,1,GL_TRUE,matrix.getEntries());
	}

template <>
inline
void
glUniformMatrix<Geometry::ProjectiveTransformation<GLfloat,3> >(
	GLint location,
	const Geometry::ProjectiveTransformation<GLfloat,3>& transform)
	{
	/* Upload the transformation's matrix to the uniform variable at the given location: */
	glUniformMatrix4fvARB(location,1,GL_TRUE,transform.getMatrix().getEntries());
	}

}

template <class TransformParam>
inline
void
Shader::uploadUniform(
	const TransformParam& transform)
	{
	glUniformMatrix<TransformParam>(uniformLocations[nextUniformIndex++],transform);
	}

/*****************************************************
Force instantiation of standard uploadUniform methods:
*****************************************************/

template void Shader::uploadUniform(const Geometry::TranslationTransformation<GLfloat,3>&);
template void Shader::uploadUniform(const Geometry::TranslationTransformation<GLdouble,3>&);
template void Shader::uploadUniform(const Geometry::RotationTransformation<GLfloat,3>&);
template void Shader::uploadUniform(const Geometry::RotationTransformation<GLdouble,3>&);
template void Shader::uploadUniform(const Geometry::OrthonormalTransformation<GLfloat,3>&);
template void Shader::uploadUniform(const Geometry::OrthonormalTransformation<GLdouble,3>&);
template void Shader::uploadUniform(const Geometry::OrthogonalTransformation<GLfloat,3>&);
template void Shader::uploadUniform(const Geometry::OrthogonalTransformation<GLdouble,3>&);
template void Shader::uploadUniform(const Geometry::AffineTransformation<GLfloat,3>&);
template void Shader::uploadUniform(const Geometry::AffineTransformation<GLdouble,3>&);
template void Shader::uploadUniform(const Geometry::ProjectiveTransformation<GLfloat,3>&);
template void Shader::uploadUniform(const Geometry::ProjectiveTransformation<GLdouble,3>&);
