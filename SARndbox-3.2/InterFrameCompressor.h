/***********************************************************************
InterFrameCompressor - Class to compress the difference between two
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

#ifndef INTERFRAMECOMPRESSOR_INCLUDED
#define INTERFRAMECOMPRESSOR_INCLUDED

#include <IO/File.h>

#include "HuffmanEncoder.h"
#include "Pixel.h"

class InterFrameCompressor
	{
	/* Elements: */
	private:
	static const unsigned int codeMax=256U; // Maximum absolute Huffman-coded pixel value
	static const unsigned int outOfRange=2U*codeMax+1U; // The value indicating an out-of-range pixel value
	static const unsigned int maxZeroRunLength=512U; // Maximum length of a zero run
	IO::FilePtr file; // Pointer to the destination file
	HuffmanEncoder encoder; // The Huffman encoder object
	unsigned int zeroRunLength; // Length of the current zero run
	
	/* Private methods: */
	void finishZeroRun(void) // Finishes a non-zero length run of zeros
		{
		/* Encode the final zero run length: */
		encoder.encode(outOfRange+zeroRunLength);
		
		/* Reset the zero run length counter: */
		zeroRunLength=0U;
		}
	
	/* Constructors and destructors: */
	public:
	InterFrameCompressor(IO::File& sFile); // Creates an inter-frame compressor writing to the given file
	
	/* Methods: */
	void compressFrame(unsigned int width,unsigned int height,const Pixel* pixels0,const Pixel* pixels1); // Compresses the difference between the two given frames
	};

#endif
