/*
 * Copyright (c) 2013-2014 Chun-Ying Huang
 *
 * This file is part of GamingAnywhere (GA).
 *
 * GA is free software; you can redistribute it and/or modify it
 * under the terms of the 3-clause BSD License as published by the
 * Free Software Foundation: http://directory.fsf.org/wiki/License:BSD_3Clause
 *
 * GA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the 3-clause BSD License along with GA;
 * if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <pthread.h>
#include <map>

#include "vsource.h"
#include "vconverter.h"
#include "rtspconf.h"
#include "encoder-common.h"

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-avcodec.h"

#include "dpipe.h"
#include "filter-d2float.h"

#define	POOLSIZE		8
#define	ENABLE_EMBED_COLORCODE	1

using namespace std;

static int filter_initialized = 0;
static int filter_started = 0;
static pthread_t filter_tid[VIDEO_SOURCE_CHANNEL_MAX];
static FILE * depth_file;//depth aggregated on 32x32 blocks level
/* filter_D2FLOAT_init: arg is two pointers to pipeline format string */
/*	1st ptr: source pipeline */
/*	2nd ptr: destination pipeline */
/* the number of pipeline(s) is equivalut to the number of video source(s) */


//debug purposes
static float min_depth=FLT_MAX;
static float max_depth=FLT_MIN;
static bool written = 0;
static bool recording =0;
static int frames = 0;

//https://gamedev.stackexchange.com/questions/26687/what-are-the-valid-depthbuffer-texture-formats-in-directx-11-and-which-are-also
//https://docs.microsoft.com/en-us/windows/desktop/api/dxgiformat/ne-dxgiformat-dxgi_format
//also using renderdoc to dump buffer contents
static void convert_depth(unsigned char * src_ptr,unsigned char * dst_ptr,int width, int height){
	
	int ptr_bytes = ga_conf_readint("d-bytes");
	int format = ga_conf_readint("d-uf");
	int internal_format = 0;
	if(ptr_bytes==2){
		internal_format=UNORM_2;
	}
	else if(ptr_bytes==4 && format==0){
		internal_format=UNORM_4;
	}
	else if(ptr_bytes==4 && format==1){
		internal_format=FLOAT_4;
	}
	else if(ptr_bytes==8){
		internal_format=FLOAT_8;
	}
	if(internal_format==FLOAT_4)
	{
		CopyMemory(dst_ptr,src_ptr,width*height*4);
		return;
	}	
	//ga_error("inside convert\n");
	for(int row=0;row<height;row++){
		int offset = row*width*ptr_bytes;
		for(int i=0;i<width;i++){

			float val=0.0;
			switch (internal_format)
			{
				case UNORM_2:			
					val=((((src_ptr[offset+1]& 0xFF)<<8|(src_ptr[offset]& 0xFF)))/(pow(2.0,16.0)-1.0));					
					break;			
				case UNORM_4:			
					val=((((src_ptr[offset+2]& 0xFF)<<16|(src_ptr[offset+1]& 0xFF)<<8|(src_ptr[offset]& 0xFF)))/(pow(2.0,24.0)-1.0));				
					break;
				case FLOAT_8:			
					val=((src_ptr[offset+4]& 0xFF)<<32|(src_ptr[offset+5]& 0xFF)<<16|(src_ptr[offset+6]& 0xFF)<<8|(src_ptr[offset+7]& 0xFF));
					break;
			}
			memcpy(dst_ptr, &val, sizeof val);
			dst_ptr=dst_ptr+4;
			/*if(row==0 && cnt==60){
				char buf [4096];
				snprintf(buf,sizeof(buf),"%d:%.05f,%d,%d,%d\n",i,depths[row][i],(ptr[offset+2]& 0xFF)<<16,(ptr[offset+1]& 0xFF)<<8,(ptr[offset]& 0xFF));
				ga_error(buf);
			}*/
			max_depth=max(max_depth,val);
			min_depth=min(min_depth,val);
			offset = offset + ptr_bytes;
		}
	}	
	//ga_error("exiting convert\n");
}

static void write_depth_png(int width,int height,void * depths_ptr){	
	float * depths=(float *)depths_ptr;
	FILE* file_d;			
	file_d = fopen( "C:\\Users\\mhegazy\\Desktop\\temp_d.csv", "wb" );	
	char buf[50];
	snprintf(buf,sizeof(buf),"min:%.05f,max:%.05f\n",min_depth,max_depth);
	ga_error(buf);
	for(int i=0;i<height;i++){
		for(int j=0;j<width;j++){
			int val = (int)((255.0 / (max_depth - min_depth)) * (depths[j+i*width] - min_depth));			
			fwrite( &val, 1, 1, file_d );			
		}		
	}
	fclose(file_d);
}

static int
filter_D2FLOAT_deinit(void *arg) {
	int iid;
	
	ga_error("filter_d: deinitialized.\n");
	return 0;
}

static int
filter_D2FLOAT_init(void *arg) {
	// arg is image source id
	int iid;
	const char **filterpipe = (const char **) arg;
	dpipe_t *srcpipe[VIDEO_SOURCE_CHANNEL_MAX];
	dpipe_t *dstpipe[VIDEO_SOURCE_CHANNEL_MAX];	
	//
	if(filter_initialized != 0)
		return 0;		
	//
	recording=ga_conf_readbool("recording", 0);
	if(recording)
		depth_file=fopen( "depth.bin","ab");
	bzero(dstpipe, sizeof(dstpipe));
	//
	for(iid = 0; iid < video_source_channels(); iid++) {		
		char srcpipename[64], dstpipename[64];
		int inputW, inputH, outputW, outputH;		
		dpipe_buffer_t *data = NULL;
		//
		snprintf(srcpipename, sizeof(srcpipename), filterpipe[0], iid);
		snprintf(dstpipename, sizeof(dstpipename), filterpipe[1], iid);
		srcpipe[iid] = dpipe_lookup(srcpipename);
		if(srcpipe[iid] == NULL) {
			ga_error("D2FLOAT filter: cannot find pipe %s\n", srcpipename);
			goto init_failed;
		}
		inputW = video_source_curr_width(iid);
		inputH = video_source_curr_height(iid);
		outputW = video_source_out_width(iid);
		outputH = video_source_out_height(iid);				
		//
		dstpipe[iid] = dpipe_create(iid, dstpipename, POOLSIZE,
				sizeof(vsource_frame_t)  + video_source_mem_size(iid));
		if(dstpipe[iid] == NULL) {
			ga_error("D2FLOAT filter: create dst-pipeline failed (%s).\n", dstpipename);
			goto init_failed;
		}
		for(data = dstpipe[iid]->in; data != NULL; data = data->next) {
			if(vsource_frame_init(iid, (vsource_frame_t*) data->pointer) == NULL) {
				ga_error("D2FLOAT filter: init frame failed for %s.\n", dstpipename);
				goto init_failed;
			}
		}
		video_source_add_pipename(iid, dstpipename);
	}
	//
	//ga_error("finished initializing depth filter\n");
	filter_initialized = 1;
	//
	return 0;
init_failed:
	for(iid = 0; iid < video_source_channels(); iid++) {
		if(dstpipe[iid] != NULL)
			dpipe_destroy(dstpipe[iid]);
		dstpipe[iid] = NULL;
	}
#if 0
	if(pipe) {
		delete pipe;
	}
#endif
	return -1;
}

/* filter_RGB2YUV_threadproc: arg is two pointers to pipeline name */
/*	1st ptr: source pipeline */
/*	2nd ptr: destination pipeline */

static void *
filter_D2FLOAT_threadproc(void *arg) {
	// arg is pointer to source pipe	
	const char **filterpipe = (const char **) arg;
	dpipe_t *srcpipe = dpipe_lookup(filterpipe[0]);
	dpipe_t *dstpipe = dpipe_lookup(filterpipe[1]);
	dpipe_buffer_t *srcdata = NULL;
	dpipe_buffer_t *dstdata = NULL;
	vsource_frame_t *srcframe = NULL;
	vsource_frame_t *dstframe = NULL;
	// image info
	//int istride = video_source_maxstride();
	//
	unsigned char *src[] = { NULL, NULL, NULL, NULL };
	unsigned char *dst[] = { NULL, NULL, NULL, NULL };
	int srcstride[] = { 0, 0, 0, 0 };
	int dststride[] = { 0, 0, 0, 0 };
	int iid;
	int outputW, outputH;
	//	
	//
	pthread_mutex_t condMutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
	//
	if(srcpipe == NULL || dstpipe == NULL) {
		ga_error("D2FLOAT filter: bad pipeline (src=%p; dst=%p).\n", srcpipe, dstpipe);
		goto filter_quit;
	}
	//
	iid = dstpipe->channel_id;
	outputW = video_source_out_width(iid);
	outputH = video_source_out_height(iid);
	//
	ga_error("D2FLOAT filter[%ld]: pipe#%d from '%s' to '%s' (output-resolution=%dx%d)\n",
		ga_gettid(), iid,
		srcpipe->name, dstpipe->name,
		outputW/*iwidth*/, outputH/*iheight*/);
	// start filtering
	while(filter_started != 0) {
		// wait for notification
		srcdata = dpipe_load(srcpipe, NULL);
		if(srcdata == NULL) {
			ga_error("D2FLOAT filter: unexpected NULL frame received (from '%s', data=%d, buf=%d).\n",
				srcpipe->name, srcpipe->out_count, srcpipe->in_count);
			exit(-1);
			// should never be here
			goto filter_quit;
		}
		srcframe = (vsource_frame_t*) srcdata->pointer;
		//
		dstdata = dpipe_get(dstpipe);
		dstframe = (vsource_frame_t*) dstdata->pointer;
		// basic info
		dstframe->imgpts = srcframe->imgpts;
		dstframe->timestamp = srcframe->timestamp;
		dstframe->pixelformat = AV_PIX_FMT_RGBA;	//yuv420p;
		dstframe->realwidth = outputW;
		dstframe->realheight = outputH;
		dstframe->realstride = outputW;
		dstframe->realsize = outputW * outputH * 4;
		// scale image: RGBA, BGRA, or YUV	
		//ga_error("before convert\n");
		convert_depth(srcframe->imgbuf,dstframe->imgbuf,outputW,outputH);	
		if(written ==0){
			written = 1;
			//write_depth_png(outputW,outputH,dstframe->imgbuf);
		}		
		//
		dpipe_put(srcpipe, srcdata);
		dpipe_store(dstpipe, dstdata);
		if(recording){
			if(frames<MAX_RECORD)
				fwrite(dstframe->imgbuf,sizeof(float),outputW*outputH,depth_file);
			else if(frames==MAX_RECORD)
				fclose(depth_file);
		}
		frames++;
		//
	}
	//
filter_quit:
	if(srcpipe) {
		srcpipe = NULL;
	}
	if(dstpipe) {
		dpipe_destroy(dstpipe);
		dstpipe = NULL;
	}		
	//
	ga_error("D2FLOAT filter: thread terminated.\n");
	//
	return NULL;
}

/* filter_D2FLOAT_start: arg is two pointers to pipeline format string */
/*	1st ptr: source pipeline */
/*	2nd ptr: destination pipeline */
/* the number of pipeline(s) is equivalut to the number of video source(s) */

static int
filter_D2FLOAT_start(void *arg) {
	int iid;
	const char **filterpipe = (const char **) arg;
	static char *filter_param[VIDEO_SOURCE_CHANNEL_MAX][2];
#define	MAXPARAMLEN	64
	static char params[VIDEO_SOURCE_CHANNEL_MAX][2][MAXPARAMLEN];
	//
	if(filter_started != 0)
		return 0;
	filter_started = 1;
	for(iid = 0; iid < video_source_channels(); iid++) {
		snprintf(params[iid][0], MAXPARAMLEN, filterpipe[0], iid);
		snprintf(params[iid][1], MAXPARAMLEN, filterpipe[1], iid);
		filter_param[iid][0] = params[iid][0];
		filter_param[iid][1] = params[iid][1];
		pthread_cancel_init();
		if(pthread_create(&filter_tid[iid], NULL, filter_D2FLOAT_threadproc, filter_param[iid]) != 0) {
			filter_started = 0;
			ga_error("filter D2FLOAT: create thread failed.\n");
			return -1;
		}
		pthread_detach(filter_tid[iid]);
	}
	return 0;
}

/* filter_D2FLOAT_stop: no arguments are required */

static int
filter_D2FLOAT_stop(void *arg) {
	int iid;
	if(filter_started == 0)
		return 0;
	filter_started = 0;
	for(iid = 0; iid < video_source_channels(); iid++) {
		pthread_cancel(filter_tid[iid]);
	}
	if(recording)
		fclose(depth_file);	
	return 0;
}

ga_module_t *
module_load() {
	static ga_module_t m;
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_FILTER;
	m.name = strdup("filter-D2FLOAT");
	m.init = filter_D2FLOAT_init;
	m.start = filter_D2FLOAT_start;
	m.stop = filter_D2FLOAT_stop;
	m.deinit = filter_D2FLOAT_deinit;	
	return &m;
}

