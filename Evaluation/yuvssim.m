function [SSIM,SSIM_F,SSIM_B] = yuvssim(File1,File2,width,height,framenumber1)
    %set factor for UV-sampling
    fwidth = 0.5;
    fheight= 0.5;
    block_w = int32(ceil(width/16));
    block_h = int32(ceil(height/16));
	%weight_roi=[1 0.95 0.9 0.85 0.8 0.75 0.7 0.65 0.6 0.55 0.5];
	%w=numel(weight_roi);	
    block_ssim = zeros(block_h,block_w,framenumber1);
    frame_ssim = zeros(1,framenumber1);    
    %weights = zeros(block_h,block_w,framenumber1,w);     		
    parfor cntf = 1:framenumber1        		
        %load data of frames
        YUV1 = loadFileYUV(width,height,cntf,char(File1),fheight,fwidth);
        YUV2 = loadFileYUV(width,height,cntf,char(File2),fheight,fwidth);        
        %get MSE for single frames          
        [ssimval,ssimmap] = ssim(YUV1(:,:,1),YUV2(:,:,1));   
        frame_ssim(cntf) = ssimval;        
        myssim=ssimmap(:,:,1);
        for i=1:block_h
               for j=1:block_w                  			   			   			   
                   i0=16 * (i-1) +1;                   
                   j0=16 * (j-1) +1; 
                   block_ssim(i,j,cntf)=sum(sum(myssim(i0:min(height,i0+15),j0:min(width,j0+15))))/(16*16);               
               end
        end			 		
    end
    
    SSIM_B = block_ssim;  
    SSIM_F = frame_ssim;
    SSIM = mean(mean(frame_ssim));        
    