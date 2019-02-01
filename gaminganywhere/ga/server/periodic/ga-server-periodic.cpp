/*
 * Copyright (c) 2013 Chun-Ying Huang
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
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "ga-common.h"
#include "ga-conf.h"
#include "ga-module.h"
#include "rtspconf.h"
#include "controller.h"
#include "encoder-common.h"

//#define	TEST_RECONFIGURE

// image source pipeline:
//	vsource -- [vsource-%d] --> filter -- [filter-%d] --> encoder

// configurations:
static char *imagepipefmt = "video-%d";
static char *filterpipefmt = "filter-%d";
static char *imagepipe0 = "video-0";
static char *filterpipe0 = "filter-0";
static char *filter_param[] = { imagepipefmt, filterpipefmt };
//static char *video_encoder_param = filterpipefmt;
static void *audio_encoder_param = NULL;
//The filter-d2float module will convert the depth image from any possible depth format to float (to apply Sobel filter on it)
static char *imagepipefmt_d = "video_d-%d";//input pipe name format for filter-d2float
static char *filterpipefmt_d = "filter_d-%d";//ouput pipe name format for filter-d2float
static char *imagepipe0_d = "video_d-0";//source pipe name for filter-d2float
static char *filterpipe0_d = "filter_d-0";//ouput pipe name for filter-d2float
static char *filter_param_d[] = { imagepipefmt_d, filterpipefmt_d };
static char *bitratespipefmt = "bitrates-%d";
static char *bitratespipe0 = "bitrates-0";

static char *filterpipefmt_rc = "rc_filter-%d";//input pipe name format for the RC module's raw frame 
static char *filterpipefmt_d_rc = filterpipefmt_d;//input pipe name for the RC module's depth frame 
static char *qpfmt_rc = "rc_map-%d";//output pipe name for the RC module's QP map
static char *filterpipe0_rc = "rc_filter-0";//input pipe name for RC's raw frame
static char *filterpipe0_d_rc = filterpipe0_d;//input pipe name for RC's depth frame
static char *qppipe0_rc = "rc_map-0";//output pipe name for the RC module's QP map
static char *rc_param[] = { filterpipefmt_rc, filterpipefmt_d_rc, qpfmt_rc, bitratespipefmt };
static char *video_encoder_param[] = { filterpipefmt, filterpipefmt_rc, bitratespipefmt, qpfmt_rc };

static struct gaRect *prect = NULL;
static struct gaRect rect;

static ga_module_t *m_vsource, *m_filter, *m_vencoder, *m_asource, *m_aencoder, *m_ctrl, *m_server, *m_rc;
#ifdef	WIN32
#define	BACKSLASHDIR(fwd, back)	back
#else
#define	BACKSLASHDIR(fwd, back)	fwd
#endif
int
load_modules() {
	if((m_vsource = ga_load_module("mod/vsource-desktop", "vsource_")) == NULL)
		return -1;
	if((m_filter = ga_load_module("mod/filter-rgb2yuv", "filter_RGB2YUV_")) == NULL)
		return -1;
	char module_path[2048] = "";
	char type[64] = "";
	ga_conf_readv("video-encoder-type", type, sizeof(type));
	if(ga_conf_readbool("content-aware", 0) != 0){
		//ga_error("before rc load\n");
		snprintf(module_path, sizeof(module_path),
			BACKSLASHDIR("mod/rc", "mod\\rc"));
		if((m_rc = ga_load_module(module_path, "content_aware_rc_")) == NULL)
			return -1;		
	}
	//
	snprintf(module_path, sizeof(module_path),
		BACKSLASHDIR("mod/encoder-%s", "mod\\encoder-%s"),
		type);

	if((m_vencoder = ga_load_module(module_path, "vencoder_")) == NULL)
		return -1;
	if(ga_conf_readbool("enable-audio", 1) != 0) {
	//////////////////////////
#ifndef __APPLE__
	if((m_asource = ga_load_module("mod/asource-system", "asource_")) == NULL)
		return -1;
#endif
	if((m_aencoder = ga_load_module("mod/encoder-audio", "aencoder_")) == NULL)
		return -1;
	//////////////////////////
	}
	if((m_ctrl = ga_load_module("mod/ctrl-sdl", "sdlmsg_replay_")) == NULL)
		return -1;
	if((m_server = ga_load_module("mod/server-live555", "live_")) == NULL)
		return -1;
	return 0;
}

int
init_modules() {
	struct RTSPConf *conf = rtspconf_global();
	//static const char *filterpipe[] = { imagepipe0, filterpipe0 };
	if(conf->ctrlenable) {
		ga_init_single_module_or_quit("controller", m_ctrl, (void *) prect);
	}
	// controller server is built-in - no need to init
	// note the order of the two modules ...
	ga_init_single_module_or_quit("video-source", m_vsource, (void*) prect);
	ga_init_single_module_or_quit("filter", m_filter, (void*) filter_param);
	//
	ga_init_single_module_or_quit("video-encoder", m_vencoder, video_encoder_param);
	if(ga_conf_readbool("content-aware", 0) != 0){
		ga_init_single_module_or_quit("rc", m_rc, (void*) rc_param);
	}
	if(ga_conf_readbool("enable-audio", 1) != 0) {
	//////////////////////////
#ifndef __APPLE__
	ga_init_single_module_or_quit("audio-source", m_asource, NULL);
#endif
	ga_init_single_module_or_quit("audio-encoder", m_aencoder, NULL);
	//////////////////////////
	}
	//
	ga_init_single_module_or_quit("server-live555", m_server, NULL);
	//
	return 0;
}

int
run_modules() {
	struct RTSPConf *conf = rtspconf_global();
	static const char *filterpipe[] =  { imagepipe0, filterpipe0 };
	// controller server is built-in, but replay is a module
	if(conf->ctrlenable) {
		ga_run_single_module_or_quit("control server", ctrl_server_thread, conf);
		// XXX: safe to comment out?
		//ga_run_single_module_or_quit("control replayer", m_ctrl->threadproc, conf);
	}
	// video
	//ga_run_single_module_or_quit("image source", m_vsource->threadproc, (void*) imagepipefmt);
	if(m_vsource->start(prect) < 0)		exit(-1);
	//ga_run_single_module_or_quit("filter 0", m_filter->threadproc, (void*) filterpipe);
	if(m_filter->start(filter_param) < 0)	exit(-1);
	encoder_register_vencoder(m_vencoder, video_encoder_param);
	if(ga_conf_readbool("content-aware", 0) != 0){				
		encoder_register_rc(m_rc, rc_param);
	}
	// audio
	if(ga_conf_readbool("enable-audio", 1) != 0) {
	//////////////////////////
#ifndef __APPLE__
	//ga_run_single_module_or_quit("audio source", m_asource->threadproc, NULL);
	if(m_asource->start(NULL) < 0)		exit(-1);
#endif
	encoder_register_aencoder(m_aencoder, audio_encoder_param);
	//////////////////////////
	}
	// server
	if(m_server->start(NULL) < 0)		exit(-1);
	//
	return 0;
}

#ifdef TEST_RECONFIGURE
static void *
test_reconfig(void *) {
	int s = 0, err;
	int kbitrate[] = { 3000, 100 };
	int framerate[][2] = { { 12, 1 }, {30, 1}, {24, 1} };
	ga_error("reconfigure thread started ...\n");
	while(1) {
		ga_ioctl_reconfigure_t reconf;
		if(encoder_running() == 0) {
#ifdef WIN32
			Sleep(1);
#else
			sleep(1);
#endif
			continue;
		}
#ifdef WIN32
		Sleep(20 * 1000);
#else
		sleep(3);
#endif
		bzero(&reconf, sizeof(reconf));
		reconf.id = 0;
		reconf.bitrateKbps = kbitrate[s%2];
#if 0
		reconf.bufsize = 5 * kbitrate[s%2] / 24;
#endif
		// reconf.framerate_n = framerate[s%3][0];
		// reconf.framerate_d = framerate[s%3][1];
		// vsource
		/*
		if(m_vsource->ioctl) {
			err = m_vsource->ioctl(GA_IOCTL_RECONFIGURE, sizeof(reconf), &reconf);
			if(err < 0) {
				ga_error("reconfigure vsource failed, err = %d.\n", err);
			} else {
				ga_error("reconfigure vsource OK, framerate=%d/%d.\n",
						reconf.framerate_n, reconf.framerate_d);
			}
		}
		*/
		// encoder
		if(m_vencoder->ioctl) {
			err = m_vencoder->ioctl(GA_IOCTL_RECONFIGURE, sizeof(reconf), &reconf);
			if(err < 0) {
				ga_error("reconfigure encoder failed, err = %d.\n", err);
			} else {
				ga_error("reconfigure encoder OK, bitrate=%d; bufsize=%d; framerate=%d/%d.\n",
						reconf.bitrateKbps, reconf.bufsize,
						reconf.framerate_n, reconf.framerate_d);
			}
		}
		s = (s + 1) % 6;
	}
	return NULL;
}
#endif

void
handle_netreport(ctrlmsg_system_t *msg) {
	ctrlmsg_system_netreport_t *msgn = (ctrlmsg_system_netreport_t*) msg;
	ga_error("net-report: capacity=%.3f Kbps; loss-rate=%.2f%% (%u/%u); overhead=%.2f [%u KB received in %.3fs (%.2fKB/s)]\n",
		msgn->capacity / 1024.0,
		100.0 * msgn->pktloss / msgn->pktcount,
		msgn->pktloss, msgn->pktcount,
		1.0 * msgn->pktcount / msgn->framecount,
		msgn->bytecount / 1024,
		msgn->duration / 1000000.0,
		msgn->bytecount / 1024.0 / (msgn->duration / 1000000.0));
	return;
}

int
main(int argc, char *argv[]) {
	int notRunning = 0;
#ifdef WIN32
	if(CoInitializeEx(NULL, COINIT_MULTITHREADED) < 0) {
		fprintf(stderr, "cannot initialize COM.\n");
		return -1;
	}
	ga_set_process_dpi_aware();
#endif
	//
	if(argc < 2) {
		fprintf(stderr, "usage: %s config-file\n", argv[0]);
		return -1;
	}
	//
	if(ga_init(argv[1], NULL) < 0)	{ return -1; }
	//
	ga_openlog();
	//
	if(rtspconf_parse(rtspconf_global()) < 0)
					{ return -1; }
	//
	prect = NULL;
	//
	if(ga_crop_window(&rect, &prect) < 0) {
		return -1;
	} else if(prect == NULL) {
		ga_error("*** Crop disabled.\n");
	} else if(prect != NULL) {
		ga_error("*** Crop enabled: (%d,%d)-(%d,%d)\n", 
			prect->left, prect->top,
			prect->right, prect->bottom);
	}
	//
	if(load_modules() < 0)	 	{ return -1; }
	if(init_modules() < 0)	 	{ return -1; }
	if(run_modules() < 0)	 	{ return -1; }
	// enable handler to monitored network status
	ctrlsys_set_handler(CTRL_MSGSYS_SUBTYPE_NETREPORT, handle_netreport);
	//
#ifdef TEST_RECONFIGURE
	pthread_t t;
	pthread_create(&t, NULL, test_reconfig, NULL);
#endif
	//rtspserver_main(NULL);
	//liveserver_main(NULL);
	while(1) {
		usleep(5000000);
	}
	// alternatively, it is able to create a thread to run rtspserver_main:
	//	pthread_create(&t, NULL, rtspserver_main, NULL);
	//
	ga_deinit();
	//
	return 0;
}

