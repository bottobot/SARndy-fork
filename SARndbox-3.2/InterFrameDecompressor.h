/***********************************************************************
InterFrameDecompressor - Class to decompress the difference between two
bathymetry or water level grids.
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

#ifndef INTERFRAMEDECOMPRESSOR_INCLUDED
#define INTERFRAMEDECOMPRESSOR_INCLUDED

#include <IO/File.h>

#include "HuffmanDecoder.h"
#include "Pixel.h"

class InterFrameDecompressor
	{
	/* Elements: */
	private:
	static const unsigned int codeMax=256U; // Maximum absolute Huffman-coded pixel value
	static const unsigned int outOfRange=2U*codeMax+1U; // The value indicating an out-of-range pixel value
	static const unsigned int maxZeroRunLength=512U; // Maximum length of a zero run
	IO::FilePtr file; // Pointer to the source file
	HuffmanDecoder decoder; // The Huffman decoder object
	
	/* Private methods: */
	Pixel decode(void) // Decodes a prediction error
		{
		/* Read the next code from the file: */
		unsigned int code=decoder.decode();
		
		/* Handle the code ranges: */
		if(code<outOfRange)
			return Pixel(code-codeMax);
		else
			{
			/* Read the out-of-range prediction error directly: */
			return Pixel(decoder.readBits(numPixelBits));
			}
		}
	
	/* Constructors and destructors: */
	public:
	InterFrameDecompressor(IO::File& sFile); // Creates an inter-frame decompressor reading from the given file
	
	/* Methods: */
	void decompressFrame(unsigned int width,unsigned int height,const Pixel* pixels0,Pixel* pixels1); // Decompresses frame differences relative to the first given pixel array into the second given pixel array
	};

#endif
