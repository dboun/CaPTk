#include "SurvivalPredictor.h"
#include "itkCSVNumericObjectFileWriter.h"
#include "CaPTkGUIUtils.h"
#include "CaPTkClassifierUtils.h"


typedef itk::Image< float, 3 > ImageType;
//SurvivalPredictor::~SurvivalPredictor()
//{
//}

VectorDouble SurvivalPredictor::GetStatisticalFeatures(const VectorDouble &intensities)
{
  VectorDouble StatisticalFeatures;

  double temp = 0.0;
  double mean = 0.0;
  double std = 0.0;

  for (unsigned int i = 0; i < intensities.size(); i++)
    temp = temp + intensities[i];
  mean = temp / intensities.size();

  temp = 0;
  for (unsigned int i = 0; i < intensities.size(); i++)
    temp = temp + (intensities[i] - mean)*(intensities[i] - mean);
  std = std::sqrt(temp / (intensities.size() - 1));

  StatisticalFeatures.push_back(mean);
  StatisticalFeatures.push_back(std);

  return StatisticalFeatures;
}

VectorDouble SurvivalPredictor::GetHistogramFeatures(const VectorDouble &intensities, const double &start, const double &interval, const double &end)
{
  VariableLengthVectorType BinCount;
  VectorDouble finalBins;
  VectorVectorDouble Ranges;
  for (double i = start; i <= end; i = i + interval)
  {
    VectorDouble onerange;
    onerange.push_back(i - (interval / 2));
    onerange.push_back(i + (interval / 2));
    Ranges.push_back(onerange);
  }
  Ranges[Ranges.size() - 1][1] = 255;
  Ranges[0][0] = 0;


  BinCount.SetSize(Ranges.size());
  for (unsigned int j = 0; j < Ranges.size(); j++)
  {
    VectorDouble onerange = Ranges[j];
    int counter = 0;
    for (unsigned int i = 0; i < intensities.size(); i++)
    {
      if (onerange[0] == 0)
      {
        if (intensities[i] >= onerange[0] && intensities[i] <= onerange[1])
          counter = counter + 1;
      }
      else
      {
        if (intensities[i] > onerange[0] && intensities[i] <= onerange[1])
          counter = counter + 1;
      }
    }
    finalBins.push_back(counter);
  }
  return finalBins;
}

VectorDouble SurvivalPredictor::GetVolumetricFeatures(const double &edemaSize,const double &tuSize, const double &neSize, const double &totalSize)
{
  VectorDouble VolumetricFeatures;
  VolumetricFeatures.push_back(tuSize);
  VolumetricFeatures.push_back(neSize);
  VolumetricFeatures.push_back(edemaSize);
  VolumetricFeatures.push_back(totalSize);

  VolumetricFeatures.push_back(tuSize + neSize);
  VolumetricFeatures.push_back( (tuSize + neSize) / totalSize);
  VolumetricFeatures.push_back(edemaSize / totalSize);
  return VolumetricFeatures;
}

int SurvivalPredictor::PrepareNewSurvivalPredictionModel(const std::string &inputdirectory,const std::vector< std::map< CAPTK::ImageModalityType, std::string > > &qualifiedsubjects, const std::string &outputdirectory)
{
  VectorDouble AllSurvival; 
  VariableSizeMatrixType FeaturesOfAllSubjects;
  VariableSizeMatrixType HistogramFeaturesConfigurations;
  HistogramFeaturesConfigurations.SetSize(33, 3); //11 modalities*3 regions = 33 configurations*3 histogram features for each configuration
  CSVFileReaderType::Pointer reader = CSVFileReaderType::New();
  MatrixType dataMatrix;
  try
  {
	  reader->SetFileName(getCaPTkDataDir() + "/survival/Survival_HMFeatures_Configuration.csv");
	  reader->SetFieldDelimiterCharacter(',');
	  reader->HasColumnHeadersOff();
	  reader->HasRowHeadersOff();
	  reader->Parse();
	  dataMatrix = reader->GetArray2DDataObject()->GetMatrix();

	  for (unsigned int i = 0; i < dataMatrix.rows(); i++)
		  for (unsigned int j = 0; j < dataMatrix.cols(); j++)
			  HistogramFeaturesConfigurations(i, j) = dataMatrix(i, j);
  }
  catch (const std::exception& e1)
  {
	  logger.WriteError("Cannot find the file 'Survival_HMFeatures_Configuration.csv' in the ../data/survival directory. Error code : " + std::string(e1.what()));
	  return false;
  }
 //---------------------------------------------------------------------------
  FeaturesOfAllSubjects.SetSize(qualifiedsubjects.size(), 161);
  for (unsigned int sid = 0; sid < qualifiedsubjects.size(); sid++)
  {
	  std::cout << "Patient's data loaded:" << sid + 1 << std::endl;
	  std::map< CAPTK::ImageModalityType, std::string > currentsubject = qualifiedsubjects[sid];
	  try
	  {
		  ImageType::Pointer LabelImagePointer = ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_SEG]));
		  ImageType::Pointer AtlasImagePointer = ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_ATLAS]));
		  //ImageType::Pointer TemplateImagePointer = ReadNiftiImage<ImageType>("../data/survival/Template.nii.gz");
      ImageType::Pointer TemplateImagePointer = ReadNiftiImage<ImageType>(getCaPTkDataDir() + "/survival/Template.nii.gz");
		  ImageType::Pointer RCBVImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_RCBV])));
		  ImageType::Pointer PHImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_PH])));
		  ImageType::Pointer T1CEImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_T1CE])));
		  ImageType::Pointer T2FlairImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR])));
		  ImageType::Pointer T1ImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_T1])));
		  ImageType::Pointer T2ImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_T2])));
		  ImageType::Pointer AXImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_AX])));
		  ImageType::Pointer RADImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_RAD])));
		  ImageType::Pointer FAImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_FA])));
		  ImageType::Pointer TRImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_TR])));
		  ImageType::Pointer PSRImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_PSR])));

		  VectorDouble ages;
		  VectorDouble survival;

		  reader->SetFileName(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_FEATURES]));
		  reader->SetFieldDelimiterCharacter(',');
		  reader->HasColumnHeadersOff();
		  reader->HasRowHeadersOff();
		  reader->Parse();
		  dataMatrix = reader->GetArray2DDataObject()->GetMatrix();

		  for (unsigned int i = 0; i < dataMatrix.rows(); i++)
		  {
			  ages.push_back(dataMatrix(i, 0));
			  survival.push_back(dataMatrix(i, 1));
			  AllSurvival.push_back(dataMatrix(i, 1));
		  }

		  VectorDouble TestFeatures = LoadTestData<ImageType>(T1CEImagePointer, T2FlairImagePointer, T1ImagePointer, T2ImagePointer,
			  RCBVImagePointer, PSRImagePointer, PHImagePointer, AXImagePointer, FAImagePointer, RADImagePointer, TRImagePointer, LabelImagePointer, AtlasImagePointer, TemplateImagePointer, HistogramFeaturesConfigurations);

		  FeaturesOfAllSubjects(sid, 0) = ages[0];
		  for (unsigned int i = 1; i <= TestFeatures.size(); i++)
			  FeaturesOfAllSubjects(sid, i) = TestFeatures[i - 1];
	  }
	  catch (const std::exception& e1)
	  {
		  logger.WriteError("Error in calculating the features for patient ID = " + static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_PSR]) + "Error code : " + std::string(e1.what()));
		  return false;
	  }
  }
  std::cout << std::endl << "Building model....." << std::endl;
  VariableSizeMatrixType scaledFeatureSet;
  scaledFeatureSet.SetSize(qualifiedsubjects.size(), 161);
  VariableLengthVectorType meanVector;
  VariableLengthVectorType stdVector;
  mFeatureScalingLocalPtr.ScaleGivenTrainingFeatures(FeaturesOfAllSubjects, scaledFeatureSet, meanVector, stdVector);

  for (unsigned int i = 0; i < scaledFeatureSet.Rows(); i++)
    for (unsigned int j = 0; j < scaledFeatureSet.Cols(); j++)
      if (std::isnan(scaledFeatureSet(i, j)))
        scaledFeatureSet(i, j) = 0;
    //
//  for (int i = 0; i < 105; i++)
//    for (int j = 0; j < 166; j++)
//      data(i, j) = scaledFeatureSet(i, j);
//  writer->SetFileName("scaled_train_features.csv");
//  writer->SetInput(&data);
//  writer->Write();
//
//
//
  //VariableSizeMatrixType ScaledFeatureSetAfterAddingAge;
  //ScaledFeatureSetAfterAddingAge.SetSize(scaledFeatureSet.Rows(), scaledFeatureSet.Cols() + 1);
  //for (unsigned int i = 0; i < scaledFeatureSet.Rows(); i++)
  //{
  //  ScaledFeatureSetAfterAddingAge(i, 0) = ages[i]; 
  //  for (unsigned int j = 0; j < scaledFeatureSet.Cols(); j++)
  //  {
  //    ScaledFeatureSetAfterAddingAge(i, j+1) = scaledFeatureSet(i, j);
  //  }
  //}
//
//  //readerMean->SetFileName("scaledfeatures.csv");
//  //readerMean->SetFieldDelimiterCharacter(',');
//  //readerMean->HasColumnHeadersOff();
//  //readerMean->HasRowHeadersOff();
//  //readerMean->Parse();
//  ////typedef vnl_matrix<double> MatrixType;
//  //dataMatrix = readerMean->GetArray2DDataObject()->GetMatrix();
//
//  //for (unsigned int i = 0; i < dataMatrix.rows(); i++)
//  //  for (unsigned int j = 0; j < dataMatrix.cols(); j++)
//  //    scaledFeatureSet(i, j) = dataMatrix(i, j);
//  //{
//  //  ages.push_back(dataMatrix(i, 0));
//  //  survival.push_back(dataMatrix(i, 1));
//  //}
//
////-----------------------writing in files to compare results------------------------------
  typedef vnl_matrix<double> MatrixType;
  MatrixType data;
  

  VariableSizeMatrixType SixModelFeatures;
  VariableSizeMatrixType EighteenModelFeatures;
  mFeatureExtractionLocalPtr.FormulateSurvivalTrainingData(scaledFeatureSet, AllSurvival, SixModelFeatures, EighteenModelFeatures);

  try
  {
	  data.set_size(161, 1); // TOCHECK - are these hard coded sizes fine?
	  for (unsigned int i = 0; i < meanVector.Size(); i++)
		  data(i, 0) = meanVector[i];
	  typedef itk::CSVNumericObjectFileWriter<double, 161, 1> WriterTypeVector;
	  WriterTypeVector::Pointer writerv = WriterTypeVector::New();
	  writerv->SetFileName(outputdirectory + "/Survival_ZScore_Mean.csv");
	  writerv->SetInput(&data);
	  writerv->Write();

	  for (unsigned int i = 0; i < stdVector.Size(); i++)
		  data(i, 0) = stdVector[i];
	  writerv->SetFileName(outputdirectory + "/Survival_ZScore_Std.csv");
	  writerv->SetInput(&data);
	  writerv->Write();
  }
  catch (const std::exception& e1)
  {
	  logger.WriteError("Error in writing output files to the output directory = " + outputdirectory + "Error code : " + std::string(e1.what()));
	  return false;
  }

//  //---------------------------------------------------------------------------
  VariableSizeMatrixType SixModelSelectedFeatures = SelectSixMonthsModelFeatures(SixModelFeatures);
  VariableSizeMatrixType EighteenModelSelectedFeatures = SelectEighteenMonthsModelFeatures(EighteenModelFeatures);

  //WriteCSVFiles(FeaturesOfAllSubjects, outputdirectory + "/FeaturesOfAllSubjects.csv");
  //WriteCSVFiles(scaledFeatureSet, outputdirectory + "/scaledFeatureSet.csv");
  //WriteCSVFiles(SixModelFeatures, outputdirectory + "/SixModelFeatures.csv");
  //WriteCSVFiles(EighteenModelFeatures, outputdirectory + "/EighteenModelFeatures.csv");
  //WriteCSVFiles(SixModelSelectedFeatures, outputdirectory + "/SixModelSelectedFeatures.csv");
  //WriteCSVFiles(EighteenModelSelectedFeatures, outputdirectory + "/EighteenModelSelectedFeatures.csv");

   try
   {
	   trainOpenCVSVM(SixModelSelectedFeatures, outputdirectory + "/" + mSixTrainedFile, false, CAPTK::ApplicationCallingSVM::Survival);
	   trainOpenCVSVM(EighteenModelSelectedFeatures, outputdirectory + "/" + mEighteenTrainedFile, false, CAPTK::ApplicationCallingSVM::Survival);
   }
   catch (const std::exception& e1)
   {
     logger.WriteError("Training on the given subjects failed. Error code : " + std::string(e1.what()));
     return false;
   }
   std::cout << std::endl << "Model saved to the output directory." << std::endl;
   return true;
}
void SurvivalPredictor::WriteCSVFiles(VariableSizeMatrixType inputdata, std::string filepath)
{
  std::ofstream myfile;
  myfile.open(filepath);
  for (unsigned int index1 = 0; index1 < inputdata.Rows(); index1++)
  {
    for (unsigned int index2 = 0; index2 < inputdata.Cols(); index2++)
    {
      if (index2 == 0)
        myfile << std::to_string(inputdata[index1][index2]);
      else
        myfile << "," << std::to_string(inputdata[index1][index2]);
    }
    myfile << "\n";
  }
}



VariableLengthVectorType SurvivalPredictor::DistanceFunction(const VariableSizeMatrixType &testData, const std::string &filename, const double &rho, const double &bestg)
{
	CSVFileReaderType::Pointer readerMean = CSVFileReaderType::New();
	readerMean->SetFileName(filename);
	readerMean->SetFieldDelimiterCharacter(',');
	readerMean->HasColumnHeadersOff();
	readerMean->HasRowHeadersOff();
	readerMean->Parse();
	MatrixType dataMatrix = readerMean->GetArray2DDataObject()->GetMatrix();

	VariableSizeMatrixType SupportVectors;
	VariableLengthVectorType Coefficients;
	VariableLengthVectorType Distances;

	SupportVectors.SetSize(dataMatrix.rows(), dataMatrix.cols() - 1);
	Coefficients.SetSize(dataMatrix.rows(), 1);
	Distances.SetSize(testData.Rows(), 1);

	for (unsigned int i = 0; i < dataMatrix.rows(); i++)
	{
		unsigned int j = 0;
		for (j = 0; j < dataMatrix.cols() - 1; j++)
			SupportVectors(i, j) = dataMatrix(i, j);
		Coefficients[i] = dataMatrix(i, j);
	}

	for (unsigned int patID = 0; patID < testData.Rows(); patID++)
	{
		double distance = 0;
		for (unsigned int svID = 0; svID < SupportVectors.Rows(); svID++)
		{
			double euclideanDistance = 0;
			for (unsigned int iterator = 0; iterator < SupportVectors.Cols(); iterator++)
				euclideanDistance = euclideanDistance + (SupportVectors(svID, iterator) - testData(patID, iterator))*(SupportVectors(svID, iterator) - testData(patID, iterator));
			double result = std::exp(-1 * bestg*euclideanDistance);
			distance = distance + result*Coefficients[svID];
		}
		Distances[patID] = distance - rho;
	}
	return Distances;
}

VectorDouble SurvivalPredictor::CombineEstimates(const VariableLengthVectorType &estimates1, const VariableLengthVectorType &estimates2)
{
	VectorDouble returnVec;
	returnVec.resize(estimates1.Size());
	for (size_t i = 0; i < estimates1.Size(); i++)
	{
		float temp_abs, temp_pos1, temp_neg1, temp_1, temp_2;
		// estimate for 1st vector
		if (std::abs(estimates1[i]) < 2)
		{
			temp_abs = estimates1[i];
		}
		else
		{
			temp_abs = 0;
		}

		if (estimates1[i] > 1)
		{
			temp_pos1 = 1;
		}
		else
		{
			temp_pos1 = 0;
		}

		if (estimates1[i] < -1)
		{
			temp_neg1 = 1;
		}
		else
		{
			temp_neg1 = 0;
		}
		temp_1 = temp_abs + (temp_pos1 - temp_neg1);

		// estimate for 2nd vector, all temp values are getting overwritten
		if (std::abs(estimates2[i]) < 2)
		{
			temp_abs = estimates2[i];
		}
		else
		{
			temp_abs = 0;
		}

		if (estimates2[i] > 1)
		{
			temp_pos1 = 1;
		}
		else
		{
			temp_pos1 = 0;
		}

		if (estimates2[i] < -1)
		{
			temp_neg1 = 1;
		}
		else
		{
			temp_neg1 = 0;
		}
		temp_2 = temp_abs + (temp_pos1 - temp_neg1);

		// combine the two
		returnVec[i] = temp_1 + temp_2;
	}
	return returnVec;
}


VectorDouble SurvivalPredictor::CombineEstimates(const VectorDouble &estimates1, const VectorDouble &estimates2)
{
	VectorDouble returnVec;
	returnVec.resize(estimates1.size());
	for (size_t i = 0; i < estimates1.size(); i++)
	{
		float temp_abs, temp_pos1, temp_neg1, temp_1, temp_2;
		// estimate for 1st vector
		if (std::abs(estimates1[i]) < 2)
		{
			temp_abs = estimates1[i];
		}
		else
		{
			temp_abs = 0;
		}

		if (estimates1[i] > 1)
		{
			temp_pos1 = 1;
		}
		else
		{
			temp_pos1 = 0;
		}

		if (estimates1[i] < -1)
		{
			temp_neg1 = 1;
		}
		else
		{
			temp_neg1 = 0;
		}
		temp_1 = temp_abs + (temp_pos1 - temp_neg1);

		// estimate for 2nd vector, all temp values are getting overwritten
		if (std::abs(estimates2[i]) < 2)
		{
			temp_abs = estimates2[i];
		}
		else
		{
			temp_abs = 0;
		}

		if (estimates2[i] > 1)
		{
			temp_pos1 = 1;
		}
		else
		{
			temp_pos1 = 0;
		}

		if (estimates2[i] < -1)
		{
			temp_neg1 = 1;
		}
		else
		{
			temp_neg1 = 0;
		}
		temp_2 = temp_abs + (temp_pos1 - temp_neg1);

		// combine the two
		returnVec[i] = temp_1 + temp_2;
	}
	return returnVec;
}

VectorDouble SurvivalPredictor::SurvivalPredictionOnExistingModel(const std::string &modeldirectory, const std::string &inputdirectory, const std::vector < std::map < CAPTK::ImageModalityType, std::string>> &qualifiedsubjects, const std::string &outputdirectory)
{
	typedef itk::CSVArray2DFileReader<double> ReaderType;
	VariableSizeMatrixType HistogramFeaturesConfigurations;
	HistogramFeaturesConfigurations.SetSize(33, 3); //11 modalities*3 regions = 33 configurations*3 histogram features for each configuration
	VectorDouble results;

	CSVFileReaderType::Pointer reader = CSVFileReaderType::New();
	VectorDouble ages;
	MatrixType dataMatrix;
	try
	{
		reader->SetFileName(getCaPTkDataDir() + "/survival/Survival_HMFeatures_Configuration.csv");
		reader->SetFieldDelimiterCharacter(',');
		reader->HasColumnHeadersOff();
		reader->HasRowHeadersOff();
		reader->Parse();
		dataMatrix = reader->GetArray2DDataObject()->GetMatrix();

		for (unsigned int i = 0; i < dataMatrix.rows(); i++)
			for (unsigned int j = 0; j < dataMatrix.cols(); j++)
				HistogramFeaturesConfigurations(i, j) = dataMatrix(i, j);
	}
	catch (const std::exception& e1)
	{
		logger.WriteError("Cannot find the file 'Survival_HMFeatures_Configuration.csv' in the ../data/survival directory. Error code : " + std::string(e1.what()));
		return results;
	}

	MatrixType meanMatrix;
	VariableLengthVectorType mean;
	VariableLengthVectorType stddevition;
	try
	{
		reader->SetFileName(modeldirectory + "/Survival_ZScore_Mean.csv");
		reader->SetFieldDelimiterCharacter(',');
		reader->HasColumnHeadersOff();
		reader->HasRowHeadersOff();
		reader->Parse();
		meanMatrix = reader->GetArray2DDataObject()->GetMatrix();

		mean.SetSize(meanMatrix.size());
		for (unsigned int i = 0; i < meanMatrix.size(); i++)
			mean[i] = meanMatrix(i, 0);
	}
	catch (const std::exception& e1)
	{
		logger.WriteError("Error in reading the file: " + modeldirectory + "/Survival_ZScore_Mean.csv. Error code : " + std::string(e1.what()));
		return results;
	}
	MatrixType stdMatrix;
	try
	{
		reader->SetFileName(modeldirectory + "/Survival_ZScore_Std.csv");
		reader->SetFieldDelimiterCharacter(',');
		reader->HasColumnHeadersOff();
		reader->HasRowHeadersOff();
		reader->Parse();
		stdMatrix = reader->GetArray2DDataObject()->GetMatrix();

		stddevition.SetSize(stdMatrix.size());
		for (unsigned int i = 0; i < stdMatrix.size(); i++)
			stddevition[i] = stdMatrix(i, 0);
	}
	catch (const std::exception& e1)
	{
		logger.WriteError("Error in reading the file: " + modeldirectory + "/Survival_ZScore_Std.csv. Error code : " + std::string(e1.what()));
		return results;
	}
	//----------------------------------------------------
	VariableSizeMatrixType FeaturesOfAllSubjects;
	FeaturesOfAllSubjects.SetSize(qualifiedsubjects.size(), 161);

	for (unsigned int sid = 0; sid < qualifiedsubjects.size(); sid++)
	{
		std::cout << "Subject No:" << sid << std::endl;
		std::map<CAPTK::ImageModalityType, std::string> currentsubject = qualifiedsubjects[sid];
		try
		{
			ImageType::Pointer LabelImagePointer = ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_SEG]));
			ImageType::Pointer AtlasImagePointer = ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_ATLAS]));
			//ImageType::Pointer TemplateImagePointer = ReadNiftiImage<ImageType>("../data/survival/Template.nii.gz");
      ImageType::Pointer TemplateImagePointer = ReadNiftiImage<ImageType>(getCaPTkDataDir() + "/survival/Template.nii.gz");
			ImageType::Pointer RCBVImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_RCBV])));
			ImageType::Pointer PHImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_PH])));
			ImageType::Pointer T1CEImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_T1CE])));
			ImageType::Pointer T2FlairImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR])));
			ImageType::Pointer T1ImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_T1])));
			ImageType::Pointer T2ImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_T2])));
			ImageType::Pointer AXImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_AX])));
			ImageType::Pointer RADImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_RAD])));
			ImageType::Pointer FAImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_FA])));
			ImageType::Pointer TRImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_TR])));
			ImageType::Pointer PSRImagePointer = RescaleImageIntensity<ImageType>(ReadNiftiImage<ImageType>(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_PSR])));

			VectorDouble TestFeatures = LoadTestData<ImageType>(T1CEImagePointer, T2FlairImagePointer, T1ImagePointer, T2ImagePointer,
				RCBVImagePointer, PSRImagePointer, PHImagePointer, AXImagePointer, FAImagePointer, RADImagePointer, TRImagePointer, LabelImagePointer, AtlasImagePointer, TemplateImagePointer, HistogramFeaturesConfigurations);

			double age;

			reader->SetFileName(static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_FEATURES]));
			reader->SetFieldDelimiterCharacter(',');
			reader->HasColumnHeadersOff();
			reader->HasRowHeadersOff();
			reader->Parse();
			dataMatrix = reader->GetArray2DDataObject()->GetMatrix();

			for (unsigned int i = 0; i < dataMatrix.rows(); i++)
				age = dataMatrix(i, 0);

			FeaturesOfAllSubjects(sid, 0) = age;
			for (unsigned int i = 1; i <= TestFeatures.size(); i++)
				FeaturesOfAllSubjects(sid, i) = TestFeatures[i - 1];
		}
		catch (const std::exception& e1)
		{
			logger.WriteError("Error in calculating the features for patient ID = " + static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_PSR]) + "Error code : " + std::string(e1.what()));
			return results;
		}
	}
	VariableSizeMatrixType ScaledTestingData = mFeatureScalingLocalPtr.ScaleGivenTestingFeatures(FeaturesOfAllSubjects, mean, stddevition);
  for (unsigned int i = 0; i < ScaledTestingData.Rows(); i++)
    for (unsigned int j = 0; j < ScaledTestingData.Cols(); j++)
      if (std::isnan(ScaledTestingData(i, j)))
        ScaledTestingData(i, j) = 0;

	VariableSizeMatrixType ScaledFeatureSetAfterAddingLabel;
	ScaledFeatureSetAfterAddingLabel.SetSize(ScaledTestingData.Rows(), ScaledTestingData.Cols() + 1);
	for (unsigned int i = 0; i < ScaledTestingData.Rows(); i++)
	{
		unsigned int j = 0;
		for (j = 0; j < ScaledTestingData.Cols(); j++)
			ScaledFeatureSetAfterAddingLabel(i, j) = ScaledTestingData(i, j);
		ScaledFeatureSetAfterAddingLabel(i, j) = 0;
	}

	VariableSizeMatrixType SixModelSelectedFeatures = SelectSixMonthsModelFeatures(ScaledFeatureSetAfterAddingLabel);
	VariableSizeMatrixType EighteenModelSelectedFeatures = SelectEighteenMonthsModelFeatures(ScaledFeatureSetAfterAddingLabel);
	
  //WriteCSVFiles(FeaturesOfAllSubjects, outputdirectory + "/PlainTestFeaturescsv");
  //WriteCSVFiles(ScaledFeatureSetAfterAddingLabel, outputdirectory + "/ScaledFeatureSetAfterAddingLabel.csv");
  //WriteCSVFiles(SixModelSelectedFeatures, outputdirectory + "/SixModelSelectedFeatures.csv");
  //WriteCSVFiles(EighteenModelSelectedFeatures, outputdirectory + "/EighteenModelSelectedFeatures.csv");
	//------------------------------------------------------------------------------------------------------------------
	//typedef itk::CSVNumericObjectFileWriter<double, 2, 161> WriterTypeMatrix;
	//WriterTypeMatrix::Pointer writermatrix = WriterTypeMatrix::New();
	//MatrixType data;
	//data.set_size(2,161);
	//for (int i = 0; i < 2; i++)
	//	for (int j = 0; j < 161; j++)
	//		data(i, j) = FeaturesOfAllSubjects(i, j);
	//writermatrix->SetFileName("plain_test_features.csv");
	//writermatrix->SetInput(&data);
	//writermatrix->Write();
	//
	//for (int i = 0; i < 2; i++)
	//	for (int j = 0; j < 161; j++)
	//		data(i, j) = ScaledTestingData(i, j);
	//writermatrix->SetFileName("scaled_test_features.csv");
	//writermatrix->SetInput(&data);
	//writermatrix->Write();

	//typedef itk::CSVNumericObjectFileWriter<double, 2, 162> WriterTypeMatrixE;
	//WriterTypeMatrixE::Pointer writermatrixe = WriterTypeMatrixE::New();
	//data.set_size(2, 162);
	//for (int i = 0; i < 2; i++)
	//	for (int j = 0; j < 162; j++)
	//		data(i, j) = ScaledFeatureSetAfterAddingLabel(i, j);
	//writermatrixe->SetFileName("scaled_test_features_labels.csv");
	//writermatrixe->SetInput(&data);
	//writermatrixe->Write();

	//typedef itk::CSVNumericObjectFileWriter<double, 2, 21> WriterTypeMatrixS;
	//WriterTypeMatrixS::Pointer writermatrixs = WriterTypeMatrixS::New();
	//data.set_size(2, 21);
	//for (int i = 0; i < 2; i++)
	//	for (int j = 0; j < 21; j++)
	//		data(i, j) = SixModelSelectedFeatures(i, j);
	//writermatrixs->SetFileName("six_model_features.csv");
	//writermatrixs->SetInput(&data);
	//writermatrixs->Write();

	//for (int i = 0; i < 2; i++)
	//	for (int j = 0; j < 21; j++)
	//		data(i, j) = EighteenModelSelectedFeatures(i, j);
	//writermatrixs->SetFileName("eighteen_model_features.csv");
	//writermatrixs->SetInput(&data);
	//writermatrixs->Write();
	//---------------------------------------------------------------------------------------------------------------	
	

	try
	{
		std::ofstream myfile;
		myfile.open(outputdirectory + "/results.csv");
		myfile << "SubjectName,SPI (6 months), SPI (18 months), Composite SPI\n";
		std::string modeldirectory1 = getCaPTkDataDir() + "/survival";
		if (cbica::fileExists(modeldirectory1 + "/Survival_SVM_Model6.csv") == true && cbica::fileExists(modeldirectory1 + "/Survival_SVM_Model18.csv") == true)
		{
			VariableLengthVectorType result_6;
			VariableLengthVectorType result_18;
			result_6 = DistanceFunction(SixModelSelectedFeatures, modeldirectory1 + "/Survival_SVM_Model6.csv", -1.0927, 0.0313);
			result_18 = DistanceFunction(EighteenModelSelectedFeatures, modeldirectory1 + "/Survival_SVM_Model18.csv", -0.2854, 0.5);
			results = CombineEstimates(result_6, result_18);
			for (size_t i = 0; i < results.size(); i++)
			{
				std::map<CAPTK::ImageModalityType, std::string> currentsubject = qualifiedsubjects[i];
				myfile << static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_SUDOID]) + "," + std::to_string(result_6[i]) + "," + std::to_string(result_18[i]) + "," + std::to_string(results[i]) + "\n";
			}
		}
		else if (cbica::fileExists(modeldirectory1 + "/Survival_SVM_Model6.xml") == true && cbica::fileExists(modeldirectory1 + "/Survival_SVM_Model18.xml") == true)
		{
			VectorDouble result_6;
			VectorDouble result_18;
			result_6 = testOpenCVSVM(ScaledTestingData, modeldirectory1 + "/Survival_SVM_Model6.xml");
			result_18 = testOpenCVSVM(ScaledTestingData, modeldirectory1 + "/Survival_SVM_Model18.xml");
			results = CombineEstimates(result_6, result_18);
			for (size_t i = 0; i < results.size(); i++)
			{
				std::map<CAPTK::ImageModalityType, std::string> currentsubject = qualifiedsubjects[i];
				myfile << static_cast<std::string>(currentsubject[CAPTK::ImageModalityType::IMAGE_TYPE_SUDOID]) + "," + std::to_string(result_6[i]) + "," + std::to_string(result_18[i]) + "," + std::to_string(results[i]) + "\n";
			}
		}
		myfile.close();
	}
	catch (itk::ExceptionObject & excp)
	{
		logger.WriteError("Error caught during testing: " + std::string(excp.GetDescription()));
		return results;
	}
	return results;

}

VariableSizeMatrixType SurvivalPredictor::SelectSixMonthsModelFeatures(const VariableSizeMatrixType &SixModelFeatures)
{
   int selectedFeatures[20] = { 1,    5,    9,    10,    20,    23,    24,    37,    38,    43,    44,    48,    49,    50,    51,    56,    57,    61,    62,    63};

//   int selectedFeatures[20] = { 1, 5, 9, 10, 13, 26, 33, 35, 38, 41, 43, 44, 48, 50, 51, 56, 57, 63, 64, 69 };

   for (unsigned int i = 0; i <20; i++)
    selectedFeatures[i] = selectedFeatures[i] - 1;
  VariableSizeMatrixType SixModelSelectedFeatures; 
  SixModelSelectedFeatures.SetSize(SixModelFeatures.Rows(),21);
  int counter = 0;
  for (unsigned int i = 0; i < 20; i++)
  {
    for (unsigned int j = 0; j < SixModelFeatures.Rows(); j++)
      SixModelSelectedFeatures(j, counter) = SixModelFeatures(j, selectedFeatures[i]);
    counter++;
  }
 for (unsigned int j = 0; j < SixModelFeatures.Rows(); j++)
      SixModelSelectedFeatures(j, 20) = SixModelFeatures(j, 161);

  return SixModelSelectedFeatures;
}

VariableSizeMatrixType SurvivalPredictor::SelectEighteenMonthsModelFeatures(const VariableSizeMatrixType &EighteenModelFeatures)
{
	int selectedFeatures[20] = { 1, 5, 10, 15, 24, 27, 37, 38, 50, 51, 53, 62, 63, 64, 67, 70, 71, 85, 158, 159 };

//   int selectedFeatures[20] = { 1, 2, 3, 5, 6, 7, 9, 10, 16, 18, 33, 35, 38, 42, 47, 48, 49, 50, 51, 56 };
   for(unsigned int i=0;i<20;i++)
    selectedFeatures[i] = selectedFeatures[i] - 1;

  VariableSizeMatrixType EighteenModelSelectedFeatures;
  EighteenModelSelectedFeatures.SetSize(EighteenModelFeatures.Rows(), 21);
  int counter = 0;
  for (unsigned int i = 0; i < 20; i++)
  {
    for (unsigned int j = 0; j < EighteenModelFeatures.Rows(); j++)
      EighteenModelSelectedFeatures(j, counter) = EighteenModelFeatures(j, selectedFeatures[i]);
    counter++;
  }
  for (unsigned int j = 0; j < EighteenModelFeatures.Rows(); j++)
    EighteenModelSelectedFeatures(j, 20) = EighteenModelFeatures(j, 161);
  return EighteenModelSelectedFeatures;
}
