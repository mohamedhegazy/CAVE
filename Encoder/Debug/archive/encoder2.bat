@echo off
setlocal enabledelayedexpansion
set base_path=\\cs-nsl-fs02.cmpt.sfu.ca\NSL\
set qp=22 27 32 37
set length=20
set size=0
set i=0
set count=5
set width=1280 1280
set height=720 720
set cnt=0
(for /L %%a in (4,1,%count%) do (		
	set /A i=%%a		
	call set width_=!width[!cnt!]!
	set height_=%height[!cnt!]%
	set p="%base_path%ga!i!\"
	echo !p!
	call echo width: %width[0]%
	REM CALL "encoder - qp.bat" !p! !width_! !height_!
	REM (for %%q in (%qp%) do ( 	
	   REM call :filesize  "!p!QP\enc_%%q.mp4" %length% size
	   REM echo !size!
	   REM START Encoder.exe !p! !size! 4 1 0
	   REM TAppDecoder.exe -b "!p!Base-x2651\!size!\enc.mp4" -o "!p!Base-x2651\!size!\enc.yuv"    	 
	   REM START Encoder.exe !p! !size! 0 1 3 
	   REM TAppDecoder.exe -b "!p!Lambda-ROI1\!size!\enc.mp4" -o "!p!Lambda-ROI1\!size!\enc.yuv"     
	   REM START Encoder.exe !p! !size! 1 1 0.7
	   REM TAppDecoder.exe -b "!p!Lambda-Depth1\!size!\enc.mp4" -o "!p!Lambda-Depth1\!size!\enc.yuv"     
	   REM START Encoder.exe !p! !size! 2 1 
	   REM TAppDecoder.exe -b "!p!ROI1\!size!\enc.mp4" -o "!p!ROI1\!size!\enc.yuv"     	   
	   REM TIMEOUT 180
	REM ))
	set /A cnt=cnt+1
))
goto :eof

:filesize
  set /A %~3 = %~z1 * 8 / %~2    
  exit /B 0