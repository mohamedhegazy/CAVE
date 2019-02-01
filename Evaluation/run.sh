#!/bin/bash
games=(ga1 ga2 ga3 ga4 ga5)
qp=(22 27 32 37)
length=20
width=(640 640 800 1280 1280)
height=(480 480 600 720 720)
idx=0
for j in "${games[@]}"; do echo $j;	
	w=${width[idx]}
	h=${height[idx]}
	echo $w $h
	for b in "${qp[@]}"; do echo $b; 
	   size=$(stat --printf="%s" "$j/QP/enc_$b.mp4")
	   bitrate=$(echo "($size * 8/$length)"|bc -l | xargs printf "%.2f\n")		   
	   bitrate=$(echo ${bitrate%.*})
	   temp_bitrate=bitrate
	   file="$j/ROI1/$bitrate/enc.mp4"
	   i=0
	   while [ ! -f "$file" ] ; do
			let bitrate=bitrate+1
			let i=i+1
			if [ "$i" -eq 10 ]; then
			  break
			fi
			file="$j/ROI1/$bitrate/enc.mp4"
		done
		if [ ! -f "$file" ]; then
			let bitrate=temp_bitrate
			let i=10
			while [ ! -f "$file" ] ; do
				let bitrate=bitrate-1
				let i=i-1
				if [ "$i" -eq 0 ]; then
				  break
				fi				
				file="$j/ROI1/$bitrate/enc.mp4"
			done
		fi		
	   ./decode_job.sh "$j/Lambda-ROI1/$bitrate/"
	   ./vmaf_job.sh $j/raw'_'$w'_'$h.yuv "$j/Lambda-ROI1/$bitrate/" $w $h	   	  
	   ./decode_job.sh "$j/ROI1/$bitrate/"
	   ./vmaf_job.sh $j/raw'_'$w'_'$h.yuv "$j/ROI1/$bitrate/" $w $h	   
	   ./decode_job.sh "$j/Base-x2651/$bitrate/"
	   ./vmaf_job.sh $j/raw'_'$w'_'$h.yuv "$j/Base-x2651/$bitrate/" $w $h	   
	done
	let idx=idx+1
done
./eval_job.sh