#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
__constant int resultImgSizeW=?w?;
__constant int resultImgSizeH=?h?;
__constant int cu_size=?c?;//to avoid integer division  16->4 , 64->6
__constant int blocks_width=?bw?;//since it's constant
__constant int blocks_height=?bh?;//since it's constant
__constant unsigned long scale=?s?;
__kernel void Weighted_Norm(__global float * inputDepth, __global unsigned long * sumDepth,__global float * inputDistance, __global unsigned long * sumDistance, __global unsigned long * saliency)
{
    
    int pixelX=get_global_id(0); 
    int pixelY=get_global_id(1);    
	int pos = pixelX+pixelY*resultImgSizeW;
	int block_y = pixelY>>cu_size;	//to avoid integer division 
	int block_x = pixelX>>cu_size; //to avoid integer division 
	int pos_block = block_x+block_y*blocks_width;
	
	float myDepth = inputDepth[pos];
	float sumDepthf = sumDepth[0]/scale;
	sumDepthf=sumDepthf/(resultImgSizeW*resultImgSizeH);
	float myNormDepth = myDepth/sumDepthf;
	myNormDepth = max(0.0,min(myNormDepth,4.0));	
	
	float myDistance = inputDistance[pos];
	float sumDistancef = sumDistance[0]/scale;
	sumDistancef=sumDistancef/(resultImgSizeW*resultImgSizeH);
	float myNormDistance = myDistance/sumDistancef;
	myNormDistance = max(0.0,min(myNormDistance,4.0));	
	
	
	float out_val = 0.5*myNormDepth+0.5*myNormDistance;
	
	atomic_add(&saliency[pos_block],out_val*scale);			
	
}