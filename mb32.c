/*
* Cedarx framework.
* Copyright (c) 2008-2015 Allwinner Technology Co. Ltd.
* Copyright (c) 2014 BZ Chen <bzchen@allwinnertech.com>
*
* This file is part of Cedarx.
*
* Cedarx is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Lesser General Public License for more details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void mb32_untile_y(unsigned char *src, unsigned char *dst, unsigned int width, unsigned int height)
{
	int nMbWidth = 0;
	int nMbHeight = 0;
	int i = 0;
	int j = 0;
	int m = 0;
	int k = 0;
    int nLineStride=0;
    int lineNum = 0;
    int offset = 0;
    char* ptr = NULL;
    char *dstAsm = NULL;
    char *srcAsm = NULL;
    char bufferU[32];
    int nWidthMatchFlag = 0;
    int nCopyMbWidth = 0;

    nLineStride = (width + 15) &~15;
    nMbWidth = (width+31)&~31;
    nMbWidth /= 32;

    nMbHeight = (height+31)&~31;
    nMbHeight /= 32;
    ptr = src;

    nWidthMatchFlag = 0;
	nCopyMbWidth = nMbWidth-1;

    if(nMbWidth*32 == nLineStride)
    {
    	nWidthMatchFlag = 1;
    	nCopyMbWidth = nMbWidth;

    }
    for(i=0; i<nMbHeight; i++)
    {
    	for(j=0; j<nCopyMbWidth; j++)
    	{
    		for(m=0; m<32; m++)
    		{
    			if((i*32 + m) >= height)
    		  	{
    				ptr += 32;
    		    	continue;
    		  	}
    			srcAsm = ptr;
    			lineNum = i*32 + m;           //line num
    			offset =  lineNum*nLineStride + j*32;
    			dstAsm = dst + offset;

    			 asm volatile (
    					        "vld1.8         {d0 - d3}, [%[srcAsm]]              \n\t"
    					        "vst1.8         {d0 - d3}, [%[dstAsm]]              \n\t"
    					       	: [dstAsm] "+r" (dstAsm), [srcAsm] "+r" (srcAsm)
    					       	:  //[srcY] "r" (srcY)
    					       	: "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31");
    			ptr += 32;
    		}
    	}

    	if(nWidthMatchFlag == 1)
    	{
    		continue;
    	}
    	for(m=0; m<32; m++)
    	{
    		if((i*32 + m) >= height)
    		{
    			ptr += 32;
    	    	continue;
    	   	}
    		dstAsm = bufferU;
    		srcAsm = ptr;
    	 	lineNum = i*32 + m;           //line num
    		offset =  lineNum*nLineStride + j*32;

    	   	 asm volatile (
    	    	      "vld1.8         {d0 - d3}, [%[srcAsm]]              \n\t"
    	              "vst1.8         {d0 - d3}, [%[dstAsm]]              \n\t"
    	         	    : [dstAsm] "+r" (dstAsm), [srcAsm] "+r" (srcAsm)
    	    	     	:  //[srcY] "r" (srcY)
    	    	    	: "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31");
    	   	ptr += 32;
    	   	for(k=0; k<32; k++)
    	   	{
    	   		if((j*32+ k) >= nLineStride)
    	   	   	{
    	   			break;
    	   	  	}
    	   	 	dst[offset+k] = bufferU[k];
    	   	}
    	}
    }
}

void mb32_untile_uv(unsigned char *src, unsigned char *dst, unsigned int width, unsigned int height)
{
	int nMbWidth = 0;
	int nMbHeight = 0;
	int i = 0;
	int j = 0;
	int m = 0;
	int k = 0;
    int nLineStride=0;
    int lineNum = 0;
    int offset = 0;
    char* ptr = NULL;
    char *dst0Asm = NULL;
    char *dst1Asm = NULL;
    char *srcAsm = NULL;
    char bufferV[16], bufferU[16];
    int nWidth = 0;
    int nHeight = 0;

    nWidth = (width+1)/2;
    nHeight = (height+1)/2;

    nLineStride = (nWidth*2 + 15) &~15;
    nMbWidth = (nWidth*2+31)&~31;
    nMbWidth /= 32;

    nMbHeight = (nHeight+31)&~31;
    nMbHeight /= 32;


    ptr = src;

    for(i=0; i<nMbHeight; i++)
    {
    	for(j=0; j<nMbWidth; j++)
    	{
    		for(m=0; m<32; m++)
    		{
    			if((i*32 + m) >= nHeight)
    			{
    				ptr += 32;
    				continue;
        		}

    			dst0Asm = bufferU;
    			dst1Asm = bufferV;
    			srcAsm = ptr;
    			lineNum = i*32 + m;           //line num
    			offset =  lineNum*nLineStride + j*32;

    			asm volatile(
    					"vld2.8         {d0-d3}, [%[srcAsm]]              \n\t"
    			    	"vst1.8         {d0,d1}, [%[dst0Asm]]              \n\t"
    			    	"vst1.8         {d2,d3}, [%[dst1Asm]]              \n\t"
    			    	: [dst0Asm] "+r" (dst0Asm), [dst1Asm] "+r" (dst1Asm), [srcAsm] "+r" (srcAsm)
    			        :  //[srcY] "r" (srcY)
    			        : "cc", "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d28", "d29", "d30", "d31"
    			     );
    			ptr += 32;


    			for(k=0; k<16; k++)
    			{
    				if((j*32+ 2*k) >= nLineStride)
    				{
    					break;
    				}
    				dst[offset+2*k]   = bufferV[k];
    			   	dst[offset+2*k+1] = bufferU[k];
    			}
    		}
    	}
    }
}
