/***********************************************************************
HuffmanBuilder - Helper class to construct a Huffman encoding codebook
and decoding tree from a list of codes with frequencies.
Copyright (c) 2022-2024 Oliver Kreylos

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

#include "HuffmanBuilder.h"

#include <Misc/StdError.h>
#include <Misc/PriorityHeap.h>

/**********************************************
Declaration of struct HuffmanBuilder::CodeNode:
**********************************************/

struct HuffmanBuilder::CodeNode
	{
	/* Elements: */
	public:
	Index parent; // Index of the node's parent node, or 0 for the root
	Index parentChildIndex; // Whether the node is the parent's first or second child; 2 for nodes that don't have a parent (yet)
	Index childIndices[2]; // Indices of an interior node's children, or (0, 0) for leaf nodes
	size_t frequency; // Total number of occurrences of the node's descendants
	
	/* Constructors and destructors: */
	CodeNode(size_t sFrequency) // Creates a leaf node
		:parent(0),parentChildIndex(2U),
		 frequency(sFrequency)
		{
		childIndices[1]=childIndices[0]=0U;
		}
	CodeNode(Index left,Index right,size_t sFrequency) // Creates an interior node with the given left and right child node indices
		:parent(0),parentChildIndex(2U),
		 frequency(sFrequency)
		{
		childIndices[0]=left;
		childIndices[1]=right;
		}
	};

/*******************************
Methods of class HuffmanBuilder:
*******************************/

void HuffmanBuilder::orderTree(HuffmanBuilder::Index nodeIndex,HuffmanBuilder::Index& nextIndex,HuffmanBuilder::Node* tree) const
	{
	/* Check whether the current node is an interior node: */
	if(nodeIndex>=numLeaves)
		{
		Index parentIndex=nextIndex;
		
		/* Add the current node to the result tree: */
		tree[parentIndex].code=~0x0U;
		++nextIndex;
		
		/* Add the current node's left and right subtrees' nodes: */
		for(int i=0;i<2;++i)
			{
			tree[parentIndex].childIndices[i]=nextIndex;
			orderTree(codeNodes[nodeIndex].childIndices[i],nextIndex,tree);
			}
		}
	else
		{
		/* Add a leaf node to the result tree: */
		tree[nextIndex].code=nodeIndex;
		tree[nextIndex].childIndices[1]=tree[nextIndex].childIndices[0]=0U;
		++nextIndex;
		}
	}

HuffmanBuilder::HuffmanBuilder(void)
	:numLeaves(0)
	{
	}

HuffmanBuilder::~HuffmanBuilder(void)
	{
	}

HuffmanBuilder::Index HuffmanBuilder::addLeaf(size_t frequency)
	{
	Index result=numLeaves;
	
	/* Add a leaf node to the code creation tree: */
	codeNodes.push_back(CodeNode(frequency));
	++numLeaves;
	
	return result;
	}

void HuffmanBuilder::buildTree(void)
	{
	/* Embedded classes: */
	struct NodeSorter // Helper class to sort nodes by frequency
		{
		/* Elements: */
		public:
		Index index;
		size_t frequency;
		
		/* Constructors and destructors: */
		NodeSorter(Index sIndex,size_t sFrequency)
			:index(sIndex),frequency(sFrequency)
			{
			}
		
		/* Methods: */
		bool operator<=(const NodeSorter& other) const
			{
			return frequency<=other.frequency;
			}
		};
	
	/* Create a priority heap of code nodes: */
	Misc::PriorityHeap<NodeSorter> nodeSorter(numLeaves);
	for(Index i=0;i<numLeaves;++i)
		nodeSorter.insert(NodeSorter(i,codeNodes[i].frequency));
	
	/* Combine nodes until there is only the root node left: */
	while(nodeSorter.getNumElements()>=2)
		{
		/* Pull the two nodes with the lowest frequencies from the heap: */
		NodeSorter ns1=nodeSorter.getSmallest(); // ns1 is the less frequent of the two
		nodeSorter.removeSmallest();
		NodeSorter ns0=nodeSorter.getSmallest(); // ns0 is the more frequent of the two
		nodeSorter.removeSmallest();
		
		/* Merge the two lowest-frequency nodes under a new parent node: */
		Index parentIndex=codeNodes.size();
		codeNodes[ns0.index].parent=parentIndex;
		codeNodes[ns0.index].parentChildIndex=0;
		codeNodes[ns1.index].parent=parentIndex;
		codeNodes[ns1.index].parentChildIndex=1;
		CodeNode parent(ns0.index,ns1.index,ns0.frequency+ns1.frequency);
		codeNodes.push_back(parent);
		
		/* Insert the merged node back into the node sorter: */
		nodeSorter.insert(NodeSorter(parentIndex,parent.frequency));
		}
	}

HuffmanBuilder::Code* HuffmanBuilder::buildEncodingCodebook(void) const
	{
	/* Create the codebook array: */
	Code* result=new Code[numLeaves];
	
	/* Calculate Huffman codes for all leaf nodes: */
	for(Index i=0;i<numLeaves;++i)
		{
		/* Calculate the Huffman code for this leaf node: */
		Code code;
		
		/* Follow the path from the leaf node to the code creation tree's root and assemble the code LSB-to-MSB: */
		Index nodeIndex=i;
		Bits mask(0x1U);
		while(codeNodes[nodeIndex].parentChildIndex<2U) // 2 is code for a node without a parent
			{
			/* Add the node's child index to the Huffman code: */
			if(codeNodes[nodeIndex].parentChildIndex!=0)
				code.bits|=mask;
			++code.numBits;
			mask<<=1;
			
			/* Go to the node's parent: */
			nodeIndex=codeNodes[nodeIndex].parent;
			}
		
		/* Ensure that the Huffman code fits into the Bits type: */
		if(code.numBits>maxNumBits)
			{
			delete[] result;
			throw Misc::makeStdErr(__PRETTY_FUNCTION__,"Huffman code requiring %u bits, more than the supported maximum of %u bits",code.numBits,maxNumBits);
			}
		
		/* Store the final Huffman code: */
		result[i]=code;
		}
	
	return result;
	}

HuffmanBuilder::Node* HuffmanBuilder::buildDecodingTree(void) const
	{
	/* Create the decoding tree node array: */
	Node* result=new Node[codeNodes.size()];
	
	/* Add nodes to the decoding tree starting from the root, in prefix order to improve locality during decoding: */
	Index nextIndex(0);
	orderTree(codeNodes.size()-1,nextIndex,result);
	
	return result;
	}
