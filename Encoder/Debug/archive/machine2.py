import time
import os
import qp as q_script
base_path='\\\\cs-nsl-fs02.cmpt.sfu.ca\\NSL\\data\\projects\\CloudGaming\\GamingAnywhere\\'
qp=[22,27,32,37]
length=20
size=0
i=0
count=5
width=[1280,1280]
height=[720,720]
cnt=0
for x in range(4,6):
	p=base_path+'ga'+str(x)+'\\';
	# q_script.encode(p,width[cnt],height[cnt])
	for q in qp:
	   size=int(os.path.getsize(p+'QP\\enc_'+str(q)+'.mp4')*8/length);
	   # os.system("START Encoder.exe "+p+" "+str(size)+" 4 1 0");
	   # os.system("START Encoder.exe "+p+" "+str(size)+" 0 1 3 ");
	   # os.system("START Encoder.exe "+p+" "+str(size)+" 1 1 0.7");
	   os.system("START Encoder.exe "+p+" "+str(size)+" 2 1 0");
	   time.sleep(20)
	
	cnt=cnt+1
