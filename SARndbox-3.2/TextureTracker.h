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

#ifndef TEXTURETRACKER_INCLUDED
#define TEXTURETRACKER_INCLUDED

#include <GL/gl.h>

class TextureTracker
	{
	/* Embedded classes: */
	private:
	struct Binding // Structure representing a texture binding to a texture unit
		{
		/* Elements: */
		public:
		GLenum target; // Bound texture target; undefined if no texture is bound
		GLuint texture; // ID of bound texture object; 0 if no texture is bound
		};
	
	/* Elements: */
	GLint numUnits; // Number of available texture units
	Binding* bindings; // Array of bindings for all available texture units
	GLint numActiveUnits; // Number of texture units that have textures bound to them
	GLint nextUnit; // Next texture unit to be used for active bindings
	
	/* Constructors and destructors: */
	public:
	static void initExtensions(void); // Initializes OpenGL extensions required by this class
	TextureTracker(void); // Creates an empty texture tracker
	~TextureTracker(void); // Unbinds all bound textures and destroys the texture tracker
	
	/* Methods: */
	GLint getNumUnits(void) const // Returns the number of available texture units
		{
		return numUnits;
		}
	void reset(void); // Resets the texture tracker so that the first texture unit will be used next; does not unbind currently bound textures
	GLint bindTexture(GLenum target,GLuint texture); // Binds the texture object of the given ID to the given texture target on the next available texture unit and activates that unit; returns index of texture unit to which the texture was bound; throws exception if no more texture units available
	};

#endif
