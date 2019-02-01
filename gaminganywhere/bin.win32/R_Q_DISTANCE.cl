#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
__constant int resultImgSizeW=?w?;
__constant int resultImgSizeH=?h?;
__constant int cu_size=?c?;
__constant int blocks_width=?bw?;
__constant int blocks_height=?bh?;
__constant unsigned long scale=?s?;
__kernel void ROI(__global float * roi_block,__global unsigned long * sumDistance, __global unsigned long * out_dist_block)
{    
    int pixelX=get_global_id(0); 
    int pixelY=get_global_id(1);    
	int block_y = pixelY>>cu_size;	//to avoid integer division 
	int block_x = pixelX>>cu_size; //to avoid integer division 
	int pos_block = block_x+block_y*blocks_width;		
	unsigned long pix_val=(unsigned long)(255*roi_block[pos_block]*scale);	
	atomic_add(&sumDistance[0],pix_val);
	atomic_add(&out_dist_block[pos_block],pix_val);	    
}