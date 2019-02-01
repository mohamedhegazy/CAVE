%   yuvpsnr computes the psnr between two YUV-Files.
%
%	yuvpsnr('Filename1','Filename1', width, height, format,parameter) reads
%	the specified files using width and height for resolution and format 
%   for YUV-subsampling. With parameter you can chose if you want to
%   compute the psnr between the y, the u, the v, or the yuv part of the
%   videos
%	
%	Filename1 --> Name of File one (e.g. 'Test1.yuv')
%   Filename2 --> Name of File two (e.g. 'Test2.yuv')
%   width    --> width of a frame  (e.g. 352)
%   height   --> height of a frame (e.g. 280)
%   format   --> subsampling rate ('400','411','420','422' or '444')
%   parameter -->chose the components of the video which are used to compute 
%                 the psnr ('y', 'u', 'v', 'yuv') 
%
%   example: psnr = yuvpsnr('Test1.yuv','Test2.yuv',352,288,'420','y');

function [PSNR,PSNR_F,PSNR_B] = yuvpsnr(File1,File2,width,height,format,parameter,framenumber1)

    %set factor for UV-sampling
    fwidth = 0.5;
    fheight= 0.5;
    block_w = int32(ceil(width/16));
    block_h = int32(ceil(height/16));
    if strcmp(format,'400')
        fwidth = 0;
        fheight= 0;
    elseif strcmp(format,'411')
        fwidth = 0.25;
        fheight= 1;
    elseif strcmp(format,'420')
        fwidth = 0.5;
        fheight= 0.5;
    elseif strcmp(format,'422')
        fwidth = 0.5;
        fheight= 1;
    elseif strcmp(format,'444')
        fwidth = 1;
        fheight= 1;
    else
        display('Error: wrong format');
    end
    %get Filesize and Framenumber
    filep = dir(File1); 
    fileBytes = filep.bytes; %Filesize1
    clear filep
    % framenumber1 = fileBytes/(width*height*(1+2*fheight*fwidth)); %Framenumber1
    filep = dir(File1); 
    fileBytes = filep.bytes; %Filesize2
    clear filep
    % framenumber2 = fileBytes/(width*height*(1+2*fheight*fwidth)); %Framenumber2
    % if mod(framenumber1,1) ~= 0 | mod(framenumber2,1) ~= 0 | framenumber1~=framenumber2
        % display('Error: wrong resolution, format, filesize or different video lengths');
    % else
        mse = zeros(framenumber1,1);
        PSNR_frame = zeros(framenumber1,1);
        block_psnr = zeros(block_h,block_w,framenumber1);        
        parfor cntf = 1:framenumber1
            %load data of frames
            YUV1 = loadFileYUV(width,height,cntf,char(File1),fheight,fwidth);
            YUV2 = loadFileYUV(width,height,cntf,char(File2),fheight,fwidth);
            %get MSE for single frames
            if parameter == 'y'                
            for i=1:block_h
               for j=1:block_w    
               i0=16 * (i-1) +1;                   
               j0=16 * (j-1) +1;                   
               temp=sum(sum((double(YUV1(i0:min(height,i0+15),j0:min(width,j0+15),1))-double(YUV2(i0:min(height,i0+15),j0:min(width,j0+15),1))).^2))/(16*16);
               block_psnr(i,j,cntf)=10*log10((255^2)/temp);
               end
            end                
            mse(cntf) = sum(sum((double(YUV1(:,:,1))-double(YUV2(:,:,1))).^2))/(width*height);
            PSNR_frame(cntf) = 10*log10((255^2)/mse(cntf));

            elseif parameter == 'u'
                mse(cntf) = sum(sum((double(YUV1(:,:,2))-double(YUV2(:,:,2))).^2))/(width*height);
            elseif parameter == 'v'
                mse(cntf) = sum(sum((double(YUV1(:,:,3))-double(YUV2(:,:,3))).^2))/(width*height);
            elseif parameter == 'yuv'                               
                mse(cntf) = sum((double(YUV1(:))-double(YUV2(:))).^2)/length(YUV1(:));                
            end
        end
        PSNR_F = PSNR_frame;
        PSNR_B = block_psnr;
        %compute the mean of the mse vector
        PSNR = mean(mean(PSNR_frame));
    % end


