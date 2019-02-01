/*
Copyright (c) 2014, Qiangli Zhao and Yanhuang Jiang
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

* Neither the name of the copyright holders nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


#ifndef OBJECT_INC
#define OBJECT_INC

#include <string>
#include <vector>
#include <cstdlib>
#include <climits>

namespace libedm
{
	const int LIBEP_VERSION=001;//version of LibEP
	const int MAX_FILE_PATH=255;//Max path length for a file
	const int MAX_OBJECT_NAME_LENGTH=32;

	class CObj
	{
	public:
		string GetName() const
		{
			return Name;
		};
		double GetCreateTime() const
		{
			return CreatingTime;
		};
	protected:
		string	Name;//Name of an object
		double	CreatingTime;//Time consumed by object creating
		int		Ref;//Number of objects using this object
	};

	class CError
	{
	public:
		string	Description;
		int		Code;
		int		Level;
		char	*tmp;
		CError(const string &Desc,const int &Cd, const int &Lvl)
			:Description(Desc),Code(Cd),Level(Lvl){}
		~CError(){}
	};

	//roulette implementing weighted sampling
	//this function is implemented in RandSequence.cpp
	int IntRand(int Max);
	class CRoulette
	{
	public:
		CRoulette(const vector<double> &Weights)
		{
			double Sum=0;
			for(int i=0;i<(int)Weights.size();i++)
				Sum+=Weights[i];

			double Intv=0;
			for(int i=0;i<(int)Weights.size();i++)
			{
				Intv+=Weights[i];
				Roulette.push_back(Intv/Sum);
			}
		}

		int Poll()
		{
			double r=(double)IntRand(INT_MAX)/(INT_MAX-1);

			int RouSize=(int)Roulette.size();
			int i;
			for(i=0;i<RouSize;i++)
				if(r<=Roulette[i])
					break;
			if(i>=RouSize)
				i=RouSize-1;

			return i;
		}

	private:
		vector<double> Roulette;
	};

typedef vector<double>		DoubleArray;
typedef vector<DoubleArray>	DoubleArray2d;
typedef vector<float>		FloatArray;
typedef vector<FloatArray>	FloatArray2d;
typedef vector<int>			IntArray;
typedef vector<bool>		BoolArray;
typedef vector<string>		StringArray;

#define pure_fill(vt, h, w, a)		\
	for(int i=0;i<h;i++)			\
	{								\
		vector<double> tmp;			\
		for(int j=0;j<w;j++)		\
			tmp.push_back(a);		\
		vt.push_back(tmp);			\
	}

#define new_d1(type,arr, w, a)		\
	type *arr=new type[w];			\
	for(int i=0;i<w;i++)			\
		arr[i]=a;					
#define fill_d1(type,arr, w, a)		\
{									\
	arr=new type[w];				\
	for(int i=0;i<w;i++)			\
		arr[i]=a;					\
}
#define free_d1(arr)				\
	if(arr!=NULL)					\
		delete [] arr;

#define new_d2(type,arr, h, w, a)	\
	type **arr=new type*[h];		\
	for(int i=0;i<h;i++)			\
		arr[i]=new type[w];			\
	for(int i=0;i<h;i++)			\
		for(int j=0;j<w;j++)		\
			arr[i][j]=a;			
#define fill_d2(type,arr, h, w, a)	\
{									\
	arr=new type*[h];				\
	for(int i=0;i<h;i++)			\
		arr[i]=new type[w];			\
	for(int i=0;i<h;i++)			\
		for(int j=0;j<w;j++)		\
			arr[i][j]=a;			\
}
#define free_d2(arr, h)				\
	if(arr!=NULL)					\
	{								\
		for(int i=0;i<h;i++)			\
			if(arr[i]!=NULL)		\
				delete [] arr[i];	\
		delete [] arr;				\
	}

}//namespace

#endif

