/***********************************************************************
HuffmanDecoder - Class to decode a stream of values using Huffman's
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

#ifndef HUFFMANDECODER_INCLUDED
#define HUFFMANDECODER_INCLUDED

#include "BitSource.h"
#include "HuffmanBuilder.h"

#if 0
#include <iostream>
#include <iomanip>
#endif

class HuffmanDecoder
	{
	/* Elements: */
	private:
	BitSource bitSource; // Bit source to read Huffman-encoded values from a file
	HuffmanBuilder::Node* treeAlloc; // Optional object-allocated memory backing the decoding tree
	const HuffmanBuilder::Node* tree; // The Huffman decoding tree
	
	/* Constructors and destructors: */
	public:
	HuffmanDecoder(IO::File& file,const HuffmanBuilder& huffmanBuilder) // Creates a Huffman decoder for the given source file and Huffman code builder
		:bitSource(file),
		 treeAlloc(huffmanBuilder.buildDecodingTree()),tree(treeAlloc)
		{
		#if 0
		/* Print the decoding tree: */
		std::cout<<"static const HuffmanBuilder::Node tree[]="<<std::endl;
		std::cout<<"\t{";
		for(HuffmanBuilder::Index i=0;i<2U*huffmanBuilder.getNumLeaves()-1U;++i)
			{
			if(i%8==0U)
				std::cout<<std::endl<<'\t';
			std::cout<<'{';
			if(tree[i].code==~0x0U)
				std::cout<<"~0U";
			else
				std::cout<<tree[i].code<<'U';
			std::cout<<','<<tree[i].childIndices[0]<<"U,"<<tree[i].childIndices[1]<<"U},";
			}
		std::cout<<std::endl<<"\t}"<<std::endl<<std::endl;
		#endif
		}
	HuffmanDecoder(IO::File& file,const HuffmanBuilder::Node* sTree) // Creates a Huffman decoder for the given source file and Huffman decoding tree
		:bitSource(file),
		 treeAlloc(0),tree(sTree)
		{
		}
	~HuffmanDecoder(void)
		{
		/* Release allocated resources: */
		delete[] treeAlloc;
		}
	
	/* Methods: */
	Bits readBits(unsigned int numBits) // Directly reads the given number of bits from the bit source
		{
		/* Bypass the Huffman decoder: */
		return bitSource.read(numBits);
		}
	unsigned int decode(void) // Returns a Huffman-decoded value from the source file
		{
		/* Traverse the decoding tree from the root to a leaf using bits from the bit source: */
		HuffmanBuilder::Index nodeIndex=0;
		do
			{
			/* Read a bit from the bit source and traverse to the appropriate child node: */
			nodeIndex=tree[nodeIndex].childIndices[bitSource.readBit()];
			}
		while(tree[nodeIndex].code==~0x0U);
		
		/* Return the leaf node's code value: */
		return tree[nodeIndex].code;
		}
	void flush(void) // Flushes the decoder
		{
		/* Flush the bit source: */
		bitSource.flush();
		}
	};

#endif
