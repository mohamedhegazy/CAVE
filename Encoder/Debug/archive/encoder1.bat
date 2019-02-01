@echo off
setlocal enabledelayedexpansion
set base_path=\\cs-nsl-fs02.cmpt.sfu.ca\NSL\
set qp=22 27 32 37
set length=20
set size=0
set i=0
set count=3
set width=640 640 800
set height=480 480 600
(for /L %%a in (1,1,%count%) do (		
	set /A i=%%a	
	set p="%base_path%ga!i!\"
	echo !p!
	CALL "encoder - qp.bat" !p!
	(for %%q in (%qp%) do ( 	
	   call :filesize  "!p!QP\enc_%%q.mp4" %length% size
	   echo !size!
	   START Encoder.exe !p! !size! 4 1 0
	   TAppDecoder.exe -b "!p!Base-x2651\!size!\enc.mp4" -o "!p!Base-x2651\!size!\enc.yuv"    	 
	   START Encoder.exe !p! !size! 0 1 3 
	   TAppDecoder.exe -b "!p!Lambda-ROI1\!size!\enc.mp4" -o "!p!Lambda-ROI1\!size!\enc.yuv"     
	   START Encoder.exe !p! !size! 1 1 0.7
	   TAppDecoder.exe -b "!p!Lambda-Depth1\!size!\enc.mp4" -o "!p!Lambda-Depth1\!size!\enc.yuv"     
	   START Encoder.exe !p! !size! 2 1 
	   TAppDecoder.exe -b "!p!ROI1\!size!\enc.mp4" -o "!p!ROI1\!size!\enc.yuv"     	   
	   TIMEOUT 180
	))
))
goto :eof

:filesize
  set /A %~3 = %~z1 * 8 / %~2    
  exit /B 0