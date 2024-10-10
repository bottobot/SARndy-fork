/***********************************************************************
BitSink - Class to write a stream of bits to a file.
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

#ifndef BITSINK_INCLUDED
#define BITSINK_INCLUDED

#include <IO/File.h>

#include "Bits.h"

class BitSink
	{
	/* Elements: */
	private:
	IO::FilePtr file; // File to which to write code stream
	Bits buffer; // The bit buffer
	unsigned int freeBits; // Number of currently unused bits in the buffer
	
	/* Constructors and destructors: */
	public:
	BitSink(IO::File& sFile)
		:file(&sFile),
		 buffer(0x0U),freeBits(maxNumBits)
		{
		}
	~BitSink(void)
		{
		/* Flush the buffer: */
		flush();
		}
	
	/* Methods: */
	void flush(void) // Empties the current bit buffer
		{
		/* Check whether the buffer has bits in it, which it always will except immediately after creation: */
		if(freeBits!=maxNumBits)
			{
			/* Shift the buffer's current contents to the MSB: */
			buffer<<=freeBits;
			
			/* Write the new buffer contents to the file: */
			file->write(buffer);
			
			/* Clear the buffer: */
			buffer=Bits(0x0U);
			freeBits=maxNumBits;
			}
		}
	void write(Bits bits,unsigned int numBits) // Writes a number of bits, starting from LSB in the given value, to the buffer; assumes numBits<=bufferSize
		{
		/* Check whether the given bits fit into the buffer: */
		if(freeBits>=numBits)
			{
			/* Append the given bits to the buffer: */
			buffer<<=numBits;
			buffer|=bits;
			freeBits-=numBits;
			}
		else if(freeBits==0U)
			{
			/* Write the previous buffer contents to the file: */
			file->write(buffer);
			
			/* Restart the buffer with the given bits: */
			buffer=bits;
			freeBits=maxNumBits-numBits;
			}
		else
			{
			/* Append the MSB part of the given bits to the buffer: */
			buffer<<=freeBits;
			unsigned int lsb=numBits-freeBits;
			buffer|=bits>>lsb;
			
			/* Write the new buffer contents to the file: */
			file->write(buffer);
			
			/* Restart the buffer with the LSB part of the given bits: */
			buffer=bits&((Bits(0x1U)<<lsb)-Bits(1U)); // Technically it's not necessary to null out the MSB bits, they'll be shifted out before the buffer is written anyway
			freeBits=maxNumBits-lsb;
			}
		}
	};

#endif
