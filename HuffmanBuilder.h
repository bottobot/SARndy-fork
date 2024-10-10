/***********************************************************************
HuffmanBuilder - Helper class to construct a Huffman encoding codebook
and decoding tree from a list of codes with frequencies.
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

#ifndef HUFFMANBUILDER_INCLUDED
#define HUFFMANBUILDER_INCLUDED

#include <stddef.h>
#include <vector>
#include <Misc/SizedTypes.h>

#include "Bits.h"

class HuffmanBuilder
	{
	/* Embedded classes: */
	public:
	typedef unsigned int Index; // Type for node indices
	
	struct CodeNode; // Structure for nodes in the Huffman code creation tree
	
	struct Code // Structure for encoding codebook entries
		{
		/* Elements: */
		public:
		Bits bits; // The code's bits, aligned with the LSB
		unsigned int numBits; // The number of bits in the code
		
		/* Constructors and destructors: */
		Code(void) // Creates an empty code
			:bits(0x0U),numBits(0)
			{
			}
		Code(Bits sBits,unsigned int sNumBits) // Elementwise constructor
			:bits(sBits),numBits(sNumBits)
			{
			}
		};
	
	struct Node // Structure for decoding tree nodes
		{
		/* Elements: */
		public:
		unsigned int code; // Code represented by a leaf node, or ~0x0U for interior nodes
		Index childIndices[2]; // Indices of the node's children for interior nodes, or (0, 0) for leaf nodes
		
		/* Constructors and destructors: */
		Node(void) // Dummy constructor
			{
			}
		Node(unsigned int sCode,Index childIndex0,Index childIndex1) // Elementwise constructor
			:code(sCode)
			{
			childIndices[0]=childIndex0;
			childIndices[1]=childIndex1;
			}
		};
	
	/* Elements: */
	std::vector<CodeNode> codeNodes; // The Huffman code creation tree
	Index numLeaves; // Number of leaf nodes in the code creation tree
	
	/* Private methods: */
	void orderTree(Index nodeIndex,Index& nextIndex,Node* tree) const; // Recursively traverses the code creation tree in prefix order and adds nodes into the given tree array
	
	/* Constructors and destructors: */
	public:
	HuffmanBuilder(void); // Creates a Huffman builder with an empty code list
	~HuffmanBuilder(void);
	
	/* Methods: */
	Index addLeaf(size_t frequency); // Adds a leaf node with the given frequency to the code creation tree; returns the new leaf node's index
	Index getNumLeaves(void) const // Returns the number of leaf nodes in the code creation tree
		{
		return numLeaves;
		}
	void buildTree(void); // Builds the Huffman code creation tree
	Code* buildEncodingCodebook(void) const; // Returns a new-allocated array of encoding codebook entries
	Node* buildDecodingTree(void) const; // Returns a new-allocated array of decoding tree nodes, with the root node at index 0
	};

#endif
