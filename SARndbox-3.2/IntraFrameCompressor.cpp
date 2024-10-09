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

#include "IntraFrameCompressor.h"

#include "HuffmanBuilder.h"

namespace {

/********************************************************
Huffman encoding codebook for the intra-frame compressor:
********************************************************/

static const HuffmanBuilder::Code intraFrameCompressorCodebook[]=
	{
	{0x1230b2aU,25},{0x1232587U,25},{0x12162dfU,25},{0x246a1baU,26},{0x12320c7U,25},{0x24641a8U,26},{0x12325beU,25},{0x91a86ecU,28},
	{0x12325de0U,29},{0x1216da09U,29},{0x1216002fU,29},{0x1216d92U,25},{0x1235de6U,25},{0x12325bfU,25},{0x485b6820U,31},{0x246a123U,26},
	{0x247ade8U,26},{0x246bbc2U,26},{0x91a86edU,28},{0x90b0016U,28},{0x91ad18U,24},{0x1216fb3U,25},{0x246b432U,26},{0x12162dcU,25},
	{0x919061U,24},{0x2461549U,26},{0x91aef2U,24},{0x1235387U,25},{0x1230aa6U,25},{0x1216f15U,25},{0x24641a9U,26},{0x48c900fU,27},
	{0x246b846U,26},{0x242daf4U,26},{0x242de29U,26},{0x1216d93U,25},{0x1232409U,25},{0x1235090U,25},{0x1232a5aU,25},{0x1232a5bU,25},
	{0x242de28U,26},{0x918555U,24},{0x91920dU,24},{0x1216d6dU,25},{0x90b16cU,24},{0x1216074U,25},{0x1230aa5U,25},{0x1235331U,25},
	{0x1235a18U,25},{0x91ad15U,24},{0x91a9ccU,24},{0x485b683U,27},{0x91ae17U,24},{0x242db5bU,26},{0x1230b25U,25},{0x12162c6U,25},
	{0x246b464U,26},{0x121606aU,25},{0x123018bU,25},{0x1232a66U,25},{0x1216dacU,25},{0x485b5b1U,27},{0x91953bU,24},{0x1235de0U,25},
	{0x2461548U,26},{0x1235de9U,25},{0x90b799U,24},{0x485b686U,27},{0x1232402U,25},{0x12350dcU,25},{0x2461649U,26},{0x90b6d1U,24},
	{0x918593U,24},{0x90b7d8U,24},{0x242db40U,26},{0x1216003U,25},{0x1216000U,25},{0x242c004U,26},{0x1216075U,25},{0x91eb7bU,24},
	{0x91951fU,24},{0x90b62eU,24},{0x48f5bcU,23},{0x48d77bU,23},{0x90b6cdU,24},{0x12162c7U,25},{0x48d68fU,23},{0x918594U,24},
	{0x9192ffU,24},{0x90b009U,24},{0x918550U,24},{0x48d762U,23},{0x91aef1U,24},{0x91eb76U,24},{0x918554U,24},{0x4858b0U,23},
	{0x91a86fU,24},{0x919068U,24},{0x90b798U,24},{0x48d68eU,23},{0x9192feU,24},{0x485b5dU,23},{0x48c2cbU,23},{0x918551U,24},
	{0x91a9c0U,24},{0x91a999U,24},{0x918082U,24},{0x1216d7bU,25},{0x9192deU,24},{0x48d70aU,23},{0x12162ddU,25},{0x48f5bfU,23},
	{0x919074U,24},{0x48d425U,23},{0x48d70cU,23},{0x919532U,24},{0x91a84cU,24},{0x90b169U,24},{0x90b16dU,24},{0x48c80fU,23},
	{0x919069U,24},{0x48f5b3U,23},{0x919015U,24},{0x91952fU,24},{0x48c976U,23},{0x48c96bU,23},{0x91951eU,24},{0x48c2abU,23},
	{0x91eb77U,24},{0x90b7dbU,24},{0x48d4e7U,23},{0x48c901U,23},{0x91ae10U,24},{0x48ca8cU,23},{0x48ca9aU,23},{0x90b008U,24},
	{0x90b62fU,24},{0x919062U,24},{0x48c836U,23},{0x90b6b5U,24},{0x48c2c8U,23},{0x48c907U,23},{0x919205U,24},{0x48d68dU,23},
	{0x90b629U,24},{0x48c83bU,23},{0x48c04eU,23},{0x48ca8eU,23},{0x485b69U,23},{0x485b5fU,23},{0x4858b3U,23},{0x48ca9cU,23},
	{0x90b037U,24},{0x48f5beU,23},{0x246a65U,22},{0x246482U,22},{0x48c97eU,23},{0x2464b4U,22},{0x247ad8U,22},{0x48c833U,23},
	{0x48c063U,23},{0x485b6aU,23},{0x48ca90U,23},{0x246544U,22},{0x24654aU,22},{0x485bc4U,23},{0x48c04dU,23},{0x246a71U,22},
	{0x485b16U,23},{0x485bcdU,23},{0x48c960U,23},{0x485805U,23},{0x242c0cU,22},{0x246a64U,22},{0x246549U,22},{0x246a72U,22},
	{0x242df0U,22},{0x246030U,22},{0x246b40U,22},{0x242de3U,22},{0x246b87U,22},{0x1235d9U,21},{0x123259U,21},{0x247adcU,22},
	{0x242c03U,22},{0x2464baU,22},{0x242de1U,22},{0x242dacU,22},{0x12350cU,21},{0x121623U,21},{0x242c0fU,22},{0x123201U,21},
	{0x12320fU,21},{0x123019U,21},{0x91a9dU,20},{0x12325cU,21},{0x1216d8U,21},{0x12301dU,21},{0x1216f2U,21},{0x123011U,21},
	{0x1216c4U,21},{0x91ae2U,20},{0x91a9eU,20},{0x91aeeU,20},{0x91ae3U,20},{0x91a87U,20},{0x91921U,20},{0x91a98U,20},
	{0x9180fU,20},{0x9180dU,20},{0x48d75U,19},{0x90b6aU,20},{0x48d74U,19},{0x90b01U,20},{0x90b02U,20},{0x48c91U,19},
	{0x48c82U,19},{0x485bfU,19},{0x48c2bU,19},{0x485bdU,19},{0x48589U,19},{0x247acU,18},{0x4858aU,19},{0x246b9U,18},
	{0x246a3U,18},{0x246a0U,18},{0x24649U,18},{0x2464aU,18},{0x24614U,18},{0x242c7U,18},{0x1235bU,17},{0x12352U,17},
	{0x1232bU,17},{0x12327U,17},{0x12300U,17},{0x12161U,17},{0x91eaU,16},{0x91acU,16},{0x9194U,16},{0x9181U,16},
	{0x90b2U,16},{0x48f4U,15},{0x48cbU,15},{0x48c1U,15},{0x485aU,15},{0x2466U,14},{0x242eU,14},{0x1234U,13},
	{0x91fU,12},{0x90aU,12},{0x484U,11},{0x122U,9},{0x49U,7},{0x13U,5},{0x5U,3},{0x1U,2},
	{0x0U,2},{0x3U,2},{0x8U,4},{0x25U,6},{0x120U,9},{0x243U,10},{0x48eU,11},{0x91bU,12},
	{0x1231U,13},{0x123cU,13},{0x242fU,14},{0x2467U,14},{0x247bU,14},{0x48c3U,15},{0x48d5U,15},{0x90b3U,16},
	{0x9184U,16},{0x9191U,16},{0x91afU,16},{0x1216eU,17},{0x12321U,17},{0x12326U,17},{0x242c1U,18},{0x123d7U,17},
	{0x242d9U,18},{0x24617U,18},{0x246a2U,18},{0x24655U,18},{0x246b5U,18},{0x4858cU,19},{0x4858dU,19},{0x485b7U,19},
	{0x48c81U,19},{0x485b4U,19},{0x48c05U,19},{0x48c2dU,19},{0x48d4dU,19},{0x48d69U,19},{0x90b10U,20},{0x90b63U,20},
	{0x90b17U,20},{0x91854U,20},{0x91858U,20},{0x91a9fU,20},{0x91a85U,20},{0x91ae0U,20},{0x91eb5U,20},{0x1216dbU,21},
	{0x91eb4U,20},{0x1216faU,21},{0x1230abU,21},{0x123d6dU,21},{0x123200U,21},{0x123012U,21},{0x121622U,21},{0x242c01U,22},
	{0x1230b3U,21},{0x1235dfU,21},{0x123508U,21},{0x242df7U,22},{0x2464b1U,22},{0x1235daU,21},{0x242de0U,22},{0x12325eU,21},
	{0x1232a0U,21},{0x1235dbU,21},{0x48581cU,23},{0x246bb0U,22},{0x242df2U,22},{0x246a67U,22},{0x1232a1U,21},{0x242df1U,22},
	{0x4858b5U,23},{0x246406U,22},{0x246038U,22},{0x4858b2U,23},{0x246039U,22},{0x242df3U,22},{0x485b15U,23},{0x246404U,22},
	{0x24641cU,22},{0x24654fU,22},{0x485b67U,23},{0x48c832U,23},{0x2464beU,22},{0x485801U,23},{0x485b5cU,23},{0x48d687U,23},
	{0x48c04fU,23},{0x485b65U,23},{0x48d682U,23},{0x48ca9bU,23},{0x90b162U,24},{0x48ca98U,23},{0x90b6c8U,24},{0x90b6bcU,24},
	{0x48c80bU,23},{0x246b44U,22},{0x48c903U,23},{0x48d68bU,23},{0x9192c2U,24},{0x485bceU,23},{0x91ae16U,24},{0x91951bU,24},
	{0x48c040U,23},{0x91a9cdU,24},{0x9192eeU,24},{0x90b6d7U,24},{0x48c043U,23},{0x48d685U,23},{0x9192daU,24},{0x9192dbU,24},
	{0x9180c4U,24},{0x91ae1bU,24},{0x90b034U,24},{0x90b78bU,24},{0x91aef5U,24},{0x90b6b7U,24},{0x9192d9U,24},{0x919200U,24},
	{0x1216fb2U,25},{0x91a99bU,24},{0x48ca91U,23},{0x48c80eU,23},{0x90b036U,24},{0x48f5baU,23},{0x12320c6U,25},{0x1230131U,25},
	{0x1216d99U,25},{0x123018aU,25},{0x91a9c2U,24},{0x123d6c8U,25},{0x9192d4U,24},{0x48d434U,23},{0x123d6c9U,25},{0x919517U,24},
	{0x90b168U,24},{0x91920cU,24},{0x90b628U,24},{0x48c837U,23},{0x91a86aU,24},{0x48d683U,23},{0x91a99aU,24},{0x91952eU,24},
	{0x90b6b4U,24},{0x48d684U,23},{0x91a9c1U,24},{0x242db42U,26},{0x1232a75U,25},{0x485800aU,27},{0x242c5bdU,26},{0x1216d98U,25},
	{0x48d709U,23},{0x90b79fU,24},{0x1216f3cU,25},{0x123d6cbU,25},{0x90b7daU,24},{0x919060U,24},{0x12325b1U,25},{0x485b687U,27},
	{0x1232a59U,25},{0x1232a2cU,25},{0x246bbd0U,26},{0x123509bU,25},{0x91a84eU,24},{0x48c96eU,23},{0x1216001U,25},{0x4858b78U,27},
	{0x1235330U,25},{0x2461656U,26},{0x242dad9U,26},{0x918083U,24},{0x91a84fU,24},{0x919075U,24},{0x123010aU,25},{0x48d763U,23},
	{0x91a86cU,24},{0x1232a74U,25},{0x91a849U,24},{0x1230109U,25},{0x247ad95U,26},{0x12325b0U,25},{0x1235c34U,25},{0x90b03bU,24},
	{0x12325dfU,25},{0x247ade9U,26},{0x246a70dU,26},{0x1230130U,25},{0x1232a58U,25},{0x919515U,24},{0x2464806U,26},{0x485b5b0U,27},
	{0x246a70cU,26},{0x2464b0dU,26},{0x91ad14U,24},{0x123509aU,25},{0x919014U,24},{0x247ad94U,26},{0x91a86dU,24},{0x12325abU,25},
	{0x12350d6U,25},{0x1232a67U,25},{0x242de7aU,26},{0x48d7787U,27},{0x1232408U,25},{0x90b79ecU,28},{0x246b465U,26},{0x123010bU,25},
	{0x48c2c91U,27},{0x1235a33U,25},{0x2464bbdU,26},{0x1235de7U,25},{0x1232a35U,25},{0x1232a34U,25},{0x48c2caeU,27},{0x123d6f5U,25},
	{0x1230132U,25},{0x2460266U,26},{0x2460267U,26},{0x121606bU,25},{0x48c900eU,27},{0x12325aaU,25},{0x1232a29U,25},{0x246b845U,26},
	{0x48d7786U,27},{0x2464b0cU,26},{0x246a1afU,26},{0x48c9779U,27},{0x48c2cafU,27},{0x246b844U,26},{0x91906bU,24},{0x246a122U,26},
	{0x90b6d05U,28},{0x1235c35U,25},{0x48c2c90U,27},{0x1230108U,25},{0x12320d5U,25},{0x485bcf7U,27},{0x48d6866U,27},{0x1230aa7U,25},
	{0x48d4377U,27},{0x91ad0dU,24},{0x246b847U,26},{0x12325de1U,29},{0x4858b79U,27},{0x48d6867U,27},{0x246bbd1U,26},{0x242db411U,30},
	{0x90b79edU,28},{0x242db5aU,26},{0x1216002eU,29},{0x246a1aeU,26},{0x485b6821U,31},{0x1232a2dU,25},{0x242daf5U,26},{0x1232a28U,25},
	{0x9192ef1U,28},{0x485b0U,19}
	};

}

/*************************************
Methods of class IntraFrameCompressor:
*************************************/

IntraFrameCompressor::IntraFrameCompressor(IO::File& sFile)
	:file(&sFile),
	 encoder(sFile,intraFrameCompressorCodebook)
	{
	}

namespace {

inline Pixel predictPaeth(Pixel a,Pixel b,Pixel c) // Predicts a pixel value based on three neighbors using Alan W. Paeth's PNG filter
	{
	/* Calculate the predictor coefficient: */
	int p=int(a)+int(b)-int(c);
	
	/* Return the neighbor value that is closest to the predictor coefficient: */
	Pixel result=a;
	int d=abs(p-int(a));
	
	int db=abs(p-int(b));
	if(d>db)
		{
		result=b;
		d=db;
		}
	
	int dc=abs(p-int(c));
	if(d>dc)
		{
		result=c;
		d=dc;
		}
	
	return result;
	}

}

void IntraFrameCompressor::compressFrame(unsigned int width,unsigned int height,const Pixel* pixels)
	{
	const Pixel* pPtr=pixels;
	ptrdiff_t stride(width);
	
	Pixel pred;
	
	/* Compress the first grid row: */
	const Pixel* rowEnd=pPtr+stride;
	
	/* Write the first pixel as-is: */
	encoder.writeBits(*pPtr,numPixelBits);
	
	/* Write the rest of the first row's pixels: */
	for(++pPtr;pPtr!=rowEnd;++pPtr)
		{
		/* Predict the current grid value: */
		pred=pPtr[-1];
		
		/* Encode the prediction error: */
		encode(*pPtr-pred);
		}
	
	/* Compress the remaining rows: */
	for(unsigned int y=1;y<height;y+=2)
		{
		/* Process the odd row right-to-left: */
		pPtr+=stride-1;
		rowEnd=pPtr-stride;
		
		/* Predict the current grid value: */
		pred=pPtr[-stride];
		
		/* Encode the prediction error: */
		encode(*pPtr-pred);
		
		/* Process the row's remaining pixels: */
		for(--pPtr;pPtr!=rowEnd;--pPtr)
			{
			/* Predict the current grid value: */
			pred=predictPaeth(pPtr[1],pPtr[-stride],pPtr[-stride+1]);
			
			/* Encode the prediction error: */
			encode(*pPtr-pred);
			}
		
		/* Bail out early if the grid's height is even: */
		if(y+1>=height)
			break;
		
		/* Process the even row left-to-right: */
		pPtr+=stride+1;
		rowEnd=pPtr+stride;
		
		/* Predict the current grid value: */
		pred=pPtr[-stride];
		
		/* Encode the prediction error: */
		encode(*pPtr-pred);
		
		/* Process the row's remaining pixels: */
		for(++pPtr;pPtr!=rowEnd;++pPtr)
			{
			/* Predict the current grid value: */
			pred=predictPaeth(pPtr[-1],pPtr[-stride],pPtr[-stride-1]);
			
			/* Encode the prediction error: */
			encode(*pPtr-pred);
			}
		}
	
	/* Flush the encoder: */
	encoder.flush();
	}
