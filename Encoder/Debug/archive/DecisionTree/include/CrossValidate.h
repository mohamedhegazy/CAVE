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


#ifndef CROSS_VALIDATION_H
#define CROSS_VALIDATION_H


namespace libedm
{

	//test a trainer
	template <class StatisticStr> bool CrossValidate(int FoldNum, const CDataset &DataSet, CreateFunc *Creator, const void *Param, vector<StatisticStr> &Statistics)
	{
		const CASE_INFO &Info=DataSet.GetInfo();
		int Size=Info.Height;

		//real number of folds for cross validation
		int SetNum=FoldNum;
		//not enough instances
		if(Size<FoldNum)
			SetNum=Size;
		//data in each training set
		int DataNum=Size/SetNum;

		//equally divided the original data set
		vector<CDataset> TrainSets;
		{
			CDataset TestSet;
			if(!DataSet.SplitData(DataNum,SetNum,TrainSets,TestSet))
				return false;
			//rest data put into last data set
			TrainSets[SetNum-1]+=TestSet;
		}

		//cross validating, i-th data set used as test set
		for(int i=0;i<SetNum;i++)
		{

			//prepare the training data sets
			//the i-th training set used as test set, the rest are combined as the training set for this fold
			CDataset TrainSet;
			for(int j=0;j<SetNum;j++)
			{
				if(SetNum>1 && j==i)
					continue;
				TrainSet+=(TrainSets[j]);
			}

			//create a classifier
			CClassifier *Classifier;
			try
			{		
				Classifier=Creator(TrainSet,Param);
			}
			catch (CError &e)
			{
				cout<<e.Description<<endl;
				if(e.Level<=0)
					return false;
			}
			//test the classifier
			CPrediction *Prediction=Classifier->Classify(TrainSets[i]);

			//create a new statistical object for return
			StatisticStr Statistic(TrainSets[i],Prediction);
			delete Prediction;
			delete Classifier;

			Statistics.push_back(Statistic);
		}

		return true;
	} 

	//test a classifier
	template <class StatisticStr> bool CrossValidate(int FoldNum, const CDataset &DataSet, const CClassifier *Classifier, vector<StatisticStr> &Statistics)
	{
		const CASE_INFO &Info=DataSet.GetInfo();
		int Size=Info.Height;

		//real number of folds for cross validation
		int SetNum=FoldNum;
		//not enough instances
		if(Size<FoldNum)
			SetNum=Size;
		//data in each training set
		int DataNum=Size/SetNum;

		//equally divided the original data set
		vector<CDataset> TrainSets;
		{
			CDataset TestSet;
			if(!DataSet.SplitData(DataNum,SetNum,TrainSets,TestSet))
				return false;
			//rest data put into last data set
			TrainSets[SetNum-1]+=TestSet;
		}

		//cross validating, i-th data set used as test set
		for(int i=0;i<SetNum;i++)
		{

			//prepare the training data sets
			//the i-th training set used as test set, the rest are combined as the training set for this fold
			CDataset TrainSet;
			for(int j=0;j<SetNum;j++)
			{
				if(SetNum>1 && j==i)
					continue;
				TrainSet+=(TrainSets[j]);
			}

			//test the classifier
			CPrediction *Prediction=Classifier->Classify(TrainSets[i]);

			//create a new statistical object for returned prediction results
			StatisticStr Statistic(TrainSets[i],Prediction);
			delete Prediction;

			Statistics.push_back(Statistic);
		}

		return true;
	} 
}


#endif

