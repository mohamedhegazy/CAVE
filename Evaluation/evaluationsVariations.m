%summary of eval. results (quality (PSNR/SSIM), quality fluctuations (standard deviation of PSNR/SSIM), 
%bd-rate (bit savings) in ROI, 
%bitrate accuracy (1-absolute difference between actual and target divided by the target)*100)
function evaluationsVariations
distcomp.feature( 'LocalUseMpiexec', false )
pc = parcluster('local');
parpool(pc, 32)
qp = [22 27 32 37];
y_lim_psnr=[25 45];
y_lim_ssim=[0.6 1;0.75 1;];
comp = {'Base-x265','Lambda-ROI','ROI'};
y_lim_psnr_std=[0 15];
y_lim_ssim_std=[0 0.08];
ticks_ssim_5=[0.75,0.80,0.85,0.90,0.95,1];
ticks_ssim_1=[0.6,0.7,0.8,0.9,1];
length = 20;
frameRate = 30;
sequences = 1;
width_ = [640 640 800 1280 1280];
height_ = [480 480 600 720 720];
y_lim_vmaf=[40 100];
widthDelta_ = width_/16;
heightDelta_ = height_/16;
period = 1;
update=2;
base_path_ = [strcat({'ga1/'}) strcat({'ga2/'}) strcat({'ga3/'}) strcat({'ga4/'}) strcat({'ga5/'})];
tic
for b=1:numel(base_path_)	
	frames = frameRate * length;
    width=width_(b);
    height=height_(b);
    widthDelta=int32(ceil(widthDelta_(b)));
    heightDelta=int32(ceil(heightDelta_(b)));
    base_path=base_path_(b);
    rates=[];
    qp_path=strcat(base_path,'QP',{'/'});
    for i=1:numel(qp)
        qp_temp=strcat(qp_path,'enc_',num2str(qp(i)),'.mp4');
        size=dir(char(qp_temp));		
        size=int32(size.bytes*8/length);
		temp_size=size;
		x=1;		
        while (~(exist(char(strcat(base_path,'/Base-x2651/',num2str(size))),'dir') == 7) && x<10)
            size=size+1;
			x=x+1;
        end
		if ~(exist(char(strcat(base_path,'/Base-x2651/',num2str(size))),'dir') == 7)
			size=temp_size;
			x=10;
			while (~(exist(char(strcat(base_path,'/Base-x2651/',num2str(size))),'dir') == 7) && x>=0)
				size=size-1;
				x=x-1;
			end
		end
        rates(i)=size
        percentage_eval(i)=1;
    end
	dlmwrite(char(strcat(base_path,'temp/eval.txt')),'');
for ss=1:sequences            
        sequence=num2str(ss);
        
        PSNR_lambda_r = zeros(1,numel(rates)*numel(period)); % PSNR of whole video for base encoder
        PSNR_lambda_r_F = zeros(frames,numel(rates)*numel(period)); % PSNR for each frame in each video for base encoder
        PSNR_lambda_r_B = zeros(heightDelta,widthDelta,frames,numel(rates)*numel(period)); % PSNR for each 64X64 block in each frame in each video for base encoder
        
        SSIM_lambda_r = zeros(1,numel(rates)*numel(period)); % PSNR of whole video for base encoder
        SSIM_lambda_r_F = zeros(frames,numel(rates)*numel(period)); % PSNR for each frame in each video for base encoder
        SSIM_lambda_r_B = zeros(heightDelta,widthDelta,frames,numel(rates)*numel(period)); % PSNR for each 64X64 block in each frame in each video for base encoder
        
        
        

        PSNR_base = zeros(1,numel(rates)*numel(period)); % PSNR of whole video for base encoder
        PSNR_base_F = zeros(frames,numel(rates)*numel(period)); % PSNR for each frame in each video for base encoder
        PSNR_base_B = zeros(heightDelta,widthDelta,frames,numel(rates)*numel(period)); % PSNR for each 64X64 block in each frame in each video for base encoder
        
        SSIM_base = zeros(1,numel(rates)*numel(period)); % PSNR of whole video for base encoder
        SSIM_base_F = zeros(frames,numel(rates)*numel(period)); % PSNR for each frame in each video for base encoder
        SSIM_base_B = zeros(heightDelta,widthDelta,frames,numel(rates)*numel(period)); % PSNR for each 64X64 block in each frame in each video for base encoder


         PSNR_roi = zeros(1,numel(rates)*numel(period)); % PSNR of whole video for base encoder
         PSNR_roi_F = zeros(frames,numel(rates)*numel(period)); % PSNR for each frame in each video for base encoder
         PSNR_roi_B = zeros(heightDelta,widthDelta,frames,numel(rates)*numel(period)); % PSNR for each 64X64 block in each frame in each video for base encoder 
         
         SSIM_roi = zeros(1,numel(rates)*numel(period)); % PSNR of whole video for base encoder
         SSIM_roi_F = zeros(frames,numel(rates)*numel(period)); % PSNR for each frame in each video for base encoder
         SSIM_roi_B = zeros(heightDelta,widthDelta,frames,numel(rates)*numel(period)); % PSNR for each 64X64 block in each frame in each video for base encoder 

		 
		vmaf_base = zeros(1,numel(rates)*numel(period));
		vmaf_lambda_r = zeros(1,numel(rates)*numel(period));		
		vmaf_roi = zeros(1,numel(rates)*numel(period));
         
        actual_base_rate = zeros(1,numel(rates)*numel(period));
        actual_lambda_r_rate = zeros(1,numel(rates)*numel(period));            
         
        actual_roi_rate = zeros(1,numel(rates)*numel(period));      
        
        
        std_base = zeros(1,numel(rates)*numel(period));
        std_lambda_r = zeros(1,numel(rates)*numel(period));
        
        std_roi = zeros(1,numel(rates)*numel(period));
        
        std_ssim_base = zeros(1,numel(rates)*numel(period));
        std_ssim_lambda_r = zeros(1,numel(rates)*numel(period));
        
        std_ssim_roi = zeros(1,numel(rates)*numel(period)); 
        
        


        for i=1:numel(rates)                
            for j=1:numel(period)

                bitrate=num2str(rates(i));
                enc_path = strcat(base_path,comp(1),sequence,{'/'},bitrate);
                lambda_r_path = strcat(base_path,comp(2),sequence,{'/'},bitrate);
                roi_path = strcat(base_path,comp(3),sequence,{'/'},bitrate);
                
               
				
                
                



                [PSNR_base(1,j+(i-1)*numel(period)),PSNR_base_F(:,j+(i-1)*numel(period)),PSNR_base_B(:,:,:,j+(i-1)*numel(period))] = yuvpsnr(char(strcat(base_path,'raw_',num2str(width),'_',num2str(height),'.yuv')),char(strcat(enc_path,'/enc.yuv')),width,height,'420','y',frames);
                [PSNR_lambda_r(1,j+(i-1)*numel(period)),PSNR_lambda_r_F(:,j+(i-1)*numel(period)),PSNR_lambda_r_B(:,:,:,j+(i-1)*numel(period))] = yuvpsnr(char(strcat(base_path,'raw_',num2str(width),'_',num2str(height),'.yuv')),char(strcat(lambda_r_path,'/enc.yuv')),width,height,'420','y',frames);
                
                [PSNR_roi(1,j+(i-1)*numel(period)),PSNR_roi_F(:,j+(i-1)*numel(period)),PSNR_roi_B(:,:,:,j+(i-1)*numel(period))] = yuvpsnr(char(strcat(base_path,'raw_',num2str(width),'_',num2str(height),'.yuv')),char(strcat(roi_path,'/enc.yuv')),width,height,'420','y',frames);
                
                
                [SSIM_base(1,j+(i-1)*numel(period)),SSIM_base_F(:,j+(i-1)*numel(period)),SSIM_base_B(:,:,:,j+(i-1)*numel(period))] = yuvssim(char(strcat(base_path,'raw_',num2str(width),'_',num2str(height),'.yuv')),char(strcat(enc_path,'/enc.yuv')),width,height,frames);
                [SSIM_lambda_r(1,j+(i-1)*numel(period)),SSIM_lambda_r_F(:,j+(i-1)*numel(period)),SSIM_lambda_r_B(:,:,:,j+(i-1)*numel(period))] = yuvssim(char(strcat(base_path,'raw_',num2str(width),'_',num2str(height),'.yuv')),char(strcat(lambda_r_path,'/enc.yuv')),width,height,frames);
                
                [SSIM_roi(1,j+(i-1)*numel(period)),SSIM_roi_F(:,j+(i-1)*numel(period)),SSIM_roi_B(:,:,:,j+(i-1)*numel(period))] = yuvssim(char(strcat(base_path,'raw_',num2str(width),'_',num2str(height),'.yuv')),char(strcat(roi_path,'/enc.yuv')),width,height,frames);
                
                
                std_base(1,j+(i-1)*numel(period))=std(PSNR_base_F(:,j+(i-1)*numel(period)));
                std_lambda_r(1,j+(i-1)*numel(period))=std(PSNR_lambda_r_F(:,j+(i-1)*numel(period)));
                
                std_roi(1,j+(i-1)*numel(period))=std(PSNR_roi_F(:,j+(i-1)*numel(period)));                

                std_ssim_base(1,j+(i-1)*numel(period))=std(SSIM_base_F(:,j+(i-1)*numel(period)));
                std_ssim_lambda_r(1,j+(i-1)*numel(period))=std(SSIM_lambda_r_F(:,j+(i-1)*numel(period)));
                
                std_ssim_roi(1,j+(i-1)*numel(period))=std(SSIM_roi_F(:,j+(i-1)*numel(period)));
                
				vmaf_base(1,j+(i-1)*numel(period))=getvmaf(char(strcat(enc_path,'/vmaf_output.xml')),frames);
				vmaf_lambda_r(1,j+(i-1)*numel(period))=getvmaf(char(strcat(lambda_r_path,'/vmaf_output.xml')),frames);				
				
				vmaf_roi(1,j+(i-1)*numel(period))=getvmaf(char(strcat(roi_path,'/vmaf_output.xml')),frames);
                

                size1=dir(char(strcat(enc_path,{'/'},{'enc.mp4'})));
                actual_base_rate(1,j+(i-1)*numel(period))=size1.bytes*8/(length);                      
                size2=dir(char(strcat(lambda_r_path,{'/'},{'enc.mp4'})));
                actual_lambda_r_rate(1,j+(i-1)*numel(period))=size2.bytes*8/(length);                                
                size5=dir(char(strcat(roi_path,{'/'},{'enc.mp4'})));
                actual_roi_rate(1,j+(i-1)*numel(period))=size5.bytes*8/(length);


            end
        end


		scale = 1024;
		step = 100;
		bw = '(Kbps)';
		if( actual_base_rate(1,1)/(1024*1024) > 1 )
			scale=1024*1024;
			step = 1;
			bw = '(Mbps)';
		end
        max_rate=max(max(max(actual_base_rate(1,:)/scale)),max(max(actual_lambda_r_rate(1,:)/scale)));                        
        max_rate=max(max_rate,max(max(actual_roi_rate(1,:)/scale)));            
		if step == 1024
			max_rate=max_rate+20;
		else
			max_rate=floor(max_rate)+1;
		end
        x_lim_rate=[0 max_rate];            
        x_tick=0:step:max_rate;            
        x_axis=cell(1,numel(x_tick));
        for k=1:numel(x_tick)               
            x_axis(1,k)={num2str(x_tick(k))};                   
        end

        for i=1:numel(percentage_eval)                            
            ratesTick(i)=int32(rates(i)*percentage_eval(i)/scale);
        end            
        x_tick_const=min(ratesTick):step:max(ratesTick);                                                  
        x_axis_const=cell(1,numel(x_tick_const));
		
        for k=1:numel(x_tick_const)               
            x_axis_const(1,k)={num2str(x_tick_const(k))};                   
        end
       
		
		f = figure('visible','off');
        plot(actual_base_rate/scale,vmaf_base,'-.b^');
        hold on           
        plot(actual_lambda_r_rate/scale,vmaf_lambda_r,'-ro');  
        hold on
        
        plot(actual_roi_rate/scale,vmaf_roi,':kx');
        hold on   
        set(gca,'xticklabel',x_axis,'xtick',x_tick)            
        xlabel(char(strcat('Bitrate ',bw)));
        ylabel('VMAF');
        
		legend('Base','CAVE','RQ');
		
        xlim(x_lim_rate)
        ylim(y_lim_vmaf)
        saveFig(f,char(strcat(base_path,'temp/VMAF',sequence)));
		
		
		
		y_lim_psnr=[min(PSNR_base,min(PSNR_lambda_r,PSNR_roi))-1];
        f = figure('visible','off');
        plot(actual_base_rate/scale,PSNR_base,'-.b^');
        hold on           
        plot(actual_lambda_r_rate/scale,PSNR_lambda_r,'-ro');  
        hold on
        
        plot(actual_roi_rate/scale,PSNR_roi,':kx');
        hold on   
        set(gca,'xticklabel',x_axis,'xtick',x_tick)            
        xlabel(char(strcat('Bitrate ',bw)));
        ylabel('PSNR (dB)');
		legend('Base','CAVE','RQ');
        xlim(x_lim_rate)
        saveFig(f,char(strcat(base_path,'temp/PSNR',sequence)));
        
        y_lim_psnr_std=[0 min(std_base,min(std_lambda_r,std_roi))+1];
        f = figure('visible','off');
        plot(actual_base_rate/scale,std_base,'-.b^');
        hold on
        plot(actual_lambda_r_rate/scale,std_lambda_r,'-ro');
        hold on
        plot(actual_roi_rate/scale,std_roi,':kx');
        set(gca,'xticklabel',x_axis,'xtick',x_tick)
        xlabel(char(strcat('Bitrate ',bw)));
        ylabel('Y-PSNR Standard Deviation');
		ytickformat('%.2f');
		xlim(x_lim_rate)
		legend('Base','CAVE','RQ');
        saveFig(f,char(strcat(base_path,'temp/STD-PSNR',sequence)));
        
        
        min_ssim=min(min(SSIM_base),min(min(SSIM_lambda_r),min(SSIM_roi)));
		if(min_ssim<ticks_ssim_5(1))
			min_ssim=ticks_ssim_1;
			q=1;
			fmt='%.1f';
		else
			q=2;
			min_ssim=ticks_ssim_5;
			fmt='%.2f';
		end
        f = figure('visible','off');
        plot(actual_base_rate/scale,SSIM_base,'-.b^');
        hold on           
        plot(actual_lambda_r_rate/scale,SSIM_lambda_r,'-ro');  
        hold on
        plot(actual_roi_rate/scale,SSIM_roi,':kx');
        hold on   
        set(gca,'xticklabel',x_axis,'xtick',x_tick)            
        xlabel(char(strcat('Bitrate ',bw)));
        ylabel('SSIM');
		legend('Base','CAVE','RQ');
        xlim(x_lim_rate)
        ylim(y_lim_ssim(q,:))
		yticks(min_ssim)
		ytickformat(fmt)
        saveFig(f,char(strcat(base_path,'temp/SSIM',sequence)));
        
        
        f = figure('visible','off');
        plot(actual_base_rate/scale,std_ssim_base,'-.b^');
        hold on
        plot(actual_lambda_r_rate/scale,std_ssim_lambda_r,'-ro');
        hold on
        plot(actual_roi_rate/scale,std_ssim_roi,':kx');
        ylim(y_lim_ssim_std)
		xlim(x_lim_rate)
        set(gca,'xticklabel',x_axis,'xtick',x_tick)
        xlabel(char(strcat('Bitrate ',bw)));
        ylabel('SSIM Standard Deviation');		
		ytickformat('%.2f');
		legend('Base','CAVE','RQ');
        saveFig(f,char(strcat(base_path,'temp/STD-SSIM',sequence)));
		
		
		
		
		
		
		      
  
        %Average PSNR for ROI areas for the three methods
        base_roi_psnr = zeros(1,numel(rates)*numel(period));
        lambda_r_roi_psnr = zeros(1,numel(rates)*numel(period));                   
        
             
        roi_roi_psnr = zeros(1,numel(rates)*numel(period));    
        
        
        base_roi_ssim = zeros(1,numel(rates)*numel(period));
        lambda_r_roi_ssim = zeros(1,numel(rates)*numel(period));                   
        
        roi_roi_ssim = zeros(1,numel(rates)*numel(period));    
        
        roi=zeros(heightDelta,widthDelta);        
        for i=1:numel(rates)            
            for j=1:numel(period)
                for k=1:frames
                    if(mod(k-1,update)==0)
                        
                        ind=floor((k-1)/update);
						if((exist(char(strcat(base_path,'/roi',num2str(ind),'.txt')),'file')) == 2)
							fid = fopen(char(strcat(base_path,'/roi',num2str(ind),'.txt')));
							roi=zeros(heightDelta,widthDelta);
							
							while ~feof(fid)
															
								
								line = fgets(fid); %# read line by line.
								C = strsplit(char(line),' ');
								category = str2double(C(1));
								x_center = str2double(C(2));
								y_center = str2double(C(3));
								wid = str2double(C(4));
								hig = str2double(C(5));
								x_center= (x_center-wid/2)*width;
								y_center= (y_center-hig/2)*height;
								wid= (wid)*width;
								hig= (hig)*height;


								x_center = max(1,floor(x_center / 16+1));
								y_center = max(1,floor(y_center / 16+1));
								xBottom=floor(x_center +wid/ 16+1);
								yBottom=floor(y_center + hig/ 16+1);

								for t =y_center:yBottom                                
									for x = x_center:xBottom                                    					
										roi(t,x)=category+1;				                                    			
									end
								end 
															
							end
							fclose(fid);
						end
                    end
                    psnr_roi_frame_base=0;                       
                    psnr_roi_frame_lambda_r=0;
        
                    psnr_roi_frame_roi=0;
                    
                    ssim_roi_frame_base=0;                       
                    ssim_roi_frame_lambda_r=0;
        
                    ssim_roi_frame_roi=0;
                    psnr_roi_frame_base_cnt=0;
                    psnr_roi_frame_lambda_r_cnt=0;
        
					psnr_roi_frame_roi_cnt=0;
                    
                    ssim_roi_frame_base_cnt=0;
                    ssim_roi_frame_lambda_r_cnt=0;
        
                    ssim_roi_frame_roi_cnt=0;
                    for l=1:heightDelta
                        for m=1:widthDelta                                
                            if(roi(l,m)~=0) % ROI area
                                if PSNR_base_B(l,m,k,j+(i-1)*numel(period)) ~= Inf
                                    psnr_roi_frame_base = psnr_roi_frame_base + PSNR_base_B(l,m,k,j+(i-1)*numel(period));
                                    psnr_roi_frame_base_cnt = psnr_roi_frame_base_cnt + 1;
                                end									                                                                                                         
                                if PSNR_lambda_r_B(l,m,k,j+(i-1)*numel(period)) ~= Inf
                                    psnr_roi_frame_lambda_r = psnr_roi_frame_lambda_r + PSNR_lambda_r_B(l,m,k,j+(i-1)*numel(period));
                                    psnr_roi_frame_lambda_r_cnt = psnr_roi_frame_lambda_r_cnt + 1;
                                end
                                
                                 if PSNR_roi_B(l,m,k,j+(i-1)*numel(period)) ~= Inf
                                     psnr_roi_frame_roi = psnr_roi_frame_roi + PSNR_roi_B(l,m,k,j+(i-1)*numel(period));
                                     psnr_roi_frame_roi_cnt = psnr_roi_frame_roi_cnt + 1;
                                 end
                                 
                                if SSIM_base_B(l,m,k,j+(i-1)*numel(period)) ~= Inf
                                    ssim_roi_frame_base = ssim_roi_frame_base + SSIM_base_B(l,m,k,j+(i-1)*numel(period));
                                    ssim_roi_frame_base_cnt = ssim_roi_frame_base_cnt + 1;
                                end									                                                                                                         
                                if SSIM_lambda_r_B(l,m,k,j+(i-1)*numel(period)) ~= Inf
                                    ssim_roi_frame_lambda_r = ssim_roi_frame_lambda_r + SSIM_lambda_r_B(l,m,k,j+(i-1)*numel(period));
                                    ssim_roi_frame_lambda_r_cnt = ssim_roi_frame_lambda_r_cnt + 1;
                                end
                                
                                 if SSIM_roi_B(l,m,k,j+(i-1)*numel(period)) ~= Inf
                                     ssim_roi_frame_roi = ssim_roi_frame_roi + SSIM_roi_B(l,m,k,j+(i-1)*numel(period));
                                     ssim_roi_frame_roi_cnt = ssim_roi_frame_roi_cnt + 1;
                                 end
                            end

                         end
                    end

                    psnr_roi_frame_base = psnr_roi_frame_base / psnr_roi_frame_base_cnt; % average PSNR of ROI area in frame k encoded with rate i and period j for base encoder
                    psnr_roi_frame_lambda_r = psnr_roi_frame_lambda_r / psnr_roi_frame_lambda_r_cnt; % average PSNR of ROI area in frame k encoded with rate i and period j for proposed
                    
                    psnr_roi_frame_roi = psnr_roi_frame_roi / psnr_roi_frame_roi_cnt; % average PSNR of ROI area in frame k encoded with rate i and period j for proposed
                    ssim_roi_frame_base = ssim_roi_frame_base / ssim_roi_frame_base_cnt; % average PSNR of ROI area in frame k encoded with rate i and period j for base encoder
                    ssim_roi_frame_lambda_r = ssim_roi_frame_lambda_r / ssim_roi_frame_lambda_r_cnt; % average PSNR of ROI area in frame k encoded with rate i and period j for proposed
                    
                    ssim_roi_frame_roi = ssim_roi_frame_roi / ssim_roi_frame_roi_cnt; % average PSNR of ROI area in frame k encoded with rate i and period j for proposed                     


                    base_roi_psnr(1,j+(i-1)*numel(period)) = base_roi_psnr(1,j+(i-1)*numel(period)) + psnr_roi_frame_base;
                    lambda_r_roi_psnr(1,j+(i-1)*numel(period)) = lambda_r_roi_psnr(1,j+(i-1)*numel(period)) + psnr_roi_frame_lambda_r;
                    roi_roi_psnr(1,j+(i-1)*numel(period)) = roi_roi_psnr(1,j+(i-1)*numel(period)) + psnr_roi_frame_roi;
                     
                    base_roi_ssim(1,j+(i-1)*numel(period)) = base_roi_ssim(1,j+(i-1)*numel(period)) + ssim_roi_frame_base;
                    lambda_r_roi_ssim(1,j+(i-1)*numel(period)) = lambda_r_roi_ssim(1,j+(i-1)*numel(period)) + ssim_roi_frame_lambda_r;
                    
                    roi_roi_ssim(1,j+(i-1)*numel(period)) = roi_roi_ssim(1,j+(i-1)*numel(period)) + ssim_roi_frame_roi;

                end
                base_roi_psnr(1,j+(i-1)*numel(period)) = base_roi_psnr(1,j+(i-1)*numel(period)) / frames; %average PSNR of ROI area in video encoded with rate i and period j for base encoder
                lambda_r_roi_psnr(1,j+(i-1)*numel(period)) = lambda_r_roi_psnr(1,j+(i-1)*numel(period)) / frames; % average PSNR of ROI area in video encoded with rate i and period j for proposed
                roi_roi_psnr(1,j+(i-1)*numel(period)) = roi_roi_psnr(1,j+(i-1)*numel(period)) / frames; % average PSNR of ROI area in video encoded with rate i and period j for proposed
                  
                base_roi_ssim(1,j+(i-1)*numel(period)) = base_roi_ssim(1,j+(i-1)*numel(period)) / frames; %average PSNR of ROI area in video encoded with rate i and period j for base encoder
                lambda_r_roi_ssim(1,j+(i-1)*numel(period)) = lambda_r_roi_ssim(1,j+(i-1)*numel(period)) / frames; % average PSNR of ROI area in video encoded with rate i and period j for proposed
                roi_roi_ssim(1,j+(i-1)*numel(period)) = roi_roi_ssim(1,j+(i-1)*numel(period)) / frames; % average PSNR of ROI area in video encoded with rate i and period j for proposed
            end
        end
		
        y_lim_psnr=min(base_roi_psnr,min(lambda_r_roi_psnr,roi_roi_psnr))-1;
        f = figure('visible','off');
        plot(actual_base_rate/scale,base_roi_psnr,'-.b^');
        hold on
        plot(actual_lambda_r_rate/scale,lambda_r_roi_psnr,'-ro');
        hold on
         plot(actual_roi_rate/scale,roi_roi_psnr,':kx');
         hold on
        set(gca,'xticklabel',x_axis,'xtick',x_tick)            
        xlabel(char(strcat('Bitrate ',bw)));
        ylabel('PSNR (dB)');
        legend('Base','CAVE','RQ');
        xlim(x_lim_rate)
        saveFig(f,char(strcat(base_path,'temp/ROI-PSNR',sequence)));
        
        
        min_ssim=min(min(base_roi_ssim),min(min(lambda_r_roi_ssim),min(roi_roi_ssim)));
		if(min_ssim<ticks_ssim_5(1))
			min_ssim=ticks_ssim_1;
			fmt='%.1f';
			q=1;
		else
			min_ssim=ticks_ssim_5;
			fmt='%.2f';
			q=2;
		end
        f = figure('visible','off');
        plot(actual_base_rate/scale,base_roi_ssim,'-.b^');
        hold on
        plot(actual_lambda_r_rate/scale,lambda_r_roi_ssim,'-ro');
        hold on
         plot(actual_roi_rate/scale,roi_roi_ssim,':kx');
         hold on
        set(gca,'xticklabel',x_axis,'xtick',x_tick)            
        xlabel(char(strcat('Bitrate ',bw)));
        ylabel('SSIM');
		legend('Base','CAVE','RQ');
        xlim(x_lim_rate)
        ylim(y_lim_ssim(q,:))
		yticks(min_ssim)
		ytickformat(fmt)
        saveFig(f,char(strcat(base_path,'temp/ROI-SSIM',sequence)));
		
		
		
		
       
		
		
        
        
        
        %SSIM
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),'SSIM','-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('BASE VS CAVE Quality Diff in ROI:',num2str(bjontegaard2(actual_base_rate,base_roi_ssim(1,:),actual_lambda_r_rate,lambda_r_roi_ssim(1,:),'dsnr'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('BASE VS CAVE Rate Diff in ROI:',num2str(bjontegaard2(actual_base_rate,base_roi_ssim(1,:),actual_lambda_r_rate,lambda_r_roi_ssim(1,:),'rate'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('RQ VS CAVE Quality Diff in ROI:',num2str(bjontegaard2(actual_roi_rate,roi_roi_ssim(1,:),actual_lambda_r_rate,lambda_r_roi_ssim(1,:),'dsnr'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('RQ VS CAVE Rate Diff in ROI:',num2str(bjontegaard2(actual_roi_rate,roi_roi_ssim(1,:),actual_lambda_r_rate,lambda_r_roi_ssim(1,:),'rate'))),'-append')
        
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('BASE VS CAVE Quality Diff Overall:',num2str(bjontegaard2(actual_base_rate,SSIM_base(1,:),actual_lambda_r_rate,SSIM_lambda_r(1,:),'dsnr'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('BASE VS CAVE Rate Diff Overall:',num2str(bjontegaard2(actual_base_rate,SSIM_base(1,:),actual_lambda_r_rate,SSIM_lambda_r(1,:),'rate'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('RQ VS CAVE Quality Diff Overall:',num2str(bjontegaard2(actual_roi_rate,SSIM_roi(1,:),actual_lambda_r_rate,SSIM_lambda_r(1,:),'dsnr'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('RQ VS CAVE Rate Diff Overall:',num2str(bjontegaard2(actual_roi_rate,SSIM_roi(1,:),actual_lambda_r_rate,SSIM_lambda_r(1,:),'rate'))),'-append')
        
        %PSNR
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),'PSNR','-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('BASE VS CAVE Quality Diff in ROI:',num2str(bjontegaard2(actual_base_rate,base_roi_psnr(1,:),actual_lambda_r_rate,lambda_r_roi_psnr(1,:),'dsnr'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('BASE VS CAVE Rate Diff in ROI:',num2str(bjontegaard2(actual_base_rate,base_roi_psnr(1,:),actual_lambda_r_rate,lambda_r_roi_psnr(1,:),'rate'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('RQ VS CAVE Quality Diff in ROI:',num2str(bjontegaard2(actual_roi_rate,roi_roi_psnr(1,:),actual_lambda_r_rate,lambda_r_roi_psnr(1,:),'dsnr'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('RQ VS CAVE Rate Diff in ROI:',num2str(bjontegaard2(actual_roi_rate,roi_roi_psnr(1,:),actual_lambda_r_rate,lambda_r_roi_psnr(1,:),'rate'))),'-append')
        
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('BASE VS CAVE Quality Diff Overall:',num2str(bjontegaard2(actual_base_rate,PSNR_base(1,:),actual_lambda_r_rate,PSNR_lambda_r(1,:),'dsnr'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('BASE VS CAVE Rate Diff Overall:',num2str(bjontegaard2(actual_base_rate,PSNR_base(1,:),actual_lambda_r_rate,PSNR_lambda_r(1,:),'rate'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('RQ VS CAVE Quality Diff Overall:',num2str(bjontegaard2(actual_roi_rate,PSNR_roi(1,:),actual_lambda_r_rate,PSNR_lambda_r(1,:),'dsnr'))),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('RQ VS CAVE Rate Diff Overall:',num2str(bjontegaard2(actual_roi_rate,PSNR_roi(1,:),actual_lambda_r_rate,PSNR_lambda_r(1,:),'rate'))),'-append')
        
        %accuracy
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('CAVE Bitrate Error:'),'-append')
		dlmwrite(char(strcat(base_path,'temp/eval.txt')),100*mean(abs((rates-actual_lambda_r_rate)/rates)),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('BASE Bitrate Error:'),'-append')
		dlmwrite(char(strcat(base_path,'temp/eval.txt')),100*mean(abs((rates-actual_base_rate)/rates)),'-append')
        dlmwrite(char(strcat(base_path,'temp/eval.txt')),strcat('RQ Bitrate Error:'),'-append')
		dlmwrite(char(strcat(base_path,'temp/eval.txt')),100*mean(abs((rates-actual_roi_rate)/rates)),'-append')
              
        
        

end
end
toc
disp('Finished evaluations');

