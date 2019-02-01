__constant char kernelx[3][3] = {{-1, 0, 1}, 
                           {-2, 0, 2}, 
                           {-1, 0, 1}};
__constant char kernely[3][3] = {{-1, -2, -1}, 
                        {0,  0,  0}, 
                        {1,  2,  1}};
__constant int resultImgSizeW=?w?;
__constant int resultImgSizeH=?h?;
__kernel void Sobel(__global uchar * input, __global uchar * output)
{
    
    int pixelX=get_global_id(0); // 1-D id list to 2D workitems(each process a single pixel)
    int pixelY=get_global_id(1);    

    int magXr=0,magYr=0; // red

    for(int a = 0; a < 3; a++)
    {
        for(int b = 0; b < 3; b++)
        {            
            int xn = min(resultImgSizeW-1,max(0,pixelX + a - 1));
            int yn = min(resultImgSizeH-1,max(0,pixelY + b - 1));
            int index = xn + yn * resultImgSizeW;
			float val = ((float *)input)[index];
            magXr += val * kernelx[a][b];
            magYr += val * kernely[a][b];
        }
     } 
	int index_out = pixelX+pixelY*resultImgSizeW;
	((float *)output)[index_out] = (float) sqrt((float)(magXr*magXr + magYr*magYr)) ;    
}