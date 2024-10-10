/***********************************************************************
IntraFrameCompressor - Class to compress a single bathymetry or water
level grid.
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

#ifndef INTRAFRAMECOMPRESSOR_INCLUDED
#define INTRAFRAMECOMPRESSOR_INCLUDED

#include <IO/File.h>

#include "HuffmanEncoder.h"
#include "Pixel.h"

class IntraFrameCompressor
	{
	/* Elements: */
	private:
	static const unsigned int codeMax=256U; // Maximum absolute Huffman-coded pixel value
	static const unsigned int outOfRange=2U*codeMax+1U; // The value indicating an out-of-range pixel value
	IO::FilePtr file; // Pointer to the destination file
	HuffmanEncoder encoder; // The Huffman encoder object
	
	/* Private methods: */
	void encode(Pixel predictionError) // Encodes the given prediction error
		{
		if(predictionError>=65536U-codeMax) // Negative in-range prediction error
			encoder.encode(predictionError-(65536U-codeMax));
		else if(predictionError<=codeMax) // Positive in-range prediction error
			encoder.encode(predictionError+codeMax);
		else // Out-of-range prediction error
			{
			/* Write the out-of-range marker followed by the out-of-range value as-is: */
			encoder.encode(outOfRange);
			encoder.writeBits(predictionError,numPixelBits);
			}
		}
	
	/* Constructors and destructors: */
	public:
	IntraFrameCompressor(IO::File& sFile); // Creates an intra-frame compressor writing to the given file
	
	/* Methods: */
	void compressFrame(unsigned int width,unsigned int height,const Pixel* pixels); // Compresses the given frame
	};

#endif
