#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable
__constant char kernelx[3][3] = {{-1, 0, 1}, 
                           {-2, 0, 2}, 
                           {-1, 0, 1}};
__constant char kernely[3][3] = {{-1, -2, -1}, 
                        {0,  0,  0}, 
                        {1,  2,  1}};
__constant int resultImgSizeW=?w?;
__constant int resultImgSizeH=?h?;
__constant int cu_size=?c?;//to avoid integer division  16->4 , 64->6
__constant int blocks_width=?bw?;//since it's constant
__constant int blocks_height=?bh?;//since it's constant
__constant unsigned long scale=?s?;
__kernel void Sobel(__global float * input, __global unsigned long * output)
{
    
    int pixelX=get_global_id(0); 
    int pixelY=get_global_id(1);    

    float magXr=0,magYr=0; 	
    for(int a = 0; a < 3; a++)
    {
        for(int b = 0; b < 3; b++)
        {            
            int xn = min(resultImgSizeW-1,max(0,pixelX + a - 1));
            int yn = min(resultImgSizeH-1,max(0,pixelY + b - 1));
            int index = xn + yn * resultImgSizeW;
			float val = input[index];
            magXr += val * kernelx[a][b];
            magYr += val * kernely[a][b];
        }
    } 	    	
	int block_y = pixelY>>cu_size;	//to avoid integer division 
	int block_x = pixelX>>cu_size; //to avoid integer division 
	int index_out = block_x+block_y*blocks_width;		
	unsigned long out=(unsigned long)(sqrt(magXr*magXr + magYr*magYr)*scale);
	//atomic_add doesn't work with floats, so the float is converted to unsigned int by multiplying by a large number (this will introduce some error depending on how large the number is)
	//and then on CPU it will be divided by the same large number to get the float back
	//overflow can't occur since the depth range is [0,1] so the largest value at a pixel is 4*sqrt(2) = 5.65 and if the 4096 (for a 64*64 block) are all equal to that value then the value will be 23170 and when multiplied
	//by the large number it will stay less than the max range of unsigned long
	atomic_add(&output[index_out],out);	
	atomic_add(&output[index_out+blocks_width*blocks_height],input[pixelX+pixelY*resultImgSizeW]*scale);	
    
	
	//int index_out = pixelX+pixelY*resultImgSizeW;
	//output[index_out] = sqrt(magXr*magXr + magYr*magYr) ;    
}