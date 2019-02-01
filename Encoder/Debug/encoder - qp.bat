set qp=22 27 32 37
(for %%q in (%qp%) do ( 
x265 --input "%~1raw_%~2_%~3.yuv" --output "%~1QP\enc_%%q.mp4" --input-res %~2'x'%~3 --preset ultrafast --tune zerolatency --qp %%q --me dia --merange 16 --fps 30 --keyint 90 --ref 1 --bframes 0 --ipratio 1.059 --sar 1 --repeat-headers
))
