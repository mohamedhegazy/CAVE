import os
def encode(p,width,height):
	qp=[22,27,32,37]
	for q in qp:		
		os.system("x265.exe --input "+p+"raw_"+str(width)+"_"+str(height)+".yuv --output " +p+"QP\enc_"+str(q)+".mp4 --input-res "+str(width)+"x"+str(height)+ " --preset ultrafast --tune zerolatency --qp "+ str(q)+" --me dia --merange 16 --fps 30 --keyint 90 --ref 1 --bframes 0 --ipratio 1.059 --sar 1 --repeat-headers --intra-refresh");

