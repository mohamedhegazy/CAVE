#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
__constant int resultImgSizeW=?w?;
__constant int resultImgSizeH=?h?;
__constant int cu_size=?c?;//to avoid integer division  16->4 , 64->6
__constant int blocks_width=?bw?;//since it's constant
__constant int blocks_height=?bh?;//since it's constant
__constant unsigned long scale=?s?;
__kernel void Weighted_Depth(__global float * input, __global unsigned long * sumDepth, __global float * output)
{
    
    int pixelX=get_global_id(0); 
    int pixelY=get_global_id(1);    
	int pos = pixelX+pixelY*resultImgSizeW;
	int block_y = pixelY>>cu_size;	//to avoid integer division 
	int block_x = pixelX>>cu_size; //to avoid integer division 
	int pos_block = block_x+block_y*blocks_width;
	float myDepth = 1-input[pos];
	float out_val = myDepth*scale;
	atomic_add(&sumDepth[0],out_val);	
	output[pos] = myDepth;			
	    
}