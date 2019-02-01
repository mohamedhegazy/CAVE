#include <string>
#include <iostream>
#include <fstream>
#include <istream>
#include <sstream>
using namespace std;
#include "Obj.h"
#include "DataSet.h"
#include "UCIData.h"
#include "Classifier.h"
#include "C45.h"
#include "bpnn.h"
#include "Prediction.h"
#include "CrossValidate.h"
using namespace libedm;

double diffclock(clock_t clock1,clock_t clock2)
{
	double diffticks=clock1-clock2;
	double diffms=(diffticks*1000)/CLOCKS_PER_SEC;
	return diffms;
} 

class StAccuracy
{
public:
	StAccuracy(const CDataset &Data,const CPrediction *Prediction)
	{
		Accuracy=Prediction->GetAccuracy();
	};

	double GetResult(){return Accuracy;};

private:
	double Accuracy;
};
int main(int argc, char* argv[])
{
	//read training data from file
	try
	{
		CUCIData Original,TrainSet,TestSet;
		Original.Load("C:\\Users\\mhegazy\\Desktop\\survival.names","C:\\Users\\mhegazy\\Desktop\\survival.data");
		//remove the instances whose label are unknown
		//TrainSet.RemoveUnknownInstance();
		const CASE_INFO &Info=Original.GetInfo();
		int Sample_num=(int)(Info.Height*0.8);
		cout<<Sample_num<<endl;
		//the rest as testing
		//Original.SplitData(Sample_num,TrainSet,TestSet);
		cout<<"splitted\n";
		//training a BP neural network by the RPORP (fast) method
		try
		{
			clock_t begin=clock();
			/*vector<StAccuracy> Accuracys;
			double AverageAccuracy=0;
			int Cross=10;
			CrossValidate<StAccuracy>(Cross,Original,CC45::Create,NULL,Accuracys);
			for(int i=0;i<Cross;i++)
			{
				double Acc=Accuracys[i].GetResult();
				cout<<"accuracy for round "<<i<<" is: "<<Acc<<endl;
				AverageAccuracy+=Acc;
			}
			AverageAccuracy/=Cross;
			cout<<"average accuracy is: "<<AverageAccuracy<<endl;*/
			CC45 BP1(TrainSet);
			//dump to check
			clock_t end=clock();
			cout<<diffclock(end,begin)<<endl;
			BP1.Dump("BP1.dmp");
			//save this BP neural network into a file under the current directory
			BP1.Save("","BP1.sav");

			//use the BPNN to predict the original dataset
			begin=clock();
			const CPrediction *Result=BP1.Classify(TestSet);
			 end=clock();
			 cout<<diffclock(end,begin)<<endl;
			//predicting accuracy
			cout<<"predictive accuracy:"<<Result->GetAccuracy()<<endl;
			delete Result;

			//clone a new classifier to predict
			CClassifier *Cls=BP1.Clone();
			//use the BPNN to predict the original dataset
			Result=Cls->Classify(TrainSet);
			//predicting accuracy
			cout<<"Cloned predictive accuracy:"<<Result->GetAccuracy()<<endl;
			delete Cls;

			//extract information of the prediction
			const vector<int> &Predicted=Result->GetPredictedLabelIndices();
			const vector<bool> &Correct=Result->GetCorrectness();
			/*for(int i=0;i<(int)Predicted.size();i++)
			{
				cout<<"Instance "<<i<<": predicted label="<<Predicted[i]<<
					", Is correct?"<<(Correct[i]?"Yes":"No")<<endl;
			}*/
			delete Result;

		}
		catch (CError &e)
		{
			cout<<e.Description<<endl;
			if(e.Level<=0)
			{
				while(true);
				return 0;
			}
		}
	}
	catch (CError &e)
	{
		cout<<e.Description<<endl;
		while(true);
		if(e.Level<=0)
			return 0;
	}
	while(true);

	return 0;
}

