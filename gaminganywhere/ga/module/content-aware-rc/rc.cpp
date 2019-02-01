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
#include<iostream>
#include <fstream>
#include <sstream>
#include "rtspconf.h"
#include "vsource.h"
#include "encoder-common.h"
#include "ga-common.h"
#include "ga-conf.h"
#include "ga-module.h"
#include "util.h"
#include "dpipe.h"
#ifdef	WIN32
#define	BACKSLASHDIR(fwd, back)	back
#else
#define	BACKSLASHDIR(fwd, back)	fwd
#endif
#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
}
#endif

#define UPDATE_STEP 2 
#define	POOLSIZE	2
#define	LOWER_BOUND 12
#define X265_LOWRES_CU_SIZE   8
#define X265_LOWRES_CU_BITS   3
#define CU_SIZE 16 // for x265 as well as x264
#define MAX_KERNELS 3
#define MAX_BUFS 10
#define ISCALE 1
#define SW 40
#define GOP 90
#define ETA 0.9
static double K = 0;
static bool reversed;
static unsigned int upSampleRatio = CU_SIZE / 16;
static double time_begin_rc=0;
static double time_sum_rc=0;
static double time_sum_w=0;


using namespace std;

static struct RTSPConf *rtspconf = NULL;

//***SHARED AMONG ALL TECHNIQUES**
static std::vector<cl::Platform> platforms[VIDEO_SOURCE_CHANNEL_MAX];
static std::vector<cl::Device> devices[VIDEO_SOURCE_CHANNEL_MAX];	
static cl::CommandQueue queue[VIDEO_SOURCE_CHANNEL_MAX];
static cl::Context context[VIDEO_SOURCE_CHANNEL_MAX];
static float const QP_BASE = 22.0f;

//***DIFFERS PER TECHNIQUE (CAVE_ROI uses one kernel while CAVE_WEIGHTED uses three kernels)
static cl::Kernel kernel[VIDEO_SOURCE_CHANNEL_MAX][MAX_KERNELS];
static cl::Program program[VIDEO_SOURCE_CHANNEL_MAX][MAX_KERNELS];
static cl::Buffer inp_buf[VIDEO_SOURCE_CHANNEL_MAX][MAX_BUFS];
static cl::Buffer out_buf[VIDEO_SOURCE_CHANNEL_MAX][MAX_BUFS];	
static cl_int err[VIDEO_SOURCE_CHANNEL_MAX];	


static enum OBJECT_CATEGORY{
	PLAYER,
	ENEMY,
	WEAPON,
	HEALTH_PACK,
	INFORMATION_MAP
};
typedef struct ROITuple_s {
	int x;//top left x
	int y;//top left y
	int width;
	int height;
	int category;
}	ROITuple_t;//For debug purposes


static int vrc_initialized = 0;
static int vrc_started = 0;
static pthread_t vrc_tid[VIDEO_SOURCE_CHANNEL_MAX];
static bool x264_x265 = 0;//0 -> x264 , 1->x265
static bool recording = 0;

static int mode = 0;
static char type[64] = "";

//****USED BY CAVE LAMBDA
static float * bitsPerBlock[VIDEO_SOURCE_CHANNEL_MAX];
static unsigned long long int * depthSobel[VIDEO_SOURCE_CHANNEL_MAX];//32x32 aggregated depth with sobel filter applied on it in the first half of the array while the second half contains only the aggregation of the depth values on 32x32 blocks level without the application of the Sobel Filter
static double ALPHA[VIDEO_SOURCE_CHANNEL_MAX];
static double BETA[VIDEO_SOURCE_CHANNEL_MAX];
static double C1			=4.2005;
static double C2			=13.7112;
static double m_alphaUpdate = 0.1;
static double m_betaUpdate  =0.05;
static double MIN_LAMBDA = 0.1;
static double MAX_LAMBDA = 10000.0;
static double FRAME_LAMBDA [VIDEO_SOURCE_CHANNEL_MAX];
static double pixelsPerBlock = 0;
static double bitrate = 0.0f;
static double totalBitsUsed[VIDEO_SOURCE_CHANNEL_MAX];
static double bitsTotalGroup[VIDEO_SOURCE_CHANNEL_MAX] ;
static double avg_bits_per_pic[VIDEO_SOURCE_CHANNEL_MAX] ;
static double old_avg_bits_per_pic[VIDEO_SOURCE_CHANNEL_MAX] ;
static unsigned int frames[VIDEO_SOURCE_CHANNEL_MAX] ;
static double avgLambda = 0.0;
static double sumRate = 0.0;
static double totalBits =0.0;
static double lambda_buf_occup=0.0;


//****USED BY CAVE WEIGHTED
static float * depth[VIDEO_SOURCE_CHANNEL_MAX];//realWidthxrealHeight depth (input to OpenCL Kernel)
static float * depthOut[VIDEO_SOURCE_CHANNEL_MAX];//realWidthxrealHeight depth (output from OpenCL Kernel)
static unsigned int*    centers[VIDEO_SOURCE_CHANNEL_MAX];
static float*  distOut[VIDEO_SOURCE_CHANNEL_MAX];
static float filter_mean[3][3]={{1.0/12,1.0/12,1.0/12}, {1.0/12,1.0/3,1.0/12}, {1.0/12,1.0/12,1.0/12}};
static const float THETA =21570;// 7800 in paper
static const float GAMMA = 1.336 ;//0.68 in paper


//****USED BY CAVE ROI
#define ADJUSTMENT_FACTOR       0.60
#define HIGH_QSTEP_ALPHA        4.9371
#define HIGH_QSTEP_BETA         0.0922
#define LOW_QSTEP_ALPHA         16.7429
#define LOW_QSTEP_BETA          -1.1494
#define HIGH_QSTEP_THRESHOLD    9.5238
#define MIN_QP 0
#define MAX_QP 51
unsigned long long int* temp[VIDEO_SOURCE_CHANNEL_MAX];
static float BIAS_ROI = 3.0f;
static float  upper;
static float  lower;
static float m_paramLowX1=LOW_QSTEP_ALPHA;
static float m_paramLowX2=LOW_QSTEP_BETA;
static float m_paramHighX1 =HIGH_QSTEP_ALPHA;
static float m_paramHighX2= HIGH_QSTEP_BETA;
static float m_Qp;
static float m_remainingBitsInGOP;
static int m_initialOVB;
static float m_occupancyVB,m_initialTBL,m_targetBufLevel;


//****USED BY ALL 
static float importance[5] = {1.0,0.95,0.7,0.6,0.9};//based on OBJECT_CATEGORY enum
static float* ROI[VIDEO_SOURCE_CHANNEL_MAX];
static float* QP[VIDEO_SOURCE_CHANNEL_MAX];
static float* weights[VIDEO_SOURCE_CHANNEL_MAX];
static std::vector<ROITuple_t> ROIs[VIDEO_SOURCE_CHANNEL_MAX];//For debug purposes
static unsigned long long int SCALE= 10000000000000;
static float diagonal = 0.0f;
static unsigned int realWidth = 0;
static unsigned int realHeight = 0;
static unsigned int widthDelta = 0;
static unsigned int heightDelta = 0;
static unsigned int fps = 0;
static unsigned int period = 0;


//****DEBUG PURPOSES
static bool written_depth = 0;
static FILE* rois;
static FILE* qp;
static FILE* imp;
static FILE* bits;
static FILE* bits_actual;
static int round(float d)
{
    return static_cast<int>(d + 0.5);
}

/*!
 *************************************************************************************
 * \brief
 *    map QP to Qstep
 *
 *************************************************************************************
*/
double QP2Qstep( int QP )
{
  int i;
  double Qstep;
  static const double QP2QSTEP[6] = { 0.625, 0.6875, 0.8125, 0.875, 1.0, 1.125 };

  Qstep = QP2QSTEP[QP % 6];
  for( i=0; i<(QP/6); i++)
    Qstep *= 2;

  return Qstep;
}


/*!
 *************************************************************************************
 * \brief
 *    map Qstep to QP
 *
 *************************************************************************************
*/
int Qstep2QP( double Qstep, int qp_offset )
{
  int q_per = 0, q_rem = 0;

  if( Qstep < QP2Qstep(MIN_QP))
    return MIN_QP;
  else if (Qstep > QP2Qstep(MAX_QP + qp_offset) )
    return (MAX_QP + qp_offset);

  while( Qstep > QP2Qstep(5) )
  {
    Qstep /= 2.0;
    q_per++;
  }

  if (Qstep <= 0.65625)
  {
    //Qstep = 0.625;
    q_rem = 0;
  }
  else if (Qstep <= 0.75)
  {
    //Qstep = 0.6875;
    q_rem = 1;
  }
  else if (Qstep <= 0.84375)
  {
    //Qstep = 0.8125;
    q_rem = 2;
  }
  else if (Qstep <= 0.9375)
  {
    //Qstep = 0.875;
    q_rem = 3;
  }
  else if (Qstep <= 1.0625)
  {
    //Qstep = 1.0;
    q_rem = 4;
  }
  else
  {
    //Qstep = 1.125;
    q_rem = 5;
  }

  return (q_per * 6 + q_rem);
}


static float getBitsOfLastFrame(char * pipename){
	dpipe_t *srcpipe = dpipe_lookup(pipename);
	if(srcpipe == NULL){
		ga_error("bitrates %s pipe is null\n",pipename);
	}
	dpipe_buffer_t *srcdata = NULL;
	vsource_frame_t *srcframe = NULL;
	srcdata = dpipe_load(srcpipe, NULL);
	//ga_error("reading bitrate\n");
	if(srcdata == NULL){
		ga_error("bitrates data is null\n");
	}
	srcframe = (vsource_frame_t*) srcdata->pointer;	
	float bitsActualSum = *((float *)(srcframe->imgbuf));
	dpipe_put(srcpipe, srcdata);
	//ga_error("frame %d size from rc:%f\n",frames[iid],bitsActualSum);
	time_begin_rc = clock();
	bitsActualSum = bitsActualSum * 8;

	return bitsActualSum;
}

static void initCL(int iid){
	ga_error("Initialize CL\n");
	clock_t begin=clock();
	err[iid]=cl::Platform::get(&platforms[iid]);
	if(err[iid]!=CL_SUCCESS)
	{
		ga_error("Platform err:%d\n",err[iid]);
		return;
	}
	string platform_name;
	string device_type;		
	ga_error("Number of Platforms Available:%d\n",platforms[iid].size());	
	platforms[iid][0].getInfo(CL_PLATFORM_NAME,&platform_name);	
	ga_error("Platform Used:%s\n",platform_name.c_str());
	err[iid]=platforms[iid][0].getDevices(CL_DEVICE_TYPE_ALL,&devices[iid]);
	if(err[iid]!=CL_SUCCESS)
	{
		ga_error("Device err:%d\n",err[iid]);
		return;
	}	
	ga_error("Number of Devices Available:%d\n",devices[iid].size());
	err[iid]=devices[iid][0].getInfo(CL_DEVICE_NAME,&device_type);
	if(err[iid]!=CL_SUCCESS)
		ga_error("Type of device\n");
	else{		
		ga_error("Type of Device Used: %s\n",device_type.c_str());
	}
	context[iid]=cl::Context(devices[iid],NULL,NULL,NULL,&err[iid]);
	if(err[iid]!=CL_SUCCESS)
		ga_error("Context err:%d\n",err[iid]);
	queue[iid]=cl::CommandQueue(context[iid],devices[iid][0],NULL,&err[iid]);
	if(err[iid]!=CL_SUCCESS)
		ga_error("Command Queue err:%d\n",err[iid]);
	clock_t end=clock();	
	ga_error("Time Constructor: %f\n",diffclock(end,begin));	
}

std::string LoadKernel (const char* name)
{
	char srcPath[1024];	
	sprintf_s(srcPath,BACKSLASHDIR("%s/%s","%s\\%s"),getenv("GA_ROOT"),name);	
	std::ifstream in (srcPath);
	std::string result (
		(std::istreambuf_iterator<char> (in)),
		std::istreambuf_iterator<char> ());
	//cout<<result<<endl;
	int index = result.find("?w?");    
	char buf[64];
	itoa(realWidth,buf,10);
    result=result.replace(index, 3,buf);
	index = result.find("?h?");    	
	itoa(realHeight,buf,10);
    result=result.replace(index, 3,buf);
	index = result.find("?c?");    	
	itoa((int)(log((double)CU_SIZE)/log(2.0)),buf,10);
    result=result.replace(index, 3,buf);
	index = result.find("?bw?");    	
	itoa(widthDelta,buf,10);
    result=result.replace(index, 4,buf);
	index = result.find("?bh?");    	
	itoa(heightDelta,buf,10);
    result=result.replace(index, 4,buf);
	index = result.find("?s?");    	
	_ui64toa(SCALE,buf,10);
    result=result.replace(index, 3,buf);
	//ga_error(result.c_str());
	return result;
}

static void loadCL(int iid,string name,int idx,string signature){
	ga_error("Load Program\n");
	clock_t begin=clock();
	std::string src = LoadKernel(name.c_str());
	cl::Program::Sources source(1,make_pair(src.c_str(),src.size()));
	program[iid][idx]=cl::Program(context[iid],source,&err[iid]);
	if(err[iid]!=CL_SUCCESS)
		ga_error("Program err:%d\n",err[iid]);
	err[iid]=program[iid][idx].build(devices[iid]);
	if(err[iid]!=CL_SUCCESS)
		ga_error("Build Error err:%d\n",err[iid]);
	ga_error("done building program\n");
	clock_t end=clock();		
	ga_error("Time Build Program: %f\n",diffclock(end,begin));	
	ga_error("Build Status: %d\n" , program[iid][idx].getBuildInfo<CL_PROGRAM_BUILD_STATUS>(devices[iid][0]));		
	ga_error("Build Options:\t%d\n", program[iid][idx].getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(devices[iid][0]));		
	if(err[iid]!=CL_SUCCESS)		
		ga_error("Build Log:\t %s\n" , program[iid][idx].getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[iid][0]));	
	ga_error("Create Kernel\n");
	
	
	begin=clock();
	kernel[iid][idx]=cl::Kernel(program[iid][idx],signature.data(),&err[iid]);
	if(err[iid]!=CL_SUCCESS)
		ga_error("Create Kernel Error err:%d\n",err[iid]);
	end=clock();
	ga_error("Time Create Kernel %f\n",diffclock(end,begin));
}



//****************************************************************ROI Q-DOMAIN FUNCTIONS BEGIN******************************************************
static void updateBufferROIAndRunKernels(int iid)
{	
   size_t result=0;         
   clock_t begin=clock();   
   inp_buf[iid][0] = cl::Buffer::Buffer(context[iid],CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,sizeof(float)*widthDelta*heightDelta,ROI[iid],&err[iid]);
   clock_t end=clock();  
   unsigned long long int sumDistance = 0;
   if(err[iid]!=CL_SUCCESS)
	   ga_error("Inp data Transfer err: %d\n",err[iid]);
   //ga_error("Time Sending data to GPU %f\n",diffclock(end,begin));   
   memset(temp[iid],0,sizeof(unsigned long long int)*widthDelta*heightDelta);
   out_buf[iid][0] = cl::Buffer::Buffer(context[iid],CL_MEM_WRITE_ONLY|CL_MEM_COPY_HOST_PTR,sizeof(unsigned long long int)*widthDelta*heightDelta,temp[iid],&err[iid]);//output buffer needs to be zeroed out
   if(err[iid]!=CL_SUCCESS)
	   ga_error("Out data Transfer err:%d\n",err[iid]);

   out_buf[iid][1] = cl::Buffer::Buffer(context[iid],CL_MEM_WRITE_ONLY|CL_MEM_COPY_HOST_PTR,sizeof(unsigned long long int),&sumDistance,&err[iid]);//output buffer needs to be zeroed out
   if(err[iid]!=CL_SUCCESS)
	   ga_error("Out data Transfer err:%d\n",err[iid]);

	err[iid]=kernel[iid][0].setArg(0,inp_buf[iid][0]);
	if(err[iid]!=CL_SUCCESS)
		ga_error("SetKernel Arg0 err:%d\n",err[iid]);
	err[iid]=kernel[iid][0].setArg(1,out_buf[iid][1]);
	if(err[iid]!=CL_SUCCESS)
		ga_error("SetKernel Arg1 err:%d\n",err[iid]);
	err[iid]=kernel[iid][0].setArg(2,out_buf[iid][0]);
	if(err[iid]!=CL_SUCCESS)
		ga_error("SetKernel Arg2 err:%d\n",err[iid]);


	begin=clock();
	err[iid]=queue[iid].enqueueNDRangeKernel(kernel[iid][0],cl::NullRange,cl::NDRange(realWidth,realHeight),cl::NullRange);
	if(err[iid]!=CL_SUCCESS)
	{
		ga_error("Error Running Kernel:%d\n",err[iid]);		
	}

	queue[iid].finish();	
	end=clock();
	double kernel_time = diffclock(end,begin);
	//ga_error("Time Running Kernel %f\n",kernel_time);
	begin=clock();	
	err[iid]=queue[iid].enqueueReadBuffer(out_buf[iid][0],CL_TRUE,0,widthDelta*heightDelta*sizeof(unsigned long long int),temp[iid]);
	if(err[iid]!=CL_SUCCESS)
	{
		ga_error("Error Reading Data1:%d\n",err[iid]);		
	}
	queue[iid].finish();	
	err[iid]=queue[iid].enqueueReadBuffer(out_buf[iid][1],CL_TRUE,0,sizeof(unsigned long long int),&sumDistance);
	if(err[iid]!=CL_SUCCESS)
	{
		ga_error("Error Reading Data2:%d\n",err[iid]);		
	}
	queue[iid].finish();
	end=clock();
	//ga_error("sumDistance:%llu\n",sumDistance);
	float sumDistancef=sumDistance/SCALE;
	sumDistancef=sumDistancef/(realWidth*realHeight);
	unsigned int block_ind =0 ;

	for(int i=0;i<heightDelta;i++){
		for(int j=0;j<widthDelta;j++){
			weights[iid][block_ind]=temp[iid][block_ind]/SCALE;
			weights[iid][block_ind]=weights[iid][block_ind]/sumDistancef;
			weights[iid][block_ind]=max(0,min(1,weights[iid][block_ind]));
			float val =0.0;
			for(unsigned int a = 0; a < 3; a++)
			{
				for(unsigned int b = 0; b < 3; b++)
				{ 
					int jn=min(widthDelta-1,max(0,j+b-1));
					int in=min(heightDelta-1,max(0,i+a-1));
					int index=jn+in*widthDelta;
					val=val+weights[iid][index]*filter_mean[a][b];
				}
			}
			weights[iid][block_ind]=val;			
			block_ind++;					
		}
	}	

	block_ind=0;
	float sumWeights=0;
	for(int i=0;i<heightDelta;i++){
		for(int j=0;j<widthDelta;j++){
			weights[iid][block_ind]=weights[iid][block_ind]*(BIAS_ROI-1)+1;
			sumWeights=sumWeights+weights[iid][block_ind];			
			block_ind++;
		}
	}
	block_ind=0;
	float carrySum=0;
	float blocks_num = widthDelta*heightDelta;
	float secondSum = 0;
	//ga_error("sum weights %.09f\n",sumWeights);
	for(int i=0;i<heightDelta;i++){
		for(int j=0;j<widthDelta;j++){
			float temp = weights[iid][block_ind];
			weights[iid][block_ind]=weights[iid][block_ind]*(blocks_num-block_ind)/(sumWeights-carrySum);	
			secondSum = secondSum + weights[iid][block_ind];
			carrySum = carrySum + temp;			
			//ga_error("weights at %d,%d is %.09f\n",i,j,weights[iid][block_ind]);
			block_ind++;
		}
	}
}


int getQP(int qp, float targetBits, int numberOfPixels, float costPredMAD)
{
  float qStep;
  float bppPerMAD = (float)(targetBits/(numberOfPixels*costPredMAD));
  
  if(QP2Qstep(qp) >= HIGH_QSTEP_THRESHOLD)
  {
#if J0260
    qStep = 1/( sqrt((bppPerMAD/m_paramHighX1)+((m_paramHighX2*m_paramHighX2)/(4*m_paramHighX1*m_paramHighX1))) - (m_paramHighX2/(2*m_paramHighX1)));
#else
    qStep = 1/( sqrt((bppPerMAD/m_paramHighX1)+((m_paramHighX2*m_paramHighX2)/(4*m_paramHighX1*m_paramHighX1*m_paramHighX1))) - (m_paramHighX2/(2*m_paramHighX1)));
#endif
  }
  else
  {
#if J0260
    qStep = 1/( sqrt((bppPerMAD/m_paramLowX1)+((m_paramLowX2*m_paramLowX2)/(4*m_paramLowX1*m_paramLowX1))) - (m_paramLowX2/(2*m_paramLowX1)));
#else
    qStep = 1/( sqrt((bppPerMAD/m_paramLowX1)+((m_paramLowX2*m_paramLowX2)/(4*m_paramLowX1*m_paramLowX1*m_paramLowX1))) - (m_paramLowX2/(2*m_paramLowX1)));
#endif
  }
  
  return Qstep2QP(qStep,0);
}

void updatePixelBasedURQQuadraticModel (int qp, float bits, int numberOfPixels, float costMAD)
{	
  float qStep     = QP2Qstep(qp);
  float invqStep = (1/qStep);
  float paramNewX1, paramNewX2;
  
  if(qStep >= HIGH_QSTEP_THRESHOLD)
  {	
    paramNewX2    = (((bits/(numberOfPixels*costMAD))-(23.3772*invqStep*invqStep))/((1-200*invqStep)*invqStep));
    paramNewX1    = (23.3772-200*paramNewX2);	
    m_paramHighX1 = 0.70*HIGH_QSTEP_ALPHA + 0.20 * m_paramHighX1 + 0.10 * paramNewX1;
    m_paramHighX2 = 0.70*HIGH_QSTEP_BETA  + 0.20 * m_paramHighX2 + 0.10 * paramNewX2;	
  }
  else
  {
    paramNewX2   = (((bits/(numberOfPixels*costMAD))-(5.8091*invqStep*invqStep))/((1-9.5455*invqStep)*invqStep));
    paramNewX1   = (5.8091-9.5455*paramNewX2);
    m_paramLowX1 = 0.90*LOW_QSTEP_ALPHA + 0.09 * m_paramLowX1 + 0.01 * paramNewX1;
    m_paramLowX2 = 0.90*LOW_QSTEP_BETA  + 0.09 * m_paramLowX2 + 0.01 * paramNewX2;
  }
}


bool checkUpdateAvailable(int qpReference )
{ 
  float qStep = QP2Qstep(qpReference);

  if (qStep > QP2Qstep(MAX_QP) 
    ||qStep < QP2Qstep(MIN_QP) )
  {
    return false;
  }

  return true;
}

float xAdjustmentBits(int &reductionBits, int &compensationBits)
{
  float adjustment  = ADJUSTMENT_FACTOR*reductionBits;
  reductionBits     -= (int)adjustment;
  compensationBits  += (int)adjustment;

  return adjustment;
}

static void calcQPROI(char* pipename,int iid){
	if(frames[iid]==0){//first frame in first GOP gets a constant qp value
		float bpp = (bitrate/fps)/(realWidth*realHeight);
		float qp_off = 0;
		if(bpp>=0.1)
			qp_off=-2;
		else{			
			qp_off=18;
		}		
		//ga_error("offset:%.2f\n",qp_off);
		m_Qp=QP_BASE+qp_off;
		for(int i=0;i<widthDelta*heightDelta;i++)
			QP[iid][i]=qp_off;		
		return;
	}

	//H0213 and Pixel-wise URQ model for multi-level rate 	
	unsigned int frame_idx = frames[iid]%GOP;//frame indexing
	float bitsActualSum = getBitsOfLastFrame(pipename);
	int occupancyBits;
	float adjustmentBits;

	if(frame_idx==0)
		m_remainingBitsInGOP = bitrate/fps*GOP - m_occupancyVB;
	else
		m_remainingBitsInGOP = m_remainingBitsInGOP - bitsActualSum;
	occupancyBits        = (int)(bitsActualSum - (bitrate/fps));
  
	if( (occupancyBits < 0) && (m_initialOVB > 0) )
	{
	adjustmentBits = xAdjustmentBits(occupancyBits, m_initialOVB );

	if(m_initialOVB < 0)
	{
		adjustmentBits = m_initialOVB;
		occupancyBits += (int)adjustmentBits;
		m_initialOVB   =  0;
	}
	}
	else if( (occupancyBits > 0) && (m_initialOVB < 0) )
	{
	adjustmentBits = xAdjustmentBits(m_initialOVB, occupancyBits );
    
	if(occupancyBits < 0)
	{
		adjustmentBits = occupancyBits;
		m_initialOVB  += (int)adjustmentBits;
		occupancyBits  =  0;
	}
	}

	if(frames-1 == 0)
	{
	m_initialOVB = occupancyBits;
	}
	else
	{
	m_occupancyVB= m_occupancyVB + occupancyBits;
	}

	

	if(frame_idx == 0)
	{		
		//ga_error("sum rate:%.5f\n",sumRate);
		sumRate=0;
		m_initialTBL = m_targetBufLevel  = (bitsActualSum - (bitrate/fps));
	}
	else
	{		
		m_targetBufLevel =  m_targetBufLevel 
							- (m_initialTBL/(GOP-1));
	}
	sumRate=sumRate+bitsActualSum;

	if(frame_idx !=0 && checkUpdateAvailable(m_Qp))
	{
		updatePixelBasedURQQuadraticModel(m_Qp, bitsActualSum, realWidth*realHeight, 1);
	}				
	
	if(frame_idx==1){
		lower=bitrate/fps;
		upper=0.8*bitrate/fps*0.9;
	}
	else if(frame_idx>1){
		lower=lower+bitrate/fps-bitsActualSum;
		upper=upper+(bitrate/fps-bitsActualSum)*0.9;
	}
	float targetBitsOccupancy  = (bitrate/(float)fps) + 0.5*(m_targetBufLevel-m_occupancyVB - (m_initialOVB/(float)fps));
    float targetBitsLeftBudget = ((m_remainingBitsInGOP)/(GOP-frame_idx));
    
	
	float frame_bits= (int)(0.9 * targetBitsLeftBudget + 0.1 * targetBitsOccupancy);
	float frame_qp = 0;
	if (frame_bits <= 0)
		frame_qp = m_Qp+2;
	else
		frame_qp = max(m_Qp-2,min(m_Qp+2,getQP(m_Qp, frame_bits, realWidth*realHeight,5)));
	//frame_bits=min(upper,max(frame_bits,lower));
	float sum_qp = 0.0f;
	unsigned int block_ind=0;
	float blocks_num = widthDelta*heightDelta;
	//ga_error("prev frame bits:%.5f ,calc frame bits:%.5f, final ratio:%.5f\n",bitsActualSum,frame_bits,secondSum);
	//ga_error("adj:%.2f, occup:%.2f , i init:%.2f, v buf frame:%.2f, t buf frame:%.2f, rem bits:%.2f, comp:%.2f, buffer:%.2f, bits:%.2f\n",ADJUST_BITS,OCCUP_BITS,I_INIT_BITS,V_BUF_FRAME,T_BUF_FRAME,REM_BITS,t_complexity,t_buffer,frame_bits);	
	float upperBoundQP = 0;
	float lowerBoundQP = 0;
	for(int i=0;i<heightDelta;i++){
		for(int j=0;j<widthDelta;j++){
			float bits = frame_bits / blocks_num * weights[iid][block_ind];
			float final_qp;
			if (block_ind == 0)
				final_qp = frame_qp;
			else {
				if (bits < 0) {
					final_qp = QP[iid][block_ind-1]+QP_BASE+ 1;
				}
				else
					final_qp = getQP(QP[iid][block_ind] + QP_BASE, bits, pixelsPerBlock, 1);
			}
			if(block_ind==0){
				upperBoundQP=final_qp+2;
				lowerBoundQP=final_qp-2;
			}
			else{
				final_qp=max(lowerBoundQP,min(upperBoundQP,final_qp));				
				upperBoundQP=final_qp+2;
				lowerBoundQP=final_qp-2;
			}
			final_qp=max(MIN_QP,min(MAX_QP,final_qp));
			//float final_qp=round(max(first_root,second_root));			
			//ga_error("qp %d,%d is %.2f and weights is %.5f\n",i,j,final_qp,weights[iid][block_ind]);
			m_Qp = sum_qp + final_qp;
			QP[iid][block_ind]=final_qp-QP_BASE;
			/*if(j>=widthDelta/2)
				QP[iid][block_ind]=51-QP_BASE;
			else
				QP[iid][block_ind]=-QP_BASE;*/
			block_ind++;
		}
	}
	m_Qp = m_Qp/blocks_num;			
}


//****************************************************************ROI Q-DOMAIN FUNCTIONS END******************************************************


static int
vrc_deinit(void *arg) {
	int iid;
	static char *enc_param[VIDEO_SOURCE_CHANNEL_MAX][4];
	char **pipefmt = (char**) arg;
#define	MAXPARAMLEN	64
	static char pipename[VIDEO_SOURCE_CHANNEL_MAX][4][MAXPARAMLEN];						
	for(iid = 0; iid < video_source_channels(); iid++) {		
		snprintf(pipename[iid][2], MAXPARAMLEN, pipefmt[2], iid);			
		dpipe_t* qp_rc = dpipe_lookup(pipename[iid][2]);
		if(qp_rc != NULL)
			dpipe_destroy(qp_rc);				
		ROIs[iid].clear();
		platforms[iid].clear();
		devices[iid].clear();		
		if(QP[iid]!=NULL)
			free(QP[iid]);
		if(bitsPerBlock[iid]!=NULL)
			free(bitsPerBlock[iid]);
		if(centers[iid]!=NULL)
			free(centers[iid]);
		if(weights[iid]!=NULL)
			free(weights[iid]);
		if(depth[iid]!=NULL)
			free(depth[iid]);
		if(ROI[iid]!=NULL)
			free(ROI[iid]);
		if(depthSobel[iid]!=NULL)
			free(depthSobel[iid]);
		if(depthOut[iid]!=NULL)
			free(depthOut[iid]);
		if(distOut[iid]!=NULL)
			free(distOut[iid]);
		if(temp[iid]!=NULL)
			free(temp[iid]);
	}
	/*fclose(rois);
	fclose(qp);
	fclose(imp);
	fclose(bits);
	fclose(bits_actual);*/
	//fclose(depthSobelf_file);	
	vrc_initialized = 0;
	ga_error("rc: deinitialized.\n");
	return 0;
}

static int
vrc_init(void *arg) {
	ga_error("Inside RC init:%d !!\n",vrc_initialized);
	if(vrc_initialized != 0)
		return 0;	
	const char ** filterpipe = (const char **) arg;
	dpipe_t *srcpipe[VIDEO_SOURCE_CHANNEL_MAX];
	dpipe_t *srcpipe_bitrates[VIDEO_SOURCE_CHANNEL_MAX];
	dpipe_t *srcpipe_d[VIDEO_SOURCE_CHANNEL_MAX];
	dpipe_t *dstpipe[VIDEO_SOURCE_CHANNEL_MAX];	
	bzero(dstpipe, sizeof(dstpipe));
	// arg is image source id
	int iid;		
		
	//General
	mode = ga_conf_readint("mode");		
	fps = period =ga_conf_readint("video-fps");	
	bitrate = ga_conf_mapreadint("video-specific", "b");
	recording = ga_conf_readbool("recording", 0);
	realWidth =   video_source_curr_width(0);
	realHeight =  video_source_curr_height(0);		
	diagonal = (float)sqrt(pow(realWidth * 1.0, 2) + pow(realHeight * 1.0, 2));	
	heightDelta = (((realHeight / 2) + X265_LOWRES_CU_SIZE - 1) >> X265_LOWRES_CU_BITS);//will get the number of 16X16 blocks in the height direction for x265
	widthDelta = (((realWidth / 2) + X265_LOWRES_CU_SIZE - 1) >> X265_LOWRES_CU_BITS);	
	pixelsPerBlock = (float)(CU_SIZE * CU_SIZE);
	time_sum_w = 0;
	time_sum_rc = 0;

	//ROI		
	m_paramLowX1=LOW_QSTEP_ALPHA;
	m_paramLowX2=LOW_QSTEP_BETA;
	m_paramHighX1 =HIGH_QSTEP_ALPHA;
	m_paramHighX2= HIGH_QSTEP_BETA;
	m_remainingBitsInGOP = bitrate;
	sumRate = 0.0f;
	m_initialOVB = 0;
	m_occupancyVB=0;
	m_initialTBL=0;
	m_targetBufLevel=0;
	m_Qp=0;
	

	//Lambda-domain From Latest HM
	avgLambda = 0.0f;
	lambda_buf_occup = 0.5 * bitrate / fps;
	float m_seqTargetBpp = bitrate / (period * realWidth * realHeight);
	/*if ( m_seqTargetBpp < 0.03 )
	{*/
	m_alphaUpdate = 0.01;
	m_betaUpdate  = 0.005;
	/*}
	else if ( m_seqTargetBpp < 0.08 )
	{
	m_alphaUpdate = 0.05;
	m_betaUpdate  = 0.025;
	}
	else if ( m_seqTargetBpp < 0.2 )
	{
	m_alphaUpdate = 0.1;
	m_betaUpdate  = 0.05;
	}
	else if ( m_seqTargetBpp < 0.5 )
	{
	m_alphaUpdate = 0.2;
	m_betaUpdate  = 0.1;
	}
	else
	{
	m_alphaUpdate = 0.4;
	m_betaUpdate  = 0.2;
	}*/

				
	for(iid = 0; iid < video_source_channels(); iid++) {
		//LAMBDA-DOMAIN
		totalBitsUsed[iid] = 0.0;
		//bitsTotalGroup[iid] = max(200.0f, (((1.0f * bitrate / fps) - 0.5 * lambda_buf_occup/fps) * GOP));
		bitsTotalGroup[iid] = max(200.0f, (((1.0f * bitrate / fps)*(frames[iid] + SW) - totalBits)*GOP) / SW);
		ALPHA[iid] = 3.2003f;
		BETA[iid] = -1.367f;			
		avg_bits_per_pic[iid] = (float)(bitrate / (1.0f * (float)fps));
		old_avg_bits_per_pic[iid] = avg_bits_per_pic[iid];		
		frames[iid] = 0;			
		
		char srcpipename[64], srcpipename_d[64], srcpipename_bitrates[64], dstpipename[64];
		
		dpipe_buffer_t *data = NULL;
		//
		
		snprintf(srcpipename, sizeof(srcpipename), filterpipe[0], iid);
		snprintf(srcpipename_d, sizeof(srcpipename_d), filterpipe[1], iid);	
		snprintf(dstpipename, sizeof(dstpipename), filterpipe[2], iid);
		snprintf(srcpipename_bitrates, sizeof(srcpipename_bitrates), filterpipe[3], iid);	


		srcpipe[iid] = dpipe_lookup(srcpipename);
		srcpipe_d[iid] = dpipe_lookup(srcpipename_d);
		srcpipe_bitrates[iid] = dpipe_lookup(srcpipename_bitrates);
		if(srcpipe[iid] == NULL) {
			ga_error("RC: cannot find pipe %s\n", srcpipename);
			goto init_failed;
		}

		/*if(srcpipe_d[iid] == NULL) {
			ga_error("RC: cannot find pipe %s\n", srcpipename_d);
			goto init_failed;
		}*/

		if(srcpipe_bitrates[iid] == NULL) {
			ga_error("RC: cannot find pipe %s\n", srcpipe_bitrates);
			goto init_failed;
		}
		
		QP[iid] = (float *) calloc(pow(upSampleRatio,2.0f)*widthDelta * heightDelta,sizeof(float));
		bitsPerBlock[iid] = (float *)calloc(heightDelta * widthDelta, sizeof(float));
		weights[iid] = (float *)calloc(heightDelta * widthDelta, sizeof(float));
		/*depth[iid] = (float *)calloc(realWidth * realHeight, sizeof(float));		
		depthOut[iid] = (float *)calloc(realWidth*realHeight, sizeof(float));*/
		distOut[iid]=(float*)calloc(realWidth*realHeight,sizeof(float));		
		ROI[iid] = (float *)calloc(heightDelta * widthDelta, sizeof(float)); //predict QPs using complexity of depth
		temp[iid] = (unsigned long long int *)calloc(widthDelta*heightDelta,sizeof(unsigned long long int));
		if(mode==ROI_)
			initCL(iid);
		if(mode==ROI_){			
			loadCL(iid,"R_Q_DISTANCE.cl",0,"ROI");
		}
		
		std::ifstream infile("roi0.txt");
		int category;
		double x,y,w,h;		
		while(infile >> category >> x >> y >> w >> h){
			ROITuple_s r;
			r.x= (x-w/2)*realWidth;
			r.y= (y-h/2)*realHeight;
			r.width= (w)*realWidth;
			r.height= (h)*realHeight;
			r.category = category;
			ROIs[iid].push_back(r);				
		}
		infile.close();	
		
		
		

		
		
		//
		if(dpipe_lookup(dstpipename)==NULL){
			dstpipe[iid] = dpipe_create(iid, dstpipename, POOLSIZE,
					sizeof(vsource_frame_t) + video_source_mem_size(iid));
			if(dstpipe[iid] == NULL) {
				ga_error("RC: create dst-pipeline failed (%s).\n", dstpipename);
				goto init_failed;
			}
			for(data = dstpipe[iid]->in; data != NULL; data = data->next) {
				if(vsource_frame_init(iid, (vsource_frame_t*) data->pointer) == NULL) {
					ga_error("RC: init frame failed for %s.\n", dstpipename);
					goto init_failed;
				}
			}
			video_source_add_pipename(iid, dstpipename);
		}
	}
	//
	vrc_initialized = 1;
	//
	return 0;
init_failed:
	for(iid = 0; iid < video_source_channels(); iid++) {
		if(dstpipe[iid] != NULL)
			dpipe_destroy(dstpipe[iid]);
		dstpipe[iid] = NULL;
	}
	vrc_deinit(NULL);
	
#if 0
	if(pipe) {
		delete pipe;
	}
#endif
	return -1;
	//		
		
}

static void meanFilter(int iid,int sumDistance) {
	int block_ind = 0;
	float sumWeights = 0;
	for (int i = 0;i<heightDelta;i++) {
		for (int j = 0;j<widthDelta;j++) {
			float val = 0.0;
			for (unsigned int a = 0; a < 3; a++)
			{
				for (unsigned int b = 0; b < 3; b++)
				{
					int jn = min(widthDelta - 1, max(0, j + b - 1));
					int in = min(heightDelta - 1, max(0, i + a - 1));
					int index = jn + in * widthDelta;
					val = val + 1.0f* (weights[iid][index]/sumDistance) * filter_mean[a][b];
				}
			}
			weights[iid][block_ind] = val;
			sumWeights = sumWeights + val;
			block_ind++;
		}
	}
}

//run on CPU as in our calculations we don't need to process every pixel	
static void updateDistanceCAVELambda(int iid){
	float sumDistance = 0.0f;
	//float * tempDist = (float *)calloc(widthDelta*heightDelta,sizeof(float));
	unsigned int block_ind = 0;

	for (unsigned int x = 0;x < heightDelta;x++) {
		for (unsigned int y = 0;y < widthDelta;y++) {
			
			if (ROI[iid][block_ind]>0.0) {
				float e = 1/(K*ROI[iid][block_ind]);
				weights[iid][block_ind] = exp(-1.0*e);				
			}
			else {
				float sumDist = 0.0f;
				int xpixIndex = x * CU_SIZE + CU_SIZE / 2;
				int ypixIndex = y * CU_SIZE + CU_SIZE / 2;
				for (unsigned int r = 0;r<ROIs[iid].size();r++) {
					int xMid = ROIs[iid][r].x + ROIs[iid][r].width / 2;
					int yMid = ROIs[iid][r].y + ROIs[iid][r].height / 2;
					float dist = max(1.0f, (float)sqrt(pow(xpixIndex - yMid, 2.0) + pow(ypixIndex - xMid, 2.0)));					
					float e = (K*dist/ diagonal) / (importance[ROIs[iid][r].category]) ;
					sumDist = sumDist + exp(-1.0*e);
				}

				if (ROIs[iid].size() > 0)//ROI and CAVE
					weights[iid][block_ind] = sumDist / ROIs[iid].size();
			}
			sumDistance = sumDistance + weights[iid][block_ind];
			block_ind++;
		}	
	}
	

	meanFilter(iid,sumDistance);

}


//This function should load the raw frame, the user input and try to predict the ROI (e.g. using a neural network model that was trained on different ROIs)
//for now assume the ROI is in the middle of the screen
static void predictROIs(char * pipename,int iid){
	//load the frame but won't be used for now
	//ga_error("copying raw frame\n");
	/*dpipe_t *srcpipe = dpipe_lookup(pipename);
	dpipe_buffer_t *srcdata = NULL;
	vsource_frame_t *srcframe = NULL;
	srcdata = dpipe_load(srcpipe, NULL);
	srcframe = (vsource_frame_t*) srcdata->pointer;
	dpipe_put(srcpipe, srcdata);*/
	//ga_error("copied raw frame\n");	
	if(frames[iid]%UPDATE_STEP != 0)
		return;
	memset(ROI[iid],0,widthDelta *heightDelta * sizeof(float));
	if(mode == ROI_){//simulate blue masks in the paper 45% is high importance and 30% is medium importance
		for(unsigned int r=0;r<ROIs[iid].size();r++){
			unsigned int xTop=ROIs[iid][r].x;
			unsigned int yTop=ROIs[iid][r].y;
			unsigned int xBottom=xTop + ROIs[iid][r].width;
			unsigned int yBottom=yTop + ROIs[iid][r].height;
			xTop = xTop / CU_SIZE;
			yTop = yTop / CU_SIZE;
			xBottom = xBottom / CU_SIZE;
			yBottom = yBottom / CU_SIZE; 		
			for (int j = yTop; j <= yBottom; j++)
			{
				for (int k = xTop; k <= xBottom; k++)
				{				
					if(ROIs[iid][r].category==PLAYER)
						ROI[iid][k+j*widthDelta]=0.45;				
					else if(ROIs[iid][r].category==ENEMY)
						ROI[iid][k+j*widthDelta]=0.3;	
					else if (ROIs[iid][r].category == INFORMATION_MAP)
						ROI[iid][k+j*widthDelta] = 0.15;
				}			
			}
		}
		updateBufferROIAndRunKernels(iid);
	}
	
	else if(mode == CAVE_R){		
		for(unsigned int r=0;r<ROIs[iid].size();r++){
			unsigned int xTop=ROIs[iid][r].x;
			unsigned int yTop=ROIs[iid][r].y;
			unsigned int xBottom=xTop + ROIs[iid][r].width;
			unsigned int yBottom=yTop + ROIs[iid][r].height;
			xTop = xTop / CU_SIZE;
			yTop = yTop / CU_SIZE;
			xBottom = xBottom / CU_SIZE;
			yBottom = yBottom / CU_SIZE; 		
			for (int j = yTop; j <= yBottom; j++)
			{
				for (int k = xTop; k <= xBottom; k++)
				{					
						ROI[iid][k+j*widthDelta]=importance[ROIs[iid][r].category];				
				}			
			}
		}
		updateDistanceCAVELambda(iid);
	}	
	//fwrite(ROI[iid],sizeof(bool),widthDelta*heightDelta,rois);
	//ga_error("finished roi assignment\n");
}



//****************************************************************LAMBDA DOMAIN FUNCTIONS BEGIN******************************************************
int compare(const void* elem1, const void* elem2)
{
	if (*(const float*)elem1 < *(const float*)elem2)
		return -1;
	return *(const float*)elem1 > *(const float*)elem2;
}





static float CLIP(double min, double max, double value)
{
	return max((min), min((max), value));
}

static void clipLambda(double * lambda)
{
	if (_isnan(*lambda))
	{
		*lambda = MAX_LAMBDA;
	}
	else
	{
		*lambda = CLIP(MIN_LAMBDA, MAX_LAMBDA, (*lambda));
	}
}

static double QPToBits(int iid,int QP)
{
	double lambda = exp((QP - C2) / C1);
	double bpp = (double)pow((lambda / ALPHA[iid])*1.0f, 1.0f / BETA[iid]);
	return bpp * (double)pow(64.0f, 2.0f);
}

static int LAMBDA_TO_QP(double lambda)
{
    return (int)CLIP(0.0f, 51.0f, (double)round(C1 * log(lambda) + C2));
}

static void updateParameters(double bits, double pixelsPerBlock, double lambdaReal, double * alpha,  double * beta)
{
	double bpp = bits / pixelsPerBlock;
	double lambdaComp = (*alpha) * pow(bpp, *beta);
	clipLambda(&lambdaComp); //in kvazaar but not in Lambda Domain Paper but to avoid 0 bits per pixel for a block
	float logRatio = log(lambdaReal) - log(lambdaComp);	
	//positive ratio if lambda real (which was my target) is bigger than the actually computed lambda using the real bpp which means 
	//that the encoder exceeded the target number of bits so this causes that alpha should be increased
	//ga_error("old alpha:%.5f beta:%.5f,",*alpha,*beta);
	*alpha = *alpha + m_alphaUpdate * (logRatio) * (*alpha);
	*alpha = CLIP(0.05f, 20.0f, *alpha); //in kvazaar but not in Lambda Domain Paper
	*beta = *beta + m_betaUpdate * (logRatio) * CLIP(-5.0f, -1.0f, log(bpp)); //in kvazaar but not in Lambda Domain Paper        
	*beta = CLIP(-3.0f, -0.1f, *beta); //in kvazaar but not in Lambda Domain Paper
	//ga_error("lambda comp:%.5f lambda real:%.5f log ratio:%.5f alpha:%.5f beta:%.5f\n",lambdaComp,lambdaReal,logRatio,*alpha,*beta);
}

static void updateParameters(char * pipename,int iid)
{	

	float bitsActualSum = getBitsOfLastFrame(pipename);
	sumRate = sumRate + bitsActualSum;
	totalBits = totalBits + bitsActualSum;
	//ga_error("frame %d size from rc:%f, ALPHA:%.5f BETA:%.5f\n",frames[iid],bitsActualSum,ALPHA[iid],BETA[iid]);
	if (frames[iid] % GOP == 0 && frames[iid] > 0)
	{
			//ga_error("Sum rate :%.5f\n",sumRate);
			sumRate = 0.0f;
	}
	//return ;
	//fwrite(&bitsActualSum,sizeof(float),1,bits_actual);
	{
		totalBitsUsed[iid] = totalBitsUsed[iid] + bitsActualSum;
		if (frames[iid] % GOP == 0 && frames[iid] > 0)
		{			
			totalBitsUsed[iid] = 0.0f;
			//bitsTotalGroup[iid] = max(200.0f,(((1.0f * bitrate / fps)*(frames[iid] + SW) - totalBits)*GOP) / SW);
			bitsTotalGroup[iid] = max(200.0f, (((1.0f * bitrate / fps) - 0.5 * lambda_buf_occup/fps) * GOP));
			//ALPHA[iid] = 3.2003f;
			//BETA[iid] = -1.367f;
		}
		else {
			lambda_buf_occup = lambda_buf_occup + bitsActualSum - bitrate / fps;
		}
		double remaining = bitsTotalGroup[iid] - totalBitsUsed[iid];				
		double buf_status = min(0,1.0*((0.5*bitrate / fps) - lambda_buf_occup))+ bitrate / fps;
		double next_frame_bits = min(bitrate/fps,remaining / (GOP - (frames[iid] % GOP)));
		//double next_frame_bits = remaining / (GOP - (frames[iid] % GOP));
		//avg_bits_per_pic[iid] = max(100.0,next_frame_bits);
		
		avg_bits_per_pic[iid] = max(100.0,ETA*next_frame_bits+(1-ETA)*buf_status);
		
		double bitsTarget = (frames[iid] % GOP == 0) ? avg_bits_per_pic[iid] * ISCALE : avg_bits_per_pic[iid];
		double targetbppFrame = (1.0f * bitsTarget) / (1.0f * realWidth * realHeight);

		//update ALPHA,BETA only if number of bits exceeds the maximum (penalizing subsequent frames to use less bits) 
		//or tell the next frames to use more bits just in case that the remaining number of bits is enough to encode the rest of the frames		
		//if ((frames[iid] - 1) % period == 0 && bitsActualSum > ISCALE * old_avg_bits_per_pic[iid] || (frames[iid] - 1) % period != 0 && bitsActualSum > old_avg_bits_per_pic[iid])
		//{
			updateParameters(bitsActualSum, (double)(realWidth * realHeight), FRAME_LAMBDA[iid], &ALPHA[iid],  &BETA[iid]);
		//}

		FRAME_LAMBDA[iid] = ALPHA[iid] * pow(targetbppFrame, BETA[iid]);
		//if((frames[iid]) % period != 0 && (frames[iid]-1) % period != 0)
		//FRAME_LAMBDA[iid] = CLIP(avgLambda * 0.6299605249474366f, avgLambda * 1.5874010519681994f, FRAME_LAMBDA[iid]);// In lambda domain paper
		clipLambda(&FRAME_LAMBDA[iid]);
	}
}






static void calcQPLambda(char * pipename,int iid)
{
	//ga_error("calcQPLambda \n");
	if (frames[iid] == 0)
	{
		double targetbppFrame = (1.0f * avg_bits_per_pic[iid] * (ISCALE)) / (1.0f * realWidth * realHeight);
		FRAME_LAMBDA[iid] = ALPHA[iid] * pow(targetbppFrame, BETA[iid]);
		clipLambda(&FRAME_LAMBDA[iid]);
	}
	else
		updateParameters(pipename,iid);//returns the actual number of bits used for each block in the previous frame

	double bitsAlloc = avg_bits_per_pic[iid];
	if (frames[iid] % GOP == 0)
		bitsAlloc = bitsAlloc * ISCALE;
	
	double qp_frame = LAMBDA_TO_QP(ALPHA[iid]*pow(bitsAlloc / (realWidth*realHeight),BETA[iid]));
	double extra = 0.0;
	double sumSaliencyImportantAreas = 0.0;
	double sumSaliency = 0.0;
	int reassigned = 0;
	double qp_delta = 0.0;
	avgLambda = 0.0f;
	double sum_non_roi = 0;
	//ga_error("before for loop \n");
    unsigned int block_ind=0;	
	for (unsigned int i = block_ind; i < widthDelta*heightDelta; i++)
	{		
			sumSaliency = sumSaliency + weights[iid][block_ind];
			double assigned = max(1.0f,1.0f/(1.0f*widthDelta*heightDelta) * bitsAlloc);
			double assignedW =  max(1.0f,weights[iid][block_ind] * bitsAlloc);
                        
			double targetbpp=0.0f;
            /*if(mode==BASE)            
				targetbpp = (assigned) / (pixelsPerBlock);
			else*/
				targetbpp = (assignedW) / (pixelsPerBlock);
			double lambdaConst = ALPHA[iid] * pow(targetbpp, BETA[iid]);//Kiana's idea of using the updated ALPHA/BETA for the whole frame 						
			//lambdaConst = CLIP(FRAME_LAMBDA[iid] * 0.6299605249474366f, FRAME_LAMBDA[iid] * 1.5874010519681994f, lambdaConst);// In lambda domain paper						
			avgLambda = avgLambda+log(lambdaConst); 

			/*if(j>widthDelta/2)
				QP[iid][block_ind]=51-QP_BASE;
			else
				QP[iid][block_ind]=-QP_BASE;*/
			double temp = (double)LAMBDA_TO_QP(lambdaConst);
			if (mode == BASE) {
				temp = qp_frame;				
			}
			qp_delta = qp_delta + temp - qp_frame;			
			QP[iid][block_ind] = temp -QP_BASE;		
			//ga_error("qp,bits %d,%d is %.5f,%.5f,%.5f\n",i,j,QP[iid][block_ind]+QP_BASE,assigned,assignedW);
			bitsPerBlock[iid][block_ind] = min(QPToBits(iid,(int)QP[iid][block_ind]),(double)assigned);
			
			block_ind++;		
		//writetext2.Write("\n");
	}
	if (qp_delta < 0) {
		qp_delta = -1 * qp_delta;
		block_ind = 0;
		if (mode == CAVE_R){	
			while (qp_delta > 0) {
				for (block_ind = 0;block_ind < widthDelta*heightDelta && qp_delta>0;block_ind++) {
					if (ROI[iid][block_ind] == 0)
						QP[iid][block_ind] = QP[iid][block_ind] + 1;
					qp_delta = qp_delta - 1;
				}
			}
		}
	}
	//ga_error("after for loop \n");
		
	avgLambda = exp(avgLambda / (widthDelta*heightDelta));
	old_avg_bits_per_pic[iid] = avg_bits_per_pic[iid];    
	//ga_error("exiting QPLambda\n");
}
//****************************************************************LAMBDA DOMAIN FUNCTIONS END******************************************************







static void upSample(float * qps, float * dst){
	if (upSampleRatio == 1) {
		memcpy(dst, qps, sizeof(float)*widthDelta*heightDelta);
		return;
	}
	unsigned int block_ind =0;
	for(unsigned int i=0,iSmall=0;i<heightDelta;i++,iSmall=iSmall+ upSampleRatio){
		for(unsigned int j=0,jSmall=0;j<widthDelta;j++,jSmall=jSmall+ upSampleRatio){
			float val=qps[block_ind];
			dst[jSmall+iSmall*widthDelta*upSampleRatio]=dst[jSmall+iSmall*widthDelta*upSampleRatio +1]=dst[jSmall+(iSmall+1)*widthDelta*upSampleRatio]=dst[jSmall+(iSmall+1)*widthDelta*upSampleRatio +1]=val;
			block_ind++;
		}
	}
	/*fwrite(dst,sizeof(float),4*widthDelta*heightDelta,qp);*/
}

static void *
vrc_base_threadproc(void *arg) {
	// arg is pointer to source pipename
	ga_error("rc thread started\n");
	int iid, outputW, outputH;
	vsource_frame_t *frame = NULL;
	char **filterpipe = (char**) arg;
	dpipe_t *pipe = dpipe_lookup(filterpipe[0]);
	//dpipe_t *pipe_d = dpipe_lookup(filterpipe[1]);
	dpipe_t *dstpipe = dpipe_lookup(filterpipe[2]);
	dpipe_t *pipe_bitrates = dpipe_lookup(filterpipe[3]);
	
	dpipe_buffer_t *srcdata = NULL;
	dpipe_buffer_t *srcdata_bitrates = NULL;
	dpipe_buffer_t *srcdata_d = NULL;
	dpipe_buffer_t *dstdata = NULL;
	
	vsource_frame_t *srcframe = NULL;
	vsource_frame_t *srcframe_bitrates = NULL;
	vsource_frame_t *srcframe_d = NULL;
	vsource_frame_t *dstframe = NULL;
	
	//if(pipe == NULL || pipe_d == NULL || dstpipe == NULL || pipe_bitrates == NULL) {
	if(pipe == NULL || dstpipe == NULL || pipe_bitrates == NULL) {
		ga_error("RC: bad pipeline (src=%p; src_d=%p; dst=%p; src_bitrates=%p;).\n", pipe,  dstpipe, pipe_bitrates);
		goto rc_quit;
	}

	rtspconf = rtspconf_global();
	iid = dstpipe->channel_id;
	outputW = video_source_out_width(iid);
	outputH = video_source_out_height(iid);

	ga_error("rc started: tid=%ld %dx%d@%dfps.\n",
		ga_gettid(),
		outputW, outputH, rtspconf->video_fps);
	

	while(vrc_started != 0 && encoder_running() > 0) {						
		dstdata = dpipe_get(dstpipe);		
		dstframe = (vsource_frame_t*) dstdata->pointer;
		// basic info
		dstframe->imgpts = frames[iid];
		gettimeofday(&dstframe->timestamp, NULL);
		dstframe->pixelformat = AV_PIX_FMT_RGBA;	//yuv420p;
		dstframe->realwidth = upSampleRatio*widthDelta;
		dstframe->realheight = upSampleRatio*heightDelta;
		dstframe->realstride = upSampleRatio*widthDelta;
		dstframe->realsize = pow(upSampleRatio,2.0)*widthDelta * heightDelta;				
		
		clock_t begin=clock();
		calcQPLambda(filterpipe[3],iid);//loads bitrate of latest frame
		clock_t end=clock();
		frames[iid]++;
		upSample(QP[iid],(float*)dstframe->imgbuf);
		dpipe_store(dstpipe,dstdata);
		
		if(frames[iid]%TIME_REPORT==0){
			/*ga_error("average run BASE %d is %.3f ms\n",frames[iid]/TIME_REPORT,time_sum/TIME_REPORT);
			time_sum=0;*/
		}
		//time_sum=time_sum+diffclock(end,begin);
		//ga_error("run for frame %d is: %f\n",frames[iid]-1,diffclock(end,begin));
	}
	//
rc_quit:
	if(pipe) {
		pipe = NULL;
	}
	/*if(pipe_d) {
		pipe_d = NULL;
	}*/
	if(pipe_bitrates) {
		pipe_bitrates = NULL;
	}	
	//	
	ga_error("RC: thread terminated.\n");
	//
	return NULL;
}



static void *
vrc_cave_roi_lambda_threadproc(void *arg) {
	// arg is pointer to source pipename
	ga_error("rc thread started\n");
	int iid, outputW, outputH;
	vsource_frame_t *frame = NULL;
	char **filterpipe = (char**) arg;
	dpipe_t *pipe = dpipe_lookup(filterpipe[0]);
	//dpipe_t *pipe_d = dpipe_lookup(filterpipe[1]);
	dpipe_t *dstpipe = dpipe_lookup(filterpipe[2]);
	dpipe_t *pipe_bitrates = dpipe_lookup(filterpipe[3]);
	
	dpipe_buffer_t *srcdata = NULL;
	dpipe_buffer_t *srcdata_bitrates = NULL;
	dpipe_buffer_t *srcdata_d = NULL;
	dpipe_buffer_t *dstdata = NULL;
	
	vsource_frame_t *srcframe = NULL;
	vsource_frame_t *srcframe_bitrates = NULL;
	vsource_frame_t *srcframe_d = NULL;
	vsource_frame_t *dstframe = NULL;
	
	//if(pipe == NULL || pipe_d == NULL || dstpipe == NULL || pipe_bitrates == NULL) {
	if(pipe == NULL ||  dstpipe == NULL || pipe_bitrates == NULL) {
		ga_error("RC: bad pipeline (src=%p; src_d=%p; dst=%p; src_bitrates=%p;).\n", pipe,  dstpipe, pipe_bitrates);
		goto rc_quit;
	}

	rtspconf = rtspconf_global();
	iid = dstpipe->channel_id;
	outputW = video_source_out_width(iid);
	outputH = video_source_out_height(iid);

	ga_error("rc started: tid=%ld %dx%d@%dfps.\n",
		ga_gettid(),
		outputW, outputH, rtspconf->video_fps);
	

	while(vrc_started != 0 && encoder_running() > 0) {						
		dstdata = dpipe_get(dstpipe);		
		dstframe = (vsource_frame_t*) dstdata->pointer;
		// basic info
		dstframe->imgpts = frames[iid];
		gettimeofday(&dstframe->timestamp, NULL);
		dstframe->pixelformat = AV_PIX_FMT_RGBA;	//yuv420p;
		dstframe->realwidth = upSampleRatio*widthDelta;
		dstframe->realheight = upSampleRatio*heightDelta;
		dstframe->realstride = upSampleRatio*widthDelta;
		dstframe->realsize = pow(upSampleRatio,2.0)*widthDelta * heightDelta;				
		
		clock_t begin_w=clock();
		predictROIs(filterpipe[0],iid);
		clock_t end_w=clock();//WEIGHT TIME -------> CAVE WEIGHTS
		//clock_t begin_rc=clock();
		calcQPLambda(filterpipe[3],iid);//loads bitrate of latest frame
		clock_t end_rc=clock();//ENCODE + RC TIME ------> BASE
		frames[iid]++;
		upSample(QP[iid],(float*)dstframe->imgbuf);
		dpipe_store(dstpipe,dstdata);
		
		if(frames[iid]%TIME_REPORT==0){
			time_sum_w=time_sum_w/TIME_REPORT;
			time_sum_rc=time_sum_rc/TIME_REPORT;
			ga_error("average run overhead for CAVE %d is %.3f, weights is %.3f ms, rc is %.3f ms\n",frames[iid]/TIME_REPORT,time_sum_w/time_sum_rc,time_sum_w,time_sum_rc);
			time_sum_rc=0;
			time_sum_w=0;
		}
		time_sum_rc=time_sum_rc+diffclock(end_rc,time_begin_rc);
		time_sum_w=time_sum_w+diffclock(end_w,begin_w);
		//ga_error("run for frame %d is: %f\n",frames[iid]-1,diffclock(end,begin));
	}
	//
rc_quit:
	if(pipe) {
		pipe = NULL;
	}
	/*if(pipe_d) {
		pipe_d = NULL;
	}*/
	if(pipe_bitrates) {
		pipe_bitrates = NULL;
	}	
	//	
	ga_error("RC: thread terminated.\n");
	//
	return NULL;
}









static void *
vrc_roi_q_threadproc(void *arg) {
	// arg is pointer to source pipename
	ga_error("rc thread started\n");
	int iid, outputW, outputH;
	vsource_frame_t *frame = NULL;
	char **filterpipe = (char**) arg;
	dpipe_t *pipe = dpipe_lookup(filterpipe[0]);
	dpipe_t *pipe_d = dpipe_lookup(filterpipe[1]);
	dpipe_t *dstpipe = dpipe_lookup(filterpipe[2]);
	dpipe_t *pipe_bitrates = dpipe_lookup(filterpipe[3]);
	
	dpipe_buffer_t *srcdata = NULL;
	dpipe_buffer_t *srcdata_bitrates = NULL;
	dpipe_buffer_t *srcdata_d = NULL;
	dpipe_buffer_t *dstdata = NULL;
	
	vsource_frame_t *srcframe = NULL;
	vsource_frame_t *srcframe_bitrates = NULL;
	vsource_frame_t *srcframe_d = NULL;
	vsource_frame_t *dstframe = NULL;
	
	if(pipe == NULL || pipe_d == NULL || dstpipe == NULL || pipe_bitrates == NULL) {
		ga_error("RC: bad pipeline (src=%p; src_d=%p; dst=%p; src_bitrates=%p;).\n", pipe, pipe_d, dstpipe, pipe_bitrates);
		goto rc_quit;
	}

	rtspconf = rtspconf_global();
	iid = dstpipe->channel_id;
	outputW = video_source_out_width(iid);
	outputH = video_source_out_height(iid);

	ga_error("rc started: tid=%ld %dx%d@%dfps.\n",
		ga_gettid(),
		outputW, outputH, rtspconf->video_fps);
	while(vrc_started != 0 && encoder_running() > 0) {						
		dstdata = dpipe_get(dstpipe);		
		dstframe = (vsource_frame_t*) dstdata->pointer;
		// basic info
		dstframe->imgpts = frames[iid];
		gettimeofday(&dstframe->timestamp, NULL);
		dstframe->pixelformat = AV_PIX_FMT_RGBA;	//yuv420p;
		dstframe->realwidth = upSampleRatio*widthDelta;
		dstframe->realheight = upSampleRatio*heightDelta;
		dstframe->realstride = upSampleRatio*widthDelta;
		dstframe->realsize = pow(upSampleRatio,2.0)*widthDelta * heightDelta;			
		//ga_error("before depth update\n");		
		clock_t begin=clock();
		predictROIs(filterpipe[0],iid);				
		calcQPROI(filterpipe[3],iid);//loads bitrate of latest frame
		clock_t end=clock();
		frames[iid]++;
		upSample(QP[iid],(float*)dstframe->imgbuf);
		//CopyMemory(dstframe->imgbuf,QP[iid],widthDelta * heightDelta * 4);
		//dstframe->imgbuf = (unsigned char *)QP[iid];
		dpipe_store(dstpipe,dstdata);
		if(frames[iid]%TIME_REPORT==0){
			/*ga_error("average run RQ(R) %d is %.3f ms\n",frames[iid]/TIME_REPORT,time_sum/TIME_REPORT);
			time_sum=0;*/
		}
		//time_sum=time_sum+diffclock(end,begin);
		//ga_error("run for frame %d is: %f\n",frames[iid]-1,diffclock(end,begin));
		//ga_error("stored qps!\n");						
	}
	//
rc_quit:
	if(pipe) {
		pipe = NULL;
	}
	if(pipe_d) {
		pipe_d = NULL;
	}
	if(pipe_bitrates) {
		pipe_bitrates = NULL;
	}	
	//	
	ga_error("RC: thread terminated.\n");
	//
	return NULL;
}





static int
vrc_start(void *arg) {
	int iid;
	char **pipefmt = (char**) arg;
	static char *enc_param[VIDEO_SOURCE_CHANNEL_MAX][4];
#define	MAXPARAMLEN	64
	static char pipename[VIDEO_SOURCE_CHANNEL_MAX][4][MAXPARAMLEN];
	if(vrc_started != 0)
		return 0;
	vrc_started = 1;
	for(iid = 0; iid < video_source_channels(); iid++) {
		snprintf(pipename[iid][0], MAXPARAMLEN, pipefmt[0], iid);
		//ga_error("source:%s\n",pipename[iid][0]);
		snprintf(pipename[iid][1], MAXPARAMLEN, pipefmt[1], iid);
		//ga_error("dest:%s\n",pipename[iid][1]);
		snprintf(pipename[iid][2], MAXPARAMLEN, pipefmt[2], iid);
		//ga_error("dest_bitrates:%s\n",pipename[iid][2]);
		snprintf(pipename[iid][3], MAXPARAMLEN, pipefmt[3], iid);
		//ga_error("src_qps:%s\n",pipename[iid][3]);
		enc_param[iid][0]=pipename[iid][0];
		enc_param[iid][1]=pipename[iid][1];
		enc_param[iid][2]=pipename[iid][2];
		enc_param[iid][3]=pipename[iid][3];
		switch(mode){
			case CAVE_R:
				K=ga_conf_readdouble("K");
				if(pthread_create(&vrc_tid[iid], NULL, vrc_cave_roi_lambda_threadproc, enc_param[iid]) != 0) {
					vrc_started = 0;
					ga_error("rc roi lambda: create thread failed.\n");
					return -1;
				}
			break;			
			case ROI_:
				if(pthread_create(&vrc_tid[iid], NULL, vrc_roi_q_threadproc, enc_param[iid]) != 0) {
					vrc_started = 0;
					ga_error("rc rq: create thread failed.\n");
					return -1;
				}
			break;
			case BASE:
				if(pthread_create(&vrc_tid[iid], NULL, vrc_base_threadproc, enc_param[iid]) != 0) {
					vrc_started = 0;
					ga_error("rc roi q: create thread failed.\n");
					return -1;
				}
			break;
		}		
	}
	ga_error("rc: all started (%d)\n", iid);
	return 0;
}

static int
vrc_stop(void *arg) {
	int iid;
	void *ignored;
	if(vrc_started == 0)
		return 0;
	vrc_started = 0;
	for(iid = 0; iid < video_source_channels(); iid++) {
		pthread_join(vrc_tid[iid], &ignored);
	}
	ga_error("rc: all stopped (%d)\n", iid);
	return 0;
}

static void *
vrc_raw(void *arg, int *size) {
#if defined __APPLE__
	int64_t in = (int64_t) arg;
	int iid = (int) (in & 0xffffffffLL);
#elif defined __x86_64__
	int iid = (long long) arg;
#else
	int iid = (int) arg;
#endif	
	return NULL;	
}




ga_module_t *
module_load() {
	static ga_module_t m;
	//
	bzero(&m, sizeof(m));
	m.type = GA_MODULE_TYPE_VENCODER;
	m.name = strdup("rc");
	m.mimetype = strdup("video/RC");
	m.init = vrc_init;
	m.start = vrc_start;
	//m.threadproc = vencoder_threadproc;
	m.stop = vrc_stop;
	m.deinit = vrc_deinit;
	//
	m.raw = vrc_raw;
	//m.ioctl = vrc_ioctl;
	return &m;
}

