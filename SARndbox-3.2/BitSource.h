/***********************************************************************
BitSource - Class to read a stream of bits from a file.
Copyright (c) 2022-2023 Oliver Kreylos

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

#ifndef BITSOURCE_INCLUDED
#define BITSOURCE_INCLUDED

#include <IO/File.h>

#include "Bits.h"

class BitSource // Class to read variable-size codes from a file
	{
	/* Elements: */
	private:
	static const Bits msbMask=Bits(0x1U)<<(maxNumBits-1U); // Mask to extract the MSB from the buffer
	IO::FilePtr file; // File from which to read code stream
	Bits buffer; // The bit buffer
	unsigned int usedBits; // Number of currently used bits in the buffer
	
	/* Constructors and destructors: */
	public:
	BitSource(IO::File& sFile)
		:file(&sFile),
		 buffer(0x0U),usedBits(0U)
		{
		}
	~BitSource(void)
		{
		}
	
	/* Methods: */
	void flush(void) // Empties the current bit buffer
		{
		/* Clear the buffer: */
		buffer=Bits(0x0U);
		usedBits=0U;
		}
	Bits read(unsigned int numBits) // Reads a number of bits and returns them in the LSB part of the result; assumes numBits<=bufferSize
		{
		Bits result;
		if(usedBits>=numBits)
			{
			/* Copy the buffer into the result: */
			result=buffer;
			
			/* Shift the extracted bits out of the buffer: */
			buffer<<=numBits;
			usedBits-=numBits;
			}
		else if(usedBits==0U)
			{
			/* Read new buffer contents from the file: */
			file->read(buffer);
			
			/* Copy the buffer into the result: */
			result=buffer;
			
			/* Shift the extracted bits out of the buffer: */
			buffer<<=numBits;
			usedBits=maxNumBits-numBits;
			}
		else
			{
			/* Copy the buffer's remaining bits into the result: */
			result=buffer;
			
			/* Read new buffer contents from the file: */
			file->read(buffer);
			
			/* Append the new buffer's MSB portion to the result: */
			result|=buffer>>usedBits;
			
			/* Shift the extracted bits out of the buffer: */
			unsigned int lsb=numBits-usedBits;
			buffer<<=lsb;
			usedBits=maxNumBits-lsb;
			}
		
		/* Shift the result to the LSB: */
		result>>=maxNumBits-numBits;
		
		return result;
		}
	Bits readBit(void) // Reads a single bit and returns it in the LSB of the result
		{
		/* Fill the buffer if it is currently empty: */
		if(usedBits==0U)
			{
			/* Read new buffer contents from the file: */
			file->read(buffer);
			
			usedBits=maxNumBits;
			}
		
		/* Return the buffer's MSB: */
		Bits result=buffer>>(maxNumBits-1U);
		
		/* Shift the MSB out of the buffer: */
		buffer<<=1;
		--usedBits;
		
		return result;
		}
	};

#endif
