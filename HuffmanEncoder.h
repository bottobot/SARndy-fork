/***********************************************************************
HuffmanEncoder - Class to encode a stream of values using Huffman's
method.
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

#ifndef HUFFMANENCODER_INCLUDED
#define HUFFMANENCODER_INCLUDED

#include "BitSink.h"
#include "HuffmanBuilder.h"

class HuffmanEncoder
	{
	/* Elements: */
	private:
	BitSink bitSink; // Bit sink to write Huffman-encoded values to a file
	HuffmanBuilder::Code* codebookAlloc; // Optional object-allocated memory backing the encoding codebook
	const HuffmanBuilder::Code* codebook; // The Huffman encoding codebook
	
	/* Constructors and destructors: */
	public:
	HuffmanEncoder(IO::File& file,HuffmanBuilder& huffmanBuilder) // Creates a Huffman encoder for the given destination file and Huffman code builder
		:bitSink(file),
		 codebookAlloc(huffmanBuilder.buildEncodingCodebook()),codebook(codebookAlloc)
		{
		}
	HuffmanEncoder(IO::File& file,const HuffmanBuilder::Code* sCodebook) // Creates a Huffman encoder for the given destination file and Huffman encoding codebook
		:bitSink(file),
		 codebookAlloc(0),codebook(sCodebook)
		{
		}
	~HuffmanEncoder(void)
		{
		/* Release allocated resources: */
		delete[] codebookAlloc;
		}
	
	/* Methods: */
	void writeBits(Bits bits,unsigned int numBits) // Directly writes the given bits to the bit sink
		{
		/* Bypass the Huffman encoder: */
		bitSink.write(bits,numBits);
		}
	void encode(unsigned int value) // Huffman-encodes the given value and writes the result to the file
		{
		/* Look up the value's code in the codebook and write it to the bit sink: */
		bitSink.write(codebook[value].bits,codebook[value].numBits);
		}
	void flush(void) // Flushes the encoder
		{
		/* Flush the bit sink: */
		bitSink.flush();
		}
	};

#endif
