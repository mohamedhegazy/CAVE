//the code here is used for evaluation purposes. the same code can be found embedded in GA
#include <stdio.h>
#include <stdarg.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>
#include <math.h>
#include <time.h>
#include "CL/cl.hpp"
#include <x265.h>
//#include <kvazaar.h>
using namespace std;

typedef long long _Longlong;
#pragma comment(linker, "/STACK:2000000")
#pragma comment(linker, "/HEAP:2000000")
//static bool KVZ = 0;
#ifdef	_WIN32
#include<windows.h>
#else
#define	BACKSLASHDIR(fwd, back)	fwd
#include <sys/stat.h>
#endif

#define	ROI_UPDATE_STEP 2
#define	LOWER_BOUND 12
#define QP_BASE 22
#define X265_LOWRES_CU_SIZE   8
#define X265_LOWRES_CU_BITS   3
#define CU_SIZE 16 // for x265 as well as x264
#define MAX_KERNELS 3
#define MAX_BUFS 10
#define ISCALE 1
#define DISTANCE 75 //average viewing distance on PC
#define PIXEL_IN_CM 37.79
#define DISTANCE_IN_PIXEL DISTANCE*PIXEL_IN_CM
static double K = 3;
static unsigned int upSampleRatio = CU_SIZE / 16;
#define CEILDIV(x,y) (((x) + (y) - 1) / (y))
#define LCU_WIDTH 64

enum MODE{	
	LAMBDA_R,//needs ROIs,
	LAMBDA_D,//needs depth--disabled
	ROI_,//needs ROIs
	WEIGHTED_,//needs ROIs, depth--disabled	
	BASE_ENCODER_,//needs nothing
};
enum OBJECT_CATEGORY{
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

double diffclock(clock_t clock1,clock_t clock2)
{
	double diffticks=clock1-clock2;
	double diffms=(diffticks*1000)/CLOCKS_PER_SEC;
	return diffms;
} 


//***SHARED AMONG ALL TECHNIQUES**
static std::vector<cl::Platform> platforms;
static std::vector<cl::Device> devices;	
static cl::CommandQueue queue;
static cl::Context context;
static float diagonal;
static float * weights;
static unsigned int frames ;
static float importance[5] = {1.0f,0.95f,0.7f,0.6f,0.9f};//based on OBJECT_CATEGORY enum

//***DIFFERS PER TECHNIQUE (CAVE_ROI uses one kernel while CAVE_WEIGHTED uses three kernels)
static cl::Kernel kernel[MAX_KERNELS];
static cl::Program program[MAX_KERNELS];
static cl::Buffer inp_buf[MAX_BUFS];
static cl::Buffer out_buf[MAX_BUFS];	
static cl_int err;	
static int reversed;

//****USED BY CAVE LAMBDA
static double * bitsPerBlock;
static double ALPHA=3.2003f;
static double BETA=-1.367f;
static double C1=4.2005f;
static double C2=13.7112f;
static double m_alphaUpdate = 0.1f;
static double m_betaUpdate  =0.05f;
static double MIN_LAMBDA = 0.1f;
static double MAX_LAMBDA = 10000.0f;
static double FRAME_LAMBDA ;
static double totalBitsUsed;
static double bitsTotalGroup ;
static double avg_bits_per_pic ;
static double old_avg_bits_per_pic ;
static double avgLambda = 0.0f;
static double sumRate = 0.0f;
static double totalBits=0.0f;
#define SW 120
#define GOP 90
static double lambda_buf_occup = 0.0;
static double * base_MAD;
static double sum_MAD;

//****USED BY CAVE WEIGHTED
static float * depth;//realWidthxrealHeight depth (input to OpenCL Kernel)
static float * depthOut;//realWidthxrealHeight depth (output from OpenCL Kernel)
static unsigned int*    centers;
static float*  distOut;
static float filter_mean[3][3]={{1.0f/12.0f,1.0f/12.0f,1.0f/12.0f}, {1.0f/12.0f,1.0f/3.0f,1.0f/12.0f}, {1.0f/12.0f,1.0f/12.0f,1.0f/12.0f}};
static const float THETA = 21570.0f;// 7800 in paper
static const float GAMMA = 1.336f ;//0.68 in paper


//****USED BY CAVE ROI
#define ADJUSTMENT_FACTOR       0.60f
#define HIGH_QSTEP_ALPHA        4.9371f
#define HIGH_QSTEP_BETA         0.0922f
#define LOW_QSTEP_ALPHA         16.7429f
#define LOW_QSTEP_BETA          -1.1494f
#define HIGH_QSTEP_THRESHOLD    9.5238f
#define MIN_QP 0
#define MAX_QP 51
unsigned long long int* temp;
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
static bool canRead = 0;

//Control variables
static unordered_map<string,string> config;
static bool reassign = 0;
static int mode = 0;
static unsigned int realWidth = 0;
static unsigned int realHeight = 0;
static unsigned int widthDelta = 0;
static unsigned int heightDelta = 0;
static float pixelsPerBlock = 0;
static float bitrate = 0.0f;
static unsigned int fps = 0;
//static unsigned int period = 0;
static float * QP;
static float * QP_out;
static float * roi;
static float*   ROI;
static unsigned long long int maxFrames;
static std::vector<ROITuple_t> ROIs;
static bool written_depth = 0;
static FILE* raw_yuv;
static FILE* encoded;
static FILE * depth_file;
static float last_size=0.0f;
static unsigned long long int SCALE= 10000000000000;
static x265_encoder* vencoder;
//static kvz_encoder* kvz;
//static const kvz_api * api;
//static kvz_config * conf;

string raw_path;
string roi_path;
string depth_path;
string depthSobel_path;
string encoded_path;
string folderIn;
string folderOut;
string ga_logfile;
string slash;


static int roundRC(float d)
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

int ga_error(const char *fmt, ...) {
	char msg[4096];
	va_list ap;	
	va_start(ap, fmt);
#ifdef ANDROID
	__android_log_vprint(ANDROID_LOG_INFO, "ga_log.native", fmt, ap);
#endif
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);
#ifdef __APPLE__
	syslog(LOG_NOTICE, "%s", msg);
#endif
	FILE *fp;	
	fp=fopen(ga_logfile.c_str(), "at");
	fprintf(fp, "%s", msg);
	fclose(fp);	
	return -1;
}

static void initCL(){
	//ga_error("Initialize CL\n");
	clock_t begin=clock();
	err=cl::Platform::get(&platforms);
	if(err!=CL_SUCCESS)
	{
		ga_error("Platform err:%d\n",err);
		return;
	}
	string platform_name;
	string device_type;		
	//ga_error("Number of Platforms Available:%d\n",platforms.size());	
	platforms[0].getInfo(CL_PLATFORM_NAME,&platform_name);	
	//ga_error("Platform Used:%s\n",platform_name.c_str());
	err=platforms[0].getDevices(CL_DEVICE_TYPE_ALL,&devices);
	if(err!=CL_SUCCESS)
	{
		ga_error("Device err:%d\n",err);
		return;
	}	
	ga_error("Number of Devices Available:%d\n",devices.size());
	err=devices[0].getInfo(CL_DEVICE_NAME,&device_type);
	if(err!=CL_SUCCESS)
		ga_error("Type of device\n");
	else{		
		//ga_error("Type of Device Used: %s\n",device_type.c_str());
	}
	context=cl::Context(devices,NULL,NULL,NULL,&err);
	if(err!=CL_SUCCESS)
		ga_error("Context err:%d\n",err);
	queue=cl::CommandQueue(context,devices[0],NULL,&err);
	if(err!=CL_SUCCESS)
		ga_error("Command Queue err:%d\n",err);
	clock_t end=clock();	
	//ga_error("Time Constructor: %f\n",diffclock(end,begin));	
}

std::string LoadKernel (const char* name)
{
	char srcPath[1024];	
	sprintf(srcPath,name);	
	std::ifstream in (srcPath);
	std::string result (
		(std::istreambuf_iterator<char> (in)),
		std::istreambuf_iterator<char> ());
	//cout<<result<<endl;
	int index = result.find("?w?");    
	string buf;	
	buf=std::to_string((_Longlong)realWidth);
    result=result.replace(index, 3,buf);
	index = result.find("?h?");    		
	buf=std::to_string((_Longlong)realHeight);
    result=result.replace(index, 3,buf);
	index = result.find("?c?");    		
	buf=std::to_string((_Longlong)(int)(log((double)CU_SIZE)/log(2.0)));
    result=result.replace(index, 3,buf);
	index = result.find("?bw?");    		
	buf=std::to_string((_Longlong)widthDelta);
    result=result.replace(index, 4,buf);
	index = result.find("?bh?");    	
	buf=std::to_string((_Longlong)heightDelta);	
    result=result.replace(index, 4,buf);
	index = result.find("?s?");    		
	buf=std::to_string((_Longlong)SCALE);
    result=result.replace(index, 3,buf);
	//ga_error(result.c_str());
	return result;
}

static void loadCL(string name,int idx,string signature){
	//ga_error("Load Program\n");
	clock_t begin=clock();
	std::string src = LoadKernel(name.c_str());
	cl::Program::Sources source(1,make_pair(src.c_str(),src.size()));
	program[idx]=cl::Program(context,source,&err);
	if(err!=CL_SUCCESS)
		ga_error("Program err:%d\n",err);
	err=program[idx].build(devices);
	if(err!=CL_SUCCESS)
		ga_error("Build Error err:%d\n",err);
	//ga_error("done building program\n");
	clock_t end=clock();		
	//ga_error("Time Build Program: %f\n",diffclock(end,begin));	
	//ga_error("Build Status: %d\n" , program[idx].getBuildInfo<CL_PROGRAM_BUILD_STATUS>(devices[0]));		
	//ga_error("Build Options: %d\n", program[idx].getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(devices[0]));		
	//if(err!=CL_SUCCESS)		
	//	ga_error("Build Log: %s\n" , program[idx].getBuildInfo<CL_PROGRAM_BUILD_LOG>(devices[0]));	
	//ga_error("Create Kernel\n");
	
	
	begin=clock();
	kernel[idx]=cl::Kernel(program[idx],signature.data(),&err);
	if(err!=CL_SUCCESS)
		ga_error("Create Kernel Error err:%d\n",err);
	end=clock();
	//ga_error("Time Create Kernel %f\n",diffclock(end,begin));
}


//****************************************************************ROI Q-DOMAIN FUNCTIONS BEGIN******************************************************
static void updateBufferROIAndRunKernels()
{	
   size_t result=0;         
   clock_t begin=clock();   
   inp_buf[0] = cl::Buffer(context,CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,sizeof(float)*widthDelta*heightDelta,ROI,&err);
   clock_t end=clock();  
   unsigned long long int sumDistance = 0;
   if(err!=CL_SUCCESS)
	   ga_error("Inp data Transfer err: %d\n",err);
   //ga_error("Time Sending data to GPU %f\n",diffclock(end,begin));   
   memset(temp,0,sizeof(unsigned long long int)*widthDelta*heightDelta);
   out_buf[0] = cl::Buffer(context,CL_MEM_WRITE_ONLY|CL_MEM_COPY_HOST_PTR,sizeof(unsigned long long int)*widthDelta*heightDelta,temp,&err);//output buffer needs to be zeroed out
   if(err!=CL_SUCCESS)
	   ga_error("Out data Transfer err:%d\n",err);

   out_buf[1] = cl::Buffer(context,CL_MEM_WRITE_ONLY|CL_MEM_COPY_HOST_PTR,sizeof(unsigned long long int),&sumDistance,&err);//output buffer needs to be zeroed out
   if(err!=CL_SUCCESS)
	   ga_error("Out data Transfer err:%d\n",err);

	err=kernel[0].setArg(0,inp_buf[0]);
	if(err!=CL_SUCCESS)
		ga_error("SetKernel Arg0 err:%d\n",err);
	err=kernel[0].setArg(1,out_buf[1]);
	if(err!=CL_SUCCESS)
		ga_error("SetKernel Arg1 err:%d\n",err);
	err=kernel[0].setArg(2,out_buf[0]);
	if(err!=CL_SUCCESS)
		ga_error("SetKernel Arg2 err:%d\n",err);


	begin=clock();
	err=queue.enqueueNDRangeKernel(kernel[0],cl::NullRange,cl::NDRange(realWidth,realHeight),cl::NullRange);
	if(err!=CL_SUCCESS)
	{
		ga_error("Error Running Kernel:%d\n",err);		
	}

	queue.finish();	
	end=clock();
	double kernel_time = diffclock(end,begin);
	//ga_error("Time Running Kernel %f\n",kernel_time);
	begin=clock();	
	err=queue.enqueueReadBuffer(out_buf[0],CL_TRUE,0,widthDelta*heightDelta*sizeof(unsigned long long int),temp);
	if(err!=CL_SUCCESS)
	{
		ga_error("Error Reading Data1:%d\n",err);		
	}
	queue.finish();	
	err=queue.enqueueReadBuffer(out_buf[1],CL_TRUE,0,sizeof(unsigned long long int),&sumDistance);
	if(err!=CL_SUCCESS)
	{
		ga_error("Error Reading Data2:%d\n",err);		
	}
	queue.finish();
	end=clock();
	//ga_error("sumDistance:%llu\n",sumDistance);
	float sumDistancef=sumDistance/SCALE;
	sumDistancef=sumDistancef/(realWidth*realHeight);
	unsigned int block_ind =0 ;
	float sum = 0;
	for(unsigned int i=0;i<heightDelta;i++){
		for(unsigned int j=0;j<widthDelta;j++){
			weights[block_ind]=temp[block_ind]/SCALE;
			weights[block_ind]=weights[block_ind]/sumDistancef;
			weights[block_ind]=max(0.0f,min(1.0f,weights[block_ind]));
			float val =0.0;
			for(unsigned int a = 0; a < 3; a++)
			{
				for(unsigned int b = 0; b < 3; b++)
				{ 
					int jn=min(widthDelta-1,max((unsigned int)0,j+b-1));
					int in=min(heightDelta-1,max((unsigned int)0,i+a-1));
					int index=jn+in*widthDelta;
					val=val+weights[index]*filter_mean[a][b];
				}
			}
			sum = sum + val;
			weights[block_ind]=val;				
			block_ind++;					
		}
	}
	
	/*block_ind = 0;
	for (unsigned int i = 0;i < heightDelta;i++) {
		for (unsigned int j = 0;j < widthDelta;j++) {
			weights[block_ind] = weights[block_ind] / sum;
			block_ind++;
		}
	}*/
}

#define J0260 1
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
    paramNewX2    = (((bits/(numberOfPixels*costMAD))-(23.3772f*invqStep*invqStep))/((1-200*invqStep)*invqStep));
    paramNewX1    = (23.3772-200*paramNewX2);		
	m_paramHighX1 = 0.70*HIGH_QSTEP_ALPHA + 0.20f * m_paramHighX1 + 0.10f * paramNewX1;	
	m_paramHighX2 = 0.70*HIGH_QSTEP_BETA  + 0.20f * m_paramHighX2 + 0.10f * paramNewX2;	
  }
  else
  {
    paramNewX2   = (((bits/(numberOfPixels*costMAD))-(5.8091*invqStep*invqStep))/((1-9.5455*invqStep)*invqStep));
    paramNewX1   = (5.8091-9.5455*paramNewX2);	
	m_paramLowX1 = 0.90*LOW_QSTEP_ALPHA + 0.09f * m_paramLowX1 + 0.01f * paramNewX1;	
	m_paramLowX2 = 0.90*LOW_QSTEP_BETA  + 0.09f * m_paramLowX2 + 0.01f * paramNewX2;
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

static void calcQPROI(){
	if(frames==0){//first frame in first GOP gets a constant qp value		
		float bpp = (bitrate/fps)/(realWidth*realHeight);
		float qp_off = 0;
		if(bpp>=0.1)
			qp_off=-2;
		else{			
			qp_off=18;
		}		
		//ga_error("offset:%.2f\n",qp_off);
		m_Qp=QP_BASE+qp_off;
		for(unsigned int i=0;i<widthDelta*heightDelta;i++)
			QP[i]=qp_off;		
		return;
	}
	unsigned int block_ind=0;
	float blocks_num = widthDelta * heightDelta;
	block_ind = 0;
	float sumWeights = 0;
	for (unsigned int i = 0;i<heightDelta;i++) {
		for (unsigned int j = 0;j<widthDelta;j++) {
			weights[block_ind] = weights[block_ind] * (BIAS_ROI - 1) + 1;
			sumWeights = sumWeights + weights[block_ind];
			block_ind++;
		}
	}
	block_ind = 0;
	float carrySum = 0;
	float secondSum = 0;
	//ga_error("sum weights %.09f\n",sumWeights);
	for (unsigned int i = 0;i<heightDelta;i++) {
		for (unsigned int j = 0;j<widthDelta;j++) {
			float temp = weights[block_ind];
			weights[block_ind] = weights[block_ind] * (blocks_num - block_ind) / (sumWeights - carrySum);
			secondSum = secondSum + weights[block_ind];
			carrySum = carrySum + temp;
			//ga_error("weights at %d,%d is %.09f\n",i,j,weights[block_ind]);
			block_ind++;
		}
	}
	//H0213 and Pixel-wise URQ model for multi-level rate 	
	unsigned int frame_idx = frames%GOP;//frame indexing
	float bitsActualSum = last_size;
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
		m_occupancyVB = m_occupancyVB + occupancyBits;
	}

	

	if(frame_idx == 0)
	{				
		sumRate=0;
		m_initialTBL = m_targetBufLevel  = (bitsActualSum - (bitrate/fps));
	}
	else
	{		
		m_targetBufLevel =  m_targetBufLevel 
							- (m_initialTBL/(GOP-1));
	}
	sumRate=sumRate+bitsActualSum;
	
	
	if(frame_idx != 0 && checkUpdateAvailable(m_Qp))
	{
		updatePixelBasedURQQuadraticModel(m_Qp, bitsActualSum, realWidth*realHeight, sum_MAD);
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
    
	
	float frame_bits= (int)(0.9f * targetBitsLeftBudget + 0.1f * targetBitsOccupancy);	
	float frame_qp = 0;
	if (frame_bits <= 0)
		frame_qp = m_Qp+2;
	else
		frame_qp = max(m_Qp-2,min(m_Qp+2,getQP(m_Qp, frame_bits, realWidth*realHeight,sum_MAD)));
	//frame_bits=min(upper,max(frame_bits,lower));
	float sum_qp = 0.0f;
	block_ind=0;
	float upperBoundQP = FLT_MAX;
	float lowerBoundQP = FLT_MIN;
	float prev_qp = m_Qp;	
	m_Qp = 0;	
	
	for(unsigned int i=0;i<heightDelta;i++){
		for (unsigned int j = 0;j < widthDelta;j++) {
			float bits = frame_bits / blocks_num * weights[block_ind];
			float final_qp;
			if (block_ind == 0)
				final_qp = frame_qp;
			else {
				if (bits < 0) {
					final_qp = QP[block_ind-1]+QP_BASE+ 1;
				}
				else
					final_qp = getQP(QP[block_ind] + QP_BASE, bits, pixelsPerBlock, base_MAD[block_ind]);
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
			final_qp=max((float)MIN_QP,min((float)MAX_QP,final_qp));
			//float final_qp=round(max(first_root,second_root));			
			//ga_error("qp %d,%d is %.2f and weights is %.5f\n",i,j,final_qp,weights[block_ind]);
			m_Qp = m_Qp + final_qp;
			QP[block_ind]=final_qp-QP_BASE;
			/*if(j>=widthDelta/2)
				QP[block_ind]=51-QP_BASE;
			else
				QP[block_ind]=-QP_BASE;*/
			block_ind++;
		}
	}
	m_Qp = m_Qp/blocks_num;		
	ga_error("remaining in GOP: %.2f frame bits based on GOP:%.2f buf_occup:%.2f buf_status:%.2f  X1:%.2f X2:%.2f\n", m_remainingBitsInGOP, frame_bits, m_occupancyVB, targetBitsOccupancy, m_paramHighX1, m_paramHighX2);
	ga_error("qp frame: %.2f\n", m_Qp);
}


//****************************************************************ROI Q-DOMAIN FUNCTIONS END******************************************************

static int vencoder_init() {
	//if(KVZ){
	//	api = kvz_api_get(8);
	//	conf=api->config_alloc();
	//	int ret =api->config_init(conf);
	//	ret =api->config_parse(conf,"preset","ultrafast");
	//	ret =api->config_parse(conf,"me","dia");
	//	ret =api->config_parse(conf,"width",std::to_string((_Longlong) realWidth).c_str());
	//	ret =api->config_parse(conf,"height",std::to_string((_Longlong) realHeight).c_str());		
	//	ret =api->config_parse(conf,"me-steps","16");
	//	ret =api->config_parse(conf,"input-fps",std::to_string((_Longlong) fps).c_str());
	//	ret =api->config_parse(conf,"period","0");
	//	ret =api->config_parse(conf,"subme","0");
	//	ret =api->config_parse(conf, "threads", "4");
	//	ret =api->config_parse(conf, "owf", "0");
	//	ret = api->config_parse(conf, "gop", "0");
	//	ret = api->config_parse(conf, "slices", "tiles");
	//	ret = api->config_parse(conf, "tiles", "1x4");

	//	if (mode != BASE_ENCODER_) {
	//		ret = api->config_parse(conf, "content-aware", "1");
	//		if (mode == ROI_) {//ROI_
	//			ret = api->config_parse(conf, "roi", "");
	//			ret = api->config_parse(conf, "qp", "22");
	//		}
	//		else {//CAVE, CAVEd
	//			ret = api->config_parse(conf, "bitrate", std::to_string((_Longlong)bitrate).c_str());
	//		}
	//	}
	//	else {//base encoder
	//		ret = api->config_parse(conf, "bitrate", std::to_string((_Longlong)bitrate).c_str());
	//	}
	//	
	//	/*ofstream myfile;
	//	myfile.open("roi_init.txt");
	//	myfile << 2*widthDelta <<" ";
	//	myfile << 2*heightDelta << " ";
	//	for (int i = 0;i < 4*widthDelta*heightDelta-1;i++) {
	//		myfile << "0 ";

	//	}
	//	myfile << "0";
	//	myfile.close();
	//	ret = api->config_parse(conf, "roi", "roi_init.txt");*/
	//	kvz = api->encoder_open(conf);
	//}
	//else{
		x265_param params;
		x265_param_default(&params);				
		x265_param_default_preset(&params, "ultrafast", "zerolatency");
		char tmpbuf[500];			
		//if(mode!=BASE_ENCODER_){
			int ret=x265_param_parse(&params, "qp", "22");
			params.rc.ipFactor = pow(2.0,1.0/12.0);//to make sure that I frames have same QP as P frames			
			ret = x265_param_parse(&params, "aq-mode", "1");						
			//ret = x265_param_parse(&params, "no-wpp", "1");
			//ret = x265_param_parse(&params, "frame-threads", "4");
			//ret = x265_param_parse(&params, "slices", "4");
			//ret = x265_param_parse(&params, "lookahead-slices", "0");
			

			/*int ret = x265_param_parse(&params, "bitrate", "256");
			ret = x265_param_parse(&params, "vbv-maxrate", "256");
			ret = x265_param_parse(&params, "vbv-minrate", "256");
			ret = x265_param_parse(&params, "vbv-bufsize", "8.53");
			ret = x265_param_parse(&params, "strict-cbr", "");*/
	
		//}
		/*else
		{	
			string tmp = std::to_string((_Longlong) bitrate/1024); 		
			x265_param_parse(&params, "bitrate", tmp.c_str());					
			x265_param_parse(&params, "vbv-maxrate", tmp.c_str());			
				tmp = std::to_string((_Longlong) bitrate/(2*1024));
			x265_param_parse(&params, "vbv-bufsize", tmp.c_str());	
			x265_param_parse(&params, "strict-cbr", "1");							
		}*/
		string tmp = std::to_string((_Longlong) fps); 
		string intra = std::to_string((_Longlong)GOP);
		ret = x265_param_parse(&params, "keyint", intra.c_str());
		ret = x265_param_parse(&params, "intra-refresh", "1");
		ret = x265_param_parse(&params, "fps", tmp.c_str());
		ret = x265_param_parse(&params, "ref", "1");
		ret = x265_param_parse(&params, "me", "dia");
		ret = x265_param_parse(&params, "merange", "16");
		ret = x265_param_parse(&params, "bframes", "0");		
		params.logLevel = X265_LOG_INFO;
		params.internalCsp = X265_CSP_I420;
		params.sourceWidth= realWidth;
		params.sourceHeight = realHeight;		
		params.bRepeatHeaders = 1;
		params.bAnnexB = 1;
		ret = x265_param_parse(&params, "sar", "1");
		vencoder = x265_encoder_open(&params);	
	//}
	return 0;
}


#define CU_MIN_SIZE_PIXELS (1 << MIN_SIZE)
#define MIN_SIZE 3
static unsigned get_padding(unsigned width_or_height) {
	if (width_or_height % CU_MIN_SIZE_PIXELS) {
		return CU_MIN_SIZE_PIXELS - (width_or_height % CU_MIN_SIZE_PIXELS);
	}
	else {
		return 0;
	}
}

static bool vencoder_encode(void * frame) {	
	//if (KVZ) {
	//	kvz_picture *frame_in = api->picture_alloc_csp(KVZ_CSP_420,
	//		realWidth + get_padding(realWidth),
	//		realHeight + get_padding(realHeight));
	//	frame_in->chroma_format = KVZ_CSP_420;		
	//	frame_in->y = (kvz_pixel *)frame;
	//	frame_in->u = (kvz_pixel *)frame+realWidth*realHeight;
	//	frame_in->v = (kvz_pixel *)frame + (int)(5.0/4.0 *realWidth * realHeight);
	//	/*if(mode!=BASE_ENCODER_ && mode != ROI_ )
	//		memcpy(frame_in->weights, weights, sizeof(float)*widthDelta*heightDelta);
	//	if(mode==ROI_)
	//		memcpy(frame_in->qp, QP, sizeof(int8_t)*widthDelta*heightDelta);*/
	//	kvz_data_chunk* chunks_out = NULL;
	//	kvz_picture *img_rec = NULL;
	//	kvz_picture *img_src = NULL;
	//	uint32_t len_out = 0;
	//	kvz_frame_info info_out;
	//	ofstream myfile;
	//	/*myfile.open("roi.txt");
	//	myfile << widthDelta << " ";
	//	myfile << heightDelta << " ";
	//	for (int i = 0;i < pow(upSampleRatio,2)*widthDelta*heightDelta - 1;i++) {
	//		myfile << (int)QP_out[i]<<" ";

	//	}
	//	myfile << (int)QP_out[(int)pow(upSampleRatio, 2) * widthDelta*heightDelta - 1];
	//	myfile.close();
	//	int ret = api->config_parse(conf, "roi", "roi.txt");*/
	//	clock_t begin = clock();
	//	api->encoder_encode(kvz,
	//		frame_in,
	//		&chunks_out,
	//		&len_out,
	//		&img_rec,
	//		&img_src,
	//		&info_out);
	//	clock_t end = clock();
	//	double temp = diffclock(end, begin);
	//	if (chunks_out != NULL) {
	//		uint64_t written = 0;
	//		// Write data into the output file.
	//		for (kvz_data_chunk *chunk = chunks_out;
	//			chunk != NULL;
	//			chunk = chunk->next) {
	//			if (fwrite(chunk->data, sizeof(uint8_t), chunk->len, encoded) != chunk->len) {
	//				fprintf(stderr, "Failed to write data to file.\n");
	//				api->picture_free(frame_in);
	//				api->chunk_free(chunks_out);
	//				return false;
	//			}
	//			written += chunk->len;
	//		}
	//		last_size = written * 8;
	//	}
	//	api->picture_free(frame_in);
	//}
	//else {
		x265_encoder *encoder = NULL;
		int pktbufsize = 0;
		int64_t x265_pts = 0;
		x265_param params;
		x265_encoder_parameters(vencoder, &params);

		x265_picture pic_in, pic_out = { 0 };
		x265_nal *nal;
		unsigned int i, size;
		uint32_t nnal;

		if (frame != NULL) {
			x265_picture_init(&params, &pic_in);
			x265_picture_init(&params, &pic_out);
			pic_out.colorSpace = X265_CSP_I420;

			pic_in.colorSpace = X265_CSP_I420;
			pic_in.stride[0] = realWidth;
			pic_in.stride[1] = realWidth >> 1;
			pic_in.stride[2] = realWidth >> 1;
			pic_in.planes[0] = frame;
			pic_in.planes[1] = (uint8_t *)(pic_in.planes[0]) + realWidth * realHeight;
			pic_in.planes[2] = (uint8_t *)(pic_in.planes[1]) + ((realWidth*realHeight) >> 2);
			pic_in.quantOffsets = QP_out;
		}
		//if(mode!=BASE_ENCODER_){		
		
		//}			
		clock_t begin = clock();
		size = x265_encoder_encode(vencoder, &nal, &nnal, &pic_in, &pic_out);
		clock_t end = clock();
		double temp = diffclock(end, begin);
		 
		//if (frame == NULL && size == 0)
		//	return true;//flush ended
		if (size > 0) {
			//if (mode == ROI_ && frame != NULL) {
				canRead = true;
				sum_MAD = 0.0;
				for (unsigned int x = 0;x < heightDelta;x++) {
					for (unsigned int y = 0;y < widthDelta;y++) {
						int x_ind = x * CU_SIZE;
						int y_ind = y * CU_SIZE;
						int pixels_num = 0;//in cases where we are the border
						float cur = 0.0f;
						for (unsigned int j = x_ind;j < x_ind + CU_SIZE && j < realHeight;j++) {
							for (unsigned int i = y_ind;i < y_ind + CU_SIZE && i < realWidth;i++) {
								unsigned int ind_in = i + j * (realWidth);
								unsigned int ind_out = i + j * (realWidth + pic_out.stride[0] - pic_in.stride[0]);
								cur = cur + abs(*((uint8_t*)pic_in.planes[0] + ind_in) - *((uint8_t*)pic_out.planes[0] + ind_out));
								pixels_num++;
							}
						}

						cur = cur / pixels_num;
						sum_MAD = sum_MAD + cur;
						base_MAD[y + x * widthDelta] = cur;
					}
				}
				sum_MAD = sum_MAD / (widthDelta*heightDelta);
			//}
			pktbufsize = 0;
			for (i = 0; i < nnal; i++) {
				fwrite(nal[i].payload, sizeof(uint8_t), nal[i].sizeBytes, encoded);
				pktbufsize += nal[i].sizeBytes;
			}
		}
		/*else {
			pktbufsize = bitrate/(8*fps);
		}*/
		//uint8_t * zeros = (uint8_t *)calloc(realWidth / 4, sizeof(uint8_t));
		//memset(zeros, 128, realWidth / 4*sizeof(uint8_t));
		///*for(int i=0;i<realHeight;i++)
		//	fwrite((uint8_t*)pic_out.planes[0]+i*(realWidth+ pic_out.stride[0]-pic_in.stride[0]), sizeof(uint8_t), realWidth, encoded);*/
		//for (int i = 0;i < realHeight;i++) {
		//	fwrite(zeros, sizeof(uint8_t), realWidth / 4, encoded);			
		//}
		//for (int i = 0;i < realHeight;i++) {
		//	fwrite(zeros, sizeof(uint8_t), realWidth / 4, encoded);			
		//}
		ga_error("frame:%d actual size:%d target size:%.2f\n", frames, pktbufsize*8, avg_bits_per_pic);
		last_size = pktbufsize * 8;
		totalBits = totalBits + last_size;
		return false;
		/*if(frame==0)
			last_size = avg_bits_per_pic * 6;*/
		
			
	//}
}


static int vrc_init() {	
	#ifdef	_WIN32	
	slash = "\\\\";
	#else
	slash = "/";
	#endif

	fps =  atoi(config["fps"].c_str());	
	bitrate = atoi(config["bitrate"].c_str());	
	m_remainingBitsInGOP = bitrate / fps * GOP;
	lambda_buf_occup = 0.5 * bitrate / fps;
	bitsTotalGroup = max(200.0f, (((1.0f * bitrate / fps) - 0.5 * lambda_buf_occup / fps) * GOP));
	//bitsTotalGroup = max(200.0f, (((1.0f * bitrate / fps)*(frames + SW) - totalBits)*GOP) / SW);
	avg_bits_per_pic = (float)(bitrate / (1.0f * (float)fps));
	old_avg_bits_per_pic = avg_bits_per_pic;		
	realWidth =  atoi(config["width"].c_str());	
	realHeight = atoi(config["height"].c_str());
	raw_path = config["raw_path"]+slash+"raw_"+config["width"]+"_"+config["height"]+".yuv";
	ifstream file( raw_path, ios::binary | ios::ate);
	maxFrames = file.tellg()/(realWidth * realHeight * 1.5f);	
	file.close();
	diagonal = (float)sqrt(pow(realWidth * 1.0, 2) + pow(realHeight * 1.0, 2));
	/*if (KVZ) {
		widthDelta = CEILDIV(realWidth, LCU_WIDTH);
		heightDelta = CEILDIV(realHeight, LCU_WIDTH);
	}
	else {*/
		heightDelta = (((realHeight / 2) + X265_LOWRES_CU_SIZE - 1) >> X265_LOWRES_CU_BITS);//will get the number of 16X16 blocks in the height direction for x265
		widthDelta = (((realWidth / 2) + X265_LOWRES_CU_SIZE - 1) >> X265_LOWRES_CU_BITS);
	//}
	pixelsPerBlock = (float)(CU_SIZE * CU_SIZE);
	QP = (float *) calloc(widthDelta * heightDelta,sizeof(float));
	QP_out = (float *) calloc(pow(upSampleRatio,2)*widthDelta * heightDelta,sizeof(float));
	bitsPerBlock = (double *)calloc(heightDelta * widthDelta, sizeof(double));
	weights = (float *)calloc(heightDelta * widthDelta, sizeof(float));
	depth = (float *)calloc(realWidth * realHeight, sizeof(float));		
	depthOut = (float *)calloc(realWidth*realHeight, sizeof(float));
	distOut=(float*)calloc(realWidth*realHeight,sizeof(float));		
	ROI = (float *)calloc(heightDelta * widthDelta, sizeof(float)); //predict QPs using complexity of depth
	base_MAD = (double *)calloc(heightDelta * widthDelta, sizeof(double)); //predict QPs using complexity of depth
	temp = (unsigned long long int *)calloc(widthDelta*heightDelta,sizeof(unsigned long long int));
					
	/*rois=fopen( "C:\\Users\\mhegazy\\Desktop\\rois.txt","ab");
	qp=fopen( "C:\\Users\\mhegazy\\Desktop\\qp.txt","ab");
	importance=fopen( "C:\\Users\\mhegazy\\Desktop\\importance.txt","ab");
	bits=fopen( "C:\\Users\\mhegazy\\Desktop\\bits.txt","ab");
	bits_actual=fopen( "C:\\Users\\mhegazy\\Desktop\\bits_actual.txt","ab");*/

	int seq = atoi(config["seq"].c_str());	
	
	size_t found=raw_path.find_last_of("/\\")-1;
	folderIn = raw_path.substr(0,found);
	folderOut = raw_path.substr(0,found);
	
	if(mode == LAMBDA_R ||  mode == BASE_ENCODER_){
		double m_seqTargetBpp = bitrate / (fps * realWidth * realHeight);
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
		else 
		{
			m_alphaUpdate = 0.2;
			m_betaUpdate = 0.1;
		}*/
		/*else if ( m_seqTargetBpp < 0.5 )
		{
		m_alphaUpdate = 0.2;
		m_betaUpdate  = 0.1;
		}
		else
		{
		m_alphaUpdate = 0.4;
		m_betaUpdate  = 0.2;
		}		*/
	}
	if(mode == BASE_ENCODER_){
		/*if(KVZ)
			folderOut = folderOut+slash + "Base-KVZ"+ config["seq"] ;
		else*/
			folderOut = folderOut + slash + "Base-x265" + config["seq"];
	}
	
	else if(mode == ROI_){
		folderOut = folderOut+slash + "ROI"+ config["seq"];
	}	
	else if(mode == LAMBDA_R){
		folderOut = folderOut+slash + "Lambda-ROI"+ config["seq"];
	}

	 #if defined(_WIN32)
    CreateDirectory(folderOut.c_str(),NULL);
     #else 
    mkdir(folderOut.c_str(), 0777); 
     #endif		

	folderOut = folderOut +slash+config["bitrate"];
	#if defined(_WIN32)
    CreateDirectory(folderOut.c_str(),NULL);
     #else 
    mkdir(folderOut.c_str(), 0777); 
     #endif	
	depth_path = folderIn +slash+ "depth.bin";
	encoded_path = folderOut +slash+ "enc.mp4";
	ga_logfile = folderOut +slash+ "log.txt";
	FILE *tmp = fopen(ga_logfile.c_str(), "wb");
	fclose(tmp);//just to remove old contents
	depth_file=fopen(depth_path.c_str(),"rb");	
	raw_yuv=fopen(raw_path.c_str(),"rb");	
	encoded=fopen(encoded_path.c_str(),"wb");		
	if(mode==ROI_ || mode==WEIGHTED_)
		initCL();
	if(mode==ROI_){			
		loadCL("R_Q_DISTANCE.cl",0,"ROI");
	}

	return 0;		
}


//This function should load the raw frame, the user input and try to predict the ROI (e.g. using a neural network model that was trained on different ROIs)
//for now assume the ROI is in the middle of the screen
static void loadROIs(){
	if(frames%ROI_UPDATE_STEP==0){
		ROIs.clear();
		memset(ROI,0,widthDelta *heightDelta * sizeof(float));
		string file_idx=std::to_string((_Longlong)(frames/ROI_UPDATE_STEP));		
		std::ifstream infile(folderIn+slash+"roi"+ file_idx+".txt");
		int category;
		double x,y,w,h;		
		while(infile >> category >> x >> y >> w >> h){
			ROITuple_s r;
			r.x= (x-w/2)*realWidth;
			r.y= (y-h/2)*realHeight;
			r.width= (w)*realWidth;
			r.height= (h)*realHeight;
			r.category = category;
			ROIs.push_back(r);				
		}
		infile.close();		
	}	
}


static float CLIP(float min_, float max_, float value)
{
	return max((min_), min((max_), value));
}

static void clipLambda(double * lambda)
{
#ifdef _WIN32
	if (_isnan(*lambda))
#else
	if (isnan(*lambda))
#endif
	{

		*lambda = MAX_LAMBDA;
	}
	else
	{
		*lambda = CLIP(MIN_LAMBDA, MAX_LAMBDA, (*lambda));
	}
}

static float QPToBits(int QP)
{
	float lambda = exp((QP - C2) / C1);
	float bpp = (float)pow((lambda / ALPHA)*1.0f, 1.0f / BETA);
	return bpp * (float)pow(64.0f, 2.0f);
}

static int LAMBDA_TO_QP(float lambda)
{
    return (int)CLIP(0.0f, 51.0f, (float)roundRC(C1 * log(lambda) + C2));
}

//This function should load the raw frame, the user input and try to predict the ROI
static void updateROIs(){		
	loadROIs();//assume for now ROIs are in a file
	memset(ROI,0,widthDelta *heightDelta * sizeof(float));
	if(mode == ROI_){//simulate blue masks in the paper 45% is high importance and 30% is medium importance
		for(unsigned int r=0;r<ROIs.size();r++){
			unsigned int xTop=ROIs[r].x;
			unsigned int yTop=ROIs[r].y;
			unsigned int xBottom=xTop + ROIs[r].width;
			unsigned int yBottom=yTop + ROIs[r].height;
			xTop = xTop / CU_SIZE;
			yTop = yTop / CU_SIZE;
			xBottom = xBottom / CU_SIZE;
			yBottom = yBottom / CU_SIZE; 		
			for (unsigned int j = yTop; j <= yBottom; j++)
			{
				for (unsigned int k = xTop; k <= xBottom; k++)
				{				
					if(ROIs[r].category==PLAYER)
						ROI[k+j*widthDelta]=0.45;				
					else if(ROIs[r].category==ENEMY)
						ROI[k+j*widthDelta]=0.3;
					else if (ROIs[r].category == INFORMATION_MAP)
						ROI[k + j * widthDelta] = 0.15;
				}			
			}
		}
	}
	
	else if(mode == LAMBDA_R){		
		for(unsigned int r=0;r<ROIs.size();r++){
			unsigned int xTop=ROIs[r].x;
			unsigned int yTop=ROIs[r].y;
			unsigned int xBottom=xTop + ROIs[r].width;
			unsigned int yBottom=yTop + ROIs[r].height;
			xTop = xTop / CU_SIZE;
			yTop = yTop / CU_SIZE;
			xBottom = xBottom / CU_SIZE;
			yBottom = yBottom / CU_SIZE; 		
			for (unsigned int j = yTop; j <= yBottom; j++)
			{
				for (unsigned int k = xTop; k <= xBottom; k++)
				{					
						ROI[k+j*widthDelta]=importance[ROIs[r].category];				
				}			
			}
		}
	}	
	//fwrite(ROI,sizeof(bool),widthDelta*heightDelta,rois);
	//ga_error("finished roi assignment\n");
}







//****************************************************************LAMBDA DOMAIN FUNCTIONS BEGIN******************************************************
//run on CPU as in our calculations we don't need to process every pixel	
// std::sort with inlining
int compare(const void* elem1, const void* elem2)
{
	if (*(const float*)elem1 < *(const float*)elem2)
		return -1;
	return *(const float*)elem1 > *(const float*)elem2;
}

static void meanFilter() {
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
					val = val + 1.0f* weights[index] * filter_mean[a][b];
				}
			}
			weights[block_ind] = val;
			sumWeights = sumWeights + val;
			block_ind++;
		}
	}
}


static void updateDepthCAVELambda(){
	float sumDistance = 0.0f;
	//float * tempDist = (float *)calloc(widthDelta*heightDelta,sizeof(float));
	unsigned int block_ind=0;
	float * temp = (float*)calloc(realWidth*realHeight,sizeof(float));
	memcpy(temp,depth,sizeof(float)*realWidth*realHeight);
	double begin = clock();
	std::qsort(temp, realWidth*realHeight, sizeof(float), compare);
	double end = clock();
	double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;	
	float val=temp[(int)((1-K)*realWidth*realHeight)];	
	int sum_less = 0;
	int sum_up = 0;
	if (reversed == 0) {
		for (unsigned int x = 0;x < heightDelta;x++) {
			for (unsigned int y = 0;y < widthDelta;y++) {
				int x_ind = min(realHeight - 1, x * CU_SIZE + CU_SIZE / 2);
				int y_ind = min(realWidth - 1, y * CU_SIZE + CU_SIZE / 2);
				float cur = depth[x_ind + y_ind * widthDelta];
				for (unsigned int j = x_ind;j < x_ind + CU_SIZE && j < realHeight;j++) {
					for (unsigned int i = y_ind;i < y_ind + CU_SIZE && i < realWidth;i++) {
						cur = min(cur, depth[i + j * realWidth]);
					}
				}

				if (cur <= val) {
					weights[block_ind] = 1;
					sum_less++;
				}

				else {
					weights[block_ind] = 0;
					sum_up++;
				}

				//weights[block_ind] = 1 - cur;
				//ga_error("distance at block %d,%d is %.5f\n",x,y,roi[block_ind]);
				sumDistance = sumDistance + weights[block_ind];
				block_ind++;
			}
		}
	}
	else {
		for (unsigned int x = 0;x < heightDelta;x++) {
			for (unsigned int y = 0;y < widthDelta;y++) {
				int x_ind = min(realHeight - 1, x * CU_SIZE + CU_SIZE / 2);
				int y_ind = min(realWidth - 1, y * CU_SIZE + CU_SIZE / 2);
				float cur = depth[x_ind + y_ind * widthDelta];
				for (unsigned int j = x_ind;j < x_ind + CU_SIZE && j < realHeight;j++) {
					for (unsigned int i = y_ind;i < y_ind + CU_SIZE && i < realWidth;i++) {
						cur = max(cur, depth[i + j * realWidth]);
					}
				}

				if (cur >= val) {
					weights[block_ind] = 1;
					sum_less++;
				}

				else {
					weights[block_ind] = 0;
					sum_up++;
				}

				sumDistance = sumDistance + weights[block_ind];
				block_ind++;
			}
		}
	}
	block_ind = 0;
	for (unsigned int x = 0;x < heightDelta;x++) {
		for (unsigned int y = 0;y < widthDelta;y++) {
			if (weights[block_ind] == 1)
				weights[block_ind] = K / sum_less;
			else
				weights[block_ind] = (1- K) / sum_up;
			block_ind++;
		}
	}
	//meanFilter();


	free(temp);

}




static void updateDistanceCAVELambda() {


	float sumDistance = 0.0f;
	//float * tempDist = (float *)calloc(widthDelta*heightDelta,sizeof(float));
	unsigned int block_ind = 0;

	for (unsigned int x = 0;x < heightDelta;x++) {
		for (unsigned int y = 0;y < widthDelta;y++) {
			
			if (ROI[block_ind]>0.0) {
				float e = 1/(K*ROI[block_ind]);
				weights[block_ind] = exp(-1.0*e);
			}
			else {
				float sumDist = 0.0f;
				int xpixIndex = x * CU_SIZE + CU_SIZE / 2;
				int ypixIndex = y * CU_SIZE + CU_SIZE / 2;
				for (unsigned int r = 0;r<ROIs.size();r++) {
					int xMid = ROIs[r].x + ROIs[r].width / 2;
					int yMid = ROIs[r].y + ROIs[r].height / 2;
					float dist = max(1.0f, (float)sqrt(pow(xpixIndex - yMid, 2.0) + pow(ypixIndex - xMid, 2.0)));					
					float e = (K*dist/ diagonal) / (importance[ROIs[r].category]) ;
					sumDist = sumDist + exp(-1.0*e);
				}

				if (ROIs.size() > 0)//ROI and CAVE
					weights[block_ind] = sumDist / ROIs.size();
			}
			sumDistance = sumDistance + weights[block_ind];
			block_ind++;
		}	
	}
	
	//for(unsigned int x=0;x<heightDelta;x++){
	//	for(unsigned int y=0;y<widthDelta;y++){
	//		if(ROI[block_ind]>0.0){
	//			weights[block_ind]=ROI[block_ind];
	//		}else{
	//			float sumDist = 0.0f;
	//			int xpixIndex= x * CU_SIZE  + CU_SIZE / 2;
	//			int ypixIndex= y * CU_SIZE  + CU_SIZE / 2;				
	//			for(unsigned int r=0;r<ROIs.size();r++){
	//				int xMid = ROIs[r].x + ROIs[r].width /2;
	//				int yMid = ROIs[r].y + ROIs[r].height /2;
	//				float dist = max(1.0f,(float)sqrt(pow(xpixIndex - yMid, 2.0) + pow(ypixIndex - xMid, 2.0)));					
	//				sumDist = sumDist + importance[ROIs[r].category]* log(diagonal / dist) / log(diagonal);
	//			}
	//			if(ROIs.size() > 0)//ROI and CAVE
	//				weights[block_ind]=sumDist/ROIs.size();				
	//		}
	//		//ga_error("distance at block %d,%d is %.5f\n",x,y,roi[block_ind]);
	//		sumDistance = sumDistance + weights[block_ind];
	//		block_ind++;
	//	}
	//}	
	block_ind = 0;
	for(unsigned int x=0;x<heightDelta;x++){
		for(unsigned int y=0;y<widthDelta;y++){	
		/*if(frames==200)
			ga_error("weights at %d,%d is %.5f\n", x, y, weights[block_ind]);*/
		weights[block_ind]=weights[block_ind]/sumDistance;		
		block_ind++;		
		}
	}

	meanFilter();
	//ga_error("sum Weights %.5f\n",sumWeights);
	//free(tempDist);
	//fwrite(roi,sizeof(float),widthDelta*heightDelta,imp);
	//ga_error("calculated distance \n");
}



static void updateParameters(double bits, double pixelsPerBlock, double lambdaReal, double * alpha, double * beta)
{
	double bpp = bits / pixelsPerBlock;
	double lambdaComp = (*alpha) * pow(bpp, *beta);
	clipLambda(&lambdaComp); //in kvazaar but not in Lambda Domain Paper but to avoid 0 bits per pixel for a block
	double logRatio = log(lambdaReal) - log(lambdaComp);
	//positive ratio if lambda real (which was my target) is bigger than the actually computed lambda using the real bpp which means 
	//that the encoder exceeded the target number of bits so this causes that alpha should be increased
	//ga_error("old alpha:%.5f beta:%.5f,",*alpha,*beta);
	*alpha = *alpha + m_alphaUpdate * (logRatio) * (*alpha);
	*alpha = CLIP(0.05f, 20.0f, *alpha); //in kvazaar but not in Lambda Domain Paper
	*beta = *beta + m_betaUpdate * (logRatio) * CLIP(-5.0f, -1.0f, log(bpp)); //in kvazaar but not in Lambda Domain Paper        
	*beta = CLIP(-3.0f, -0.1f, *beta); //in kvazaar but not in Lambda Domain Paper
	//ga_error("lambda comp:%.5f lambda real:%.5f log ratio:%.5f alpha:%.5f beta:%.5f\n",lambdaComp,lambdaReal,logRatio,*alpha,*beta);
}



static void updateParameters()
{	

	float bitsActualSum = last_size;
	sumRate = sumRate + bitsActualSum;	
	if (frames % GOP == 0 && frames > 0)
	{			
		//cout<<sumRate<<endl;	
		sumRate = 0.0f;			
	}
	//return ;
	//fwrite(&bitsActualSum,sizeof(float),1,bits_actual);
	{
		totalBitsUsed = totalBitsUsed + bitsActualSum;
		if (frames % GOP == 0 && frames > 0)
		{			
			totalBitsUsed = 0.0f;
			bitsTotalGroup = max(200.0f, (((1.0f * bitrate / fps) - 0.5 * lambda_buf_occup/fps) * GOP));
			//bitsTotalGroup = max(200.0f,(((1.0f * bitrate / fps)*(frames + SW) - totalBits)*GOP) / SW);
			//ALPHA = 3.2003f;
			//BETA = -1.367f;
		}
		else {
			lambda_buf_occup = lambda_buf_occup + bitsActualSum - bitrate / fps;
		}

		double remaining = bitsTotalGroup - totalBitsUsed;
		double buf_status = min(0,1.0*((0.5*bitrate / fps) - lambda_buf_occup))+ bitrate / fps;
		double next_frame_bits = min(bitrate/fps,remaining / (GOP - (frames % GOP)));
		avg_bits_per_pic = max(100.0,0.9*next_frame_bits+0.1*buf_status);
		//avg_bits_per_pic = max(100.0,next_frame_bits);		
		ga_error("remaining in GOP: %.2f frame bits based on GOP:%.2f buf_occup:%.2f buf_status:%.2f ALPHA:%.2f BETA:%.2f\n", remaining, avg_bits_per_pic, lambda_buf_occup, buf_status, ALPHA, BETA);
		//ga_error("remaining in GOP: %.2f frame bits based on GOP:%.2f buf_occup:%.2f buf_status:%.2f  \n", remaining, next_frame_bits, lambda_buf_occup);
		double bitsTarget = (frames % GOP == 0) ? avg_bits_per_pic * ISCALE : avg_bits_per_pic;
		double targetbppFrame = (1.0 * bitsTarget) / (1.0 * realWidth * realHeight);

		//update ALPHA,BETA only if number of bits exceeds the maximum (penalizing subsequent frames to use less bits) 
		//or tell the next frames to use more bits just in case that the remaining number of bits is enough to encode the rest of the frames		
		//if ((frames - 1) % period == 0 && bitsActualSum > ISCALE * old_avg_bits_per_pic || (frames - 1) % period != 0 && bitsActualSum > old_avg_bits_per_pic)
		//{
			updateParameters(bitsActualSum, (double)(realWidth * realHeight), FRAME_LAMBDA, &ALPHA,  &BETA);
		//}

		FRAME_LAMBDA = ALPHA * pow(targetbppFrame, BETA);
		//if((frames) % period != 0 && (frames-1) % period != 0)
		//FRAME_LAMBDA = CLIP(avgLambda * 0.6299605249474366f, avgLambda * 1.5874010519681994f, FRAME_LAMBDA);// In lambda domain paper
		clipLambda(&FRAME_LAMBDA);
	}
}






static void calcQPLambda()
{
	//ga_error("calcQPLambda \n");
	if (frames == 0)
	{
		double targetbppFrame = (1.0f * avg_bits_per_pic * (ISCALE)) / (1.0f * realWidth * realHeight);
		FRAME_LAMBDA = ALPHA * pow(targetbppFrame, BETA);
		clipLambda(&FRAME_LAMBDA);
	}
	else
		updateParameters();//returns the actual number of bits used for each block in the previous frame


	double bitsAlloc = avg_bits_per_pic;
	if (frames % GOP == 0)
		bitsAlloc = bitsAlloc * ISCALE;
	
	double qp_frame = LAMBDA_TO_QP(ALPHA*pow(bitsAlloc / (realWidth*realHeight),BETA));
	double qp_delta = 0.0;
	double extra = 0.0;
	double sumSaliencyImportantAreas = 0.0;
	double sumSaliency = 0.0;
	int reassigned = 0;
	avgLambda = 0.0f;
	//ga_error("before for loop \n");
    unsigned int block_ind=0;	
	double avg_qp = 0;			
	double sum_non_roi = 0;
	/*if (frames == 0 || frames == 500)
		ga_error("ALPHA:%.2f BETA:%.2f\n", ALPHA, BETA);*/
	for (unsigned int i = block_ind; i < widthDelta*heightDelta; i++)
	{		
			

			sumSaliency = sumSaliency + weights[block_ind];			
			//double assigned = max(1.0f,frames==0?1.0f/(1.0f*widthDelta*heightDelta) * bitsAlloc: base_MAD[block_ind] / (sum_MAD) * bitsAlloc);			
			double assigned = max(1.0f, 1.0f / (1.0f*widthDelta*heightDelta) * bitsAlloc);
			//double assignedW = max(1.0f, weights[block_ind] * bitsAlloc);
			double assignedW = weights[block_ind] * bitsAlloc;
			double targetbpp=0.0f;
            if(mode==BASE_ENCODER_)            
				targetbpp = (assigned) / (pixelsPerBlock);
			else
				targetbpp = (assignedW) / (pixelsPerBlock);


			if ((mode == LAMBDA_R || mode == LAMBDA_D) && ROI[block_ind] == 0) {
				sum_non_roi++;
			}
			double lambdaConst = ALPHA * pow(targetbpp, BETA);//Kiana's idea of using the updated ALPHA/BETA for the whole frame 			
			
			//lambdaConst = CLIP(FRAME_LAMBDA * 0.6299605249474366f, FRAME_LAMBDA * 1.5874010519681994f, lambdaConst);// In lambda domain paper						
			avgLambda = avgLambda+log(lambdaConst); 

			
			//double temp=CLIP(qp_frame-5, qp_frame +5,(double)LAMBDA_TO_QP(lambdaConst));
			double temp = (double)LAMBDA_TO_QP(lambdaConst);
			if (mode == BASE_ENCODER_) {
				temp = qp_frame;				
			}
			qp_delta = qp_delta + temp - qp_frame;			
			QP[block_ind] = temp -QP_BASE;
			/*if (i%widthDelta>=widthDelta / 2)
				QP[block_ind] = 51 - QP_BASE;
			else
				QP[block_ind] = -QP_BASE;*/
			avg_qp = avg_qp + QP[block_ind]+QP_BASE;
			/*if(frames == 200)
				ga_error("qp,bits,weights %d,%d is %.5f,%.5f,%.5f\n",i/widthDelta,i%widthDelta,QP[block_ind]+QP_BASE, weights[block_ind]);*/
			bitsPerBlock[block_ind] = min(QPToBits((int)QP[block_ind]),(double)assigned);
			block_ind++;		
		//writetext2.Write("\n");
	}
	if (qp_delta != 0) {		
		if (mode == LAMBDA_R){	
			//while (qp_delta > 0) {//undershoot
			//	for (block_ind = 0;block_ind < widthDelta*heightDelta && qp_delta>0;block_ind++) {
			//		if (ROI[block_ind] != 0)//decrease ROI QPs -> more gain
			//			QP[block_ind] = max(-QP_BASE+12,QP[block_ind] - 1);//saturation point of quality at QP=12
			//		qp_delta = qp_delta - 1;
			//	}
			//	for (block_ind = 0;block_ind < widthDelta*heightDelta && qp_delta>0;block_ind++) {
			//		if (ROI[block_ind] == 0)//decrease non-ROI QPs as ROIs QPs reached their minimum value (12) and there's still accumulated delta QP
			//			QP[block_ind] = max(-QP_BASE + 12, QP[block_ind] - 1);//saturation point of quality at QP=12
			//		qp_delta = qp_delta - 1;
			//	}
			//}
			while (qp_delta < 0) {//overshoot
				for (block_ind = 0;block_ind < widthDelta*heightDelta && qp_delta<0;block_ind++) {
					if (ROI[block_ind] == 0)//increase non-ROI QPs -> keep ROI gains intact
						QP[block_ind] = min(51-QP_BASE, QP[block_ind] + 1);//make sure QP doesn't exceed 51
					qp_delta = qp_delta + 1;
				}
				//if qp_delta is still below 0 we won't increase the QPs inside the ROIs in order not to sacrifice their quality
			}
		}
	}
	//cout << "alpha:" << ALPHA << ",beta:" << BETA << endl;
	ga_error("qp frame: %.2f , qp delta : %.2f avg qp:%.2f\n" ,qp_frame, qp_delta, avg_qp / (widthDelta*heightDelta));
	avgLambda = exp(avgLambda / (widthDelta*heightDelta));	
	old_avg_bits_per_pic = avg_bits_per_pic;    
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


static void vrc_start() {	
	void * frame = calloc(1.5f*realHeight*realWidth,sizeof(char));
		switch(mode){
		case BASE_ENCODER_:
			while(frames<maxFrames) {										
				fseek(raw_yuv,sizeof(char)*frames*1.5f*realHeight*realWidth,0);
				fread(frame,sizeof(char),1.5f*realHeight*realWidth,raw_yuv);
				calcQPLambda();
				upSample(QP,QP_out);
				vencoder_encode(frame);
				frames++;				
			}
			break;
		case LAMBDA_R:
			while(frames<maxFrames) {										
				fseek(raw_yuv,sizeof(char)*frames*1.5f*realHeight*realWidth,0);
				fread(frame,sizeof(char),1.5f*realHeight*realWidth,raw_yuv);
				updateROIs();
				updateDistanceCAVELambda();		
				calcQPLambda();
				upSample(QP,QP_out);				
				vencoder_encode(frame);
				frames++;				
			}				
		break;
	
		case ROI_:
			while(frames<maxFrames) {										
				fseek(raw_yuv,sizeof(char)*frames*1.5f*realHeight*realWidth,0);
				fread(frame,sizeof(char),1.5f*realHeight*realWidth,raw_yuv);
				updateROIs();
				updateBufferROIAndRunKernels();
				calcQPROI();				
				upSample(QP,QP_out);						
				vencoder_encode(frame);
				frames++;				
			}
		break;
		

	}
	//while (vencoder_encode(NULL));
	free(frame);
	//

}





int
main(int argc, char *argv[]) {
	char * configFileName = (char *)argv[1];//config file contains a config line with the following format: <key1>=<val1>:<key2>=<val2> , expected keys are fps, raw_video_path, width, height, length
	config["bitrate"]=(char *)argv[2];
	config["mode"]=(char *)argv[3];
	config["seq"]=(char *)argv[4];			
	mode = atoi(config["mode"].c_str());
	//if (mode == BASE_ENCODER_) {
	
	//}
	if (mode == LAMBDA_D || mode == LAMBDA_R) {
		K = strtod((char *)argv[5],NULL);
	}

	config["encoder"] = (char *)argv[6];
	//KVZ = (bool)atoi(config["encoder"].c_str());
	
	string f(configFileName);
	config["raw_path"] = f;
	f = f + "conf.txt";
	ifstream infile(f);
	if (infile.good())
	{		
		std::string param;
		while (std::getline(infile, param)) {
			std::stringstream paramStream(param);
			std::string key;
			std::string val;
			std::getline(paramStream, key, '=');
			std::getline(paramStream, val, '=');
			config[key]=val;		
			cout<<key<<"="<<val<<endl;
		}	
		infile.close();
	}

	
	vrc_init();
	vencoder_init();
	vrc_start();
	ga_error("bitrate : %.2f", (totalBits / (maxFrames/fps))/1024);
	return 0;
}

