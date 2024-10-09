/***********************************************************************
Shader - Helper class to represent a GLSL shader program and its uniform
variable locations.
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

#ifndef SHADER_INCLUDED
#define SHADER_INCLUDED

#include <vector>
#include <GL/gl.h>
#include <GL/Extensions/GLARBShaderObjects.h>

class Shader
	{
	/* Embedded classes: */
	private:
	struct LinkListItem // Structure holding a compiled shader object to be linked into the shader program
		{
		/* Elements: */
		public:
		GLhandleARB shader; // Handle of the compiled shader
		bool deleteAfterLink; // Flag to indicate that the shader can be deleted after the shader program is linked successfully
		
		/* Constructors and destructors: */
		LinkListItem(GLhandleARB sShader,bool sDeleteAfterLink) // Elementwise constructor
			:shader(sShader),deleteAfterLink(sDeleteAfterLink)
			{
			}
		};
	
	/* Elements: */
	private:
	std::vector<LinkListItem> linkList; // List of compiled vertex/fragment/etc. shaders to be linked into the shader program; will be released after linking
	GLhandleARB shaderProgram; // Handle of the linked shader program object
	unsigned int numUniforms; // Number of uniform variables used by the shader program
	GLint* uniformLocations; // Array of locations of the shader program's uniform variables
	unsigned int nextUniformIndex; // Next uniform variable index to be used when creating a shader or uploading uniform variables
	
	/* Constructors and destructors: */
	public:
	static void initExtensions(void); // Initializes OpenGL extensions required by this class
	Shader(void); // Creates an uninitialized shader
	private:
	Shader(const Shader& source); // Prohibit copy constructor
	Shader& operator=(const Shader& source); // Prohibit assignment operator
	public:
	~Shader(void); // Destroys the shader program
	
	/* Methods to create a shader program: */
	void addShader(GLhandleARB shader,bool deleteAfterLink =true); // Adds a shader to the shader program's link list; shader will be deleted after linking if given flag is true
	void clearLinkList(void); // Deletes all compiled shaders marked for deletion in the link list, then clears the link list
	void link(void); // Links the shader program from all previously defined shaders; throws exception and clears the link list and retains current shader program if linking errors occur
	void setNumUniforms(unsigned int newNumUniforms); // Sets the number of uniform variables used by the shader
	unsigned int setUniformLocation(const char* uniformName); // Stores the location of the uniform variable of the given name at the next uniform variable index; returns the index of the uniform variable that was set
	GLint getUniformLocation(unsigned int uniformIndex) const // Returns the location of the uniform variable of the given index
		{
		return uniformLocations[uniformIndex];
		}
	
	/* Methods to use and disable shader programs: */
	void use(void) // Installs the shader program as the active shader program in the current OpenGL context and prepares to upload uniform variables
		{
		/* Install the shader program: */
		glUseProgramObjectARB(shaderProgram);
		
		/* Prepare to upload uniform variables: */
		nextUniformIndex=0;
		}
	void resetUniforms(void) // Explicitly resets uniform variable upload
		{
		nextUniformIndex=0;
		}
	static void unuse(void) // Uninstalls any currently installed shader programs
		{
		glUseProgramObjectARB(0);
		}
	
	/* Methods to upload uniform variables in the same order that they were set during shader creation: */
	void uploadUniform(GLint i0)
		{
		glUniform1iARB(uniformLocations[nextUniformIndex++],i0);
		}
	void uploadUniform(GLint i0,GLint i1)
		{
		glUniform2iARB(uniformLocations[nextUniformIndex++],i0,i1);
		}
	void uploadUniform(GLint i0,GLint i1,GLint i2)
		{
		glUniform3iARB(uniformLocations[nextUniformIndex++],i0,i1,i2);
		}
	void uploadUniform(GLint i0,GLint i1,GLint i2,GLint i3)
		{
		glUniform4iARB(uniformLocations[nextUniformIndex++],i0,i1,i2,i3);
		}
	void uploadUniform(GLfloat f0)
		{
		glUniform1fARB(uniformLocations[nextUniformIndex++],f0);
		}
	void uploadUniform(GLfloat f0,GLfloat f1)
		{
		glUniform2fARB(uniformLocations[nextUniformIndex++],f0,f1);
		}
	void uploadUniform(GLfloat f0,GLfloat f1,GLfloat f2)
		{
		glUniform3fARB(uniformLocations[nextUniformIndex++],f0,f1,f2);
		}
	void uploadUniform(GLfloat f0,GLfloat f1,GLfloat f2,GLfloat f3)
		{
		glUniform4fARB(uniformLocations[nextUniformIndex++],f0,f1,f2,f3);
		}
	void uploadUniform1v(GLsizei count,const GLint* components)
		{
		glUniform1ivARB(uniformLocations[nextUniformIndex++],count,components);
		}
	void uploadUniform2v(GLsizei count,const GLint* components)
		{
		glUniform2ivARB(uniformLocations[nextUniformIndex++],count,components);
		}
	void uploadUniform3v(GLsizei count,const GLint* components)
		{
		glUniform3ivARB(uniformLocations[nextUniformIndex++],count,components);
		}
	void uploadUniform4v(GLsizei count,const GLint* components)
		{
		glUniform4ivARB(uniformLocations[nextUniformIndex++],count,components);
		}
	void uploadUniform1v(GLsizei count,const GLfloat* components)
		{
		glUniform1fvARB(uniformLocations[nextUniformIndex++],count,components);
		}
	void uploadUniform2v(GLsizei count,const GLfloat* components)
		{
		glUniform2fvARB(uniformLocations[nextUniformIndex++],count,components);
		}
	void uploadUniform3v(GLsizei count,const GLfloat* components)
		{
		glUniform3fvARB(uniformLocations[nextUniformIndex++],count,components);
		}
	void uploadUniform4v(GLsizei count,const GLfloat* components)
		{
		glUniform4fvARB(uniformLocations[nextUniformIndex++],count,components);
		}
	void uploadUniformMatrix2(GLsizei count,GLboolean transpose,const GLfloat* value)
		{
		glUniformMatrix2fvARB(uniformLocations[nextUniformIndex++],count,transpose,value);
		}
	void uploadUniformMatrix3(GLsizei count,GLboolean transpose,const GLfloat* value)
		{
		glUniformMatrix3fvARB(uniformLocations[nextUniformIndex++],count,transpose,value);
		}
	void uploadUniformMatrix4(GLsizei count,GLboolean transpose,const GLfloat* value)
		{
		glUniformMatrix4fvARB(uniformLocations[nextUniformIndex++],count,transpose,value);
		}
	template <class TransformParam>
	void uploadUniform(const TransformParam& transform); // Uploads the given transformation into the uniform variable matrix of the given index
	};

#endif
