#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
__constant int resultImgSizeW=?w?;
__constant int resultImgSizeH=?h?;
__constant int cu_size=?c?;//to avoid integer division  16->4 , 64->6
__constant int blocks_width=?bw?;//since it's constant
__constant int blocks_height=?bh?;//since it's constant
__constant unsigned long scale=?s?;

//Input :size -> number of rois
//		:roi_block-> importance of a block, if it belongs to an important object, it'll be the importance of the important object, otherwise it's 0
//		:rois-> center of each important object (x,y) coordinates
//Output:out_dist->distance function as in the paper
//		:sumDistance-> sum of distances in all pixels
__kernel void Weighted_Dist(__global int * size, __global float * roi_block, __global unsigned int * rois,__global float * out_dist, __global unsigned long* sumDistance)
{
    
    int pixelX=get_global_id(0); 
    int pixelY=get_global_id(1);    
	int pos = pixelX+pixelY*resultImgSizeW;
	int block_y = pixelY>>cu_size;	//to avoid integer division 
	int block_x = pixelX>>cu_size; //to avoid integer division 
	int pos_block = block_x+block_y*blocks_width;
	
	if(roi_block[pos_block]>0){
		out_dist[pos]=roi_block[pos_block];		
	}
	else{
		float sumDist=0;
		float diagonal = sqrt(pow(resultImgSizeW,2.0)+pow(resultImgSizeH,2.0));
		for(int i=0;i<size[0];i=i+2){
			int x_center=rois[i];
			int y_center=rois[i+1];
			int block_center_x = x_center>>cu_size;
			int block_center_y = y_center>>cu_size;
			float importance = roi_block[block_center_x+block_center_y*blocks_width];
			float dist = sqrt(pow(x_center-pixelX,2.0)+pow(y_center-pixelY,2.0));
			sumDist=sumDist+importance*(log(diagonal/dist)/log(diagonal));			
		}	
		sumDist=sumDist/size[0];
		out_dist[pos]=sumDist;		
	}	
	atomic_add(&sumDistance[0],out_dist[pos]*scale);
	    
}