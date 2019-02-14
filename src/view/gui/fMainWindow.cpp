//\*file  fMainWindow.cpp
//
//brief Implementation of fMainWindow class
//
//http://www.med.upenn.edu/sbia/software/ <br>
//software@cbica.upenn.edu
//
//Copyright (c) 2018 University of Pennsylvania. All rights reserved. <br>
//See COPYING file or https://www.med.upenn.edu/sbia/software-agreement.html
//
//*/
/////
#include "fMainWindow.h"
#include "OutputInteractorStyleNavigator.h"
#include "SimpleImageManager.h"
#include "fHelpDialog.h"
#include "EGFRvIIISurrogateIndex.h"
#include "TrainingModule.h"
#include "GeodesicSegmentation.h"
#include "GeodesicTrainingSegmentation.h"
#include "N3BiasCorrection.h"
#include "SusanDenoising.h"
#include "WhiteStripe.h"
#include "PerfusionPCA.h"
#include "PerfusionDerivatives.h"
#include "DiffusionDerivatives.h"
#include "DeepMedicNormalizer.h"
#include "SBRT_LungField.h"
#include "SBRT_Nodule.h"
#include "SBRT_Analysis.h"

#include "cbicaITKSafeImageIO.h"
#include "itkFlipImageFilter.h"

#include "lddmm_common.h"
#include "lddmm_data.h"
#include "GreedyAPI.h"

#include <vnl/vnl_cost_function.h>
#include <vnl/vnl_random.h>
#include <vnl/algo/vnl_powell.h>
#include <vnl/algo/vnl_svd.h>
#include <vnl/vnl_trace.h>

#include "cbicaCmdParser.h"

#include "Registry.h"

#include "DirectionalityEstimate.h"
#include "SlicerManager.h"
#include "Slicer.h"
#include "InteractorStyleNavigator.h"
#include "CaPTkUtils.h"
#include "CaPTkGUIUtils.h"

#include "vtkTransform.h"
#include "vtkImageMapToWindowLevelColors.h"
#include "vtkLookupTable.h"

// this function calls an external application from CaPTk in the most generic way while waiting for output
int fMainWindow::startExternalProcess(const QString &application, const QStringList &arguments)
{
  cbica::Logging(loggerFile, application.toStdString() + " " + arguments.join(" ").toStdString());
  return std::system((application.toStdString() + " " + arguments.join(" ").toStdString()).c_str());
#ifdef _WIN32
  //QProcess process;
  //process.setStandardOutputFile((m_tempFolderLocation + "/process_" + application.toStdString() + ".log").c_str());
  //if (arguments.isEmpty())
  //{
  //  if (QFileInfo(application).completeSuffix() == "py")
  //  {
  //    process.start("python.exe " + application + ".py");
  //  }
  //  else
  //  {
  //    process.start(application);
  //  }
  //}
  //else
  //{
  //  if (QFileInfo(application).completeSuffix() == "py")
  //  {
  //    process.start("python.exe " + application + ".py", arguments);
  //  }
  //  else
  //  {
  //    process.start(application, arguments);
  //  }
  //}
  //process.write("exit\n\r");
  //process.waitForFinished(-1);
  //process.close();

  //return process.exitCode();
#else
  //std::string args_string = ""/*arguments.join(" ")*/, app_string = application.toStdString();
  //for (size_t i = 0; i < arguments.size(); i++)
  //{
  //  args_string += " " + arguments[i].toStdString();
  //}

  ////if (cbica::getFilenameExtension(application.toStdString()) == ".py")
  ////{
  ////  app_string = "python " + application.toStdString();
  ////}

  //return std::system((app_string + args_string).c_str());
#endif

}

int GetNumberOfDimensions(vtkImageData* input)
{
  int dim = 0;
#if VTK_MAJOR_VERSION <= 5
  int* extent = input->GetWholeExtent();
#else
  int* extent = input->GetExtent();
#endif
  if (extent[4] != extent[5])
  {
    dim = 3;
  }
  else if (extent[3] != extent[4])
  {
    dim = 2;
  }
  else if (extent[0] != extent[1])
  {
    dim = 1;
  }
  return dim;
}

inline std::string correctExtension(const std::string &inputFileName)
{
  std::string returnString = inputFileName, tempPath, tempBase, tempExt;
  cbica::splitFileName(returnString, tempPath, tempBase, tempExt);
  if (tempExt.empty())
  {
    returnString += NII_GZ_EXT;
  }

  return returnString;
}

fMainWindow::fMainWindow()
{

  setupUi(this);

  this->bottomLayout = new QHBoxLayout();

  help_discussion = new QAction(this);
  help_forum = new QAction(this);
  help_bugs = new QAction(this);
  help_features = new QAction(this);
  help_download = new QAction(this);
  actionLoad_Recurrence_Images = new QAction(this);
  actionLoad_Nifti_Images = new QAction(this);
  actionLoad_Nifti_ROI = new QAction(this);
  actionSave_Nifti_Images = new QAction(this);
  actionSave_Dicom_Images = new QAction(this);
  actionSave_ROI_Images = new QAction(this);
  actionSave_ROI_Dicom_Images = new QAction(this);
  actionExit = new QAction(this);
  actionAppEGFR = new QAction(this);
  actionAppRecurrence = new QAction(this);
  actionAppGeodesic = new QAction(this);
  actionAppGeodesicTraining = new QAction(this);
  actionHelp_Interactions = new QAction(this);
  actionAbout = new QAction(this);

  //---------------setting menu and status bar for the main window---------------
  this->setStatusBar(statusbar);

  menubar = new QMenuBar(this);
  menuFile = new QMenu("File");
  menuLoadFile = new QMenu("Load");
  menuSaveFile = new QMenu("Save");
  menuExit = new QMenu("Exit");
  menuLoadFileDicom = new QMenu("Dicom");
  menuLoadFileNifti = new QMenu("Nifti");
  menuFile->addMenu(menuLoadFile);
  menuFile->addMenu(menuSaveFile);
  menuApp = new QMenu("Applications");
  menuPreprocessing = new QMenu("Preprocessing");
  menuHelp = new QMenu("Help");

  SaggitalViewWidget.reset(new QVTKOpenGLWidget(SaggitalWidget));
  AxialViewWidget.reset(new QVTKOpenGLWidget(AxialWidget));
  CoronalViewWidget.reset(new QVTKOpenGLWidget(CoronalWidget));

  SaggitalRenWin = vtkSmartPointer< vtkGenericOpenGLRenderWindow>::New();
  AxialRenWin = vtkSmartPointer< vtkGenericOpenGLRenderWindow>::New();
  CoronalRenWin = vtkSmartPointer< vtkGenericOpenGLRenderWindow>::New();

  SaggitalViewWidget->SetRenderWindow(SaggitalRenWin);
  AxialViewWidget->SetRenderWindow(AxialRenWin);
  CoronalViewWidget->SetRenderWindow(CoronalRenWin);

  SaggitalViewWidget->setMouseTracking(true);
  AxialViewWidget->setMouseTracking(true);
  CoronalViewWidget->setMouseTracking(true);

  SaggitalWidgetGridLayout->addWidget(SaggitalViewWidget.data(), 0, 0, 1, 1);
  AxialWidgetGridLayout->addWidget(AxialViewWidget.data(), 0, 0, 1, 1);
  CoronalWidgetGridLayout->addWidget(CoronalViewWidget.data(), 0, 0, 1, 1);

  QSizePolicy sizePolicy5(QSizePolicy::Preferred, QSizePolicy::Expanding);
  sizePolicy5.setHorizontalStretch(0);
  sizePolicy5.setVerticalStretch(0);

  infoPanel = new fBottomImageInfoTip(centralwidget);
  imagesPanel = new fImagesPanel(); // New Images Panel
  m_tabWidget->addTab(imagesPanel, QString());
  tumorPanel = new fTumorPanel();
  m_tabWidget->addTab(tumorPanel, QString());
  drawingPanel = new fDrawingPanel();
  featurePanel = new fFeaturePanel();
  m_tabWidget->addTab(drawingPanel, QString());
  m_tabWidget->addTab(featurePanel, "Feature Extraction");
  int minheight = /*std::max(drawingPanel->sizeHint().height(), featurePanel->sizeHint().height())*/featurePanel->sizeHint().height() + 25;
  m_tabWidget->setMinimumHeight(minheight);
  m_tabWidget->setMaximumHeight(m_tabWidget->minimumHeight());

  m_toolTabdock->setWindowFlags(Qt::Window);

#ifdef Q_OS_WIN
  m_toolTabdock->setFeatures(QDockWidget::DockWidgetFloatable);
#else
  //TBD fix this - work around untill solved
  m_toolTabdock->setFeatures(QDockWidget::NoDockWidgetFeatures);
#endif
  m_toolTabdock->setWidget(m_tabWidget);
  overallGridLayout->addWidget(m_toolTabdock, 0, 0, 1, 3);

  QFrame * frame = new QFrame(this);
  sizePolicy5.setHeightForWidth(frame->sizePolicy().hasHeightForWidth());
  frame->setSizePolicy(sizePolicy5);
  frame->setFrameShape(QFrame::HLine);
  frame->setFrameShadow(QFrame::Sunken);

  overallGridLayout->addWidget(frame, 3, 0, 1, 3);
  
  this->setCentralWidget(centralwidget);
  AxialViewWidget->raise();
  CoronalViewWidget->raise();
  SaggitalViewWidget->raise();
  infoPanel->raise();
  m_tabWidget->raise();

  menuHelp->addAction(actionHelp_Interactions);
  menuDownload = menuHelp->addMenu("Sample Data");
  auto supportMenu = menuHelp->addMenu("Support Links");
  menuHelp->addAction(actionAbout);

  supportMenu->addAction(help_bugs);
  supportMenu->addAction(help_download);

  menubar->addMenu(menuFile);
  menubar->addMenu(menuPreprocessing);
#ifndef PACKAGE_VIEWER
  menubar->addMenu(menuApp);
#endif
  menubar->addMenu(menuHelp);
  this->setMenuBar(menubar);

  menubar->addAction(menuFile->menuAction());
  menubar->addAction(menuPreprocessing->menuAction());
#ifndef PACKAGE_VIEWER
  menubar->addAction(menuApp->menuAction());
#endif
  menubar->addAction(menuHelp->menuAction());

  menuLoadFile->addAction(actionLoad_Nifti_Images);
  menuLoadFile->addAction(actionLoad_Nifti_ROI);

  menuSaveFile->addAction(actionSave_Nifti_Images);
  menuSaveFile->addAction(actionSave_ROI_Images);

  menuFile->addAction(actionExit);

  menuDownload->addAction("GreedyRegistration");
  m_tabWidget->setCurrentIndex(0);

  bottomLayout->addWidget(infoPanel);
  bottomLayout->addStretch();
  bottomLayout->addWidget(preferencesGroupBox);
  overallGridLayout->addLayout(bottomLayout, 4, 0, 2, 3);

  std::string nonNativeAppPaths_wrap = std::string(CAPTK_APP_LIST_PY_GUI);
  if (nonNativeAppPaths_wrap[0] == ' ')
  {
    nonNativeAppPaths_wrap.erase(0, 1);
  }
  nonNativeAppPaths_wrap = nonNativeAppPaths_wrap + " itksnap";
  m_pyGUIApps = cbica::stringSplit(nonNativeAppPaths_wrap, " ");
  nonNativeAppPaths_wrap = std::string(CAPTK_APP_LIST_PY_CLI);
  if (nonNativeAppPaths_wrap[0] == ' ')
  {
    nonNativeAppPaths_wrap.erase(0, 1);
  }

  m_pyCLIApps = cbica::stringSplit(nonNativeAppPaths_wrap, " ");
  size_t allAppCounter = 0;
  for (size_t i = 0; i < m_pyGUIApps.size(); i++)
  {
    if (m_pyGUIApps[i] == "confetti")
    {
      m_pyGUIApps[i] = "ConfettiGUI";
    }
    if ((m_pyGUIApps[i] == "librabatch") || (m_pyGUIApps[i] == "librasingle"))
    {
      m_pyGUIApps[i] = "libra";
    }
    if (m_pyGUIApps[i] == "SBRT_Segment")
    {
      m_pyGUIApps[i] = "SBRT_Lung_Segment";
    }
    if (m_pyGUIApps[i] == "SBRT_Analyze")
    {
      m_pyGUIApps[i] = "SBRT_Lung_Analyze";
    }

    m_allNonNativeApps[m_pyGUIApps[i]] = getApplicationPath(m_pyGUIApps[i]);
    allAppCounter++;
  }

  for (size_t i = 0; i < m_pyCLIApps.size(); i++)
  {
    m_allNonNativeApps[m_pyCLIApps[i]] = getApplicationPath(m_pyCLIApps[i]);
  }

  // TBD: this needs to be controlled from CMake and not hard-coded here
  auto brainAppList = " EGFRvIIISVMIndex EGFRvIIISurrogateIndex RecurrenceEstimator PseudoProgressionEstimator SurvivalPredictor MolecularSubtypePredictor PopulationAtlases WhiteStripe confetti";
  std::string breastAppList = "";

#ifndef APPLE
  breastAppList = " librasingle librabatch";
#endif

  auto lungAppList = " LungField Nodule Analysis";
  std::string miscAppList = " DirectionalityEstimate DiffusionDerivatives PerfusionDerivatives PerfusionPCA TrainingModule";
  std::string segAppList = " itksnap GeodesicSegmentation GeodesicTrainingSegmentation";
#ifdef WIN32
  segAppList += " deepmedic";
#endif
  auto preProcessingAlgos = " DCM2NIfTI BiasCorrect-N3 Denoise-SUSAN GreedyRegistration HistogramMatching DeepMedicNormalizer";

  vectorOfGBMApps = populateStringListInMenu(brainAppList, this, menuApp, "Glioblastoma", false);
  menuApp->addSeparator();
  if (!breastAppList.empty())
  {
    vectorOfBreastApps = populateStringListInMenu(breastAppList, this, menuApp, "Breast Cancer", false);
    menuApp->addSeparator();
  }
  vectorOfLungApps = populateStringListInMenu(lungAppList, this, menuApp, "Lung Cancer", false);
  menuApp->addSeparator();
  vectorOfSegmentationApps = populateStringListInMenu(segAppList, this, menuApp, "Segmentation", false);
  vectorOfMiscApps = populateStringListInMenu(miscAppList, this, menuApp, "Miscellaneous", false);
  vectorOfPreprocessingActionsAndNames = populateStringListInMenu(preProcessingAlgos, this, menuPreprocessing, "", false);

  menuDownload->addAction("All");
  for (const auto &currentActionAndName : vectorOfGBMApps)
  {
    if (currentActionAndName.name != "Glioblastoma")
    {
      if (currentActionAndName.name == "confetti")
      {
        menuDownload->addAction("Confetti");
      }
      else
      {
        menuDownload->addAction(currentActionAndName.name.c_str());
      }
    }
  }

  bool libraCheck = false;
  for (const auto &currentActionAndName : vectorOfBreastApps)
  {
    if (currentActionAndName.name != "Breast Cancer")
    {
      if (!libraCheck)
      {
        if (currentActionAndName.name.find("libra") != std::string::npos)
        {
          libraCheck = true;
          menuDownload->addAction("LIBRA");
        }
      }
    }
  }

  bool sbrtCheck = false;
  for (const auto &currentActionAndName : vectorOfLungApps)
  {
    if (currentActionAndName.name != "Lung Cancer")
    {
      if (!sbrtCheck)
      {
        if ((currentActionAndName.name.find("LungField") != std::string::npos) ||
          (currentActionAndName.name.find("Nodule") != std::string::npos) ||
          (currentActionAndName.name.find("Analysis") != std::string::npos))
        {
          sbrtCheck = true;
          menuDownload->addAction("LungCancer");
        }
      }
    }
  }

  for (const auto &currentActionAndName : vectorOfMiscApps)
  {
    if (currentActionAndName.name != "Miscellaneous")
    {
      if ((currentActionAndName.name != "itksnap") && (currentActionAndName.name != "deepmedic"))
      {
        if (!currentActionAndName.name.empty())
        {
          menuDownload->addAction(currentActionAndName.name.c_str());
        }
      }
    }
  }


  featurePanel->setListner(this);//TBD bad design RK
  featurePanel->setListner(this);//TBD bad design RK
  m_imagesTable = imagesPanel->GetImagesTable();
  m_nonVisImagesTable = imagesPanel->GetNonViewingImagesTable();
  assert(m_imagesTable != NULL);
  assert(m_nonVisImagesTable != NULL);

  mSequenceNumber = 0;

  t1cePath = "";

  QString msg = tr(EXE_NAME);
#ifdef SW_VER
  msg += " - v" + tr(SW_VER);
#endif
  this->setWindowTitle(msg);

#ifdef Q_OS_WIN32
  currentPlatform = "windows";
#endif

  mInputPathName = "";
  //mMainWidget = this;
  mCurrentSelectedImageId = "";
  mCurrentPickedImageId = "";
  mCurrentPickedImageIndex = 0;
  mCurrentNearPoints = 0;
  mCurrentFarPoints = 0;
  mCurrentInitPoints = 0;
  mCustomImageToThreshold = itk::Image< short, 3 >::New();
  mProjectVariant = std::string(PROJECT_VARIANT);

  connect(featurePanel, SIGNAL(helpClicked_FeaUsage(std::string)), this, SLOT(help_contextual(std::string)));
  connect(&registrationPanel, SIGNAL(Registrationsignal(std::string, std::vector<std::string>, std::vector<std::string>, std::vector<std::string>, bool, std::string, bool, std::string, std::string)), this, SLOT(Registration(std::string, std::vector<std::string>, std::vector<std::string>, std::vector<std::string>, bool, std::string, bool, std::string, std::string)));

  m_tempFolderLocation = loggerFolder + "tmp_" + cbica::getCurrentProcessID();
  if (cbica::directoryExists(m_tempFolderLocation))
  {
    auto temp = cbica::stringSplit(cbica::getCurrentLocalTime(), ":");
    m_tempFolderLocation += temp[0] + temp[1] + temp[2] + "/";
  }
  cbica::createDir(m_tempFolderLocation);
  featurePanel->setTempFolderLocation(m_tempFolderLocation);

  mLandmarks = new Landmarks(LANDMARK_TYPE::DEFAULT);
  mSeedPoints = new Landmarks(LANDMARK_TYPE::TUMOR_POINTS);
  mTissuePoints = new Landmarks(LANDMARK_TYPE::TISSUE_POINTS);

  mMask = vtkSmartPointer<vtkImageData>::New();

  mSlicerManagers.resize(0);

  image4DSlider->setEnabled(false);
  image4DSlider->setValue(0);

  connect(imagesPanel, SIGNAL(sigOverlayCheckBoxChanged(int)), this, SLOT(overlayUseStateChanged(int)));
  connect(imagesPanel, SIGNAL(sigOverlaySliderChanged(int)), this, SLOT(overlaySliderChanged(int)));
  connect(imagesPanel, SIGNAL(sigOverlayChanged()), this, SLOT(overlayChanged()));
  connect(imagesPanel, SIGNAL(sigTheiaClicked()), this, SLOT(ApplicationTheia()));
  connect(imagesPanel, SIGNAL(sigImageModalityChanged(int)), this, SLOT(imageModalityChanged(int)));
  connect(imagesPanel, SIGNAL(helpClicked_Interaction(std::string)), this, SLOT(help_contextual(std::string)));

  connect(image4DSlider, SIGNAL(valueChanged(int)), this, SLOT(imageSliderChanged()));
  SetPresetComboBox();

  // init the sliders
  verticalSliders.push_back(AxialViewSlider);
  verticalSliders.push_back(CoronalViewSlider);
  verticalSliders.push_back(SaggitalViewSlider);
  for (int i = 0; i < 3; i++)
  {
    verticalSliders[i]->hide();
  }
  connect(AxialViewSlider, SIGNAL(valueChanged(int)), this, SLOT(AxialViewSliderChanged()));
  connect(CoronalViewSlider, SIGNAL(valueChanged(int)), this, SLOT(CoronalViewSliderChanged()));
  connect(SaggitalViewSlider, SIGNAL(valueChanged(int)), this, SLOT(SaggitalViewSliderChanged()));

  connect(actionLoad_Recurrence_Images, SIGNAL(triggered()), this, SLOT(openImages()));
  connect(actionLoad_Nifti_Images, SIGNAL(triggered()), this, SLOT(openImages()));

  connect(actionSave_ROI_Images, SIGNAL(triggered()), this, SLOT(SaveDrawing()));
  connect(actionSave_ROI_Dicom_Images, SIGNAL(triggered()), this, SLOT(SaveDicomDrawing()));
  connect(actionSave_Nifti_Images, SIGNAL(triggered()), this, SLOT(SaveImage()));
  connect(actionSave_Dicom_Images, SIGNAL(triggered()), this, SLOT(SaveDicomImage()));

  connect(actionLoad_Nifti_ROI, SIGNAL(triggered()), this, SLOT(LoadDrawing()));

  connect(actionExit, SIGNAL(triggered()), this, SLOT(close()));
  connect(actionAbout, SIGNAL(triggered()), this, SLOT(about()));
  connect(actionHelp_Interactions, SIGNAL(triggered()), this, SLOT(help_Interactions()));
  connect(help_discussion, SIGNAL(triggered()), this, SLOT(help_Discussion()));
  connect(help_download, SIGNAL(triggered()), this, SLOT(help_Downloads()));
  connect(help_forum, SIGNAL(triggered()), this, SLOT(help_HelpForum()));
  connect(help_bugs, SIGNAL(triggered()), this, SLOT(help_BugTracker()));
  connect(help_features, SIGNAL(triggered()), this, SLOT(help_FeatureRequests()));

  connect(menuDownload, SIGNAL(triggered(QAction*)), this, SLOT(help_Download(QAction*)));

  connect(&mHelpTutorial, SIGNAL(skipTutorialOnNextRun(bool)), this, SLOT(skipTutorial(bool)));

  for (size_t i = 0; i < vectorOfGBMApps.size(); i++)
  {
    if (vectorOfGBMApps[i].name.find("EGFRvIIISurrogate") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  Glioblastoma EGFRvIII Surrogate Index (PHI Estimator)"); // TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationEGFR()));
    }
    if (vectorOfGBMApps[i].name.find("EGFRvIIISVM") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  Glioblastoma EGFRvIII SVM Index"); // TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationEGFRvIIISVM()));
    }
    else if (vectorOfGBMApps[i].name.find("Recurrence") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  Glioblastoma Infiltration Index"); // TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationRecurrence()));
    }
    else if (vectorOfGBMApps[i].name.find("PseudoProgression") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  Glioblastoma PseudoProgression Infiltration Index"); // TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationPseudoProgression()));
    }
    else if (vectorOfGBMApps[i].name.find("Survival") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  Glioblastoma Survival Prediction Index"); // TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationSurvival()));
    }
    else if (vectorOfGBMApps[i].name.find("PopulationAtlases") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  Population Atlas"); // TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationPopulationAtlas()));
    }
    else if (vectorOfGBMApps[i].name.find("ImagingSubtype") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  Glioblastoma Imaging Subtype Predictor"); // TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationImagingSubtype()));
    }
    else if (vectorOfGBMApps[i].name.find("MolecularSubtypePredictor") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  Glioblastoma Molecular Subtype Predictor"); // TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationMolecularSubtype()));
    }
    else if (vectorOfGBMApps[i].name.find("WhiteStripe") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  WhiteStripe Normalization"); // TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationWhiteStripe()));
    }
    else if (vectorOfGBMApps[i].name.find("confetti") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  Confetti"); //TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationConfetti()));
    }
    else if (vectorOfGBMApps[i].name.find("DirectionalityEstimate") != std::string::npos)
    {
      vectorOfGBMApps[i].action->setText("  Directionality Estimator"); //TBD set at source
      connect(vectorOfGBMApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationDirectionality()));
    }
  }

  for (size_t i = 0; i < vectorOfBreastApps.size(); i++)
  {
    if (vectorOfBreastApps[i].name.find("librasingle") != std::string::npos)
    {
      vectorOfBreastApps[i].action->setText("  Breast Density Estimator (LIBRA) SingleImage"); //TBD set at source
      connect(vectorOfBreastApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationLIBRASingle()));
    }
    else if (vectorOfBreastApps[i].name.find("librabatch") != std::string::npos)
    {
      vectorOfBreastApps[i].action->setText("  Breast Density Estimator (LIBRA) BatchMode"); //TBD set at source
      connect(vectorOfBreastApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationLIBRABatch()));
    }
  }

  for (size_t i = 0; i < vectorOfLungApps.size(); i++)
  {
	  if (vectorOfLungApps[i].name.find("LungField") != std::string::npos)
	  {
		  vectorOfLungApps[i].action->setText("  Lung Field Segmentation"); 
		  connect(vectorOfLungApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationSBRTLungField()));
	  }
    if (vectorOfLungApps[i].name.find("Nodule") != std::string::npos)
    {
      vectorOfLungApps[i].action->setText("  Lung Nodule Segmentation");
      connect(vectorOfLungApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationSBRTNodule()));
    }
    if (vectorOfLungApps[i].name.find("Analysis") != std::string::npos)
    {
      vectorOfLungApps[i].action->setText("  Prognostic Modeling");
      connect(vectorOfLungApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationSBRTAnalysis()));
    }
  }

  for (size_t i = 0; i < vectorOfSegmentationApps.size(); i++)
  {
    if (vectorOfSegmentationApps[i].name.find("itksnap") != std::string::npos)
    {
      vectorOfSegmentationApps[i].action->setText("  ITK-SNAP"); //TBD set at source
      connect(vectorOfSegmentationApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationITKSNAP()));
    }
    else if (vectorOfSegmentationApps[i].name.find("GeodesicSegmentation") != std::string::npos)
    {
      vectorOfSegmentationApps[i].action->setText("  Geodesic Segmentation"); // TBD set at source
      connect(vectorOfSegmentationApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationGeodesic()));
    }
    else if (vectorOfSegmentationApps[i].name.find("GeodesicTrainingSegmentation") != std::string::npos)
    {
      vectorOfSegmentationApps[i].action->setText("  Geodesic Training Segmentation"); // TBD set at source
      connect(vectorOfSegmentationApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationGeodesicTraining()));
    }
    else if (vectorOfSegmentationApps[i].name.find("deepmedic") != std::string::npos)
    {
      vectorOfSegmentationApps[i].action->setText("  DeepMedic Segmentation (Brain)"); // TBD set at source
      connect(vectorOfSegmentationApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationDeepMedicSegmentation()));
    }
  }

  for (size_t i = 0; i < vectorOfMiscApps.size(); i++)
  {
    if (vectorOfMiscApps[i].name.find("DirectionalityEstimate") != std::string::npos)
    {
      vectorOfMiscApps[i].action->setText("  Directionality Estimator"); //TBD set at source
      connect(vectorOfMiscApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationDirectionality()));
    }
    else if (vectorOfMiscApps[i].name.find("PerfusionPCA") != std::string::npos)
    {
      vectorOfMiscApps[i].action->setText("  PCA Volume Extraction"); //TBD set at source
      connect(vectorOfMiscApps[i].action, SIGNAL(triggered()), this, SLOT(ApplicationPCA()));
    }
    else if (vectorOfMiscApps[i].name.find("PerfusionDerivatives") != std::string::npos)
    {
      vectorOfMiscApps[i].action->setText("  Perfusion Derivatives"); //TBD set at source
      connect(vectorOfMiscApps[i].action, SIGNAL(triggered()), this, SLOT(PerfusionMeasuresCalculation()));
    }
    else if (vectorOfMiscApps[i].name.find("PerfusionAlignment") != std::string::npos)
    {
      vectorOfMiscApps[i].action->setText("  Perfusion Alignment");
      connect(vectorOfMiscApps[i].action, SIGNAL(triggered()), this, SLOT(PerfusionAlignmentCalculation()));
    }
    else if (vectorOfMiscApps[i].name.find("DiffusionDerivatives") != std::string::npos)
    {
      vectorOfMiscApps[i].action->setText("  Diffusion Derivatives"); //TBD set at source
      connect(vectorOfMiscApps[i].action, SIGNAL(triggered()), this, SLOT(DiffusionMeasuresCalculation()));
    }
    else if (vectorOfMiscApps[i].name.find("TrainingModule") != std::string::npos)
    {
      vectorOfMiscApps[i].action->setText("  Training Module"); //TBD set at source
      connect(vectorOfMiscApps[i].action, SIGNAL(triggered()), this, SLOT(ClassifierTraining()));
    }
  }

  // add a single function for all preprocessing steps, this function will check for the specific names and then initiate that algorithm
  for (size_t i = 0; i < vectorOfPreprocessingActionsAndNames.size(); i++)
  {
    if (vectorOfPreprocessingActionsAndNames[i].name.find("Denoise") != std::string::npos)
    {
      connect(vectorOfPreprocessingActionsAndNames[i].action, SIGNAL(triggered()), this, SLOT(ImageDenoising()));
    }

    else if (vectorOfPreprocessingActionsAndNames[i].name.find("BiasCorrect") != std::string::npos)
    {
      vectorOfPreprocessingActionsAndNames[i].action->setText("BiasCorrection");//TBD set at source
      connect(vectorOfPreprocessingActionsAndNames[i].action, SIGNAL(triggered()), this, SLOT(ImageBiasCorrection()));
    }
    else if (vectorOfPreprocessingActionsAndNames[i].name.find("GreedyRegistration") != std::string::npos)
    {
      vectorOfPreprocessingActionsAndNames[i].action->setText("Registration");
      connect(vectorOfPreprocessingActionsAndNames[i].action, SIGNAL(triggered()), this, SLOT(ImageRegistration()));
    }
    else if (vectorOfPreprocessingActionsAndNames[i].name.find("HistogramMatching") != std::string::npos)
    {
      connect(vectorOfPreprocessingActionsAndNames[i].action, SIGNAL(triggered()), this, SLOT(ImageHistogramMatching()));
    }
    else if (vectorOfPreprocessingActionsAndNames[i].name.find("DeepMedicNormalizer") != std::string::npos)
    {
      connect(vectorOfPreprocessingActionsAndNames[i].action, SIGNAL(triggered()), this, SLOT(ImageDeepMedicNormalizer()));
    }
    else if (vectorOfPreprocessingActionsAndNames[i].name.find("SkullStripping") != std::string::npos)
    {
      vectorOfPreprocessingActionsAndNames[i].action->setText("Skull Stripping");
      vectorOfPreprocessingActionsAndNames[i].action->setDisabled(true);
      connect(vectorOfPreprocessingActionsAndNames[i].action, SIGNAL(triggered()), this, SLOT(ImageSkullStripping()));
    }
    else if (vectorOfPreprocessingActionsAndNames[i].name.find("DCM2NIfTI") != std::string::npos)
    {
      vectorOfPreprocessingActionsAndNames[i].action->setText("DICOM to NIfTI");
      connect(vectorOfPreprocessingActionsAndNames[i].action, SIGNAL(triggered()), this, SLOT(DCM2NIfTIConversion()));
    }
  }

  connect(&fetalbrainpanel, SIGNAL(skullstripfun()), this, SLOT(skullstripfunc()));
  connect(&fetalbrainpanel, SIGNAL(drawlinear()), this, SLOT(Predict()));
  connect(&fetalbrainpanel, SIGNAL(TrainNewFetalModel(std::string, std::string)), this, SLOT(TrainNewFetalModel(const std::string &, const std::string &)));


  connect(m_imagesTable, SIGNAL(itemSelectionChanged()), this, SLOT(DisplayChanged()));
  connect(m_imagesTable, SIGNAL(itemClicked(QTableWidgetItem*)), this, SLOT(DisplayChanged(QTableWidgetItem*)));

  connect(imagesPanel, SIGNAL(sigImageTableSelectionChanged()), this, SLOT(DisplayChanged()));

  connect(windowSpinBox, SIGNAL(editingFinished()), this, SLOT(WindowLevelEdited()));
  connect(levelSpinBox, SIGNAL(editingFinished()), this, SLOT(WindowLevelEdited()));

  connect(presetComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateWindowLevel()));
  connect(thresholdSpinBox, SIGNAL(valueChanged(double)), this, SLOT(thresholdSpinBoxChanged()));
  thresholdSpinBox->setValue(20);



  connect(tumorPanel, SIGNAL(UpdateRenderWindows()), this, SLOT(UpdateRenderWindows()));
  connect(tumorPanel, SIGNAL(SetActiveLandmarksTypeSignal(int, int, int)), this, SLOT(SetActiveLandmarksType(int, int, int)));
  connect(tumorPanel, SIGNAL(MoveSlicerCursor(double, double, double)), this, SLOT(MoveSlicerCursor(double, double, double)));
  connect(tumorPanel, SIGNAL(helpClicked_Interaction(std::string)), this, SLOT(help_contextual(std::string)));

  connect(drawingPanel, SIGNAL(clearMask(int)), this, SLOT(clearMask(int)));
  connect(drawingPanel, SIGNAL(CurrentBrushSizeChanged(int)), this, SLOT(ChangeBrushSize(int)));
  connect(drawingPanel, SIGNAL(UndoButtonClicked()), this, SLOT(UndoFunctionality()));
  //connect(drawingPanel, SIGNAL(FillButtonClicked(int)), this, SLOT(FillLabel(int)));
  connect(drawingPanel, SIGNAL(shapesButtonClicked(int)), this, SLOT(updateDrawMode(int)));
  connect(drawingPanel, SIGNAL(CurrentDrawingLabelChanged(int)), this, SLOT(updateDrawMode()));
  connect(drawingPanel, SIGNAL(CurrentMaskOpacityChanged(int)), this, SLOT(ChangeMaskOpacity(int)));
  connect(drawingPanel, SIGNAL(helpClicked_Interaction(std::string)), this, SLOT(help_contextual(std::string)));


  connect(&recurrencePanel, SIGNAL(SubjectBasedRecurrenceEstimate(std::string, bool, bool, bool, bool)), this, SLOT(StartRecurrenceEstimate(const std::string &, bool, bool, bool, bool)));
  connect(&recurrencePanel, SIGNAL(SubjectBasedExistingRecurrenceEstimate(std::string, std::string, bool, bool, bool, bool)), this, SLOT(LoadedSubjectExistingRecurrenceEstimate(const std::string &, const std::string &, bool, bool, bool, bool)));
  connect(&recurrencePanel, SIGNAL(ExistingModelBasedRecurrenceEstimate(std::string, std::string, std::string, bool, bool, bool, bool)), this, SLOT(RecurrenceEstimateOnExistingModel(const std::string &, const std::string &, const std::string &, bool, bool, bool, bool)));
  connect(&recurrencePanel, SIGNAL(TrainNewModel(std::string, std::string, bool, bool, bool, bool)), this, SLOT(TrainNewModelOnGivenData(const std::string &, const std::string &, bool, bool, bool, bool)));

  connect(&pseudoPanel, SIGNAL(ExistingModelBasedPseudoprogressionEstimate(std::string, std::string, std::string, bool, bool, bool, bool)), this, SLOT(PseudoprogressionEstimateOnExistingModel(const std::string &, const std::string &, const std::string &, bool, bool, bool, bool)));
  connect(&pseudoPanel, SIGNAL(TrainNewPseudoModel(std::string, std::string, bool, bool, bool, bool)), this, SLOT(TrainNewPseudoprogressionModelOnGivenData(const std::string &, const std::string &, bool, bool, bool, bool)));


  connect(&survivalPanel, SIGNAL(SurvivalPredictionOnExistingModel(const std::string, const std::string, const std::string)), this, SLOT(CallForSurvivalPredictionOnExistingModelFromMain(const std::string, const std::string, const std::string)));
  connect(&survivalPanel, SIGNAL(PrepareNewSurvivalPredictionModel(const std::string, const std::string)), this, SLOT(CallForNewSurvivalPredictionModelFromMain(const std::string, const std::string)));

  connect(&egfrv3Panel, SIGNAL(EGFRvIIIPredictionOnExistingModel(const std::string, const std::string, const std::string)), this, SLOT(CallForEGFRvIIIPredictionOnExistingModelFromMain(const std::string, const std::string, const std::string)));
  connect(&egfrv3Panel, SIGNAL(PrepareNewEGFRvIIIPredictionModel(const std::string, const std::string)), this, SLOT(CallForNewEGFRvIIIPredictionModelFromMain(const std::string, const std::string)));


  connect(&msubtypePanel, SIGNAL(MolecularSubtypePredictionOnExistingModel(const std::string, const std::string, const std::string)), this, SLOT(CallForMolecularSubtypePredictionOnExistingModelFromMain(const std::string, const std::string, const std::string)));
  connect(&msubtypePanel, SIGNAL(PrepareNewMolecularSubtypePredictionModel(const std::string, const std::string)), this, SLOT(CallForNewMolecularSubtypePredictionModelFromMain(const std::string, const std::string)));


  connect(&skullStrippingPanel, SIGNAL(RunSkullStripping(const std::string, const std::string, const std::string, const std::string)), this, SLOT(CallImageSkullStripping(const std::string, const std::string, const std::string, const std::string)));
  connect(&dcmConverter, SIGNAL(RunDICOMConverter(const std::string, const std::string)), this, SLOT(CallDCM2NIfTIConversion(const std::string, const std::string)));
  connect(&histoMatchPanel, SIGNAL(RunHistogramMatching(const std::string, const std::string, const std::string)), this, SLOT(CallImageHistogramMatching(const std::string, const std::string, const std::string)));
  connect(&deepMedicNormPanel, SIGNAL(RunDeepMedicNormalizer(const std::string, const std::string, const std::string, const std::string, const std::string, const std::string, const std::string, bool)), this, SLOT(CallImageDeepMedicNormalizer(const std::string, const std::string, const std::string, const std::string, const std::string, const std::string, const std::string, bool)));
  connect(&directionalityEstimator, SIGNAL(RunDirectionalityEstimator(const std::string, const std::string, const std::string)), this, SLOT(CallDirectionalityEstimator(const std::string, const std::string, const std::string)));
  connect(&pcaPanel, SIGNAL(RunPCAEstimation(const int, const std::string)), this, SLOT(CallPCACalculation(const int, const std::string)));
  connect(&trainingPanel, SIGNAL(RunTrainingSimulation(const std::string, const std::string, const std::string, int, int, int)), this, SLOT(CallTrainingSimulation(const std::string, const std::string, const std::string, int, int, int)));

  connect(&perfmeasuresPanel, SIGNAL(RunPerfusionMeasuresCalculation(const double, const bool, const bool, const bool, const std::string, const std::string)), this, SLOT(CallPerfusionMeasuresCalculation(const double, const bool, const bool, const bool, const std::string, const std::string)));
  connect(&diffmeasuresPanel, SIGNAL(RunDiffusionMeasuresCalculation(const std::string, const std::string, const std::string, const std::string, const bool, const bool, const bool, const bool, const std::string)), this,
    SLOT(CallDiffusionMeasuresCalculation(const std::string, const std::string, const std::string, const std::string, const bool, const bool, const bool, const bool, const std::string)));

  connect(&whiteStripeNormalizer, SIGNAL(RunWhiteStripe(double, int, int, int, double, double, int, bool, const std::string)), this, SLOT(CallWhiteStripe(double, int, int, int, double, double, int, bool, const std::string)));

  connect(&atlasPanel, SIGNAL(GeneratePopualtionAtlas(const std::string, const std::string, const std::string, const std::string)), this, SLOT(CallGeneratePopualtionAtlas(const std::string, const std::string, const std::string, const std::string)));

  connect(&nodulePanel, SIGNAL(SBRTNoduleParamReady(const std::string, const int)), this, SLOT(CallSBRTNodule(const std::string, const int)));

  connect(&deepMedicDialog, SIGNAL(RunDeepMedic(const std::string)), this, SLOT(CallDeepMedicSegmentation(const std::string)));

  connect(this, SIGNAL(SeedPointsFocused(bool)), tumorPanel, SLOT(sTableFocused(bool)));
  connect(this, SIGNAL(TissuePointsFocused(bool)), tumorPanel, SLOT(tTableFocused(bool)));
  connect(m_tabWidget, SIGNAL(currentChanged(int)), this, SLOT(panelChanged(int)));
  connect(infoPanel, SIGNAL(MoveSlicerCursor(double, double, double, int)), this, SLOT(MoveSlicerCursor(double, double, double, int)));

  AxialViewWidget->hide();
  CoronalViewWidget->hide();
  SaggitalViewWidget->hide();
  infoPanel->show();

  windowLabel->setEnabled(false);
  windowSpinBox->setEnabled(false);
  levelSpinBox->setEnabled(false);
  levelLabel->setEnabled(false);

  thresholdLabel->setEnabled(false);
  thresholdSpinBox->setEnabled(false);
  presetLabel->setEnabled(false);
  presetComboBox->setEnabled(false);

  setAcceptDrops(true);

  mCustomImageToThreshold_min = 0;
  mCustomImageToThreshold_max = 0;

  m_drawShapeMode = SHAPE_MODE_NONE;

  m_messageLabel = new QLabel(":");
  statusBar()->addPermanentWidget(m_messageLabel);
  m_progressBar = new QProgressBar(this);
  QSize tempSize = this->size();
  m_progressBar->setFixedWidth(tempSize.width() * 0.75);
  statusBar()->addPermanentWidget(m_progressBar);
  m_progressBar->setValue(0);

  mHelpDlg = new fHelpDialog();

  //connect
  connect(m_toolTabdock, SIGNAL(topLevelChanged(bool)), this, SLOT(toolTabDockChanged(bool)));
  
  recurrencePanel.SetCurrentLoggerPath(m_tempFolderLocation);
  msubtypePanel.SetCurrentLoggerPath(m_tempFolderLocation);
  survivalPanel.SetCurrentLoggerPath(m_tempFolderLocation);

  //
  actionLoad_Nifti_Images->setText(QApplication::translate("fMainWindow", "Image(s)", 0));
  actionLoad_Nifti_ROI->setText(QApplication::translate("fMainWindow", "ROI", 0));

  actionSave_Nifti_Images->setText(QApplication::translate("fMainWindow", "Image (NIfTI)", 0));
  actionSave_Dicom_Images->setText(QApplication::translate("fMainWindow", "Image (DICOM)", 0));
  actionSave_ROI_Images->setText(QApplication::translate("fMainWindow", "ROI (NIfTI)", 0));
  actionSave_ROI_Dicom_Images->setText(QApplication::translate("fMainWindow", "ROI (DICOM)", 0));
  actionHelp_Interactions->setText(QApplication::translate("fMainWindow", "Usage", 0));
  help_discussion->setText(QApplication::translate("fMainWindow", "Discussion Forum", 0));
  help_forum->setText(QApplication::translate("fMainWindow", "Help Forum", 0));
  help_bugs->setText(QApplication::translate("fMainWindow", "Bugs and Feature", 0));
  help_features->setText(QApplication::translate("fMainWindow", "Feature Requests", 0));
  help_download->setText(QApplication::translate("fMainWindow", "Latest Downloads", 0));
  actionAbout->setText(QApplication::translate("fMainWindow", "About", 0));
  actionExit->setText(QApplication::translate("fMainWindow", "Exit", 0));
  actionAppGeodesic->setText(QApplication::translate("fMainWindow", "Geodesic segmentation", 0));
  actionAppGeodesicTraining->setText(QApplication::translate("fMainWindow", "Geodesic Training Segmentation", 0));
  m_tabWidget->setTabText(m_tabWidget->indexOf(tumorPanel), QApplication::translate("fMainWindow", "Seed Points", 0));
  m_tabWidget->setTabText(m_tabWidget->indexOf(drawingPanel), QApplication::translate("fMainWindow", "Drawing", 0));
  m_tabWidget->setTabText(m_tabWidget->indexOf(imagesPanel), QApplication::translate("fMainWindow", "Images", 0));
}

fMainWindow::~fMainWindow()
{
  for (int i = 0; i < (int)mSlicerManagers.size(); i++)
  {
    if (mSlicerManagers[i] != NULL)
    {
      delete mSlicerManagers[i];
    }
  }
  if (mLandmarks)
  {
    delete mLandmarks;
  }
  if (mSeedPoints)
  {
    delete mSeedPoints;
  }
  if (mTissuePoints)
  {
    delete mTissuePoints;
  }

  if (cbica::filesInDirectory(m_tempFolderLocation).empty())
  {
    cbica::deleteDir(m_tempFolderLocation);
  }

  if (m_skipTutorialOnNextRun)
  {
    std::ofstream file;
    file.open(tutorialScreen.c_str());
    file << "User doesn't need the tutorial screen.\n";
    file.close();
  }

  if (mHelpDlg)
    delete mHelpDlg;

}

void fMainWindow::ConversionFrom2Dto3D(const std::string &fileName, bool loadAsImage)
{
  using ImageTypeFloat2D = itk::Image< float, 2 >;
  auto reader = itk::ImageFileReader< ImageTypeFloat2D >::New();
  reader->SetFileName(fileName);
  auto ext = cbica::getFilenameExtension(fileName);
  if (ext == ".dcm")
  {
    reader->SetImageIO(itk::GDCMImageIO::New());
  }
  else if ((ext == ".nii") || (ext == ".nii.gz"))
  {
    reader->SetImageIO(itk::NiftiImageIO::New());
  }

  try
  {
    reader->Update();
  }
  catch (itk::ExceptionObject& e)
  {
    ShowErrorMessage("Exception caught while reading the image '" + fileName + "':\n\n" + e.what());
    return;
  }

  auto image_2D = reader->GetOutput();
  auto index2D = image_2D->GetLargestPossibleRegion().GetIndex();
  auto size2D = image_2D->GetLargestPossibleRegion().GetSize();
  auto spacing2D = image_2D->GetSpacing();
  auto origin2D = image_2D->GetOrigin();

  // write into m_tempFolderLocation and then read pass that file to LoadSlicerImages
  auto image_3D = ImageTypeFloat3D::New();
  ImageTypeFloat3D::RegionType region;
  ImageTypeFloat3D::RegionType::SizeType size;
  ImageTypeFloat3D::RegionType::IndexType start;
  ImageTypeFloat3D::PointType origin;
  ImageTypeFloat3D::SpacingType spacing;

  // populate the region to place the slice in
  size[0] = size2D[0];
  size[1] = size2D[1];
  size[2] = 1;
  start[0] = index2D[0];
  start[1] = index2D[1];
  start[2] = 0;
  spacing[0] = image_2D->GetSpacing()[0];
  spacing[1] = image_2D->GetSpacing()[1];
  spacing[2] = 1;
  origin[0] = origin2D[0];
  origin[1] = origin2D[1];
  origin[2] = 0;

  region.SetSize(size);
  region.SetIndex(start);
  image_3D->SetRegions(region);
  image_3D->SetRequestedRegion(region);
  image_3D->SetBufferedRegion(region);
  image_3D->Allocate();
  image_3D->FillBuffer(0);
  image_3D->SetOrigin(origin);
  image_3D->SetSpacing(spacing);

  itk::ImageRegionIteratorWithIndex <ImageTypeFloat3D> iter(image_3D, image_3D->GetLargestPossibleRegion());
  itk::ImageRegionIteratorWithIndex <ImageTypeFloat2D> iter2d(image_2D, image_2D->GetLargestPossibleRegion());

  for (iter.GoToBegin(), iter2d.GoToBegin(); !iter2d.IsAtEnd(); ++iter, ++iter2d)
  {
    iter.Set(iter2d.Get());
  }

  auto imageName = m_tempFolderLocation + "/" + cbica::getFilenameBase(fileName) + ".nii.gz";
  cbica::WriteImage< ImageTypeFloat3D >(image_3D, imageName);

  if (loadAsImage)
  {
    LoadSlicerImages(imageName, CAPTK::NIfTI);
  }
  else
  {
    readMaskFile(imageName);
  }
}

void fMainWindow::about()
{
#if CAPTK_PACKAGE_PROJECT
  mHelpTutorial.exec();
#endif
}

void fMainWindow::help_Interactions()
{
  mHelpDlg->show();
}

void fMainWindow::help_Discussion()
{
  std::string link = "https://www.nitrc.org/forum/forum.php?forum_id=6500", command;
#if WIN32
  command = "start ";
#elif __linux__
  command = "xdg-open ";
#else
  command = "open ";
#endif
  if (std::system((command + link).c_str()) != 0)
  {
    ShowErrorMessage("CaPTk couldn't open the browser to open the Discussion Forum");
    return;
  }
}

void fMainWindow::help_Downloads()
{
  std::string link = "https://www.nitrc.org/frs/?group_id=1059", command;
#if WIN32
  command = "start ";
#elif __linux__
  command = "xdg-open ";
#else
  command = "open ";
#endif
  if (std::system((command + link).c_str()) != 0)
  {
    ShowErrorMessage("CaPTk couldn't open the browser to open the Downloads page");
    return;
  }
}

void fMainWindow::help_HelpForum()
{
  std::string link = "https://www.nitrc.org/forum/forum.php?forum_id=6501", command;
#if WIN32
  command = "start ";
#elif __linux__
  command = "xdg-open ";
#else
  command = "open ";
#endif
  if (std::system((command + link).c_str()) != 0)
  {
    ShowErrorMessage("CaPTk couldn't open the browser to open the Help Forum");
    return;
  }
}

void fMainWindow::help_BugTracker()
{
  std::string link = "https://github.com/CBICA/CaPTk/issues", command;
#if WIN32
  command = "start ";
#elif __linux__
  command = "xdg-open ";
#else
  command = "open ";
#endif
  if (std::system((command + link).c_str()) != 0)
  {
    ShowErrorMessage("CaPTk couldn't open the browser to open the Bug Tracker");
    return;
  }
}

void fMainWindow::help_FeatureRequests()
{
  std::string link = "https://www.nitrc.org/tracker/?atid=3995&group_id=1059&func=browse ", command;
#if WIN32
  command = "start ";
#elif __linux__
  command = "xdg-open ";
#else
  command = "open ";
#endif
  if (std::system((command + link).c_str()) != 0)
  {
    ShowErrorMessage("CaPTk couldn't open the browser to open the Feature Requests");
    return;
  }
}

void fMainWindow::EnableThresholdOfMask()
{

  // only do calculations on current image(s)
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index < 0 || index >= (int)mSlicerManagers.size())
  {
    return;
  }

  typedef itk::MinimumMaximumImageCalculator < ImageTypeFloat3D > ImageCalculatorFilterType;
  ImageCalculatorFilterType::Pointer imageCalculatorFilter = ImageCalculatorFilterType::New();
  imageCalculatorFilter->SetImage(mSlicerManagers[index]->mITKImage);
  imageCalculatorFilter->Compute();
  double actualMin = imageCalculatorFilter->GetMinimum();
  double actualMax = imageCalculatorFilter->GetMaximum();

  thresholdSpinBox->setRange(actualMin, actualMax);
  thresholdSpinBox->setValue((actualMin + actualMax) / 2);
}



void fMainWindow::SaveImage()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty()) {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index < 0 || index >= (int)mSlicerManagers.size()) {
    return;
  }
  //
  QString saveFileName = getSaveFile(this, mInputPathName, mInputPathName + "_new.nii.gz");
  if (!saveFileName.isEmpty())
  {
    auto saveFileName_string = saveFileName.toStdString();
    typedef ImageTypeFloat3D ImageType;
    ImageType::DirectionType originaldirection;
    originaldirection[0][0] = mSlicerManagers[index]->mDirection(0, 0);
    originaldirection[0][1] = mSlicerManagers[index]->mDirection(0, 1);
    originaldirection[0][2] = mSlicerManagers[index]->mDirection(0, 2);
    originaldirection[1][0] = mSlicerManagers[index]->mDirection(1, 0);
    originaldirection[1][1] = mSlicerManagers[index]->mDirection(1, 1);
    originaldirection[1][2] = mSlicerManagers[index]->mDirection(1, 2);
    originaldirection[2][0] = mSlicerManagers[index]->mDirection(2, 0);
    originaldirection[2][1] = mSlicerManagers[index]->mDirection(2, 1);
    originaldirection[2][2] = mSlicerManagers[index]->mDirection(2, 2);


    if (mSlicerManagers[index]->GetPreset() == PRESET_THRESHOLD)
    {
      auto img = convertVtkToItk<ImageTypeFloat3D::PixelType, ImageTypeFloat3D::ImageDimension>(mSlicerManagers[index]->mImage);
      double threshold = mSlicerManagers[index]->mThreshold;
      //
      auto duplicator = itk::ImageDuplicator< ImageTypeFloat3D >::New();
      duplicator->SetInputImage(img);
      duplicator->Update();
      auto seg = duplicator->GetOutput();
      //
      itk::ImageRegionIterator< ImageTypeFloat3D > imgIterator(seg, seg->GetLargestPossibleRegion());
      for (imgIterator.GoToBegin(); !imgIterator.IsAtEnd(); ++imgIterator)
      {
        // TBD: this was the original action to handle threshold -- needs testing
        if (imgIterator.Get() <= threshold)
        {
          imgIterator.Set(1);
        }
        else
        {
          imgIterator.Set(0);
        }
      }
      //
      cbica::WriteImage< ImageTypeFloat3D >(seg, correctExtension(saveFileName_string));
    }
    else
    {
      std::string InputPixelType = mSlicerManagers[index]->mImage->GetScalarTypeAsString();
      if (InputPixelType == "short")
      {
        using ImageTypeToWrite = itk::Image<short, ImageTypeFloat3D::ImageDimension>;
        auto img = convertVtkToItk<ImageTypeToWrite::PixelType, ImageTypeFloat3D::ImageDimension>(mSlicerManagers[index]->mImage);
        img->SetDirection(originaldirection);
        cbica::WriteImage< ImageTypeToWrite >(img, correctExtension(saveFileName_string));
      }
      else if (InputPixelType == "unsigned short")
      {
        using ImageTypeToWrite = itk::Image<unsigned short, ImageTypeFloat3D::ImageDimension>;
        auto img = convertVtkToItk<ImageTypeToWrite::PixelType, ImageTypeFloat3D::ImageDimension>(mSlicerManagers[index]->mImage);
        img->SetDirection(originaldirection);
        cbica::WriteImage< ImageTypeToWrite >(img, correctExtension(saveFileName_string));
      }
      else if (InputPixelType == "char")
      {
        using ImageTypeToWrite = itk::Image<char, ImageTypeFloat3D::ImageDimension>;
        auto img = convertVtkToItk<ImageTypeToWrite::PixelType, ImageTypeFloat3D::ImageDimension>(mSlicerManagers[index]->mImage);
        img->SetDirection(originaldirection);
        cbica::WriteImage< ImageTypeToWrite >(img, correctExtension(saveFileName_string));
      }
      else if (InputPixelType == "unsigned char")
      {
        using ImageTypeToWrite = itk::Image<unsigned char, ImageTypeFloat3D::ImageDimension>;
        auto img = convertVtkToItk<ImageTypeToWrite::PixelType, ImageTypeFloat3D::ImageDimension>(mSlicerManagers[index]->mImage);
        img->SetDirection(originaldirection);
        cbica::WriteImage< ImageTypeToWrite >(img, correctExtension(saveFileName_string));
      }
      else if (InputPixelType == "int")
      {
        using ImageTypeToWrite = itk::Image<int, ImageTypeFloat3D::ImageDimension>;
        auto img = convertVtkToItk<ImageTypeToWrite::PixelType, ImageTypeFloat3D::ImageDimension>(mSlicerManagers[index]->mImage);
        img->SetDirection(originaldirection);
        cbica::WriteImage< ImageTypeToWrite >(img, correctExtension(saveFileName_string));
      }
      else if (InputPixelType == "unsigned int")
      {
        using ImageTypeToWrite = itk::Image<unsigned int, ImageTypeFloat3D::ImageDimension>;
        auto img = convertVtkToItk<ImageTypeToWrite::PixelType, ImageTypeFloat3D::ImageDimension>(mSlicerManagers[index]->mImage);
        img->SetDirection(originaldirection);
        cbica::WriteImage< ImageTypeToWrite >(img, correctExtension(saveFileName_string));
      }
      else if (InputPixelType == "double")
      {
        using ImageTypeToWrite = itk::Image<double, ImageTypeFloat3D::ImageDimension>;
        auto img = convertVtkToItk<ImageTypeToWrite::PixelType, ImageTypeFloat3D::ImageDimension>(mSlicerManagers[index]->mImage);
        img->SetDirection(originaldirection);
        cbica::WriteImage< ImageTypeToWrite >(img, correctExtension(saveFileName_string));
      }
      else if (InputPixelType == "float")
      {
        using ImageTypeToWrite = itk::Image<float, ImageTypeFloat3D::ImageDimension>;
        auto img = convertVtkToItk<ImageTypeToWrite::PixelType, ImageTypeFloat3D::ImageDimension>(mSlicerManagers[index]->mImage);
        img->SetDirection(originaldirection);
        cbica::WriteImage< ImageTypeToWrite >(img, correctExtension(saveFileName_string));
      }
      else
      {
        cbica::Logging(loggerFile, "Error, input pixel type, '" + InputPixelType + "' is unknown!");
      }
      updateProgress(0, "Image saved! (" + saveFileName_string + ")");
    }
  }
}

void fMainWindow::InitMask(vtkImageData* image)
{
  int extent[6];
  double spacing[3];
  double origin[3];
  int i, j, k;

  image->GetExtent(extent);
  image->GetSpacing(spacing);
  image->GetOrigin(origin);

  mMask->Initialize();
  mMask->SetExtent(extent);
  mMask->SetSpacing(spacing);
  mMask->SetOrigin(origin);
#if VTK_MAJOR_VERSION <= 5
  mMask->SetScalarTypeToFloat();
  mMask->SetNumberOfScalarComponents(1);
  mMask->AllocateScalars();
#else
  mMask->AllocateScalars(VTK_FLOAT, 1);
#endif
  {
    int vd_x, vd_y, vd_z;
    vd_x = mMask->GetDimensions()[0];
    vd_y = mMask->GetDimensions()[1];
    vd_z = mMask->GetDimensions()[2];
    float* pData = (float*)mMask->GetScalarPointer();
    for (k = 0; k < vd_z; k++)
    {
      for (j = 0; j < vd_y; j++)
      {
        for (i = 0; i < vd_x; i++)
        {
          *pData = 0;
          pData++;
        }
      }
    }
  }
}
void fMainWindow::updateDrawMode(int shapeMode)
{
  if (shapeMode >= 0)
  {
    m_drawShapeMode = SHAPE_MODE(shapeMode);
  }
  if (m_drawShapeMode == SHAPE_MODE_NONE)
  {
    AxialViewWidget->unsetCursor();
    CoronalViewWidget->unsetCursor();
    SaggitalViewWidget->unsetCursor();
    return;
  }

  int color[4] = { 0, 0, 0, 0 };
  int drawLabel = this->getSelectedDrawLabel();
  int drawSize = this->getSelectedDrawSize();

  switch (drawLabel)
  {
  case DRAW_MODE_LABEL_1: // near
    color[0] = 255;
    color[1] = 0;
    color[2] = 0;
    color[3] = 255;
    break;
  case DRAW_MODE_LABEL_2: // far
    color[0] = 0;
    color[1] = 255;
    color[2] = 0;
    color[3] = 255;
    break;
  case DRAW_MODE_LABEL_3:
    color[0] = 255;
    color[1] = 255;
    color[2] = 0;
    color[3] = 255;
    break;
  case DRAW_MODE_LABEL_4:
    color[0] = 0;
    color[1] = 0;
    color[2] = 255;
    color[3] = 255;
    break;
  case DRAW_MODE_LABEL_5:
    color[0] = 255;
    color[1] = 0;
    color[2] = 255;
    color[3] = 255;
    break;


  default:
    break;
  }
  if (m_drawShapeMode == SHAPE_MODE_ERASER)
  {
    color[0] = 255;
    color[1] = 255;
    color[2] = 255;
    color[3] = 255;
  }

  QImage img(33, 33, QImage::Format_ARGB32);
  for (int j = -16; j <= 16; j++)
  {
    for (int i = -16; i <= 16; i++)
    {
      if (abs(i) <= drawSize && abs(j) <= drawSize)
      {
        img.setPixel(i + 16, j + 16, qRgba(color[0], color[1], color[2], color[3]));
      }
      else
      {
        img.setPixel(i + 16, j + 16, qRgba(0, 0, 0, 0));
      }
    }
  }
  AxialViewWidget->setCursor(QCursor(Qt::CrossCursor));
  CoronalViewWidget->setCursor(QCursor(Qt::CrossCursor));
  SaggitalViewWidget->setCursor(QCursor(Qt::CrossCursor));


}

void fMainWindow::LoadNonViewingImages(const std::string &directoryname, const int &imagetype_int, const int &imagesubtype)
{
  for (unsigned int index = 0; index < mNonViewingImageManager.size(); index++)
  {
    if (mNonViewingImageManager[index]->GetPathFileName() == directoryname)
    {
      return;
    }
  }
  SimpleImageManager* nonViewingImage = new SimpleImageManager();
  nonViewingImage->SetPathFileName(directoryname);
  nonViewingImage->SetFileName(directoryname);
  if (imagetype_int == CAPTK::NIfTI)
    nonViewingImage->ReadGivenNonViewingNiftiImage(directoryname, imagesubtype);
  else
    nonViewingImage->ReadGivenNonViewingDicomImage(directoryname, imagesubtype);

  if (nonViewingImage->GetLastEncounteredError() != "")
    delete nonViewingImage;
  else
    mNonViewingImageManager.push_back(nonViewingImage);


  //Add image Information in NonViewing images table
  //----------------------------------------------
  nonViewingImage->mImageType = imagetype_int;
  nonViewingImage->mImageSubType = imagesubtype;

  std::string strImageType;
  if (nonViewingImage->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_DTI)
    strImageType = "DTI";

  int rowindex = m_nonVisImagesTable->rowCount();
  m_nonVisImagesTable->setRowCount(rowindex + 1);

  QFileInfo fileinfo(nonViewingImage->GetFileName().c_str());
  QString id = directoryname.c_str() + QString::number(mNonViewingImageManager.size() - 1);
  {
    QTableWidgetItem *item = new QTableWidgetItem(fileinfo.fileName());
    item->setData(Qt::UserRole, directoryname.c_str());

    QTablePushButton* cButton = new QTablePushButton;
    cButton->setItem(item);
    cButton->setText(tr("X"));
    cButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);


    QString in = QString::fromStdString(strImageType);
    QTableWidgetItem *item2 = new QTableWidgetItem(in);
    item2->setData(Qt::UserRole, in.toStdString().c_str());

    if (nonViewingImage->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_DTI)
      connect(cButton, SIGNAL(clickedInto(QTableWidgetItem*)), this, SLOT(CloseNonViewingDTIImage(QTableWidgetItem*)));

    m_nonVisImagesTable->setCellWidget(rowindex, TAB_IMAGES_COLUMN_CLOSE, cButton);
    m_nonVisImagesTable->setItem(rowindex, TAB_IMAGES_COLUMN_NAME, item);
    m_nonVisImagesTable->setItem(rowindex, TAB_IMAGES_COLUMN_TYPE, item2);

    m_nonVisImagesTable->resizeRowsToContents();
  }
}



void fMainWindow::LoadSlicerImages(const std::string &fileName, const int &imagetype_int, bool bSkipDup)
{
  auto extension = cbica::getFilenameExtension(fileName);
  if (extension != ".dcm")
  {
    if (extension == ".zip")
    {
      ShowErrorMessage("Please extract the zip file before trying to load into CaPTk.");
      return;
    }
    if ((extension != ".nii") && (extension != ".nii.gz"))
    {
      ShowErrorMessage("Only DICOM (dcm) or NIfTI (nii/nii.gz) images are supported right now; please contact CBICA for adding extended support");
      return;
    }
    auto imageInfo = cbica::ImageInfo(fileName);
    SlicerManager* imageManager = new SlicerManager(3, mLandmarks, mSeedPoints, mTissuePoints);
    imageManager->mImageSubType = CAPTK::ImageModalityType::IMAGE_TYPE_UNDEFINED;

    bool bFirstLoad = false;
    if (mSlicerManagers.size() == 0)
    {
      bFirstLoad = true;
    }
    if (imageInfo.GetImageDimensions() == 2)
    {
      ConversionFrom2Dto3D(fileName, true);
    }
    else if (!bFirstLoad)
    {
      {
        auto imageInfoPrev = cbica::ImageInfo(mSlicerManagers[0]->GetPathFileName());
        auto sizePrev = imageInfoPrev.GetImageSize();
        auto size = imageInfo.GetImageSize();
        const std::string errorMsg = " not matching. Please register the image(s). Skipping file: ";
        for (size_t i = 0; i < ImageTypeFloat3D::ImageDimension; i++)
        {
          if (sizePrev[i] != size[i])
          {
            updateProgress(0, "Size" + errorMsg + fileName);
            ShowErrorMessage("Size" + errorMsg + fileName);
            return; //
          }
        }
        auto spacingPrev = imageInfoPrev.GetImageSpacings();
        auto spacing = imageInfo.GetImageSpacings();
        for (size_t i = 0; i < ImageTypeFloat3D::ImageDimension; i++)
        {
          if (spacing[i] != spacingPrev[i])
          {
            updateProgress(0, "Spacing" + errorMsg + fileName);
            ShowErrorMessage("Spacing" + errorMsg + fileName);
            return; //
          }
        }
        auto originPrev = imageInfoPrev.GetImageOrigins();
        auto origin = imageInfo.GetImageOrigins();
        if (!m_advancedVisualizer)
        {
          for (size_t i = 0; i < ImageTypeFloat3D::ImageDimension; i++)
          {
            if (origin[i] != originPrev[i])
            {
              updateProgress(0, "Origin" + errorMsg + fileName);
              ShowErrorMessage("Origin" + errorMsg + fileName);
              return; //
            }
          }
        }
      }

      if (bSkipDup)
      {
        for (int j = 0; j < (int)mSlicerManagers.size(); j++)
        {
          if (fileName == mSlicerManagers[j]->GetPathFileName())
          {
            updateProgress(0, "Duplicate file skipped :" + fileName);
            return;
          }
        }
      }
    }

    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    if (imageInfo.GetImageDimensions() == 4)
    {
      image4DSlider->setEnabled(true);
      image4DSlider->setRange(0, imageInfo.GetImageSize()[3] - 1);
      ImageTypeFloat4D::Pointer imagePerf = cbica::ReadImage<ImageTypeFloat4D>(fileName);
      imageManager->SetPerfImage(imagePerf);
      imageManager->mImageSubType = CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION;
      //return;
    }
    else
    {
      imageManager->SetOriginalOrigin(imageInfo.GetImageOrigins());
      auto currentImage = cbica::ReadImage<ImageTypeFloat3D>(fileName);
       imageManager->SetOriginalDirection(currentImage->GetDirection());
      currentImage = ChangeImageDirectionToIdentity< ImageTypeFloat3D >(cbica::ReadImageWithOrientFix< ImageTypeFloat3D >(fileName));
      imageManager->SetImage(currentImage);
      imageManager->mImageSubType = guessImageType(fileName);
    }
    mInputPathName = cbica::getFilenamePath(fileName).c_str();
    imageManager->SetFilename(fileName);
    imageManager->SetMask(mMask);
    imageManager->setTempFolderLocation(m_tempFolderLocation);
    int rowIndex = (int)mSlicerManagers.size();

    m_imagesTable->setRowCount(rowIndex + 1);
    mSlicerManagers.push_back(imageManager);


    QFileInfo fileinfo(imageManager->GetFileName().c_str());
    QString id = fileName.c_str() + QString::number(mSlicerManagers.size() - 1);
    //
    std::string strImageType = " IMAGE ";
    {
      QTableWidgetItem *item = new QTableWidgetItem(fileinfo.fileName());
      item->setData(Qt::UserRole, id.toStdString().c_str());
      item->setFlags(item->flags() & ~Qt::ItemIsEditable);

      QTablePushButton* cButton = new QTablePushButton;
      cButton->setItem(item);
      cButton->setText(QString("X"));
      connect(cButton, SIGNAL(clickedInto(QTableWidgetItem*)), this, SLOT(CloseImage(QTableWidgetItem*)));

      QLabel * label = new QLabel;
      label->setText(QString::fromStdString(strImageType));
      m_imagesTable->setCellWidget(rowIndex, TAB_IMAGES_COLUMN_CLOSE, cButton);
      m_imagesTable->setCellWidget(rowIndex, TAB_IMAGES_COLUMN_TYPE, label);
      m_imagesTable->setItem(rowIndex, TAB_IMAGES_COLUMN_NAME, item);
    }

    imagesPanel->NewImageLoaded(id, imageManager->GetBaseFileName(), rowIndex, strImageType, imageManager->mImageSubType, this);



    QTableWidgetItem *item = new QTableWidgetItem(fileinfo.fileName());
    item->setData(Qt::UserRole, id.toStdString().c_str());

    QTablePushButton* cButton = new QTablePushButton;
    cButton->setItem(item);
    cButton->setText(QString("X"));
    connect(cButton, SIGNAL(clickedInto(QTableWidgetItem*)), this, SLOT(CloseImage(QTableWidgetItem*)));

    mSlicerManagers.back()->SetId(id.toStdString());
    connect(mSlicerManagers.back(), SIGNAL(LeftButtonReleaseSignal(int)), this, SLOT(propogateSlicerPosition(int)));
    connect(mSlicerManagers.back(), SIGNAL(currentImageChanged(std::string &)), this, SLOT(CurrentImageChanged(std::string &)));
    connect(mSlicerManagers.back(), SIGNAL(currentPickedImageChanged(std::string)), this, SLOT(CurrentPickedImageChanged(std::string)));
    connect(mSlicerManagers.back(), SIGNAL(UpdatePosition(int, double, double, double, double, double, double, double)), this, SLOT(MousePositionChanged(int, double, double, double, double, double, double, double)));
    connect(mSlicerManagers.back(), SIGNAL(WindowLevelChanged()), this, SLOT(WindowLevelChanged()));
    connect(mSlicerManagers.back(), SIGNAL(UpdateSlice(int, int)), this, SLOT(UpdateSlice(int, int)));
    connect(mSlicerManagers.back(), SIGNAL(UpdateSliceRange(int, int, int)), this, SLOT(UpdateSliceRange(int, int, int)));
    connect(mSlicerManagers.back(), SIGNAL(UpdateLinkManager(std::string, int, double, double, double)), this, SLOT(UpdateLinkManager(std::string, int, double, double, double)));
    connect(mSlicerManagers.back(), SIGNAL(ChangeImageWithOrder(SlicerManager*, int)), this, SLOT(ChangeImageWithOrder(SlicerManager*, int)));
    connect(mSlicerManagers.back(), SIGNAL(UpdateBorderWidgetInMain(double, double, double, double)), this, SLOT(UpdateBorderWidget(double, double, double, double)));
    connect(mSlicerManagers.back(), SIGNAL(UpdateBorderWidgetInMain(double, double)), this, SLOT(UpdateBorderWidget(double, double)));
    connect(mSlicerManagers.back(), SIGNAL(UpdateActionInMain(const QVariantList&)), this, SLOT(UpdateActionQ(const QVariantList&)));

    connect(mSlicerManagers.back(), SIGNAL(SeedPointsAdded()), tumorPanel, SLOT(sAddPoint()));
    connect(mSlicerManagers.back(), SIGNAL(SeedPointsAdded(int, bool)), tumorPanel, SLOT(sAddPoint(int, bool)));
    connect(mSlicerManagers.back(), SIGNAL(TissuePointsAdded(int)), tumorPanel, SLOT(tAddPoint(int)));
    connect(m_tabWidget, SIGNAL(currentChanged(int)), tumorPanel, SLOT(tabSelected()));
    InitSlicers();

    if (bFirstLoad)
    {
      InitMask(mSlicerManagers.back()->mImage);
    }
    for (int j = 0; j < (int)mSlicerManagers.back()->mSlicers.size(); j++)
    {
      mSlicerManagers.back()->mSlicers[j]->SetMask(mSlicerManagers.back()->GetMask());
    }
    if (fileName.find("scan_label_map") != std::string::npos)
    {
      mSlicerManagers.back()->SetPreset(PRESET_LABEL);
    }
    if (fileName.find("gt") != std::string::npos)
    {
      mSlicerManagers.back()->SetPreset(PRESET_LABEL2);
    }
    if (fileName.find("roiDE") != std::string::npos)
    {
      mSlicerManagers.back()->SetPreset(PRESET_PROB);
    }

    if (mSlicerManagers.size() > 0)
    {
      if (mSlicerManagers.back()->mMask->GetDimensions()[2] != 1)
      {
        CoronalViewWidget->show();
        SaggitalViewWidget->show();
      }
      AxialViewWidget->show();
      infoPanel->show();

      windowLabel->setEnabled(true);
      windowSpinBox->setEnabled(true);
      levelLabel->setEnabled(true);
      levelSpinBox->setEnabled(true);
      presetLabel->setEnabled(true);
      presetComboBox->setEnabled(true);

      if (bFirstLoad)
      {
        for (int i = 0; i < 3; i++)
        {
          mSlicerManagers.back()->GetSlicer(i)->SetInitPosition();
        }
        DisplayChanged(item);
      }
      else
      {
        QTableWidgetItem* item = NULL;
        for (int i = 0; i < (int)mSlicerManagers.size(); i++)
        {
          item = GetItemFromSlicerManager(mSlicerManagers[i]);
          if (!item->isSelected())
          {
            item->setSelected(true);
          }
        }
        DisplayChanged(item);
      }

      if (mSlicerManagers.size() > 1)
      {
        for (int i = 0; i < (int)mSlicerManagers.size(); i++)
        {
          for (int j = i + 1; j < (int)mSlicerManagers.size(); j++)
          {
            AddLink(/*QString::fromStdString*/(mSlicerManagers[i]->GetId().c_str()), /*QString::fromStdString*/(mSlicerManagers[j]->GetId().c_str()));
          }
        }
      }
      QTableWidgetItem* item = GetItemFromSlicerManager(mSlicerManagers.back());
      item->setSelected(true);
      InitDisplay();
    }
    propogateSlicerPosition();
    updateProgress(0);
    QApplication::restoreOverrideCursor();
  }
  else
  {
    auto path = cbica::getFilenamePath(fileName);
    dicomfilename = fileName;
    auto filesInDir = cbica::filesInDirectory(path, false);

#ifndef _WIN32
    for (auto it = filesInDir.begin(); it != filesInDir.end();)
    {
      if ((*it == ".") || (*it == ".."))
      {
        filesInDir.erase(it);
      }
      else
      {
        ++it;
      }
    }
#endif

    // remove any files that aren't DICOM (thumbs.db and stuff like that)
    for (size_t i = 0; i < filesInDir.size(); i++)
    {
      if (cbica::getFilenameExtension(path + "/" + filesInDir[i]) != ".dcm")
      {
        filesInDir.erase(filesInDir.begin() + i);
      }
    }

    if (filesInDir.size() == 1) // single DICOM slice
    {
      ConversionFrom2Dto3D(fileName, true);
    }
    else // for 3D images, call dcm2nii
    {
      CallDCM2NIfTIConversion(fileName, true);
    }

    return;
  }
}

void fMainWindow::CurrentImageChanged(std::string &id)
{
  if (id == mCurrentSelectedImageId)
  {
    return;
  }
  int selected = 0;
  for (int i = 0; i < m_imagesTable->rowCount(); i++)
  {
    if (m_imagesTable->item(i, TAB_IMAGES_COLUMN_NAME)->data(Qt::UserRole).toString().toStdString() == id)
    {
      selected = i;
    }
    else
    {
      m_imagesTable->item(i, TAB_IMAGES_COLUMN_NAME)->setSelected(false);
    }
  }
  m_imagesTable->item(selected, TAB_IMAGES_COLUMN_NAME)->setSelected(true);
  mCurrentSelectedImageId = id;
  emit SelectedImageHasChanged(mSlicerManagers[selected]);
  m_imagesTable->resizeColumnsToContents();
  m_imagesTable->resizeRowsToContents();
}

void fMainWindow::propogateSlicerPosition(int slicerId, int imageId)
{
  //TBD this not a proper fix a lot work needs to be done to make slicer, slicerManager slicerManagerCommand to be made clean OOP
  if (imageId < 0)
  {
    QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
    if (!items.empty())
    {
      imageId = GetSlicerIndexFromItem(items[0]);
    }

  }
  if (imageId < 0 || imageId >= (int)mSlicerManagers.size())
  {
    return;
  }
  const int MAX_SLICES = 3;
  if (slicerId < 0 || slicerId >= MAX_SLICES)
  {
    return;
  }
  double* pos = mSlicerManagers[imageId]->GetSlicer(slicerId)->GetCurrentPosition();
  for (size_t r = 0; r < mSlicerManagers.size(); r++)
  {
    for (size_t i = 0; i < MAX_SLICES; i++)
    {
      mSlicerManagers[r]->GetSlicer(i)->SetCurrentPosition(pos[0], pos[1], pos[2]);
    }
  }
}
void fMainWindow::CurrentPickedImageChanged(std::string id)
{
  if (id == mCurrentPickedImageId) {
    return;
  }
  int selected = 0;
  for (int i = 0; i < m_imagesTable->rowCount(); i++) {
    if (m_imagesTable->item(i, TAB_IMAGES_COLUMN_NAME)->data(Qt::UserRole).toString().toStdString() == id) {
      selected = i;
    }
    else {
      m_imagesTable->item(i, TAB_IMAGES_COLUMN_NAME)->setSelected(false);
    }
  }
  m_imagesTable->item(selected, TAB_IMAGES_COLUMN_NAME)->setSelected(true);
  mCurrentPickedImageId = id;
  mCurrentPickedImageIndex = selected;
}

void fMainWindow::ImageInfoChanged()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty()) {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index < 0 || index >= (int)mSlicerManagers.size()) {
    return;
  }

  vtkSmartPointer<vtkMatrix4x4> transformation;
  QString image = m_imagesTable->selectedItems()[0]->data(Qt::DisplayRole).toString();

  vtkSmartPointer<vtkImageData> imageSelected;
  vtkSmartPointer<vtkTransform> transformSelected;
  imageSelected = mSlicerManagers[index]->GetSlicer(0)->GetImage();
  transformSelected = mSlicerManagers[index]->GetSlicer(0)->GetTransform();

  std::string vtktype = mSlicerManagers[index]->GetImage()->GetScalarTypeAsString();
  QString pixelType = vtktype.c_str();

  infoPanel->setFileName(image);
  infoPanel->setSizePixel(imageSelected->GetDimensions()[0], imageSelected->GetDimensions()[1], imageSelected->GetDimensions()[2]);
  infoPanel->setOrigin(mSlicerManagers[index]->mOrigin[0], mSlicerManagers[index]->mOrigin[1], mSlicerManagers[index]->mOrigin[2]);
  infoPanel->setSpacing(imageSelected->GetSpacing()[0], imageSelected->GetSpacing()[1], imageSelected->GetSpacing()[2]);
  transformation = transformSelected->GetMatrix();
  tumorPanel->SetCurrentSPoints(mSlicerManagers[index]->mSeedPoints);
  tumorPanel->SetCurrentTPoints(mSlicerManagers[index]->mTissuePoints);
  tumorPanel->SetCurrentPath(mInputPathName.toStdString());

  for (int i = 0; i < 3; i++)
  {
    mSlicerManagers[index]->UpdateInfoOnCursorPosition(i);
  }

  WindowLevelChanged();

  // reset SliceManager order
  for (int j = 0; j < (int)mSlicerManagers.size(); j++) {
    mSlicerManagers[j]->SetOrder(j);
  }
  for (int i = 0; i < m_imagesTable->rowCount(); i++) {
    QString id_table = m_imagesTable->item(i, TAB_IMAGES_COLUMN_NAME)->data(Qt::UserRole).toString();
    for (int j = 0; j < (int)mSlicerManagers.size(); j++) {
      QString id_sm = mSlicerManagers[j]->GetId().c_str();
      if (id_table == id_sm) {
        mSlicerManagers[j]->SetOrder(i);
        break;
      }
    }
  }
}

void fMainWindow::DisplayChanged()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    return;
  }
  DisplayChanged(items[0]);
}
void fMainWindow::DisplayChanged(QTableWidgetItem *clickedItem)
{
  int slicerManagerIndex = GetSlicerIndexFromItem(clickedItem);
  if (slicerManagerIndex < 0 || slicerManagerIndex >= (int)mSlicerManagers.size())
  {
    return;
  }

  QTableWidgetItem* clickedParentItem = m_imagesTable->item(slicerManagerIndex, TAB_IMAGES_COLUMN_NAME);
  for (int i = 0; i < (int)mSlicerManagers.size(); i++)
  {
    QTableWidgetItem* currentParentItem = m_imagesTable->item(i, TAB_IMAGES_COLUMN_NAME);
    if (currentParentItem != clickedParentItem)
    {
      for (int j = 0; j < 3; j++)
      {
        mSlicerManagers[i]->UpdateSlicer(j, false);
      }
    }
    else
    {
      int VisibleInWindow = 0;
      for (int j = 0; j < 3; j++)
      {
        mSlicerManagers[i]->UpdateSlicer(j, true);
        mSlicerManagers[i]->UpdateInfoOnCursorPosition(j);
        DisplaySliders(i, j);
        //
        if (mSlicerManagers[i]->GetSlicer(j)->GetActive())
        {
          VisibleInWindow = j;
        }
      }

      mSlicerManagers[i]->Picked();
      mSlicerManagers[i]->UpdateViews(VisibleInWindow);
      mSlicerManagers[i]->UpdateLinked(VisibleInWindow);
      mSlicerManagers[i]->UpdateInfoOnCursorPosition(VisibleInWindow);
    }

    for (int j = 0; j < 3; j++)
    {
      mSlicerManagers[i]->mSlicers[j]->RemoveOverlay();
    }
  }
  UpdateRenderWindows();

  ImageInfoChanged();
}



int fMainWindow::GetSlicerIndexFromItem(QTableWidgetItem* item)
{
  if (item != NULL) {
    QString id = item->data(Qt::UserRole).toString();
    std::string id_string = id.toStdString();
    for (int i = 0; i < m_imagesTable->rowCount(); i++)
    {
      QString id_table_string = m_imagesTable->item(i, TAB_IMAGES_COLUMN_NAME)->data(Qt::UserRole).toString();
      std::string table_string = id_table_string.toStdString();
      if (m_imagesTable->item(i, TAB_IMAGES_COLUMN_NAME)->data(Qt::UserRole).toString() == id)
      {
        return i;
      }
    }
  }
  return -1;
}

QTableWidgetItem* fMainWindow::GetItemFromSlicerManager(SlicerManager* sm)
{
  QString id = sm->GetId().c_str();
  for (int i = 0; i < m_imagesTable->rowCount(); i++)
  {
    if (m_imagesTable->item(i, TAB_IMAGES_COLUMN_NAME)->data(Qt::UserRole).toString() == id)
    {
      return m_imagesTable->item(i, TAB_IMAGES_COLUMN_NAME);
    }
  }
  return NULL;
}

void fMainWindow::InitSlicers()
{
  if (mSlicerManagers.size()) {
    mSlicerManagers.back()->GenerateDefaultLookupTable();

    mSlicerManagers.back()->SetSlicerWindow(0, AxialViewWidget->GetRenderWindow());
    mSlicerManagers.back()->SetSlicerWindow(1, CoronalViewWidget->GetRenderWindow());
    mSlicerManagers.back()->SetSlicerWindow(2, SaggitalViewWidget->GetRenderWindow());
  }
}

void fMainWindow::InitDisplay()
{
  if (!mSlicerManagers.empty())
  {
    for (int j = 0; j < 3; j++)
    {
      InteractorStyleNavigator* style = InteractorStyleNavigator::New();
      style->SetAutoAdjustCameraClippingRange(1);
      for (int i = 0; i < m_imagesTable->rowCount(); i++)
      {
        mSlicerManagers[i]->SetInteractorStyleNavigator(j, style);
        //
        mSlicerManagers[i]->updateToRefCam(mSlicerManagers[i]->GetSlicer(0));
        mSlicerManagers[i]->GetSlicer(j)->SetInitPosition();
      }
      style->Delete();
    }
  }
}

void fMainWindow::DisplaySliders(int slicer, int window)
{
  int range[2];
  mSlicerManagers[slicer]->GetSlicer(window)->GetSliceRange(range);
  int position = mSlicerManagers[slicer]->GetSlicer(window)->GetSlice();

  bool showVertical = false;
  if (GetNumberOfDimensions(mSlicerManagers[slicer]->GetSlicer(window)->GetImage()) >= 3) {
    showVertical = true;
  }
  if (showVertical) {
    verticalSliders[window]->show();
  }
  else {
    verticalSliders[window]->hide();
  }
  verticalSliders[window]->setRange(range[0], range[1]);
  verticalSliders[window]->setValue(position);
}

void fMainWindow::CloseImage(QTableWidgetItem* item)
{
  int index = GetSlicerIndexFromItem(item);
  if (index < 0 || index >= (int)mSlicerManagers.size())
  {
    return;
  }
  if (mSlicerManagers.size() > 1)
  {
    for (int k = 0; k < (int)mSlicerManagers.size() - 1; k++)
    {
      if (k != index)
      {
        RemoveLink(/*QString::fromStdString*/(mSlicerManagers[k]->GetId()), /*QString::fromStdString*/(mSlicerManagers[index]->GetId()));
      }
    }
  }
  std::vector<SlicerManager*>::iterator Manageriter = mSlicerManagers.begin();
  for (int i = 0; i < index; i++)
  {
    Manageriter++;
  }

  mSlicerManagers[index]->RemoveActors();
  mSlicerManagers[index]->mTissuePoints->Clear();
  mSlicerManagers[index]->mSeedPoints->Clear();
  delete mSlicerManagers[index];
  mSlicerManagers.erase(Manageriter);
  m_imagesTable->removeRow(index);

  if (mSlicerManagers.size() >= 1)
  {
    QTableWidgetItem* item_tmp = GetItemFromSlicerManager(mSlicerManagers.back());
    item_tmp->setSelected(true);
    DisplayChanged(item_tmp);
  }
  else
  {
    this->ResetNumberOfPoints();
    AxialViewWidget->hide();
    CoronalViewWidget->hide();
    SaggitalViewWidget->hide();

    for (int i = 0; i < 3; i++)
    {
      verticalSliders[i]->hide();
    }
    //
    windowLabel->setEnabled(false);
    windowSpinBox->setEnabled(false);
    levelLabel->setEnabled(false);
    levelSpinBox->setEnabled(false);
    thresholdLabel->setEnabled(false);
    thresholdSpinBox->setEnabled(false);
    presetLabel->setEnabled(false);
    presetComboBox->setEnabled(false);
    m_imgGeodesicOut = NULL;
    mLandmarks->Clear();
    mSeedPoints->Clear();
    mTissuePoints->Clear();
  }

  InitDisplay();
}

void fMainWindow::MousePositionChanged(int visibility, double x, double y, double z, double X, double Y, double Z, double value)
{
  infoPanel->setCurrentInfo(visibility, x, y, z, X, Y, Z, value);
  tumorPanel->HighlightCurrentSelctedPoints(x, y, z, X, Y, Z, value);
}

void fMainWindow::WindowLevelChanged()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty()) {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index < 0 || index >= (int)mSlicerManagers.size())
  {
    return;
  }
  windowSpinBox->setValue(mSlicerManagers[index]->GetColorWindow());
  levelSpinBox->setValue(mSlicerManagers[index]->GetColorLevel());
  thresholdSpinBox->setValue(mSlicerManagers[index]->GetThresholdIndex());
  presetComboBox->setCurrentIndex(mSlicerManagers[index]->GetPreset());
  if (presetComboBox->currentIndex() == PRESET_THRESHOLD || presetComboBox->currentIndex() == PRESET_GEODESIC)
  {
    thresholdLabel->setEnabled(true);
    thresholdSpinBox->setEnabled(true);
  }
  else
  {
    thresholdLabel->setEnabled(false);
    thresholdSpinBox->setEnabled(false);
  }
}

void fMainWindow::WindowLevelEdited()
{
  presetComboBox->setCurrentIndex(PRESET_USER);
  UpdateWindowLevel();
}

void fMainWindow::SetWindowLevel(double w, double l)
{
  windowSpinBox->setValue(w);
  levelSpinBox->setValue(l);
  presetComboBox->setCurrentIndex(PRESET_USER);
  UpdateWindowLevel();
}

void fMainWindow::UpdateWindowLevel()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty()) {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index >= 0 && index < (int)mSlicerManagers.size())
  {
    mSlicerManagers[index]->SetColorWindow(windowSpinBox->value());
    mSlicerManagers[index]->SetColorLevel(levelSpinBox->value());
    mSlicerManagers[index]->SetPreset(presetComboBox->currentIndex());
    mSlicerManagers[index]->Render();
    //
    if (presetComboBox->currentIndex() == PRESET_THRESHOLD || presetComboBox->currentIndex() == PRESET_GEODESIC) {
      thresholdLabel->setEnabled(true);
      thresholdSpinBox->setEnabled(true);
    }
    else
    {
      thresholdLabel->setEnabled(false);
      thresholdSpinBox->setEnabled(false);
    }
    //
    WindowLevelChanged();
  }
}

void fMainWindow::thresholdSpinBoxChanged()
{
  if (presetComboBox->currentIndex() == PRESET_THRESHOLD)
  {
    QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
    if (items.empty())
    {
      return;
    }
    int index = GetSlicerIndexFromItem(items[0]);
    if (index >= 0 && index < (int)mSlicerManagers.size())
    {
      mSlicerManagers[index]->SetThresholdIndex(thresholdSpinBox->value());
      mSlicerManagers[index]->SetPreset(mSlicerManagers[index]->GetPreset());
      mSlicerManagers[index]->Render();
      WindowLevelChanged();
    }
  }
  else if (presetComboBox->currentIndex() == PRESET_GEODESIC)
  {
    ApplicationGeodesicTreshold();
  }

}

void fMainWindow::UpdateLinkManager(std::string id, int slicer, double x, double y, double z)
{
  for (int i = 0; i < (int)mSlicerManagers.size(); i++)
  {
    if (mSlicerManagers[i]->GetId() == id)
    {
      mSlicerManagers[i]->GetSlicer(slicer)->SetCurrentPosition(x, y, z);
      mSlicerManagers[i]->UpdateViews(slicer);
      break;
    }
  }
}
void fMainWindow::UpdateLinkedNavigation(Slicer* refSlicer)
{
  for (int i = 0; i < (int)mSlicerManagers.size(); i++)
  {
    mSlicerManagers[i]->updateToRefCam(refSlicer);
  }
}

void fMainWindow::CloseImage()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty()) {
    return;
  }
  CloseImage(items[0]);
}
void fMainWindow::CloseAllImages()
{
  while (m_imagesTable->rowCount() > 0)
  {
    m_imagesTable->selectRow(0);//TBD speedup this
    CloseImage();
  }
  infoPanel->setFileName("");
  infoPanel->setSizePixel(0, 0, 0);
  infoPanel->setOrigin(0, 0, 0);
  infoPanel->setSpacing(0, 0, 0);
  infoPanel->setCurrentInfo(0, 0, 0, 0, 0, 0, 0, 0);
}
void fMainWindow::ResetTransformationToIdentity()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty()) {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index >= 0 && index < (int)mSlicerManagers.size()) {
    mSlicerManagers[index]->ResetTransformationToIdentity();
    ImageInfoChanged();
  }
}

void fMainWindow::AddLink(const std::string &image1, const std::string &image2)
{

  for (int i = 0; i < (int)mSlicerManagers.size(); i++)
  {
    if (image1/*.toStdString()*/ == mSlicerManagers[i]->GetId())
    {
      mSlicerManagers[i]->AddLink(image2/*.toStdString()*/);
      //sm1 = i;
    }
    if (image2/*.toStdString()*/ == mSlicerManagers[i]->GetId())
    {
      mSlicerManagers[i]->AddLink(image1/*.toStdString()*/);
      //sm2 = i;
    }
  }
}

void fMainWindow::RemoveLink(const std::string &image1, const std::string &image2)
{
  for (int i = 0; i < (int)mSlicerManagers.size(); i++)
  {
    if (image1/*.toStdString()*/ == mSlicerManagers[i]->GetId())
    {
      mSlicerManagers[i]->RemoveLink(image2/*.toStdString()*/);
    }
    if (image2/*.toStdString()*/ == mSlicerManagers[i]->GetId()) {
      mSlicerManagers[i]->RemoveLink(image1/*.toStdString()*/);
    }
  }
}

void fMainWindow::ChangeImageWithOrder(SlicerManager *sm, int order)
{
  if (mSlicerManagers.size() <= 1) {
    return;
  }
  if (order >= (int)mSlicerManagers.size()) {
    return;
  }

  QTableWidgetItem* item;
  item = GetItemFromSlicerManager(mSlicerManagers[order]);
  item->setSelected(true);
  DisplayChanged(item);
}

void fMainWindow::AxialViewSliderChanged()
{
  static int value = -1;

  value = AxialViewSlider->value();
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty()) {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index >= 0 && index < (int)mSlicerManagers.size()) {
    if (mSlicerManagers[index]->GetSlicer(0)->GetSlice() != value) {
      mSlicerManagers[index]->GetSlicer(0)->SetSlice(value);
      mSlicerManagers[index]->VerticalSliderHasChanged(0, value);
      mSlicerManagers[index]->UpdateSlice(0);
    }
  }
}

void fMainWindow::SaggitalViewSliderChanged()
{
  static int value = -1;
  if (value == SaggitalViewSlider->value())
  {
    return;
  }
  else {
    value = SaggitalViewSlider->value();
  }
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty()) {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index >= 0 && index < (int)mSlicerManagers.size()) {
    if (mSlicerManagers[index]->GetSlicer(2)->GetSlice() != value) {
      mSlicerManagers[index]->GetSlicer(2)->SetSlice(value);
      mSlicerManagers[index]->VerticalSliderHasChanged(2, value);
      mSlicerManagers[index]->UpdateSlice(2);
    }
  }
}

void fMainWindow::CoronalViewSliderChanged()
{
  static int value = -1;
  if (value == CoronalViewSlider->value())
  {
    return;
  }
  else
  {
    value = CoronalViewSlider->value();
  }
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty()) {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index >= 0 && index < (int)mSlicerManagers.size()) {
    if (mSlicerManagers[index]->GetSlicer(1)->GetSlice() != value) {
      mSlicerManagers[index]->GetSlicer(1)->SetSlice(value);
      mSlicerManagers[index]->VerticalSliderHasChanged(1, value);
      mSlicerManagers[index]->UpdateSlice(1);
    }
  }
}

void fMainWindow::UpdateSlice(int slicer, int slice)
{
  if (slicer == 0)
  {
    AxialViewSlider->setValue(slice);
  }
  else if (slicer == 1)
  {
    CoronalViewSlider->setValue(slice);
  }
  else if (slicer == 2)
  {
    SaggitalViewSlider->setValue(slice);
  }
  propogateSlicerPosition();
}

void fMainWindow::UpdateSliceRange(int slicer, int min, int max)
{
  int position = int((min + max) / 2);
  if (slicer == 0) {
    AxialViewSlider->setValue(position);
    AxialViewSlider->setRange(min, max);
  }
  else if (slicer == 1) {
    CoronalViewSlider->setValue(position);
    CoronalViewSlider->setRange(min, max);
  }
  else if (slicer == 2) {
    SaggitalViewSlider->setValue(position);
    SaggitalViewSlider->setRange(min, max);
  }
}

void fMainWindow::UpdateRenderWindows()
{
  if (m_imagesTable->rowCount() <= 0)
  {
    return;
  }
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty()) {
    return;
  }
  QTableWidgetItem* item = items[0];
  if (item == NULL) {
    return;
  }
  int index = GetSlicerIndexFromItem(item);
  if (index >= 0 && index < (int)mSlicerManagers.size())
  {
    mSlicerManagers[index]->Render();
  }
  //*/
}

void fMainWindow::SetActiveLandmarksType(int type, int row, int col)
{
  mCurrentLandmarkTissueType = row;
  if (type == LANDMARK_TYPE::DEFAULT)
  {
    emit LandmarksFocused(true);
    emit SeedPointsFocused(false);
    emit TissuePointsFocused(false);
  }
  else if (type == LANDMARK_TYPE::TUMOR_POINTS)
  {
    emit LandmarksFocused(false);
    emit SeedPointsFocused(true);
    emit TissuePointsFocused(false);
    tumorPanel->mTumorPointsSelected = true;
  }
  else if (type == LANDMARK_TYPE::TISSUE_POINTS)
  {
    emit LandmarksFocused(false);
    emit SeedPointsFocused(false);
    emit TissuePointsFocused(true);
  }
  else
  {
    emit LandmarksFocused(false);
    emit SeedPointsFocused(false);
    emit TissuePointsFocused(false);
  }

  for (int i = 0; i < (int)mSlicerManagers.size(); i++)
    mSlicerManagers[i]->SetCurrentLandmarksType(type, row, col);

  UpdateRenderWindows();
}
void fMainWindow::panelChanged(int current)
{
  if (drawingPanel) //Reset shape mode on every panle switch
  {
    m_drawShapeMode = SHAPE_MODE_NONE;
    drawingPanel->shapesNoneButtonFunctionality();
  }

  if (current == TAB_IMAGES)
  {
    SetActiveLandmarksType(LANDMARK_TYPE::NONE, 0, 0);
  }
  else if (current == TAB_TUMOR)
  {
    SetActiveLandmarksType(LANDMARK_TYPE::NONE, 0, 0);
    tumorPanel->SetCurrentSelectedTissueType();
  }
}

void fMainWindow::MoveSlicerCursor(double x, double y, double z, int mode)
{
  if (mCurrentPickedImageIndex < 0 || mCurrentPickedImageIndex >= (int)mSlicerManagers.size()) {
    return;
  }
  //
  if (mode == 0)
  {
    // x, y, z are LPS
    mSlicerManagers[mCurrentPickedImageIndex]->GetSlicer(0)->SetCurrentPosition(x, y, z);
    //
    mSlicerManagers[mCurrentPickedImageIndex]->Picked();
    mSlicerManagers[mCurrentPickedImageIndex]->UpdateViews(0);
    mSlicerManagers[mCurrentPickedImageIndex]->UpdateLinked(0);
    mSlicerManagers[mCurrentPickedImageIndex]->UpdateInfoOnCursorPosition(0);
  }
  else if (mode == 1)
  {
    // x, y, z are pixel
    x = x * mSlicerManagers[mCurrentPickedImageIndex]->GetSlicer(0)->GetInput()->GetSpacing()[0] + mSlicerManagers[mCurrentPickedImageIndex]->GetSlicer(0)->GetInput()->GetOrigin()[0];
    y = y * mSlicerManagers[mCurrentPickedImageIndex]->GetSlicer(0)->GetInput()->GetSpacing()[1] + mSlicerManagers[mCurrentPickedImageIndex]->GetSlicer(0)->GetInput()->GetOrigin()[1];
    z = z * mSlicerManagers[mCurrentPickedImageIndex]->GetSlicer(0)->GetInput()->GetSpacing()[2] + mSlicerManagers[mCurrentPickedImageIndex]->GetSlicer(0)->GetInput()->GetOrigin()[2];
    //
    mSlicerManagers[mCurrentPickedImageIndex]->GetSlicer(0)->SetCurrentPosition(x, y, z);
    //
    mSlicerManagers[mCurrentPickedImageIndex]->Picked();
    mSlicerManagers[mCurrentPickedImageIndex]->UpdateViews(0);
    mSlicerManagers[mCurrentPickedImageIndex]->UpdateLinked(0);
    mSlicerManagers[mCurrentPickedImageIndex]->UpdateInfoOnCursorPosition(0);
  }
  propogateSlicerPosition();
}

VectorVectorDouble fMainWindow::FormulateDrawingPointsForEdemaSegmentation()
{
  VectorVectorDouble Indices;
  if (mSlicerManagers.size() <= 0)
    return Indices;

  ImageTypeShort3D::Pointer img = convertVtkToItk<short, 3>(mSlicerManagers[0]->mMask);
  typedef itk::ImageRegionIteratorWithIndex <ImageTypeShort3D> IteratorType;
  IteratorType maskIt(img, img->GetLargestPossibleRegion());
  maskIt.GoToBegin();
  while (!maskIt.IsAtEnd())
  {
    if (maskIt.Get() == 1 || maskIt.Get() == 2)
    {
      VectorDouble localIndex;
      localIndex.push_back(maskIt.GetIndex()[0]);
      localIndex.push_back(maskIt.GetIndex()[1]);
      localIndex.push_back(maskIt.GetIndex()[2]);

      Indices.push_back(localIndex);
    }
    ++maskIt;
  }
  return Indices;
}

VectorVectorDouble fMainWindow::GetMaskLabelIndices(const int label)
{
  VectorVectorDouble Indices;
  if (mSlicerManagers.size() <= 0)
    return Indices;

  ImageTypeShort3D::Pointer img = convertVtkToItk<short, 3>(mSlicerManagers[0]->mMask);
  typedef itk::ImageRegionIteratorWithIndex <ImageTypeShort3D> IteratorType;
  IteratorType maskIt(img, img->GetLargestPossibleRegion());
  maskIt.GoToBegin();
  while (!maskIt.IsAtEnd())
  {
    if (maskIt.Get() == label)
    {
      VectorDouble localIndex;
      localIndex.push_back(maskIt.GetIndex()[0]);
      localIndex.push_back(maskIt.GetIndex()[1]);
      localIndex.push_back(maskIt.GetIndex()[2]);

      Indices.push_back(localIndex);
    }
    ++maskIt;
  }
  return Indices;
}
VectorVectorDouble fMainWindow::FormulateDrawingPointsForTumorSegmentation()
{
  VectorVectorDouble Indices;
  if (mSlicerManagers.size() <= 0)
    return Indices;

  ImageTypeShort3D::Pointer img = convertVtkToItk<short, 3>(mSlicerManagers[0]->mMask);
  typedef itk::ImageRegionIteratorWithIndex <ImageTypeShort3D> IteratorType;
  IteratorType maskIt(img, img->GetLargestPossibleRegion());
  maskIt.GoToBegin();
  while (!maskIt.IsAtEnd())
  {
    if (maskIt.Get() == 1)
    {
      VectorDouble localIndex;
      localIndex.push_back(maskIt.GetIndex()[0]);
      localIndex.push_back(maskIt.GetIndex()[1]);
      localIndex.push_back(maskIt.GetIndex()[2]);

      Indices.push_back(localIndex);
    }
    ++maskIt;
  }
  return Indices;
}


void fMainWindow::SaveDicomImage()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
    return;
  int index = GetSlicerIndexFromItem(items[0]);

  typedef short PixelType;
  const unsigned int Dimensions = 3;

  QString directory = getExistingDirectory(this, mInputPathName);
  if (directory.isNull())
    return;

  std::string directory_wrap = directory.toStdString();
  if (directory_wrap[directory_wrap.length() - 1] != '/')
  {
    directory_wrap += "/";
  }
  
  typedef itk::Image<PixelType, Dimensions> ImageType;
  typedef itk::CastImageFilter< itk::Image< float, Dimensions >, ImageType > CastFilterType;
  CastFilterType::Pointer castFilter = CastFilterType::New();
  castFilter->SetInput(mSlicerManagers[index]->mITKImage);
  castFilter->Update();

  try
  {
    updateProgress(0, "Image saved! (" + directory_wrap + ")");
  }
  catch (itk::ExceptionObject & excp)
  {
    ShowErrorMessage("Couldn't write mask as DICOM: " + std::string(excp.GetDescription()));
    return;
  }

}

void fMainWindow::SaveDicomDrawing()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
    return;
  int index = GetSlicerIndexFromItem(items[0]);

  typedef short MaskPixelType;
  const unsigned int Dimensions = 3;

  typedef itk::Image<MaskPixelType, Dimensions> ImageTypeMask;
  ImageTypeMask::Pointer imageToWrite = convertVtkToItk<MaskPixelType, Dimensions>(mSlicerManagers[index]->mMask);

  typedef itk::MinimumMaximumImageCalculator< ImageTypeMask > CalculatorType;
  CalculatorType::Pointer calculator = CalculatorType::New();
  calculator->SetImage(imageToWrite);
  calculator->Compute();

  if (calculator->GetMaximum() == 0) // this means that the mask image contains no values at all
  {
    ShowErrorMessage("There should be at least one region to save.");
    return;
  }

  QString directory = getExistingDirectory(this, mInputPathName);
  if (directory.isNull())
    return;

  std::string directory_wrap = directory.toStdString();
  if (directory_wrap[directory_wrap.length() - 1] != '/')
  {
    directory_wrap += "/";
  }

  if (cbica::getFilenamePath(mSlicerManagers[index]->mSeriesReader->GetFileNames()[0]) == directory_wrap)
  {
    ShowErrorMessage("Cannot save to source directory. Please select another.");
    return;
  }

  try
  {
    updateProgress(0, "Success!");
  }
  catch (itk::ExceptionObject & excp)
  {
    ShowErrorMessage("Couldn't write mask as DICOM: " + std::string(excp.GetDescription()));
    return;
  }


}

void fMainWindow::SaveDrawing()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
    return;
  int index = GetSlicerIndexFromItem(items[0]);

  typedef unsigned short MaskPixelType;
  const unsigned int Dimensions = 3;

  typedef itk::Image<MaskPixelType, Dimensions> ImageTypeMask;
  ImageTypeMask::Pointer imageToWrite = convertVtkToItk<MaskPixelType, Dimensions>(mSlicerManagers[index]->mMask);

  typedef itk::MinimumMaximumImageCalculator< ImageTypeMask > CalculatorType;
  CalculatorType::Pointer calculator = CalculatorType::New();
  calculator->SetImage(imageToWrite);
  calculator->Compute();

  if (calculator->GetMaximum() == 0) // this means that the mask image contains no values at all
  {
    ShowErrorMessage("There should be at least one region (near or far) for saving.");
    return;
  }

  auto imageToWrite_wrap = imageToWrite;
  imageToWrite->DisconnectPipeline();
  //if (mSlicerManagers[index]->mImageSubType != CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION)
  //{
  //  ImageTypeMask::DirectionType originalDirection;
  //  originalDirection[0][0] = mSlicerManagers[index]->mDirection(0, 0);
  //  originalDirection[0][1] = mSlicerManagers[index]->mDirection(0, 1);
  //  originalDirection[0][2] = mSlicerManagers[index]->mDirection(0, 2);
  //  originalDirection[1][0] = mSlicerManagers[index]->mDirection(1, 0);
  //  originalDirection[1][1] = mSlicerManagers[index]->mDirection(1, 1);
  //  originalDirection[1][2] = mSlicerManagers[index]->mDirection(1, 2);
  //  originalDirection[2][0] = mSlicerManagers[index]->mDirection(2, 0);
  //  originalDirection[2][1] = mSlicerManagers[index]->mDirection(2, 1);
  //  originalDirection[2][2] = mSlicerManagers[index]->mDirection(2, 2);

  //  ImageTypeMask::PointType originalOrigin;
  //  originalOrigin = mSlicerManagers[index]->mOrigin;

  //  auto infoChanger = itk::ChangeInformationImageFilter< ImageTypeMask >::New();
  //  infoChanger->SetInput(imageToWrite);
  //  infoChanger->ChangeDirectionOn();
  //  infoChanger->ChangeOriginOn();
  //  infoChanger->SetOutputDirection(originalDirection);
  //  infoChanger->SetOutputOrigin(originalOrigin);
  //  infoChanger->Update();
  //  imageToWrite_wrap = infoChanger->GetOutput();
  //}

  QString saveFileName = getSaveFile(this, mInputPathName, mInputPathName + "mask.nii.gz");
  if (!saveFileName.isEmpty())
  {
    std::string filename = saveFileName.toStdString();

    cbica::WriteImage< ImageTypeMask >(imageToWrite_wrap, filename);

    if (cbica::isFile(filename))
    {
      updateProgress(0, "ROI saved!(" + filename + ")");
    }
    else
    {
      ShowErrorMessage("Couldn't write to file: " + filename);
    }
  }
}

void fMainWindow::SaveSeedDrawing()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
    return;

  QString saveFileName = getSaveFile(this, mInputPathName, mInputPathName + "drawing_seed.nii.gz");
  if (!saveFileName.isEmpty())
  {
    std::string filename = saveFileName.toStdString();

    ImageTypeShort3D::Pointer img = convertVtkToItk<ImageTypeShort3D::PixelType, ImageTypeShort3D::ImageDimension>(mSlicerManagers[0]->mMask);
    std::vector<ImageTypeShort3D::IndexType> seedIndices;
    itk::ImageRegionIteratorWithIndex <ImageTypeShort3D> maskIt(img, img->GetLargestPossibleRegion());
    maskIt.GoToBegin();
    while (!maskIt.IsAtEnd())
    {
      if (maskIt.Get() == 3/*seeds are always defined as label '3'*/)
        seedIndices.push_back(maskIt.GetIndex());
      ++maskIt;
    }
    std::string errormsg = "";
    if (seedIndices.size() == 0)
      errormsg = "Draw seed points (label 3) before saving.";
    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.addButton(QMessageBox::Ok);

    if (errormsg.length() > 0)
    {
      box.setText(QString::fromStdString(errormsg));
      box.setWindowTitle(tr("Error message"));
      box.exec();
      return;
    }

    //save actual near and far region according to the current image type
    std::string InputPixelType = mSlicerManagers[0]->mImage->GetScalarTypeAsString();
    std::string subjectname = m_imagesTable->selectedItems()[0]->data(Qt::DisplayRole).toString().toStdString();
    subjectname = subjectname.substr(0, subjectname.find("_"));
    if (InputPixelType == "short")
    {
      using MaskPixelType = short;
      auto img = convertVtkToItk<MaskPixelType, 3>(mSlicerManagers[0]->mImage);
      mOutputManager.WriteSeedMasks< itk::Image<MaskPixelType, 3> >(img, filename, seedIndices);
    }
    else if (InputPixelType == "unsigned short")
    {
      using MaskPixelType = unsigned short;
      auto img = convertVtkToItk<MaskPixelType, 3>(mSlicerManagers[0]->mImage);
      mOutputManager.WriteSeedMasks<itk::Image<MaskPixelType, 3>>(img, filename, seedIndices);
    }
    else if (InputPixelType == "char")
    {
      using MaskPixelType = char;
      auto img = convertVtkToItk<MaskPixelType, 3>(mSlicerManagers[0]->mImage);
      mOutputManager.WriteSeedMasks<itk::Image<MaskPixelType, 3>>(img, filename, seedIndices);
    }
    else if (InputPixelType == "unsigned char")
    {
      using MaskPixelType = unsigned char;
      auto img = convertVtkToItk<MaskPixelType, 3>(mSlicerManagers[0]->mImage);
      mOutputManager.WriteSeedMasks<itk::Image<MaskPixelType, 3>>(img, filename, seedIndices);
    }
    else if (InputPixelType == "int")
    {
      using MaskPixelType = int;
      auto img = convertVtkToItk<MaskPixelType, 3>(mSlicerManagers[0]->mImage);
      mOutputManager.WriteSeedMasks<itk::Image<MaskPixelType, 3>>(img, filename, seedIndices);
    }
    else if (InputPixelType == "unsigned int")
    {
      using MaskPixelType = unsigned int;
      auto img = convertVtkToItk<MaskPixelType, 3>(mSlicerManagers[0]->mImage);
      mOutputManager.WriteSeedMasks<itk::Image<MaskPixelType, 3>>(img, filename, seedIndices);
    }
    else if (InputPixelType == "double")
    {
      using MaskPixelType = double;
      auto img = convertVtkToItk<MaskPixelType, 3>(mSlicerManagers[0]->mImage);
      mOutputManager.WriteSeedMasks<itk::Image<MaskPixelType, 3>>(img, subjectname, seedIndices);
    }
    else if (InputPixelType == "float")
    {
      using MaskPixelType = float;
      auto img = convertVtkToItk<MaskPixelType, 3>(mSlicerManagers[0]->mImage);
      mOutputManager.WriteSeedMasks<itk::Image<MaskPixelType, 3>>(img, filename, seedIndices);
    }
    else
    {
      cbica::Logging(loggerFile, "Error, input pixel type, '" + InputPixelType + "' is unknown!");
    }

    std::string msg = "Mask saved: " + filename;
    updateProgress(0, msg);

  }
}


void fMainWindow::makeStroke(std::vector<itk::Image<short, 3>::IndexType>& indices, const int value)
{
  std::vector<PointVal> strokePoints;
  for (unsigned int i = 0; i < indices.size(); i++)
  {
    float* pData = (float*)this->mSlicerManagers[0]->GetSlicer(0)->mMask->GetScalarPointer((int)indices[i][0], (int)indices[i][1], (int)indices[i][2]);

    PointVal pt;
    pt.x = indices[i][0];
    pt.y = indices[i][1];
    pt.z = indices[i][1];
    pt.value = (int)*pData;
    strokePoints.push_back(pt);
    *pData = value;
  }
  UpdateAction(strokePoints);
  return;
}
void fMainWindow::clearMask(int label)
{
  if ((mSlicerManagers.size() <= 0))
    return;

  auto img = convertVtkToItk<ImageTypeShort3D::PixelType, 3>(mSlicerManagers[0]->mMask);
  std::vector<ImageTypeShort3D::IndexType> indecesToErase;
  itk::ImageRegionIteratorWithIndex <ImageTypeShort3D> maskIt(img, img->GetLargestPossibleRegion());
  maskIt.GoToBegin();
  while (!maskIt.IsAtEnd())
  {
    if (label < 0)//Clear all
    {
      if (maskIt.Get() > 0)
      {
        indecesToErase.push_back(maskIt.GetIndex());
      }

    }
    else
    {
      if (maskIt.Get() == label)
      {
        indecesToErase.push_back(maskIt.GetIndex());
      }
    }
    ++maskIt;
  }
  makeStroke(indecesToErase, 0);
  this->mSlicerManagers[0]->GetSlicer(0)->mMask->Modified();
  this->mSlicerManagers[0]->Render();
  UpdateNumberOfPointsInTable();
}


void fMainWindow::StartEGFREstimate()
{
}

ImageTypeFloat3D::Pointer fMainWindow::getMaskImage()
{
  ImageTypeFloat3D::Pointer img = NULL;
  if (mSlicerManagers[0]->mMask != NULL)
  {
    img = convertVtkToItk<float, 3>(mSlicerManagers[0]->mMask);
  }

  return img;
}

void fMainWindow::readMaskFile(const std::string &maskFileName)
{
  if (!mSlicerManagers.empty())
  {
    if (cbica::getFilenameExtension(maskFileName) == ".dcm")
    {

      auto path = cbica::getFilenamePath(maskFileName);
      auto filesInDir = cbica::filesInDirectory(path, false);

      if (filesInDir.size() == 1) // single DICOM slice
      {
        dicomfilename = maskFileName;
        ConversionFrom2Dto3D(maskFileName, false);
      }
    }
    else
    {
      using ImageType = itk::Image<unsigned int, 3>; 
      auto inputImage = cbica::ReadImageWithOrientFix< ImageType >(maskFileName);
      inputImage = ChangeImageDirectionToIdentity< ImageType >(inputImage);

      auto minMaxCalc = itk::MinimumMaximumImageCalculator< ImageType >::New();
      minMaxCalc->SetImage(inputImage);
      minMaxCalc->Compute();
      auto maxVal = minMaxCalc->GetMaximum();

      if (maxVal > 0)
      {
        itk::ImageRegionIteratorWithIndex <ImageType> maskIt(inputImage, inputImage->GetLargestPossibleRegion());
        maskIt.GoToBegin();
        while (!maskIt.IsAtEnd())
        {
          /*
          change to this & also in manual:
          1 for necrosis
          2 for edema
          3 for non-enhancing tumor
          4 for enhancing tumor
          */
          ImageType::IndexType currentIndex = maskIt.GetIndex();
          float* pData = (float*)this->mSlicerManagers[0]->GetSlicer(0)->mMask->GetScalarPointer((int)currentIndex[0], (int)currentIndex[1], (int)currentIndex[2]);
          *pData = 0; // this is done in order to ensure that previously loaded mask is removed
          // this is done to take into account all possible label drawings
          switch (maskIt.Get())
          { // multiLabel: uncomment everything inside this loop and remove references to "near" and "far"
          case DRAW_MODE_LABEL_1:
            *pData = DRAW_MODE_LABEL_1;
            break;
          case 10: // GLISTR defines this as CSF
            *pData = DRAW_MODE_LABEL_7;
            break;
          case DRAW_MODE_LABEL_2:
            *pData = DRAW_MODE_LABEL_2;
            break;
          case 150: // GLISTR defines this is as GM
            *pData = DRAW_MODE_LABEL_5;
            break;
          case DRAW_MODE_LABEL_3:
            *pData = DRAW_MODE_LABEL_3;
            break;
          case 250: // GLISTR defines this is as WM
            *pData = DRAW_MODE_LABEL_3;
            break;
          case DRAW_MODE_LABEL_4:
            *pData = DRAW_MODE_LABEL_4;
            break;
          case 25: // GLISTR defines this is as VS
            *pData = DRAW_MODE_LABEL_4;
            break;
          case DRAW_MODE_LABEL_5: // this is an ambiguous index since GLISTR also uses this for CB
          {
            if (maxVal > DRAW_MODE_LABEL_9) // this means we are reading in GLISTR output
            {
              *pData = DRAW_MODE_LABEL_9;
            }
            else
            {
              *pData = DRAW_MODE_LABEL_5;
            }
            break;
          }
          case 100: // GLISTR defines this is as ED
            *pData = DRAW_MODE_LABEL_2;
            break;
          case DRAW_MODE_LABEL_6:
            *pData = DRAW_MODE_LABEL_6;
            break;
          case 175: // GLISTR defines this is as NCR
            *pData = DRAW_MODE_LABEL_1;
            break;
          case DRAW_MODE_LABEL_7:
            *pData = DRAW_MODE_LABEL_7;
            break;
          case 200: // GLISTR defines this is as TU
            *pData = DRAW_MODE_LABEL_4;
            break;
          case DRAW_MODE_LABEL_8:
            *pData = DRAW_MODE_LABEL_8;
            break;
          case 185: // GLISTR defines this is as NE
            *pData = DRAW_MODE_LABEL_1;
            break;
          case DRAW_MODE_LABEL_9:
            *pData = DRAW_MODE_LABEL_9;
            break;
          case 255: // contingency case in case a map is defined as 255 and 0
            *pData = DRAW_MODE_LABEL_1;
            break;
          default:
            // nothing defined for other cases
            break;
          }
          ++maskIt;
        }
      }
      else
      {
        ShowErrorMessage("Mask file has no pixels greater than '0'");
      }

      UpdateRenderWindows();
      updateProgress(0, "Mask loaded");
    }
  }
  else
  {
    ShowErrorMessage("Please load an image before trying to load an ROI");
    return;
  }

  // Force a render of the mask since updating the render windows doesn't cut it
  this->mSlicerManagers[0]->GetSlicer(0)->mMask->Modified();
  this->mSlicerManagers[0]->Render();
}

std::vector<ImageTypeFloat3D::Pointer> fMainWindow::getLodedImages(std::vector<std::string> &fileNames, std::vector<std::string> &modality, bool onlySelected)
{

  std::vector < ImageTypeFloat3D::Pointer> images;
  if (onlySelected)
  {
    QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
    if (!items.empty())
    {
      int index = GetSlicerIndexFromItem(items[0]);
      images.push_back(mSlicerManagers[index]->mITKImage);
      fileNames.push_back(mSlicerManagers[index]->mFileName);
      std::string pp = ImageModalityString[mSlicerManagers[index]->mImageSubType];
      modality.push_back(ImageModalityString[mSlicerManagers[index]->mImageSubType]);
    }
  }
  else
  {
    for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
    {
      images.push_back(mSlicerManagers[index]->mITKImage);
      fileNames.push_back(mSlicerManagers[index]->mFileName);
      modality.push_back(ImageModalityString[mSlicerManagers[index]->mImageSubType]);
    }
  }
  return images;
}


void fMainWindow::LoadedSubjectExistingRecurrenceEstimate(const std::string &outputdirectory, const std::string &modeldirectory, bool convDataPresent, bool dtiDataPresent, bool perfusionDataPresent, bool distanceDataPresent)
{
  int imagetype_int = CAPTK::ImageExtension::NIfTI;
  bool useOtherModalities = false;
  mOutputManager.SetOutputDirectoryPath(outputdirectory);

  UpdateNumberOfPointsInTable();
  if (!CheckCompletenessOfInputData(convDataPresent, perfusionDataPresent, dtiDataPresent, true))
    return;

  if (convDataPresent || dtiDataPresent)
    useOtherModalities = true;

  ImageTypeFloat3D::Pointer T1CEImagePointer;
  ImageTypeFloat3D::Pointer T2FlairImagePointer;
  ImageTypeFloat3D::Pointer T1ImagePointer;
  ImageTypeFloat3D::Pointer T2ImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	PerfusionImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	DTIImagePointer;

  ImageTypeFloat3D::Pointer FinalT1CEImagePointer;
  ImageTypeFloat3D::Pointer FinalT2FlairImagePointer;
  ImageTypeFloat3D::Pointer FinalT1ImagePointer;
  ImageTypeFloat3D::Pointer FinalT2ImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	FinalPerfusionImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	FinalDTIImagePointer;

  std::vector<std::string> filenames;
  std::string t1cebasefilename;
  filenames.resize(6);


  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_FA)
    {
      DTIImagePointer.push_back(RescaleImageIntensity(mSlicerManagers[index]->mITKImage));
      filenames[5] = mSlicerManagers[index]->mFileName;
    }
  }
  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_RAD)
    {
      DTIImagePointer.push_back(RescaleImageIntensity(mSlicerManagers[index]->mITKImage));
      filenames[5] = mSlicerManagers[index]->mFileName;
    }
  }
  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_TR)
    {
      DTIImagePointer.push_back(RescaleImageIntensity(mSlicerManagers[index]->mITKImage));
      filenames[5] = mSlicerManagers[index]->mFileName;
    }
  }
  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_AX)
    {
      DTIImagePointer.push_back(RescaleImageIntensity(mSlicerManagers[index]->mITKImage));
      filenames[5] = mSlicerManagers[index]->mFileName;
    }
  }

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1CE)
    {
      imagetype_int = mSlicerManagers[index]->mImageType;
      T1CEImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[0] = mSlicerManagers[index]->mFileName;
      t1cebasefilename = mSlicerManagers[index]->mPathFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR)
    {
      T2FlairImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[1] = mSlicerManagers[index]->mFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1)
    {
      T1ImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[2] = mSlicerManagers[index]->mFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2)
    {
      T2ImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[3] = mSlicerManagers[index]->mFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION)
    {
      ImageTypeFloat4D::RegionType region = mSlicerManagers[index]->mPerfusionImagePointer->GetLargestPossibleRegion();
      ImageTypeFloat4D::IndexType regionIndex;
      ImageTypeFloat4D::SizeType regionSize;

      regionSize[0] = region.GetSize()[0];
      regionSize[1] = region.GetSize()[1];
      regionSize[2] = region.GetSize()[2];
      regionSize[3] = 0; // this is 0 because we need a 3D image
      regionIndex[0] = 0;
      regionIndex[1] = 0;
      regionIndex[2] = 0;
      regionIndex[3] = 0;

      for (size_t i = 0; i < region.GetSize()[3]; i++)
      {
        regionIndex[3] = i;
        ImageTypeFloat4D::RegionType desiredRegion(regionIndex, regionSize);
        auto filter = itk::ExtractImageFilter< ImageTypeFloat4D, ImageTypeFloat3D >::New();
        filter->SetExtractionRegion(desiredRegion);
        filter->SetInput(mSlicerManagers[index]->mPerfusionImagePointer);

        filter->SetDirectionCollapseToIdentity(); // This is required.
        filter->Update();
        PerfusionImagePointer.push_back(filter->GetOutput());
      }

      filenames[4] = mSlicerManagers[index]->mFileName;
    }
    else
    {
      cbica::Logging(loggerFile, "Unknown Image type");
    }
  }

  //------------------------------------------noise reduction----------------------------------------------
  if (imagetype_int == CAPTK::ImageExtension::DICOM)
  {
    if (convDataPresent)
    {
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T2FlairImagePointer);
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T1CEImagePointer);
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T1ImagePointer);
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T2ImagePointer);
    }
    if (perfusionDataPresent)
      for (unsigned int index = 0; index < PerfusionImagePointer.size(); index++)
        mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(PerfusionImagePointer[index]);
    if (dtiDataPresent)
      for (unsigned int index = 0; index < DTIImagePointer.size(); index++)
        mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(DTIImagePointer[index]);

    mOutputManager.WriteOutput<ImageTypeFloat3D>(T1CEImagePointer, T2FlairImagePointer, T1ImagePointer, T2ImagePointer, PerfusionImagePointer, DTIImagePointer, filenames, WritingType::SUSAN,
      convDataPresent, perfusionDataPresent, dtiDataPresent);

    //------------------------------------------bias correction----------------------------------------------
    ImageTypeFloat3D::Pointer T1_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T2_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T1CE_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T2Flair_N3Corrected_ImagePointer;
    std::vector<ImageTypeFloat3D::Pointer> Perfusion_N3Corrected_ImagePointer;
    std::vector<ImageTypeFloat3D::Pointer> DTI_N3Corrected_ImagePointer;

    if (convDataPresent)
    {
      T2Flair_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T2FlairImagePointer);
      T1CE_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T1CEImagePointer);
      T1_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T1ImagePointer);
      T2_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T2ImagePointer);
    }
    if (perfusionDataPresent)
      for (unsigned int index = 0; index < PerfusionImagePointer.size(); index++)
        Perfusion_N3Corrected_ImagePointer[index] = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(PerfusionImagePointer[index]);
    if (dtiDataPresent)
      for (unsigned int index = 0; index < DTIImagePointer.size(); index++)
        DTI_N3Corrected_ImagePointer[index] = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(DTIImagePointer[index]);

    mOutputManager.WriteOutput<ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T2Flair_N3Corrected_ImagePointer, T1_N3Corrected_ImagePointer, T2_N3Corrected_ImagePointer, Perfusion_N3Corrected_ImagePointer, DTI_N3Corrected_ImagePointer, filenames, WritingType::BIAS_CORRECT,
      convDataPresent, perfusionDataPresent, dtiDataPresent);

    //-------------------------------------------------------registration------------------------------------------
    itk::MultiResolutionImageRegistrationMethod<ImageTypeFloat3D, ImageTypeFloat3D>::Pointer Registrar;
    ImageTypeFloat3D::Pointer T2Flair_Registered = T2Flair_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T1_Registered;
    ImageTypeFloat3D::Pointer T2_Registered;
    std::vector<ImageTypeFloat3D::Pointer> Perfusion_Registered;
    Perfusion_Registered.resize(Perfusion_N3Corrected_ImagePointer.size());
    std::vector<ImageTypeFloat3D::Pointer> DTI_Registered;
    DTI_Registered.resize(DTI_N3Corrected_ImagePointer.size());

    if (convDataPresent)
    {
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T2_N3Corrected_ImagePointer);
      T2_Registered = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, T2_N3Corrected_ImagePointer);
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T1_N3Corrected_ImagePointer);
      T1_Registered = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, T1_N3Corrected_ImagePointer);
    }
    if (perfusionDataPresent)
    {
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, Perfusion_N3Corrected_ImagePointer[0]);
      for (unsigned int index = 0; index < Perfusion_N3Corrected_ImagePointer.size(); index++)
        Perfusion_Registered[index] = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, Perfusion_N3Corrected_ImagePointer[index]);
    }
    if (dtiDataPresent)
    {
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, DTI_N3Corrected_ImagePointer[0]);
      for (unsigned int index = 0; index < DTI_N3Corrected_ImagePointer.size(); index++)
        DTI_Registered[index] = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, DTI_N3Corrected_ImagePointer[index]);
    }
    mOutputManager.WriteOutput<ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T2Flair_Registered, T1_Registered, T2_Registered, Perfusion_Registered, DTI_Registered, filenames, WritingType::REGISTRATION, convDataPresent, perfusionDataPresent, dtiDataPresent);

    FinalT1ImagePointer = T1_Registered;
    FinalT2ImagePointer = T2_Registered;
    FinalT1CEImagePointer = T1CE_N3Corrected_ImagePointer;
    FinalT2FlairImagePointer = T2Flair_N3Corrected_ImagePointer;
    FinalPerfusionImagePointer = Perfusion_Registered;
    FinalDTIImagePointer = DTI_Registered;
  }
  else
  {
    FinalT1ImagePointer = T1ImagePointer;
    FinalT2ImagePointer = T2ImagePointer;
    FinalT1CEImagePointer = T1CEImagePointer;
    FinalT2FlairImagePointer = T2FlairImagePointer;
    FinalPerfusionImagePointer = PerfusionImagePointer;
    FinalDTIImagePointer = DTIImagePointer;
  }
  cbica::Logging(loggerFile, "All images read.");
  //--------------------------------------------load edema and tumor mask--------------------------------------------------
  std::vector<double> labels;
  labels.push_back(CAPTK::GLISTR_OUTPUT_LABELS::TUMOR);
  labels.push_back(CAPTK::GLISTR_OUTPUT_LABELS::NONENHANCING);
  ImageTypeFloat3D::Pointer tumorMaskFinal = GetImageWithLabels<ImageTypeFloat3D>(labels, FinalT1CEImagePointer);

  cbica::Logging(loggerFile, "Tumor mask finalized.");

  labels.clear();
  labels.push_back(CAPTK::GLISTR_OUTPUT_LABELS::EDEMA);
  ImageTypeFloat3D::Pointer edemaMask = GetImageWithLabels<ImageTypeFloat3D>(labels, FinalT1CEImagePointer);

  cbica::Logging(loggerFile, "Edema mask finalized.");

  VectorVectorDouble empty;
  mRecurrenceEstimator.RunLoadedSubjectOnExistingModel<ImageTypeFloat3D>(edemaMask, tumorMaskFinal,
    FinalT1CEImagePointer, FinalT2FlairImagePointer, FinalT1ImagePointer, FinalT2ImagePointer, FinalPerfusionImagePointer, FinalDTIImagePointer,
    CAPTK::ImageExtension::NIfTI, convDataPresent, dtiDataPresent, perfusionDataPresent, distanceDataPresent,
    useOtherModalities, t1cebasefilename, empty, empty, mOutputManager.GetOutputDirectoryPath(), modeldirectory);

  cbica::Logging(loggerFile, "Probability map calculated.");

  LoadSlicerImages(mOutputManager.GetOutputDirectoryPath() + "/" + mRecurrenceEstimator.mRecurrenceMapFileName, CAPTK::ImageExtension::NIfTI);
  presetComboBox->setCurrentIndex(PRESET_PROB);
  UpdateWindowLevel();
  updateProgress(0, "Recurrence estimate for the given subject has been saved and loaded.");
}
void fMainWindow::LoadedSubjectExistingPseudoprogressionEstimate(const std::string &outputdirectory, const std::string &modeldirectory, bool convDataPresent, bool dtiDataPresent, bool perfusionDataPresent, bool distanceDataPresent)
{
  int imagetype_int = CAPTK::ImageExtension::NIfTI;
  bool useOtherModalities = false;
  mOutputManager.SetOutputDirectoryPath(outputdirectory);

  UpdateNumberOfPointsInTable();
  if (!CheckCompletenessOfInputData(convDataPresent, perfusionDataPresent, dtiDataPresent, true))
    return;

  if (convDataPresent || dtiDataPresent)
    useOtherModalities = true;

  ImageTypeFloat3D::Pointer T1CEImagePointer;
  ImageTypeFloat3D::Pointer T2FlairImagePointer;
  ImageTypeFloat3D::Pointer T1ImagePointer;
  ImageTypeFloat3D::Pointer T2ImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	PerfusionImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	DTIImagePointer;

  ImageTypeFloat3D::Pointer FinalT1CEImagePointer;
  ImageTypeFloat3D::Pointer FinalT2FlairImagePointer;
  ImageTypeFloat3D::Pointer FinalT1ImagePointer;
  ImageTypeFloat3D::Pointer FinalT2ImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	FinalPerfusionImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	FinalDTIImagePointer;

  std::vector<std::string> filenames;
  std::string t1cebasefilename;
  filenames.resize(6);


  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_FA)
    {
      DTIImagePointer.push_back(RescaleImageIntensity(mSlicerManagers[index]->mITKImage));
      filenames[5] = mSlicerManagers[index]->mFileName;
    }
  }
  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_RAD)
    {
      DTIImagePointer.push_back(RescaleImageIntensity(mSlicerManagers[index]->mITKImage));
      filenames[5] = mSlicerManagers[index]->mFileName;
    }
  }
  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_TR)
    {
      DTIImagePointer.push_back(RescaleImageIntensity(mSlicerManagers[index]->mITKImage));
      filenames[5] = mSlicerManagers[index]->mFileName;
    }
  }
  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_AX)
    {
      DTIImagePointer.push_back(RescaleImageIntensity(mSlicerManagers[index]->mITKImage));
      filenames[5] = mSlicerManagers[index]->mFileName;
    }
  }

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1CE)
    {
      imagetype_int = mSlicerManagers[index]->mImageType;
      T1CEImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[0] = mSlicerManagers[index]->mFileName;
      t1cebasefilename = mSlicerManagers[index]->mPathFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR)
    {
      T2FlairImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[1] = mSlicerManagers[index]->mFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1)
    {
      T1ImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[2] = mSlicerManagers[index]->mFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2)
    {
      T2ImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[3] = mSlicerManagers[index]->mFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION)
    {
      ImageTypeFloat4D::RegionType region = mSlicerManagers[index]->mPerfusionImagePointer->GetLargestPossibleRegion();
      ImageTypeFloat4D::IndexType regionIndex;
      ImageTypeFloat4D::SizeType regionSize;

      regionSize[0] = region.GetSize()[0];
      regionSize[1] = region.GetSize()[1];
      regionSize[2] = region.GetSize()[2];
      regionSize[3] = 0; // this is 0 because we need a 3D image
      regionIndex[0] = 0;
      regionIndex[1] = 0;
      regionIndex[2] = 0;
      regionIndex[3] = 0;

      for (size_t i = 0; i < region.GetSize()[3]; i++)
      {
        regionIndex[3] = i;
        ImageTypeFloat4D::RegionType desiredRegion(regionIndex, regionSize);
        auto filter = itk::ExtractImageFilter< ImageTypeFloat4D, ImageTypeFloat3D >::New();
        filter->SetExtractionRegion(desiredRegion);
        filter->SetInput(mSlicerManagers[index]->mPerfusionImagePointer);

        filter->SetDirectionCollapseToIdentity(); // This is required.
        filter->Update();
        PerfusionImagePointer.push_back(filter->GetOutput());
      }

      filenames[4] = mSlicerManagers[index]->mFileName;
    }
    else
    {
      cbica::Logging(loggerFile, "Unknown Image type");
    }
  }

  //------------------------------------------noise reduction----------------------------------------------
  if (imagetype_int == CAPTK::ImageExtension::DICOM)
  {
    if (convDataPresent)
    {
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T2FlairImagePointer);
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T1CEImagePointer);
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T1ImagePointer);
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T2ImagePointer);
    }
    if (perfusionDataPresent)
      for (unsigned int index = 0; index < PerfusionImagePointer.size(); index++)
        mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(PerfusionImagePointer[index]);
    if (dtiDataPresent)
      for (unsigned int index = 0; index < DTIImagePointer.size(); index++)
        mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(DTIImagePointer[index]);

    mOutputManager.WriteOutput<ImageTypeFloat3D>(T1CEImagePointer, T2FlairImagePointer, T1ImagePointer, T2ImagePointer, PerfusionImagePointer, DTIImagePointer, filenames, WritingType::SUSAN,
      convDataPresent, perfusionDataPresent, dtiDataPresent);

    //------------------------------------------bias correction----------------------------------------------
    ImageTypeFloat3D::Pointer T1_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T2_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T1CE_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T2Flair_N3Corrected_ImagePointer;
    std::vector<ImageTypeFloat3D::Pointer> Perfusion_N3Corrected_ImagePointer;
    std::vector<ImageTypeFloat3D::Pointer> DTI_N3Corrected_ImagePointer;

    if (convDataPresent)
    {
      T2Flair_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T2FlairImagePointer);
      T1CE_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T1CEImagePointer);
      T1_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T1ImagePointer);
      T2_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T2ImagePointer);
    }
    if (perfusionDataPresent)
      for (unsigned int index = 0; index < PerfusionImagePointer.size(); index++)
        Perfusion_N3Corrected_ImagePointer[index] = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(PerfusionImagePointer[index]);
    if (dtiDataPresent)
      for (unsigned int index = 0; index < DTIImagePointer.size(); index++)
        DTI_N3Corrected_ImagePointer[index] = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(DTIImagePointer[index]);

    mOutputManager.WriteOutput<ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T2Flair_N3Corrected_ImagePointer, T1_N3Corrected_ImagePointer, T2_N3Corrected_ImagePointer, Perfusion_N3Corrected_ImagePointer, DTI_N3Corrected_ImagePointer, filenames, WritingType::BIAS_CORRECT,
      convDataPresent, perfusionDataPresent, dtiDataPresent);

    //-------------------------------------------------------registration------------------------------------------
    itk::MultiResolutionImageRegistrationMethod<ImageTypeFloat3D, ImageTypeFloat3D>::Pointer Registrar;
    ImageTypeFloat3D::Pointer T2Flair_Registered = T2Flair_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T1_Registered;
    ImageTypeFloat3D::Pointer T2_Registered;
    std::vector<ImageTypeFloat3D::Pointer> Perfusion_Registered;
    Perfusion_Registered.resize(Perfusion_N3Corrected_ImagePointer.size());
    std::vector<ImageTypeFloat3D::Pointer> DTI_Registered;
    DTI_Registered.resize(DTI_N3Corrected_ImagePointer.size());

    if (convDataPresent)
    {
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T2_N3Corrected_ImagePointer);
      T2_Registered = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, T2_N3Corrected_ImagePointer);
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T1_N3Corrected_ImagePointer);
      T1_Registered = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, T1_N3Corrected_ImagePointer);
    }
    if (perfusionDataPresent)
    {
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, Perfusion_N3Corrected_ImagePointer[0]);
      for (unsigned int index = 0; index < Perfusion_N3Corrected_ImagePointer.size(); index++)
        Perfusion_Registered[index] = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, Perfusion_N3Corrected_ImagePointer[index]);
    }
    if (dtiDataPresent)
    {
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, DTI_N3Corrected_ImagePointer[0]);
      for (unsigned int index = 0; index < DTI_N3Corrected_ImagePointer.size(); index++)
        DTI_Registered[index] = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, DTI_N3Corrected_ImagePointer[index]);
    }
    mOutputManager.WriteOutput<ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T2Flair_Registered, T1_Registered, T2_Registered, Perfusion_Registered, DTI_Registered, filenames, WritingType::REGISTRATION, convDataPresent, perfusionDataPresent, dtiDataPresent);

    FinalT1ImagePointer = T1_Registered;
    FinalT2ImagePointer = T2_Registered;
    FinalT1CEImagePointer = T1CE_N3Corrected_ImagePointer;
    FinalT2FlairImagePointer = T2Flair_N3Corrected_ImagePointer;
    FinalPerfusionImagePointer = Perfusion_Registered;
    FinalDTIImagePointer = DTI_Registered;
  }
  else
  {
    FinalT1ImagePointer = T1ImagePointer;
    FinalT2ImagePointer = T2ImagePointer;
    FinalT1CEImagePointer = T1CEImagePointer;
    FinalT2FlairImagePointer = T2FlairImagePointer;
    FinalPerfusionImagePointer = PerfusionImagePointer;
    FinalDTIImagePointer = DTIImagePointer;
  }
  cbica::Logging(loggerFile, "All images read.");
  //--------------------------------------------load edema and tumor mask--------------------------------------------------
  std::vector<double> labels;
  labels.push_back(CAPTK::GLISTR_OUTPUT_LABELS::TUMOR);
  labels.push_back(CAPTK::GLISTR_OUTPUT_LABELS::NONENHANCING);
  ImageTypeFloat3D::Pointer tumorMaskFinal = GetImageWithLabels<ImageTypeFloat3D>(labels, FinalT1CEImagePointer);

  cbica::Logging(loggerFile, "Tumor mask finalized.");

  labels.clear();
  labels.push_back(CAPTK::GLISTR_OUTPUT_LABELS::EDEMA);
  ImageTypeFloat3D::Pointer edemaMask = GetImageWithLabels<ImageTypeFloat3D>(labels, FinalT1CEImagePointer);

  cbica::Logging(loggerFile, "Edema mask finalized.");

  VectorVectorDouble empty;
  mRecurrenceEstimator.RunLoadedSubjectOnExistingModel<ImageTypeFloat3D>(edemaMask, tumorMaskFinal,
    FinalT1CEImagePointer, FinalT2FlairImagePointer, FinalT1ImagePointer, FinalT2ImagePointer, FinalPerfusionImagePointer, FinalDTIImagePointer,
    CAPTK::ImageExtension::NIfTI, convDataPresent, dtiDataPresent, perfusionDataPresent, distanceDataPresent,
    useOtherModalities, t1cebasefilename, empty, empty, mOutputManager.GetOutputDirectoryPath(), modeldirectory);

  cbica::Logging(loggerFile, "Probability map calculated.");

  LoadSlicerImages(mOutputManager.GetOutputDirectoryPath() + "/" + mRecurrenceEstimator.mRecurrenceMapFileName, CAPTK::ImageExtension::NIfTI);
  presetComboBox->setCurrentIndex(PRESET_PROB);
  UpdateWindowLevel();
  updateProgress(0, "Recurrence estimate for the given subject has been saved and loaded.");
}

void fMainWindow::StartRecurrenceEstimate(const std::string &outputdirectory, bool convDataPresent, bool dtiDataPresent, bool perfusionDataPresent, bool distanceDataPresent)
{
  int imagetype_int = CAPTK::ImageExtension::NIfTI;
  bool useOtherModalities = false;
  mOutputManager.SetOutputDirectoryPath(outputdirectory);

  UpdateNumberOfPointsInTable();
  if (!this->CheckCompletenessOfInputData(convDataPresent, perfusionDataPresent, dtiDataPresent, false))
    return;

  if (convDataPresent || dtiDataPresent)
    useOtherModalities = true;

  ImageTypeFloat3D::Pointer T1CEImagePointer;
  ImageTypeFloat3D::Pointer T2FlairImagePointer;
  ImageTypeFloat3D::Pointer T1ImagePointer;
  ImageTypeFloat3D::Pointer T2ImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	PerfusionImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	DTIImagePointer;

  ImageTypeFloat3D::Pointer FinalT1CEImagePointer;
  ImageTypeFloat3D::Pointer FinalT2FlairImagePointer;
  ImageTypeFloat3D::Pointer FinalT1ImagePointer;
  ImageTypeFloat3D::Pointer FinalT2ImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	FinalPerfusionImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	FinalDTIImagePointer;

  std::vector<std::string> filenames;
  std::string t1cebasefilename;
  filenames.resize(6);

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1CE)
    {
      imagetype_int = mSlicerManagers[index]->mImageType;
      T1CEImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[0] = mSlicerManagers[index]->mFileName;
      t1cebasefilename = mSlicerManagers[index]->mPathFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR)
    {
      T2FlairImagePointer = mSlicerManagers[index]->mITKImage;
      filenames[1] = mSlicerManagers[index]->mFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1)
    {
      T1ImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[2] = mSlicerManagers[index]->mFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2)
    {
      T2ImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
      filenames[3] = mSlicerManagers[index]->mFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_AX || 
      mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_FA || 
      mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_RAD ||
      mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_TR)
    {
      DTIImagePointer.push_back(RescaleImageIntensity(mSlicerManagers[index]->mITKImage));
      filenames[5] = mSlicerManagers[index]->mFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION)
    {
      ImageTypeFloat4D::RegionType region = mSlicerManagers[index]->mPerfusionImagePointer->GetLargestPossibleRegion();
      ImageTypeFloat4D::IndexType regionIndex;
      ImageTypeFloat4D::SizeType regionSize;

      regionSize[0] = region.GetSize()[0];
      regionSize[1] = region.GetSize()[1];
      regionSize[2] = region.GetSize()[2];
      regionSize[3] = 0; // this is 0 because we need a 3D image
      regionIndex[0] = 0;
      regionIndex[1] = 0;
      regionIndex[2] = 0;
      regionIndex[3] = 0;


      for (size_t i = 0; i < region.GetSize()[3]; i++)
      {
        regionIndex[3] = i;
        ImageTypeFloat4D::RegionType desiredRegion(regionIndex, regionSize);
        auto filter = itk::ExtractImageFilter< ImageTypeFloat4D, ImageTypeFloat3D >::New();
        filter->SetExtractionRegion(desiredRegion);
        filter->SetInput(mSlicerManagers[index]->mPerfusionImagePointer);

        filter->SetDirectionCollapseToIdentity(); // This is required.
        filter->Update();
        PerfusionImagePointer.push_back(filter->GetOutput());
      }

      filenames[4] = mSlicerManagers[index]->mFileName;
    }
    else
    {
      cbica::Logging(loggerFile, "Unknown Image type");
    }
  }
  //------------------------------------------noise reduction----------------------------------------------
  if (imagetype_int == CAPTK::ImageExtension::DICOM)
  {
    if (convDataPresent)
    {
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T2FlairImagePointer);
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T1CEImagePointer);
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T1ImagePointer);
      mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(T2ImagePointer);
    }
    if (perfusionDataPresent)
      for (unsigned int index = 0; index < PerfusionImagePointer.size(); index++)
        mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(PerfusionImagePointer[index]);
    if (dtiDataPresent)
      for (unsigned int index = 0; index < DTIImagePointer.size(); index++)
        mPreprocessingObj.DenoisingMethodCall<ImageTypeFloat3D>(DTIImagePointer[index]);

    mOutputManager.WriteOutput<ImageTypeFloat3D>(T1CEImagePointer, T2FlairImagePointer, T1ImagePointer, T2ImagePointer, PerfusionImagePointer, DTIImagePointer, filenames, WritingType::SUSAN,
      convDataPresent, perfusionDataPresent, dtiDataPresent);

    //------------------------------------------bias correction----------------------------------------------
    ImageTypeFloat3D::Pointer T1_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T2_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T1CE_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T2Flair_N3Corrected_ImagePointer;
    std::vector<ImageTypeFloat3D::Pointer> Perfusion_N3Corrected_ImagePointer;
    std::vector<ImageTypeFloat3D::Pointer> DTI_N3Corrected_ImagePointer;

    if (convDataPresent)
    {
      T2Flair_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T2FlairImagePointer);
      T1CE_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T1CEImagePointer);
      T1_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T1ImagePointer);
      T2_N3Corrected_ImagePointer = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(T2ImagePointer);
    }
    if (perfusionDataPresent)
      for (unsigned int index = 0; index < PerfusionImagePointer.size(); index++)
        Perfusion_N3Corrected_ImagePointer[index] = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(PerfusionImagePointer[index]);
    if (dtiDataPresent)
      for (unsigned int index = 0; index < DTIImagePointer.size(); index++)
        DTI_N3Corrected_ImagePointer[index] = mPreprocessingObj.N4BiasCorrectionMethodCall<ImageTypeFloat3D>(DTIImagePointer[index]);

    mOutputManager.WriteOutput<ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T2Flair_N3Corrected_ImagePointer, T1_N3Corrected_ImagePointer, T2_N3Corrected_ImagePointer, Perfusion_N3Corrected_ImagePointer, DTI_N3Corrected_ImagePointer, filenames, WritingType::BIAS_CORRECT,
      convDataPresent, perfusionDataPresent, dtiDataPresent);

    //-------------------------------------------------------registration------------------------------------------
    itk::MultiResolutionImageRegistrationMethod<ImageTypeFloat3D, ImageTypeFloat3D>::Pointer Registrar;
    ImageTypeFloat3D::Pointer T2Flair_Registered = T2Flair_N3Corrected_ImagePointer;
    ImageTypeFloat3D::Pointer T1_Registered;
    ImageTypeFloat3D::Pointer T2_Registered;
    std::vector<ImageTypeFloat3D::Pointer> Perfusion_Registered;
    Perfusion_Registered.resize(Perfusion_N3Corrected_ImagePointer.size());
    std::vector<ImageTypeFloat3D::Pointer> DTI_Registered;
    DTI_Registered.resize(DTI_N3Corrected_ImagePointer.size());

    if (convDataPresent)
    {
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T2_N3Corrected_ImagePointer);
      T2_Registered = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, T2_N3Corrected_ImagePointer);
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T1_N3Corrected_ImagePointer);
      T1_Registered = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, T1_N3Corrected_ImagePointer);
    }
    if (perfusionDataPresent)
    {
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, Perfusion_N3Corrected_ImagePointer[0]);
      for (unsigned int index = 0; index < Perfusion_N3Corrected_ImagePointer.size(); index++)
        Perfusion_Registered[index] = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, Perfusion_N3Corrected_ImagePointer[index]);
    }
    if (dtiDataPresent)
    {
      Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, DTI_N3Corrected_ImagePointer[0]);
      for (unsigned int index = 0; index < DTI_N3Corrected_ImagePointer.size(); index++)
        DTI_Registered[index] = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CE_N3Corrected_ImagePointer, DTI_N3Corrected_ImagePointer[index]);
    }
    mOutputManager.WriteOutput<ImageTypeFloat3D>(T1CE_N3Corrected_ImagePointer, T2Flair_Registered, T1_Registered, T2_Registered, Perfusion_Registered, DTI_Registered, filenames, WritingType::REGISTRATION, convDataPresent, perfusionDataPresent, dtiDataPresent);

    FinalT1ImagePointer = T1_Registered;
    FinalT2ImagePointer = T2_Registered;
    FinalT1CEImagePointer = T1CE_N3Corrected_ImagePointer;
    FinalT2FlairImagePointer = T2Flair_N3Corrected_ImagePointer;
    FinalPerfusionImagePointer = Perfusion_Registered;
    FinalDTIImagePointer = DTI_Registered;
  }
  else
  {
    FinalT1ImagePointer = T1ImagePointer;
    FinalT2ImagePointer = T2ImagePointer;
    FinalT1CEImagePointer = T1CEImagePointer;
    FinalT2FlairImagePointer = T2FlairImagePointer;
    FinalPerfusionImagePointer = PerfusionImagePointer;
    FinalDTIImagePointer = DTIImagePointer;
  }

  //--------------------------------------------load edema and tumor mask--------------------------------------------------
  VectorVectorDouble allDrawingPoints = FormulateDrawingPointsForEdemaSegmentation();
  ImageTypeFloat3D::Pointer edemaMask = mPreprocessingObj.Edema3DSegmentationInGivenImage<ImageTypeFloat3D>(FinalT2FlairImagePointer, allDrawingPoints);
  ImageTypeFloat3D::Pointer tumorMaskFinal;

  if (distanceDataPresent)
  {
    VectorVectorDouble tumorPoints = FormulateDrawingPointsForTumorSegmentation();
    tumorMaskFinal = mPreprocessingObj.PrepareTumroImageFromPoints<ImageTypeFloat3D>(FinalT1CEImagePointer, tumorPoints);
  }

  mRecurrenceEstimator.Run<ImageTypeFloat3D>(edemaMask, tumorMaskFinal,
    FinalT1CEImagePointer, FinalT2FlairImagePointer, FinalT1ImagePointer, FinalT2ImagePointer, FinalPerfusionImagePointer, FinalDTIImagePointer,
    CAPTK::ImageExtension::NIfTI, convDataPresent, dtiDataPresent, perfusionDataPresent, distanceDataPresent,
    useOtherModalities, t1cebasefilename, GetMaskLabelIndices(1), GetMaskLabelIndices(2), mOutputManager.GetOutputDirectoryPath());

  LoadSlicerImages(mOutputManager.GetOutputDirectoryPath() + "/" + mRecurrenceEstimator.mRecurrenceMapFileName, CAPTK::ImageExtension::NIfTI);
  updateProgress(0, "Recurrence estimate for the given subject has been saved and loaded.");
}







void fMainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasFormat("text/uri-list")) {
    event->acceptProposedAction();
  }
}
void fMainWindow::dropEvent(QDropEvent *event)
{
  QList<QUrl> urls = event->mimeData()->urls();
  QStringList vectorOfFiles;
  for (int i = 0; i < (int)urls.size(); i++)
  {
    vectorOfFiles.push_back(urls[i].toLocalFile());
  }
  openImages(vectorOfFiles);
}


void fMainWindow::CloseNonViewingDTIImage(QTableWidgetItem* item)
{
  int itemIndexToBeDeleted = 0;
  m_nonVisImagesTable->removeRow(item->row());

  for (unsigned int index = 0; index < mNonViewingImageManager.size(); index++)
  {
    if (mNonViewingImageManager[index]->mImageType == CAPTK::ImageModalityType::IMAGE_TYPE_DTI)
    {
      itemIndexToBeDeleted = index;
      delete mNonViewingImageManager[index];
      break;
    }
  }
  std::vector<SimpleImageManager*>::iterator simpleImageIterator = mNonViewingImageManager.begin();
  for (int i = 0; i < itemIndexToBeDeleted; i++)
    simpleImageIterator++;
  mNonViewingImageManager.erase(simpleImageIterator);
}

void fMainWindow::UpdateNumberOfPointsInTable()
{
  if (mSlicerManagers.size() <= 0)
    return;

  ImageTypeShort3D::Pointer img = convertVtkToItk<short, 3>(mSlicerManagers[0]->mMask);
  int nearCounter = 0;
  int  farCounter = 0;
  int  initCounter = 0;
  typedef itk::ImageRegionIteratorWithIndex <ImageTypeShort3D> IteratorType;
  IteratorType maskIt(img, img->GetLargestPossibleRegion());

  maskIt.GoToBegin();
  while (!maskIt.IsAtEnd())
  {
    if (maskIt.Get() == DRAW_MODE_LABEL_1)
      nearCounter++;
    else if (maskIt.Get() == DRAW_MODE_LABEL_2)
      farCounter++;
    else if (maskIt.Get() == DRAW_MODE_LABEL_3)
      initCounter++;
    ++maskIt;
  }
  mCurrentNearPoints = nearCounter;
  mCurrentFarPoints = farCounter;
  mCurrentInitPoints = initCounter;

}

void fMainWindow::SetPresetComboBox()
{
  presetComboBox->addItem("Auto Scale");
  presetComboBox->addItem("User Scale");
  presetComboBox->addItem("Label Map");
  presetComboBox->addItem("Label map 2");
  presetComboBox->addItem("Threshold");
  presetComboBox->addItem("Probability");
  presetComboBox->addItem("Geodesic");
}
void fMainWindow::ResetNumberOfPoints()
{
  UpdateNumberOfPointsInTable();
}

bool fMainWindow::CheckCompletenessOfInputData(bool & convDataPresent, bool & perfusionDataPresent, bool & dtiDataPresent, bool existingmodel)
{
  bool t1P = false;
  bool t2P = false;
  bool t1ceP = false;
  bool t2flairP = false;
  bool dtiAXP = false;
  bool dtiFAP = false;
  bool dtiRADP = false;
  bool dtiTRP = false; bool dtiP = false;
  bool perfP = false; bool convP = false;

  std::string msg = "";

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1CE)
      t1ceP = true;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR)
      t2flairP = true;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2)
      t2P = true;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1)
      t1P = true;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_AX)
      dtiAXP = true;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_FA)
      dtiFAP = true;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_RAD)
      dtiRADP = true;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_TR)
      dtiTRP = true;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION)
      perfP = true;
  }
  if (mOutputManager.GetOutputDirectoryPath().empty())
    msg = "Output directory.";

  if (convDataPresent == true && t1ceP == false)
    msg = msg + "\n" + "Conventional Images: T1-Gd.";
  if (convDataPresent == true && t2flairP == false)
    msg = msg + "\n" + "Conventional Images: T2-FLAIR.";
  if (convDataPresent == true && t2P == false)
    msg = msg + "\n" + "Conventional Images: T2.";
  if (convDataPresent == true && t1P == false)
    msg = msg + "\n" + "Conventional Images: T1.";

  if (perfusionDataPresent == true && perfP == false)
    msg = msg + "\n" + "Perfusion Image.";

  if (dtiDataPresent == true && dtiAXP == false)
    msg = msg + "\n" + "DTI Images: Axial diffusivity.";
  if (dtiDataPresent == true && dtiRADP == false)
    msg = msg + "\n" + "DTI Images: Radial diffusivity.";
  if (dtiDataPresent == true && dtiFAP == false)
    msg = msg + "\n" + "DTI Images: Fractional anisotropy.";
  if (dtiDataPresent == true && dtiTRP == false)
    msg = msg + "\n" + "DTI Images: Apparent diffusion coefficient.";

  if (msg != "")
    msg = msg + "\n\n";
  if (mSlicerManagers.size() > 0)
  {
    ImageTypeShort3D::Pointer img = convertVtkToItk<short, 3>(mSlicerManagers[0]->mMask);
    int  NET_counter = 0;
    int  ET_counter = 0;
    int  ED_counter = 0;

    typedef itk::ImageRegionIteratorWithIndex <ImageTypeShort3D> IteratorType;
    IteratorType maskIt(img, img->GetLargestPossibleRegion());
    maskIt.GoToBegin();
    while (!maskIt.IsAtEnd())
    {
      if (maskIt.Get() == CAPTK::GLISTR_OUTPUT_LABELS::NONENHANCING)
        NET_counter++;
      else if (maskIt.Get() == CAPTK::GLISTR_OUTPUT_LABELS::TUMOR)
        ET_counter++;
      else if (maskIt.Get() == CAPTK::GLISTR_OUTPUT_LABELS::EDEMA)
        ED_counter++;
      ++maskIt;
    }
    if (ET_counter == 0)
      msg = msg + "\n" + "Segmentation Labels: Enhancing tumor (Label-4).";
    if (NET_counter == 0)
      msg = msg + "\n" + "Segmentation Labels: Non-enhancing tumor (Label-1).";
    if (ED_counter == 0)
      msg = msg + "\n" + "Segmentation Labels: Edema (Label-2).";
  }
  else
  {
    msg = msg + "\n" + "Segmentation Labels: Enhancing tumor (Label-4).";
    msg = msg + "\n" + "Segmentation Labels: Non-enhancing tumor (Label-1).";
    msg = msg + "\n" + "Segmentation Labels: Edema (Label-2).";
  }
  if (msg != "")
  {
    msg = "Please provide the following items:\n" + msg;
    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.addButton(QMessageBox::Ok);
    box.setText(QString::fromStdString(msg));
    box.setWindowTitle(tr("Missing Data"));
    box.exec();
    return false;
  }
  else
    return true;
}

bool fMainWindow::CheckCompletenessOfInputDataForEGFR(bool & t1ceDataPresent, bool & t2flairDataPresent, bool & perfusionDataPresent)
{
  std::string msg = "";
  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1CE)
      t1ceDataPresent = true;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR)
      t2flairDataPresent = true;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION)
      perfusionDataPresent = true;
  }
  if (t1ceDataPresent == false)
    msg = msg + "\n" + "T1-Gd data.";
  if (t2flairDataPresent == false)
  {
    msg = msg + "\n" + "T2-FLAIR data.";
  }
  if (perfusionDataPresent == false)
    msg = msg + "\n" + "DSC-MRI data.";
  if (mCurrentNearPoints == 0)
    msg = msg + "\n" + "Near ROI (Label-1) mask.";
  if (mCurrentFarPoints == 0)
    msg = msg + "\n" + "Far ROI (Label-2) mask.";

  if (msg != "")
  {
    ShowErrorMessage("Please provide the following items:\n" + msg);
    help_contextual("Glioblastoma_PHI.html");
    return false;
  }
  else
    return true;
}

void fMainWindow::RecurrenceEstimateOnExistingModel(const std::string &modeldirectory, const std::string &inputdirectory, const std::string &outputdirectory, bool useConventionalData, bool useDTIData, bool usePerfData, bool useDistData)
{
  if (modeldirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having SVM model");
    help_contextual("Glioblastoma_Recurrence.html");
    return;
  }
  if (inputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having input images");
    help_contextual("Glioblastoma_Recurrence.html");
    return;
  }
  if (outputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory to save output");
    help_contextual("Glioblastoma_Recurrence.html");
    return;
  }
  if (!cbica::isDir(outputdirectory))
  {
    if (!cbica::createDir(outputdirectory))
    {
      ShowErrorMessage("Unable to create the output directory");
      help_contextual("Glioblastoma_Recurrence.html");
      return;
    }
  }



  std::vector<double> finalresult;
  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects = LoadQualifiedSubjectsFromGivenDirectoryForRecurrence(CAPTK::MachineLearningApplicationSubtype::TESTING, inputdirectory, useConventionalData, useDTIData, usePerfData, useDistData);
  if (QualifiedSubjects.size() == 0)
  {
    ShowErrorMessage("The specified directory does not have any subject with all the required imaging sequences.");
    help_contextual("Glioblastoma_Recurrence.html");
    return;
  }
  if (mRecurrenceEstimator.RecurrenceEstimateOnExistingModel(QualifiedSubjects, modeldirectory, inputdirectory, outputdirectory, useConventionalData, useDTIData, usePerfData, useDistData))
    ShowMessage("Recurrence maps have been saved at the specified locations.", this);
  else
  {
    std::string msg;
    msg = "Survival model did not finish as expected, please see log file for details: " + loggerFile;
    ShowErrorMessage(msg, this);
  }
  return;
}

void fMainWindow::PseudoprogressionEstimateOnExistingModel(const std::string &modeldirectory, const std::string &inputdirectory, const std::string &outputdirectory, bool useConventionalData, bool useDTIData, bool usePerfData, bool useDistData)
{
  if (modeldirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having SVM model");
    help_contextual("Glioblastoma_Pseudoprogression.html");
    return;
  }
  if (inputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having input images");
    help_contextual("Glioblastoma_Pseudoprogression.html");
    return;
  }
  if (outputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory to save output");
    help_contextual("Glioblastoma_Pseudoprogression.html");
    return;
  }
  if (!cbica::isDir(outputdirectory))
  {
    if (!cbica::createDir(outputdirectory))
    {
      ShowErrorMessage("Unable to create the output directory");
      help_contextual("Glioblastoma_Pseudoprogression.html");
      return;
    }
  }

  std::vector<double> finalresult;
  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects = LoadQualifiedSubjectsFromGivenDirectoryForPseudoProgression(CAPTK::MachineLearningApplicationSubtype::TESTING, inputdirectory, useConventionalData, useDTIData, usePerfData, useDistData);
  if (QualifiedSubjects.size() == 0)
  {
    ShowErrorMessage("The specified directory does not have any subject with all the required imaging sequences.");
    help_contextual("Glioblastoma_Pseudoprogression.html");
    return;
  }
  if (mPseudoEstimator.PseudoProgressionEstimateOnExistingModel(QualifiedSubjects, modeldirectory, inputdirectory, outputdirectory, useConventionalData, useDTIData, usePerfData, useDistData))
    ShowMessage("Output has been saved at the specified location.", this);
  else
  {
    std::string msg;
    msg = "Pseudoprogression model did not finish as expected, please see log file for details: " + loggerFile;
    ShowErrorMessage(msg, this);
  }
  return;
}
void fMainWindow::CallGeneratePopualtionAtlas(const std::string inputdirectory, const std::string inputlabel, const std::string inputatlas, const std::string outputdirectory)
{
  std::vector<typename ImageTypeFloat3D::Pointer> atlases = mPopulationAtlas.GeneratePopualtionAtlas(inputdirectory, inputlabel, inputatlas, outputdirectory);
  if (mPopulationAtlas.mLastErrorMessage.empty() && atlases.size() > 0)
  {
    for (int i = 0; i < atlases.size(); i++)
    {
      auto tempFileName = "/AtlasMap_" + std::to_string(i) + ".nii.gz";
      cbica::WriteImage< ImageTypeFloat3D >(atlases[i], outputdirectory + tempFileName);
      LoadSlicerImages(outputdirectory + tempFileName, CAPTK::ImageExtension::NIfTI);

    }
    LoadSlicerImages(inputatlas, CAPTK::ImageExtension::NIfTI);
    ShowMessage("Statistical atlases have been saved at the specified location and loaded.", this);
  }
  else
  {
    ShowErrorMessage("Error in creating statistical atlases for the given set of subjects.", this);
    return;
  }
  updateProgress(0, "Statistical atlases have been saved at the specified location and loaded.");
}

void fMainWindow::CallSBRTNodule(const std::string seedImage, const int labelValue)
{
  cbica::Logging(loggerFile, "entering CallSBRTNodule");

  std::string petName, ctName, maskName, oName, seedName, logName;

  const int imageDimension = 3;	// image dimension, now support 3D only
  const int inputImageNum = 2;	// number of input images (modality), PET and CT in this application

  if (mSlicerManagers.size() < 2) //TODO: check for mask as well here
  {
    ShowErrorMessage("Load two images with first being the CT and second being the PET image.", this);
    help_contextual("LungCancer_SBRT.html");
    return;
  }

  if (!isMaskDefined())
  {
    ShowErrorMessage("Please load lung field mask", this);
    return;
  }

  std::string ctImageFile, petImageFile;

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_CT)
    {
      ctImageFile = mSlicerManagers[index]->mPathFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PET)
    {
      petImageFile = mSlicerManagers[index]->mPathFileName;
    }
    else
    {
      ShowErrorMessage("Only CT and PET need to be loaded for SBRT");
      return;
    }
  }

  if (ctImageFile.empty() && petImageFile.empty())
  {
    ShowErrorMessage("Both CT and PET need to be loaded for SBRT");
    return;
  }

  //! Following image flip is needed to correct the image orientation issue
  typedef itk::FlipImageFilter< ImageType> FlipType;
  FlipType::Pointer flip = FlipType::New();
  FlipType::FlipAxesArrayType flipAxesSet;

  flipAxesSet[0] = 0;
  flipAxesSet[1] = -1;
  flipAxesSet[2] = 0;

  flip->SetFlipAxes(flipAxesSet);
  flip->FlipAboutOriginOff();
  flip->SetInput(getMaskImage());
  flip->Update();

  cbica::WriteImage< ImageTypeFloat3D >(flip->GetOutput(), m_tempFolderLocation + "/sbrtLoadedMask_flipped.nii.gz");

  maskName = m_tempFolderLocation + "/sbrtLoadedMask_flipped.nii.gz";
  cbica::Logging(loggerFile, "written temp mask");

  int maskAvail;
  if (isMaskDefined())
    maskAvail = 1;
  else
    maskAvail = 0;

  int lf_lab = labelValue;
  int seedAvail = 0;
  if (!seedImage.empty())
  {
    seedAvail = 1;
    seedName = seedImage;
  }

  std::vector<std::string> inputFileName;
  inputFileName.push_back(petImageFile);
  inputFileName.push_back(ctImageFile);

  std::vector<float> modalityWt(inputImageNum, 0.5);
  modalityWt[0] = 0.6;
  modalityWt[1] = 0.4;

  float disThreshold = 5.0;
  float sigma = 1.0;
  float suvRatio = 3.0;

  int iterNum = 200;
  int smoothIterNum = 3;
  int smoothR = 1;
  int minNumFgSeed = 25;

  oName = m_tempFolderLocation + "/outputImage";
  qApp->processEvents();

  //!calling algorithm
  SBRT_Nodule< float, imageDimension, inputImageNum > segObject;

  if (!logName.empty())
  {
    segObject.SetLogger(logName);
  }

  segObject.SetParameters(lf_lab, oName, suvRatio, minNumFgSeed, modalityWt, disThreshold, sigma, iterNum, smoothIterNum, smoothR);
  cbica::Logging(loggerFile, "paramters setting done");

  updateProgress(20, "Initialization");
  segObject.Initialization(inputFileName, maskName);
  cbica::Logging(loggerFile, "Initialization done");

  updateProgress(40, "Seeding the ROIs");
  if (seedName.empty())
  {
    segObject.GenerateSeeds();
    cbica::Logging(loggerFile, "seed generation done");
  }
  else
  {
    segObject.ReadSeedImage(seedName);
    cbica::Logging(loggerFile, "reading seed image done");
  }

  updateProgress(80, "Segmentation of nodule");
  segObject.PerformSegmentation();
  cbica::Logging(loggerFile, "segmentation done");
 
  updateProgress(100, "Finishing up");
  segObject.WriteLabel(seedAvail);
  cbica::Logging(loggerFile, "written labels");

  readMaskFile(m_tempFolderLocation + "/outputImage" + "_segmentation.nii.gz");

  cbica::Logging(loggerFile, "loaded images");

  cbica::Logging(loggerFile, "exiting CallSBRTNodule");
}

void fMainWindow::CallForSurvivalPredictionOnExistingModelFromMain(const std::string modeldirectory, const std::string inputdirectory, const std::string outputdirectory)
{
  if (modeldirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having SVM model");
    help_contextual("Glioblastoma_Survival.html");
    return;
  }
  if (!cbica::isDir(modeldirectory))
  {
    ShowErrorMessage("The given SVM model directory does not exist");
    help_contextual("Glioblastoma_Survival.html");
    return;
  }
  if (!(cbica::fileExists(modeldirectory + "/Survival_SVM_Model6.csv") || cbica::fileExists(modeldirectory + "/Survival_SVM_Model6.xml"))
    || !(cbica::fileExists(modeldirectory + "/Survival_SVM_Model18.csv") || cbica::fileExists(modeldirectory + "/Survival_SVM_Model18.xml"))
    || !cbica::fileExists(modeldirectory + "/Survival_ZScore_Std.csv") || !cbica::fileExists(modeldirectory + "/Survival_ZScore_Mean.csv"))
  {
    ShowErrorMessage("The given SVM model directory does not have all the model files");
    help_contextual("Glioblastoma_Survival.html");
    return;
  }

  if (inputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having input images");
    help_contextual("Glioblastoma_Survival.html");
    return;
  }
  if (!cbica::isDir(inputdirectory))
  {
    ShowErrorMessage("The given input directory does not exist");
    help_contextual("Glioblastoma_Survival.html");
    return;
  }
  if (outputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory to save output");
    help_contextual("Glioblastoma_Survival.html");
    return;
  }
  if (!cbica::isDir(outputdirectory))
  {
    if (!cbica::createDir(outputdirectory))
    {
      ShowErrorMessage("Unable to create the output directory");
      help_contextual("Glioblastoma_Survival.html");
      return;
    }
  }


  std::vector<double> finalresult;
  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects = LoadQualifiedSubjectsFromGivenDirectoryForSurvival(inputdirectory);
  if (QualifiedSubjects.size() == 0)
  {
    ShowErrorMessage("No patient inside the given input directory has required scans");
    help_contextual("Glioblastoma_Survival.html");
    return;
  }

  VectorDouble result = mSurvivalPredictor.SurvivalPredictionOnExistingModel(modeldirectory, inputdirectory, QualifiedSubjects, outputdirectory);
  QString msg;
  if (result.size() == 0)
  {
    msg = "Survival model did not finish as expected, please see log file for details: ";
    msg = msg + QString::fromStdString(loggerFile);
    ShowMessage(msg.toStdString(), this);
  }
  else
  {
    msg = "A Survival Prediction Index (SPI) has been calculated for the given subjects by applying the specified model. \n\n";
    msg = msg + "SPI = " + QString::number(result[0]) + "\n\n";
    msg = msg + "SPI index saved in 'results.csv' file in the output directory. \n\n";

    msg = msg + "Input Directory = " + QString::fromStdString(inputdirectory) + "\nOutput Directory = " + QString::fromStdString(outputdirectory) + "\nModel Directory = " + QString::fromStdString(modeldirectory);
    ShowErrorMessage(msg.toStdString(), this);
  }
}

void fMainWindow::CallForNewSurvivalPredictionModelFromMain(const std::string inputdirectory, const std::string outputdirectory)
{
  std::vector<double> finalresult;

  if (inputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having input images", this);
    help_contextual("Glioblastoma_Survival.html");
    return;
  }
  if (!cbica::isDir(inputdirectory))
  {
    ShowErrorMessage("The given input directory does not exist", this);
    help_contextual("Glioblastoma_Survival.html");
    return;
  }


  if (outputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory to save output");
    help_contextual("Glioblastoma_Survival.html");
    return;
  }
  if (!cbica::isDir(outputdirectory))
  {
    if (!cbica::createDir(outputdirectory))
    {
      ShowErrorMessage("Unable to create the output directory");
      help_contextual("Glioblastoma_Survival.html");
      return;
    }
  }

  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects = LoadQualifiedSubjectsFromGivenDirectoryForSurvival(inputdirectory);
  if (QualifiedSubjects.size() == 0)
  {
    ShowErrorMessage("No patient inside the given input directory has required scans");
    help_contextual("Glioblastoma_Survival.html");
    return;
  }


  if (mSurvivalPredictor.PrepareNewSurvivalPredictionModel(inputdirectory, QualifiedSubjects, outputdirectory) == false)
  {
    std::string message;
    message = "Survival Training did not finish as expected, please see log file for details: ";
    message = message + loggerFile;
    ShowErrorMessage(message, this);
  }
  else
  {
    ShowMessage("A Survival Prediction Index (SPI) model has been prepared and saved. \n\nInput Directory = " + inputdirectory + "\nOutput Directory = " + outputdirectory, this);
  }
}







void fMainWindow::CallForEGFRvIIIPredictionOnExistingModelFromMain(const std::string modeldirectory, const std::string inputdirectory, const std::string outputdirectory)
{
  if (modeldirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having SVM model");
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }
  if (!cbica::isDir(modeldirectory))
  {
    ShowErrorMessage("The given SVM model directory does not exist");
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }
  if (!(cbica::fileExists(modeldirectory + "/EGFRvIII_SVM_Model.csv") || cbica::fileExists(modeldirectory + "/EGFRvIII_SVM_Model.xml"))
    || !cbica::fileExists(modeldirectory + "/EGFRvIII_ZScore_Std.csv") || !cbica::fileExists(modeldirectory + "/EGFRvIII_ZScore_Mean.csv"))
  {
    ShowErrorMessage("The given SVM model directory does not have all the model files");
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }

  if (inputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having input images");
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }
  if (!cbica::isDir(inputdirectory))
  {
    ShowErrorMessage("The given input directory does not exist");
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }
  if (outputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory to save output");
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }
  if (!cbica::isDir(outputdirectory))
  {
    if (!cbica::createDir(outputdirectory))
    {
      ShowErrorMessage("Unable to create the output directory");
      help_contextual("Glioblastoma_EGFRvIII.html");
      return;
    }
  }


  std::vector<double> finalresult;
  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects = LoadQualifiedSubjectsFromGivenDirectoryForSurvival(inputdirectory);
  if (QualifiedSubjects.size() == 0)
  {
    ShowErrorMessage("No patient inside the given input directory has required scans");
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }

  VectorDouble result = mEGFRvIIIPredictor.EGFRvIIIPredictionOnExistingModel(modeldirectory, inputdirectory, QualifiedSubjects, outputdirectory);
  QString msg;
  if (result.size() == 0)
  {
    msg = "EGFRvIII model did not finish as expected, please see log file for details: ";
    msg = msg + QString::fromStdString(loggerFile);
    ShowErrorMessage(msg.toStdString(), this);
  }
  else
  {
    msg = "EGFRvIII mutation detection has been done for the given subjects by applying the specified model. \n\n";
    if (result[0]>0)
      msg = msg + "EGFRvIII Mutation = Detected. \n\n";
    else
      msg = msg + "EGFRvIII Mutation = Not detected. \n\n";
    msg = msg + "Result saved in 'results.csv' file in the output directory. \n\n";

    msg = msg + "Input Directory = " + QString::fromStdString(inputdirectory) + "\nOutput Directory = " + QString::fromStdString(outputdirectory) + "\nModel Directory = " + QString::fromStdString(modeldirectory);
    ShowMessage(msg.toStdString(), this);
  }
}

void fMainWindow::CallForNewEGFRvIIIPredictionModelFromMain(const std::string inputdirectory, const std::string outputdirectory)
{
  std::vector<double> finalresult;

  if (inputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having input images", this);
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }
  if (!cbica::isDir(inputdirectory))
  {
    ShowErrorMessage("The given input directory does not exist", this);
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }


  if (outputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory to save output", this);
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }
  if (!cbica::isDir(outputdirectory))
  {
    if (!cbica::createDir(outputdirectory))
    {
      ShowErrorMessage("Unable to create the output directory", this);
      help_contextual("Glioblastoma_EGFRvIII.html");
      return;
    }
  }

  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects = LoadQualifiedSubjectsFromGivenDirectoryForSurvival(inputdirectory);
  if (QualifiedSubjects.size() == 0)
  {
    ShowErrorMessage("No patient inside the given input directory has required scans", this);
    help_contextual("Glioblastoma_EGFRvIII.html");
    return;
  }


  if (mEGFRvIIIPredictor.PrepareNewEGFRvIIIPredictionModel(inputdirectory, QualifiedSubjects, outputdirectory) == false)
  {
    std::string message;
    message = "EGFRvIII model training did not finish as expected, please see log file for details: ";
    message = message + loggerFile;
    ShowErrorMessage(message);
  }
  else
  {
    ShowMessage("An EGFRvIII model has been prepared and saved. \n\nInput Directory = " + inputdirectory + "\nOutput Directory = " + outputdirectory, this);
  }
}






ImageTypeFloat3D::Pointer fMainWindow::RescaleImageIntensity(ImageTypeFloat3D::Pointer image)
{
  typedef itk::RescaleIntensityImageFilter< ImageTypeFloat3D, ImageTypeFloat3D > RescaleFilterType;
  RescaleFilterType::Pointer rescaleFilter = RescaleFilterType::New();
  rescaleFilter->SetInput(image);
  rescaleFilter->SetOutputMinimum(0);
  rescaleFilter->SetOutputMaximum(255);
  rescaleFilter->Update();

  ImageTypeFloat3D::Pointer outputimage = rescaleFilter->GetOutput();

  return outputimage;

}

void fMainWindow::TrainNewModelOnGivenData(const std::string &inputdirectory, const std::string &outputdirectory, bool useConvData, bool useDTIData, bool usePerfData, bool useDistData)
{
  std::string errorMsg;
  if (inputdirectory.empty())
  {
    ShowErrorMessage("Please provide input directory.");
    help_contextual("Glioblastoma_Recurrence.html");
    return;
  }
  if (outputdirectory.empty())
  {
    ShowErrorMessage("Please provide output directory.");
    help_contextual("Glioblastoma_Recurrence.html");
    return;
  }
  if (!cbica::isDir(outputdirectory))
  {
    if (!cbica::createDir(outputdirectory))
    {
      ShowErrorMessage("Unable to create the output directory");
      help_contextual("Glioblastoma_Recurrence.html");
      return;
    }
  }

  std::vector<double> finalresult;
  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects = LoadQualifiedSubjectsFromGivenDirectoryForRecurrence(CAPTK::MachineLearningApplicationSubtype::TRAINING, inputdirectory, useConvData, useDTIData, usePerfData, useDistData);

  if (QualifiedSubjects.size() == 0)
  {
    ShowErrorMessage("The specified directory does not have any subject with all the required imaging sequences.");
    help_contextual("Glioblastoma_Recurrence.html");
    return;
  }

  if (mRecurrenceEstimator.TrainNewModelOnGivenData(QualifiedSubjects, outputdirectory, useConvData, useDTIData, usePerfData, useDistData))
    ShowMessage("Trained infiltration model has been saved at the specified location.", this);
  else
    ShowErrorMessage("Recurrence Estimator wasn't able to save the training files as expected. See log file for details: " + loggerFile);
}


void fMainWindow::TrainNewPseudoprogressionModelOnGivenData(const std::string &inputdirectory, const std::string &outputdirectory, bool useConvData, bool useDTIData, bool usePerfData, bool useDistData)
{
  std::string errorMsg;
  if (inputdirectory.empty())
  {
    ShowErrorMessage("Please provide input directory.", this);
    help_contextual("Glioblastoma_Pseudoprogression.html");
    return;
  }
  if (outputdirectory.empty())
  {
    ShowErrorMessage("Please provide output directory.", this);
    help_contextual("Glioblastoma_Pseudoprogression.html");
    return;
  }
  if (!cbica::isDir(outputdirectory))
  {
    if (!cbica::createDir(outputdirectory))
    {
      ShowErrorMessage("Unable to create the output directory", this);
      help_contextual("Glioblastoma_Pseudoprogression.html");
      return;
    }
  }

  std::vector<double> finalresult;
  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects = LoadQualifiedSubjectsFromGivenDirectoryForPseudoProgression(CAPTK::MachineLearningApplicationSubtype::TRAINING, inputdirectory, useConvData, useDTIData, usePerfData, useDistData);

  if (QualifiedSubjects.size() == 0)
  {
    ShowErrorMessage("The specified directory does not have any subject with all the required imaging sequences.", this);
    help_contextual("Glioblastoma_Pseudoprogression.html");
    return;
  }
  if (QualifiedSubjects.size() >0 && QualifiedSubjects.size()<=20)
  {
    ShowErrorMessage("There should be atleast 20 patients to build reliable pseudo-progression model.");
    return;
  }
  if (mPseudoEstimator.TrainNewModelOnGivenData(QualifiedSubjects, outputdirectory, useConvData, useDTIData, usePerfData, useDistData))
    ShowMessage("Trained pseudoprogression model has been saved at the specified location.", this);
  else
    ShowErrorMessage("Pseudoprogression Estimator wasn't able to save the training files as expected. See log file for details: " + loggerFile, this);
}

//
//void fMainWindow::LoadDicomDrawing()
//{
//  std::string root_directory;
//  QString directory = getExistingDirectory(this, mInputPathName);
//  if (directory.isNull())
//    return;
//
//  typedef itk::Image<unsigned short, 3> InputImageType;
//  typedef itk::ImageSeriesReader< InputImageType > ReaderType;
//  ReaderType::Pointer seriesreader = ReaderType::New();
//
//  typedef itk::GDCMImageIO ImageIOType;
//  ImageIOType::Pointer dicomIO = ImageIOType::New();
//  seriesreader->SetImageIO(dicomIO);
//  typedef itk::GDCMSeriesFileNames NamesGeneratorType;
//  NamesGeneratorType::Pointer nameGenerator = NamesGeneratorType::New();
//  nameGenerator->SetUseSeriesDetails(true);
//  nameGenerator->AddSeriesRestriction("0008|0021");
//
//  nameGenerator->SetInputDirectory(directory.toStdString());
//  try
//  {
//    typedef std::vector< std::string > SeriesIdContainer;
//    const SeriesIdContainer & seriesUID = nameGenerator->GetSeriesUIDs();
//
//    SeriesIdContainer::const_iterator seriesItr = seriesUID.begin();
//    SeriesIdContainer::const_iterator seriesEnd = seriesUID.end();
//    while (seriesItr != seriesEnd)
//    {
//      typedef std::vector< std::string > FileNamesContainer;
//      FileNamesContainer fileNames;
//      fileNames = nameGenerator->GetFileNames(seriesItr->c_str());
//      seriesreader->SetFileNames(fileNames);
//      try
//      {
//        seriesreader->Update();
//        typedef unsigned short ROIPixelType;
//        typedef::itk::Image<ROIPixelType, 3> OutputImageType;
//        OutputImageType::Pointer outputImage = seriesreader->GetOutput();
//
//        typedef itk::ImageRegionIteratorWithIndex <OutputImageType> IteratorType;
//        IteratorType maskIt(outputImage, outputImage->GetLargestPossibleRegion());
//        maskIt.GoToBegin();
//
//        auto minMaxCalc = itk::MinimumMaximumImageCalculator< OutputImageType >::New();
//        minMaxCalc->SetImage(outputImage);
//        minMaxCalc->Compute();
//        auto maxVal = minMaxCalc->GetMaximum();
//
//        while (!maskIt.IsAtEnd())
//        {
//          OutputImageType::IndexType currentIndex = maskIt.GetIndex();
//          float* pData = (float*)this->mSlicerManagers[0]->GetSlicer(0)->mMask->GetScalarPointer((int)currentIndex[0], (int)currentIndex[1], (int)currentIndex[2]);
//          // this is done to take into account all possible label drawings
//          switch (maskIt.Get())
//          { // multiLabel: uncomment everything inside this loop and remove references to "near" and "far"
//          case DRAW_MODE_LABEL_1:
//            *pData = DRAW_MODE_LABEL_1;
//            break;
//          case 10: // GLISTR map contingency case
//            *pData = DRAW_MODE_LABEL_1;
//            break;
//          case DRAW_MODE_LABEL_2:
//            *pData = DRAW_MODE_LABEL_2;
//            break;
//          case 150: // GLISTR map contingency case
//            *pData = DRAW_MODE_LABEL_2;
//            break;
//          case DRAW_MODE_LABEL_3:
//            *pData = DRAW_MODE_LABEL_3;
//            break;
//          case 250: // GLISTR map contingency case
//            *pData = DRAW_MODE_LABEL_3;
//            break;
//          case DRAW_MODE_LABEL_4:
//            *pData = DRAW_MODE_LABEL_4;
//            break;
//          case 25: // GLISTR map contingency case
//            *pData = DRAW_MODE_LABEL_4;
//            break;
//          case DRAW_MODE_LABEL_5:
//            if (maxVal > DRAW_MODE_LABEL_9) // if GLISTR map has been defined, this is Cerebellum, i.e., tissue #9
//            {
//              *pData = DRAW_MODE_LABEL_9;
//            }
//            else
//            {
//              *pData = DRAW_MODE_LABEL_5;
//            }
//            break;
//          case 100: // GLISTR map contingency case
//            *pData = DRAW_MODE_LABEL_5;
//            break;
//          case DRAW_MODE_LABEL_6:
//            *pData = DRAW_MODE_LABEL_6;
//            break;
//          case 175: // GLISTR map contingency case
//            *pData = DRAW_MODE_LABEL_6;
//            break;
//          case DRAW_MODE_LABEL_7:
//            *pData = DRAW_MODE_LABEL_7;
//            break;
//          case 200: // GLISTR map contingency case
//            *pData = DRAW_MODE_LABEL_7;
//            break;
//          case DRAW_MODE_LABEL_8:
//            *pData = DRAW_MODE_LABEL_8;
//            break;
//          case 185: // GLISTR map contingency case
//            *pData = DRAW_MODE_LABEL_8;
//            break;
//          case DRAW_MODE_LABEL_9:
//            *pData = DRAW_MODE_LABEL_9;
//            break;
//          default:
//            // nothing defined for other cases
//            break;
//          }
//          ++maskIt;
//        }
//
//        this->mSlicerManagers[0]->GetSlicer(0)->mMask->Modified();
//        this->mSlicerManagers[0]->Render();
//      }
//      catch (itk::ExceptionObject & err)
//      {
//        std::stringstream error;
//        error << err;
//      }
//      ++seriesItr;
//    }
//  }
//  catch (itk::ExceptionObject & err)
//  {
//    std::stringstream error;
//    error << err;
//  }
//}

void fMainWindow::LoadDrawing(const std::string &maskFile)
{
  auto reader = itk::ImageIOFactory::CreateImageIO(maskFile.c_str(), itk::ImageIOFactory::ReadMode);
  if (reader)
  {
    readMaskFile(maskFile);
  }
}

void fMainWindow::LoadDrawing()
{
  if (!mSlicerManagers.empty())
  {
    auto filename = getExistingFile(this, mInputPathName);

    std::string filename_string = filename.toStdString();
    auto reader = itk::ImageIOFactory::CreateImageIO(filename_string.c_str(), itk::ImageIOFactory::ReadMode);
    if (reader)
    {
      readMaskFile(filename_string);
    }
  }
  else
  {
    ShowErrorMessage("Please load an image before trying to load an ROI", this);
    return;
  }
}

void fMainWindow::UpdateBorderWidget(double startX, double startY, double endX, double endY)
{
  mBorderStartX = std::round(startX);
  mBorderStartY = std::round(startY);
  mBorderEndX = std::round(endX);
  mBorderEndY = std::round(endY);
}
void fMainWindow::UpdateBorderWidget(double startZ, double endZ)
{
  mBorderStartZ = std::round(startZ);
  mBorderStartZ = std::round(startZ);
}

void fMainWindow::overlayUseStateChanged(int state)
{
  if (state == 0)
  {
    for (int i = 0; i < (int)mSlicerManagers.size(); i++)
    {
      for (int j = 0; j < 3; j++) {
        mSlicerManagers[i]->mSlicers[j]->RemoveOverlay();
      }
    }
    UpdateRenderWindows();
  }
}

void fMainWindow::overlaySliderChanged(int value)
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index >= 0 && index < (int)mSlicerManagers.size())
  {
    for (int i = 0; i < 3; i++)
    {
      mSlicerManagers[index]->GetSlicer(i)->SetOverlayOpacity((double)value / (10 + 1e-6));
    }
  }
  UpdateRenderWindows();
}

void fMainWindow::imageModalityChanged(int value)
{
  for (size_t i = 0; i < mSlicerManagers.size(); i++)
  {
    mSlicerManagers[i]->mImageSubType = imagesPanel->getModality(i);
  }
}

//---------------------------------------------
void fMainWindow::imageSliderChanged()
{
  static int value = -1;
  if (value == image4DSlider->value())
    return;
  else
    value = image4DSlider->value();

  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
    return;

  int index = GetSlicerIndexFromItem(items[0]);

  if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION)
  {
    mSlicerManagers[index]->Get3DImageAtCurrentPerfusionIndex(value);
  }
  AxialViewSliderChanged();
  mSlicerManagers[index]->Picked();
  mSlicerManagers[index]->UpdateViews(0);
  mSlicerManagers[index]->UpdateLinked(0);
  mSlicerManagers[index]->UpdateInfoOnCursorPosition(0);
}
//---------------------------------------------
void fMainWindow::overlayChanged()
{

  overlayChanged(imagesPanel->getSelectedOverlay());
}
void fMainWindow::overlayChanged(QTableWidgetItem *clickedItem)
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    return;
  }
  int slicerManagerIndex = GetSlicerIndexFromItem(items[0]);
  if (slicerManagerIndex < 0 || slicerManagerIndex >= (int)mSlicerManagers.size())
  {
    return;
  }
  //
  int slicerManagerOverlayIndex = GetSlicerIndexFromItem(clickedItem);
  if (slicerManagerOverlayIndex < 0 || slicerManagerOverlayIndex >= (int)mSlicerManagers.size())
  {
    return;
  }
  for (unsigned int i = 0; i < mSlicerManagers.size(); i++)
  {
    if (i != static_cast<unsigned int>(slicerManagerIndex)) {
      for (int j = 0; j < 3; j++) {
        mSlicerManagers[i]->mSlicers[j]->RemoveOverlay();
      }
    }
    else
    {
      for (int j = 0; j < 3; j++)
      {
        mSlicerManagers[slicerManagerIndex]->mSlicers[j]->SetOverlay(mSlicerManagers[slicerManagerOverlayIndex]->mSlicers[j]->mImage);
        //
        double window = mSlicerManagers[slicerManagerOverlayIndex]->mSlicers[j]->GetColorWindow();
        double level = mSlicerManagers[slicerManagerOverlayIndex]->mSlicers[j]->GetColorLevel();
        vtkLookupTable* LUT = static_cast<vtkLookupTable*>(mSlicerManagers[slicerManagerOverlayIndex]->mSlicers[j]->GetWindowLevel()->GetLookupTable());
        if (LUT != NULL)
        {
          mSlicerManagers[slicerManagerIndex]->mSlicers[j]->mOverlayMapper->SetWindow(window);
          mSlicerManagers[slicerManagerIndex]->mSlicers[j]->mOverlayMapper->SetLevel(level);
          mSlicerManagers[slicerManagerIndex]->mSlicers[j]->mOverlayMapper->SetLookupTable(LUT);
        }
        else
        {
          mSlicerManagers[slicerManagerIndex]->mSlicers[j]->mOverlayMapper->SetLookupTable(NULL);
          mSlicerManagers[slicerManagerIndex]->mSlicers[j]->mOverlayMapper->SetWindow(window);
          mSlicerManagers[slicerManagerIndex]->mSlicers[j]->mOverlayMapper->SetLevel(level);
        }
      }
    }
  }
  UpdateRenderWindows();
}

void fMainWindow::openImages(QStringList files, bool callingFromCmd)
{
  if (files.isEmpty())
  {
    if (!callingFromCmd)
    {
      QString extensions = IMAGES_EXTENSIONS;
      extensions += ";;All Files (*)";
      files = QFileDialog::getOpenFileNames(this, tr("Load Images"), mInputPathName, extensions, 0, QFileDialog::DontResolveSymlinks | QFileDialog::DontUseNativeDialog);
      if (files.isEmpty())
        return;
    }
    else
    {
      return;
    }
  }

  for (int i = 0; i < files.size(); i++)
  {
    std::string fileName = files[i].toStdString();
    fileName = cbica::normPath(fileName);
    updateProgress(i + 1, "Opening " + fileName, files.size());
    LoadSlicerImages(fileName, CAPTK::ImageExtension::NIfTI);

  }
  updateProgress(0, "Loading complete", 100);
}

void fMainWindow::ApplicationLIBRABatch()
{
  std::string scriptToCall = m_allNonNativeApps["libra"];

  if (cbica::fileExists(scriptToCall))
  {
    startExternalProcess(scriptToCall.c_str(), QStringList());
    return;
  }
  else
  {
    ShowErrorMessage("Cannot find :" + scriptToCall, this);
  }

}
void fMainWindow::ApplicationLIBRASingle()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    ShowErrorMessage("At least 1 supported image needs to be loaded and selected", this);
    return;
  }
  updateProgress(15, "Initializing and running LIBRA compiled by MCC");

  std::string scriptToCall = getApplicationPath("libra");// m_allNonNativeApps["libra"];

  if (cbica::fileExists(scriptToCall))
  {
    std::string casename = cbica::getFilenameBase(dicomfilename);
    cbica::createDir(m_tempFolderLocation + "/" + casename); // this is ensure that multiple LIBRA runs happen without issues

    std::string command = scriptToCall + " " + dicomfilename + " " + m_tempFolderLocation + "/" + casename + " true true";
    cbica::Logging(loggerFile, "Running LIBRA Single Image with command '" + command + "'");
    startExternalProcess(command.c_str(), QStringList());
    
    updateProgress(100, "Finished and loading mask");

    readMaskFile(m_tempFolderLocation + "/" + casename + "/Result_Images/totalmask/totalmask.dcm");
  }
  else
  {
    ShowErrorMessage("Cannot find :" + scriptToCall, this);
  }
}

void fMainWindow::ApplicationConfetti()
{
  std::string scriptToCall = m_allNonNativeApps["ConfettiGUI"];

  if (startExternalProcess(scriptToCall.c_str(), QStringList()) != 0)
  {
    ShowErrorMessage("Confetti failed to execute. Please check installation requirements and retry.", this);
  }
}

void fMainWindow::ApplicationSBRTLungField()
{
  cbica::Logging(loggerFile, "entering ApplicationSBRTLungField");
  if (mSlicerManagers.size() < 2)
  {
    ShowErrorMessage("Load two images with first being the CT and second being the PET image.", this);
    help_contextual("LungCancer_SBRT.html");
    return;
  }
  std::string ctImageFile, petImageFile;

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_CT)
    {
      ctImageFile = mSlicerManagers[index]->mPathFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PET)
    {
      petImageFile = mSlicerManagers[index]->mPathFileName;
    }
    else
    {
      ShowErrorMessage("Only CT and PET need to be loaded for SBRT", this);
      return;
    }
  }

  if (ctImageFile.empty() && petImageFile.empty())
  {
    ShowErrorMessage("Both CT and PET need to be loaded for SBRT", this);
    return;
  }

  float svxSize = 512;
  float compactness = 1;
  float minSize = 125;
  int iter = 150;
  int ptSz = 1;

  //! Following image flip is needed to correct the image orientation issue
  typedef itk::FlipImageFilter< ImageType> FlipType;
  FlipType::Pointer flip = FlipType::New();
  FlipType::FlipAxesArrayType flipAxesSet;

  flipAxesSet[0] = 0;
  flipAxesSet[1] = -1;
  flipAxesSet[2] = 0;

  flip->SetFlipAxes(flipAxesSet);
  flip->FlipAboutOriginOff();
  flip->SetInput(getMaskImage());
  flip->Update();

  cbica::WriteImage< ImageTypeFloat3D >(flip->GetOutput(), m_tempFolderLocation + "/sbrtLoadedMask_flipped.nii.gz");
  cbica::Logging(loggerFile, "written temp mask");
  auto loadedMaskFile = m_tempFolderLocation + "/sbrtLoadedMask_flipped.nii.gz";

  int maskAvail;
  if (isMaskDefined())
    maskAvail = 1;
  else
    maskAvail = 0;

  std::vector<std::string> inputFileName;
  inputFileName.push_back(petImageFile);
  inputFileName.push_back(ctImageFile);

  const unsigned int imageDimension = 3;	// image dimension, now support 3D only
  const unsigned int inputImageNum = 2;	// number of input images (modality)

  std::string petName, ctName, logName;

  //! calling algorithm
  SBRT_LungField< float, ImageTypeFloat3D::ImageDimension, inputImageNum > lfObject;
  if (!logName.empty())
  {
    lfObject.SetLogger(logName);
  }

  lfObject.SetParameters(maskAvail, svxSize, compactness, minSize, iter, ptSz);
  cbica::Logging(loggerFile, "setparameters");

  if (maskAvail == 1)
  {
    lfObject.ReadMask(loadedMaskFile);
  }

  cbica::Logging(loggerFile, "read mask");

  qApp->processEvents();

  updateProgress(20, "Initialization");
  int result = lfObject.Initialization(inputFileName);
  cbica::Logging(loggerFile, "Intialization done");

  updateProgress(50, "SuperVoxel Segmentation");
  lfObject.DoSupervoxelSegmentation();
  cbica::Logging(loggerFile, "Supervoxel segmentation done");

  updateProgress(70, "SuperVoxel features");
  lfObject.GetSvxlFea(inputFileName[1]);
  cbica::Logging(loggerFile, "Supervoxel features");

  updateProgress(90, "KMeans");
  lfObject.DoKmeans(5, inputFileName[1]);
  cbica::Logging(loggerFile, "kmeans");

  updateProgress(100, "Writing Labels");
  lfObject.WriteLabel(m_tempFolderLocation + "/outputImage");
  cbica::Logging(loggerFile, "written labels");

  readMaskFile(m_tempFolderLocation + "/outputImage_lf.nii.gz");

  cbica::Logging(loggerFile, "loaded images");

  cbica::Logging(loggerFile, "exiting ApplicationSBRTLungField");
}

void fMainWindow::ApplicationSBRTNodule()
{
  cbica::Logging(loggerFile, " ApplicationSBRTNodule ");

  if (mSlicerManagers.size() < 2)
  {
    ShowErrorMessage("Load two images with first being the CT and second being the PET image.", this);
    help_contextual("LungCancer_SBRT.html");
    return;
  }

  if (!isMaskDefined())
  {
    ShowErrorMessage("Please load lung nodule mask", this);
    return;
  }

  nodulePanel.SetCurrentImagePath(mInputPathName);
  nodulePanel.exec();
}

void fMainWindow::ApplicationSBRTAnalysis()
{
  cbica::Logging(loggerFile, "Entering ApplicationSBRTAnalysis ");

  std::string ctImageFile, petImageFile;

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_CT)
    {
      ctImageFile = mSlicerManagers[index]->mPathFileName;
    }
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PET)
    {
      petImageFile = mSlicerManagers[index]->mPathFileName;
    }
    else
    {
      ShowErrorMessage("PET image needs to be loaded for SBRT Analysis", this);
      return;
    }
  }

  if (petImageFile.empty())
  {
    ShowErrorMessage("PET image needs to be loaded for SBRT Analysis", this);
    return;
  }
  
  if (!isMaskDefined())
  {
    ShowErrorMessage("Please load lung nodule mask", this);
    return;
  }

  analysisPanel.exec();

  std::string inputFileName;
  std::string maskName;
  int roiLabel = 1;
  std::string oname;
  int outputFea = 0;
  std::string logName;
  std::string modelDir;

  std::string metaName = analysisPanel.mInputPathName.toStdString() + "/meta_fea_proj.txt";
  std::string projName = analysisPanel.mInputPathName.toStdString() + "/triFac_res_cpp_kc3_kr5_pet_cox_coeff_train_all.txt";

  if (cbica::fileExists(metaName) == false ||
    cbica::fileExists(projName) == false)
  {
    ShowErrorMessage("Model files not found. Please re-select the directory containing model files.", this);
    return;
  }

  //! Following image flip is needed to correct the image orientation issue
  typedef itk::FlipImageFilter< ImageType> FlipType;
  FlipType::Pointer flip = FlipType::New();
  FlipType::FlipAxesArrayType flipAxesSet;

  flipAxesSet[0] = 0;
  flipAxesSet[1] = -1;
  flipAxesSet[2] = 0;

  flip->SetFlipAxes(flipAxesSet);
  flip->FlipAboutOriginOff();
  flip->SetInput(getMaskImage());
  flip->Update();

  cbica::WriteImage< ImageTypeFloat3D >(flip->GetOutput(), m_tempFolderLocation + "/sbrtLoadedMask_flipped.nii.gz");
  cbica::Logging(loggerFile, "written temp mask");
  auto loadedMaskFile = m_tempFolderLocation + "/sbrtLoadedMask_flipped.nii.gz";

  //! calling algorithm
  SBRT_Analysis< float, ImageTypeFloat3D::ImageDimension > anaObject;

  if (!logName.empty())
  {
    anaObject.SetLogger(logName);
  }

  inputFileName = petImageFile;
  maskName = loadedMaskFile;

  anaObject.SetParameters(roiLabel);

  updateProgress(20, "Initializing");
  anaObject.Initialization(inputFileName, maskName);
  cbica::Logging(loggerFile, "SBRT Analysis Initialization complete");
 
  updateProgress(50, "Feature Extraction");
  anaObject.FeaExtraction();
  cbica::Logging(loggerFile, "SBRT Analysis Feature Extraction complete");

  updateProgress(100, "Risk prediction");
  anaObject.GetPredictedRisk(metaName, projName);
  cbica::Logging(loggerFile, "SBRT Analysis Risk Prediction complete");

  if (outputFea == 1)
  {
    anaObject.OutputFeature(oname);
  }
  
  updateProgress(0, ""); //! reset progress bar

  QString msgStr = QString("Predicted Risk (Survival): %1\nPredicted Risk(Nodal Failure): %2").arg(anaObject.GetSurivalRisk())
    .arg(anaObject.GetNodalFailureRisk());
  QMessageBox::information(this, "Predicted Risk", msgStr, QMessageBox::Ok);

  cbica::Logging(loggerFile, "Exiting ApplicationSBRTAnalysis ");
}

void fMainWindow::ApplicationDirectionality()
{
  directionalityEstimator.exec(); // .exec is nonModal because we do not want user to have access to main UI until 2 ROIs have been put in
}

#ifdef BUILD_FETALBRAIN
void fMainWindow::ApplicationFetalBrain()
{

  typedef ImageTypeFloat3D ImageType;

  std::vector<std::string> modality;

  std::vector< std::string > fileNames;
  this->getLodedImages(fileNames, modality);


  fetalbrainpanel.SetCurrentImagePath(mInputPathName);
  //fetalbrainpanel.outputDirectoryName->setText(m_tempFolderLocation);

  fetalbrainpanel.show();


  std::string subjectUnderConsideration;
  //Fetalbrain<ImageType>   fetalbrain(mSlicerManagers[0]->mITKImage);

}
#endif
void fMainWindow::TrainNewFetalModel(const std::string &datadirectory, const std::string &outputdirectory)
{
  updateProgress(0, "Parsing Input directory");
  updateProgress(30, "Feature Extraction");
  mfetalbrain.training(datadirectory, outputdirectory, m_fetalbrainfeatures);
  updateProgress(100, "Training complete.");
  statusbar->showMessage(QString::fromStdString(" Trained model saved in " + outputdirectory), 100);

}

void fMainWindow::skullstripfunc()
{
  fetalbrainpanel.setModal(false);
  std::string msg = "";
  ImageTypeFloat3D::Pointer mask;
  ImageTypeFloat3D::Pointer image;
  if (mSlicerManagers.size() > 0)
  {
    mask = getMaskImage();
    image = convertVtkToItk< ImageTypeFloat3D::PixelType, ImageTypeFloat3D::ImageDimension >(mSlicerManagers[0]->mImage);
  }
  else
  {
    ShowErrorMessage("Please provide an Image and mask", this);
    return;
  }
  std::string imagefilename = mSlicerManagers[0]->mPathFileName;
  std::string path, base, ext;

  updateProgress(0, "Reading Image and mask");

  cbica::splitFileName(imagefilename, path, base, ext);
  ImageTypeFloat3D::Pointer processed_input = mfetalbrain.apply_slicemask(image, mask, base);
  m_fetalslice = mfetalbrain.linear_features(mask, m_fetalbrainfeatures);
  ImageTypeFloat3D::Pointer segmask = mfetalbrain.segment(processed_input, m_fetalslice, m_tempFolderLocation);
  updateProgress(30, "Segmentation in Progress");
  if (segmask.IsNull())
  {
    return;
  }
  itk::ImageRegionIterator<ImageTypeFloat3D> imageIterator(segmask, segmask->GetLargestPossibleRegion());
  while (!imageIterator.IsAtEnd())
  {
    auto currentIndex = imageIterator.GetIndex();
    float val = imageIterator.Get();
    float* pData = (float*)this->mSlicerManagers[0]->GetSlicer(0)->mMask->GetScalarPointer((int)currentIndex[0], (int)currentIndex[1], (int)currentIndex[2]);
    if (val != 0)
    {
      *pData = val;
    }
    else
    {
      *pData = 0.0;
    }
    ++imageIterator;
  }
  this->mSlicerManagers[0]->GetSlicer(0)->mMask->Modified();
  this->mSlicerManagers[0]->Render();

  updateProgress(50, "Segmentation finished");
}

void fMainWindow::Predict()
{
  ImageTypeFloat3D::Pointer mask = convertVtkToItk< ImageTypeFloat3D::PixelType, ImageTypeFloat3D::ImageDimension >(mSlicerManagers[0]->mMask);
  cbica::WriteImage(mask, m_tempFolderLocation + "/corrected_ventri.nii");
  updateProgress(75, "Calculating features");
  mfetalbrain.Calculatefeatures(m_fetalbrainfeatures, m_fetalslice, m_tempFolderLocation);
  updateProgress(100, "Prediction done");

  cv::Mat predictedval = mfetalbrain.testsubject(m_fetalbrainfeatures);


  ShowMessage("Prediction Result: " + std::to_string(predictedval.at<float>(0, 0)) + "\n\n"
    "Threshold Range:" + " \n\n"     "Distance measure between 0 to 12.5 = Shunting required " + " \n"
    "Distance measure greater than 14 = Shunting required " + " \n"
    "Distance measure between 12.5 and 14 = Further analysis required " + " \n\n\n"
    "Threshold calculated based on 73 cohort subjects from CHOP" + " \n", this
  );


}

#ifdef BUILD_EGFRvIII
void fMainWindow::ApplicationEGFR()
{
  bool t1ceDataPresent = false;
  bool t2flairDataPresent = false;
  bool perfusionDataPresent = false;

  updateProgress(0, "Preprocessing");
  UpdateNumberOfPointsInTable();

  if (CheckCompletenessOfInputDataForEGFR(t1ceDataPresent, t2flairDataPresent, perfusionDataPresent) == false)
    return;

  if (!isMaskDefined())
  {
    ShowErrorMessage("EGFRvIII Estimation requires Near and Far regions to be initialized", this);
    help_contextual("Glioblastoma_PHI.html");
    return;
  }
  updateProgress(5);


  typedef ImageTypeFloat4D PerfusionImageType;

  ImageTypeFloat3D::Pointer T1CEImagePointer;
  ImageTypeFloat3D::Pointer T2FlairImagePointer;
  std::vector<ImageTypeFloat3D::Pointer>	PerfusionImagePointer;

  itk::MultiResolutionImageRegistrationMethod<ImageTypeFloat3D, ImageTypeFloat3D>::Pointer Registrar;
  std::vector<ImageTypeFloat3D::Pointer> Perfusion_Registered;
  PerfusionImageType::Pointer perfusionImage = PerfusionImageType::New();

  std::vector<ImageTypeFloat3D::IndexType> nearIndices;
  std::vector<ImageTypeFloat3D::IndexType> farIndices;
  FormulateNearFarPoints<ImageTypeFloat3D>(nearIndices, farIndices);
  std::string imagetype_string = "";

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1CE)
      T1CEImagePointer = mSlicerManagers[index]->mITKImage;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR)
      T2FlairImagePointer = mSlicerManagers[index]->mITKImage;
    else if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION)
    {
      if (mSlicerManagers[index]->mImageType == CAPTK::ImageExtension::NIfTI)
      {
        perfusionImage = mSlicerManagers[index]->mPerfusionImagePointer;
        imagetype_string = "nifti";
      }
      else
      {
        for (unsigned int seriesindex = 0; seriesindex < mSlicerManagers[index]->mPerfusionImagePointerDicom.size(); seriesindex++)
          PerfusionImagePointer.push_back(mSlicerManagers[index]->mPerfusionImagePointerDicom[seriesindex]);
        imagetype_string = "dicom";
      }
    }
  }

  Perfusion_Registered.resize(PerfusionImagePointer.size());
  if (imagetype_string == "dicom")
  {
    Registrar = mPreprocessingObj.Registration<ImageTypeFloat3D, ImageTypeFloat3D>(T1CEImagePointer, PerfusionImagePointer[0]);
    updateProgress(10);

    for (unsigned int index = 0; index < PerfusionImagePointer.size(); index++)
    {
      Perfusion_Registered[index] = mPreprocessingObj.ResampleTransform<ImageTypeFloat3D>(Registrar, T1CEImagePointer, PerfusionImagePointer[index]);
      updateProgress((index + 1) * 2 + 10);
    }
  }
  VectorDouble EGFRStatusParams;

  EGFRStatusPredictor EGFRPredictor;

  if (imagetype_string == "dicom")
    EGFRStatusParams = EGFRPredictor.PredictEGFRStatus<ImageTypeFloat3D, PerfusionImageType>(perfusionImage, Perfusion_Registered, nearIndices, farIndices, CAPTK::ImageExtension::DICOM);
  else
    EGFRStatusParams = EGFRPredictor.PredictEGFRStatus<ImageTypeFloat3D, PerfusionImageType>(perfusionImage, Perfusion_Registered, nearIndices, farIndices, CAPTK::ImageExtension::NIfTI);


  QString msg;
  msg = "PHI = " + QString::number(EGFRStatusParams[0]) + "\n\n----------\n\n(Near:Far) Peak Height ratio = " +
    QString::number(EGFRStatusParams[1] / EGFRStatusParams[2]) + "\n\nNear ROI voxels used = " +
    QString::number(EGFRStatusParams[3]) + "/" + QString::number(nearIndices.size()) + "\nFar ROI voxels used =   " +
    QString::number(EGFRStatusParams[4]) + "/" + QString::number(farIndices.size()) +
    "\n\n\n----------\n\nPHI Threshold = 0.1377\n[based on 142 UPenn brain tumor scans]";

  updateProgress(0);
  ShowMessage(msg.toStdString(), this);
}
#endif

#ifdef BUILD_RECURRENCE
void fMainWindow::ApplicationRecurrence()
{
  {
    recurrencePanel.SetCurrentImagePath(m_tempFolderLocation.c_str());
    recurrencePanel.exec();
  }
}
#endif


#ifdef BUILD_PSEUDOPROGRESSION
void fMainWindow::ApplicationPseudoProgression()
{
  {
    pseudoPanel.SetCurrentImagePath(m_tempFolderLocation.c_str());
    pseudoPanel.exec();
  }
}
#endif


#ifdef BUILD_WHITESTRIPE
void fMainWindow::ApplicationWhiteStripe()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    ShowErrorMessage("At least 1 supported image needs to be loaded and selected", this);
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);

  if ((mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T1) || 
    (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_T2))
  {
    auto tmp = mInputPathName.toStdString();
    whiteStripeNormalizer.SetCurrentImagePath(mInputPathName);
    whiteStripeNormalizer.SetImageModality(mSlicerManagers[index]->mImageSubType);
    whiteStripeNormalizer.exec();
  }
  else
  {
    ShowErrorMessage("Modalities other than T1 or T2 are not supported in WhiteStripe.", this);
    return;
  }
}
#endif

#ifdef BUILD_ATLAS
void fMainWindow::ApplicationPopulationAtlas()
{
  atlasPanel.SetCurrentImagePath(mInputPathName);
  atlasPanel.exec();
}
#endif

#ifdef BUILD_ISUBTYPE
void fMainWindow::ApplicationImagingSubtype()
{
  int imagetype_int = NIfTI;
  bool t1P = false;
  bool t2P = false;
  bool t1ceP = false;
  bool t2flairP = false;
  bool dtiAXP = false;
  bool dtiFAP = false;
  bool dtiRADP = false;
  bool dtiTRP = false;
  bool perfPHP = false;
  bool perfPSRP = false;
  bool perfRCBVP = false;


  std::string msg = "";

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_T1CE)
      t1ceP = true;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_T2FLAIR)
      t2flairP = true;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_T2)
      t2P = true;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_T1)
      t1P = true;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_AX)
      dtiAXP = true;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_FA)
      dtiFAP = true;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_RAD)
      dtiRADP = true;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_TR)
      dtiTRP = true;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_PH)
      perfPHP = true;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_PSR)
      perfPSRP = true;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_RCBV)
      perfRCBVP = true;
  }
  if (t1ceP == false)
    msg = msg + "\n" + "T1CE Data.";
  if (t1P == false)
    msg = msg + "\n" + "T1 Data.";
  if (t2P == false)
    msg = msg + "\n" + "T2 Data.";
  if (t2flairP == false)
    msg = msg + "\n" + "T2-FLAIR Data.";
  if (dtiAXP == false)
    msg = msg + "\n" + "AX Data.";
  if (dtiFAP == false)
    msg = msg + "\n" + "FA Data.";
  if (dtiRADP == false)
    msg = msg + "\n" + "RAD Data.";
  if (dtiTRP == false)
    msg = msg + "\n" + "TR Data.";

  if (perfPHP == false)
    msg = msg + "\n" + "PH Data.";
  if (perfPSRP == false)
    msg = msg + "\n" + "PSR Data.";
  if (perfRCBVP == false)
    msg = msg + "\n" + "RCBV Data.";

  if (msg != "")
  {
    msg = "Please provide the following items:\n" + msg;
    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.addButton(QMessageBox::Ok);
    box.setText(QString::fromStdString(msg));
    box.setWindowTitle(tr("Missing Data"));
    box.exec();
    return;
  }

  ImageTypeFloat3D::Pointer T1CEImagePointer;
  ImageTypeFloat3D::Pointer T2FlairImagePointer;
  ImageTypeFloat3D::Pointer T1ImagePointer;
  ImageTypeFloat3D::Pointer T2ImagePointer;
  ImageTypeFloat3D::Pointer AXImagePointer;
  ImageTypeFloat3D::Pointer FAImagePointer;
  ImageTypeFloat3D::Pointer RADImagePointer;
  ImageTypeFloat3D::Pointer TRImagePointer;
  ImageTypeFloat3D::Pointer PHImagePointer;
  ImageTypeFloat3D::Pointer PSRImagePointer;
  ImageTypeFloat3D::Pointer RCBVImagePointer;

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_T1CE)
      T1CEImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_T2FLAIR)
      T2FlairImagePointer = mSlicerManagers[index]->mITKImage;
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_T1)
      T1ImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_T2)
      T2ImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_AX)
      AXImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_FA)
      FAImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_RAD)
      RADImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_TR)
      TRImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);

    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_PH)
      PHImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_PSR)
      PSRImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
    else if (mSlicerManagers[index]->mImageSubType == IMAGE_TYPE_RCBV)
      RCBVImagePointer = RescaleImageIntensity(mSlicerManagers[index]->mITKImage);
  }
  ImageTypeFloat3D::Pointer LabelImagePointer = convertVtkToItk<float, 3>(mSlicerManagers[0]->mMask);
  //------------------------------------------noise reduction----------------------------------------------

  mImagingSubtype.SubtypePredictionOnExistingModel<ImageType>(T1CEImagePointer, T2FlairImagePointer, T1ImagePointer, T2ImagePointer,
    RCBVImagePointer, PSRImagePointer, PHImagePointer, AXImagePointer, FAImagePointer, RADImagePointer, TRImagePointer, LabelImagePointer);
  LoadSlicerImages(mOutputManager.GetOutputDirectoryPath() + "/" + mRecurrenceEstimator.mRecurrenceMapFileName, NIfTI);
  updateProgress(0, "Recurrence estimate for the given subject has been saved and loaded.");
}
#endif

#ifdef BUILD_MSUBTYPE
void fMainWindow::ApplicationMolecularSubtype()
{
  msubtypePanel.SetCurrentImagePath(mInputPathName);
  msubtypePanel.exec();
}
#endif


#ifdef BUILD_SURVIVAL
void fMainWindow::ApplicationSurvival()
{
  survivalPanel.SetCurrentImagePath(mInputPathName);
  survivalPanel.setModal(false);
  survivalPanel.exec();
}
#endif

#ifdef BUILD_EGFRvIIISVM
void fMainWindow::ApplicationEGFRvIIISVM()
{
  egfrv3Panel.SetCurrentImagePath(mInputPathName);
  egfrv3Panel.setModal(false);
  egfrv3Panel.exec();
}
#endif

#ifdef BUILD_ITKSNAP
void fMainWindow::ApplicationITKSNAP()
{
  if (mSlicerManagers.size() > 0)
  {
  }
  else
  {
    ShowErrorMessage("Please load a single image and ROI before calling ITK-SNAP", this);
    return;
  }

  ImageTypeFloat3D::Pointer mask = convertVtkToItk< ImageTypeFloat3D::PixelType, ImageTypeFloat3D::ImageDimension >(mSlicerManagers[0]->mMask);
  std::string maskFile = m_tempFolderLocation + "/mask.nii.gz";

  cbica::WriteImage<ImageTypeFloat3D>(mask, maskFile);

  Registry r;
  r["SaveLocation"] << m_tempFolderLocation;
  r["Version"] << "20161029";

  for (int i = 0; i < m_imagesTable->rowCount(); i++)
  {
    Registry &rl = r.Folder(Registry::Key("Layers.Layer[%03d]", i));
    rl["AbsolutePath"] << mSlicerManagers[i]->mPathFileName;
    if (i == 0)
    {
      rl["Role"] << "MainRole";
    }
    else
    {
      rl["Role"] << "OverlayRole";
    }

    // LayerMetaData
    Registry &rlLayerMetaData = rl.Folder(Registry::Key("LayerMetaData"));
    rlLayerMetaData["Alpha"] << "255";
    rlLayerMetaData["CustomNickName"] << "";
    rlLayerMetaData["Sticky"] << "0";
    Registry &rlMetaDataDisplayMap = rlLayerMetaData.Folder(Registry::Key("DisplayMapping"));
    Registry &rlMetaDataDisplayMapColor = rlMetaDataDisplayMap.Folder(Registry::Key("ColorMap"));
    rlMetaDataDisplayMapColor["ColorMap"] << "";
    rlMetaDataDisplayMapColor["Preset"] << "Grayscale";

    // ProjectMetaData
    Registry &rlProjectMetaData = rl.Folder(Registry::Key("ProjectMetaData"));
    rlProjectMetaData["GaussianBlurScale"] << "1";
    rlProjectMetaData["RemappingExponent"] << "3";
    rlProjectMetaData["RemappingSteepness"] << "0.04";
    Registry &rlProjectMetaData_IRIS = rlProjectMetaData.Folder(Registry::Key("IRIS"));
    rlProjectMetaData_IRIS["SliceViewLayerLayout"] << "Stacked";

  }

  // stuff for the mask
  Registry &rMask = r.Folder(Registry::Key("Layers.Layer[%03d]", m_imagesTable->rowCount()));
  rMask["AbsolutePath"] << maskFile;
  rMask["Role"] << "SegmentationRole";
  Registry &rMaskFolder = rMask.Folder(Registry::Key("LayerMetaData"));
  rMaskFolder["Alpha"] << "6.21436e-310";
  rMaskFolder["CustomNickName"] << "";
  rMaskFolder["Sticky"] << "1";

  r.WriteToXMLFile(std::string(m_tempFolderLocation + "/testXML.itksnap").c_str());

  std::string scriptToCall = captk_currentApplicationPath +
#ifdef Q_OS_WIN32
    "/itksnap.exe"
#else
    "/itksnap"
#endif
    ;

  QString itkSnapLocation = scriptToCall.c_str();
  QStringList itkSnapArgs;
  itkSnapArgs << "-w" << std::string(m_tempFolderLocation + "/testXML.itksnap").c_str();
  startExternalProcess(itkSnapLocation, itkSnapArgs);
  //  std::system((scriptToCall + "-w " + m_tempFolderLocation + "/testXML.itksnap").c_str());
  readMaskFile(maskFile);
  updateProgress(0, "Mask saved from ITK-SNAP loaded");
}
#endif

#ifdef BUILD_GEODESICTRAIN
void fMainWindow::ApplicationGeoTrain()
{
  m_imgGeodesicOut = NULL;
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    ShowErrorMessage("Please specify an input image.");
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index < 0 || index >= (int)mSlicerManagers.size())
  {
    ShowErrorMessage("Please specify an input image.");
    return;
  }
  updateProgress(5, "Running Geodesic Training");
  VectorVectorDouble tumorPoints = FormulateDrawingPointsForTumorSegmentation();
  if (tumorPoints.size() == 0)
  {
    ShowErrorMessage("Please draw inner and outer seed points.");
    return;
  }

  //if (cbica::directoryExists(m_tempFolderLocation))
  //{
  //  auto temp = cbica::stringSplit(cbica::getCurrentLocalTime(), ":");
  //  m_tempFolderLocation += temp[0] + temp[1] + temp[2] + "/";
  //}


  QString extensions = IMAGES_EXTENSIONS;
  extensions += ";;All Files (*)";
  QString file;
  typedef ImageTypeFloat3D ImageType;
  typedef ImageTypeShort3D ImageTypeGeodesic;
  updateProgress(10, "Running Geodesic Training");
  std::vector<ImageType::Pointer> Inp;
  for (size_t i = 0; i < mSlicerManagers.size(); i++)
  {
    Inp.push_back(mSlicerManagers[i]->mITKImage);
  }
  ImageTypeFloat3D::Pointer mask = convertVtkToItk<float, 3>(mSlicerManagers[0]->mMask);
  updateProgress(15, "Running Geodesic Training");

  itk::ImageRegionIterator<ImageTypeFloat3D> maskiter(mask, mask->GetLargestPossibleRegion());
  maskiter.GoToBegin();

  if (m_InputGeomasks.IsNotNull())
  {
    itk::ImageRegionIterator<ImageTypeFloat3D> previousmask(m_InputGeomasks, m_InputGeomasks->GetLargestPossibleRegion());
    previousmask.GoToBegin();
    while (!maskiter.IsAtEnd())
    {
      auto currentIndex = maskiter.GetIndex();
      previousmask.SetIndex(currentIndex);
      previousmask.Set(maskiter.Get());
    }
  }
  else
  {
    m_InputGeomasks = mask;
  }

  Geotrain<ImageTypeFloat3D> geotraining;
  geotraining.SetInputImage(Inp);
  geotraining.SetMask(m_InputGeomasks);
  geotraining.SetOutputpath(loggerFolder + "../GeodesicSVM");
  geotraining.Update();


  updateProgress(85, "Running Geodesic Training");
  auto filter = itk::RescaleIntensityImageFilter< ImageTypeShort3D, ImageTypeShort3D >::New();

  //for positve probablity
  filter->SetInput(geotraining.m_geoOutputPos);
  filter->SetOutputMinimum(0);
  filter->SetOutputMaximum(255);
  filter->Update();
  //  Inp = filter->GetOutput();
  m_imgGeodesicOutPositive = filter->GetOutput();
  updateProgress(90, "Displaying Geodesic Segmentation");
  ApplicationGeodesicTreshold();


  //for negative probablity
  filter->SetInput(geotraining.m_geoOutputNeg);
  filter->SetOutputMinimum(0);
  filter->SetOutputMaximum(255);
  filter->Update();
  //  Inp = filter->GetOutput();
  m_imgGeodesicOutNegative = filter->GetOutput();
  updateProgress(90, "Displaying Geodesic Segmentation");
  ApplicationGeodesicTreshold();


  updateProgress(0, "Geodesic Segmentation Finished!");
  presetComboBox->setCurrentIndex(PRESET_GEODESIC);

}
#endif

//#ifdef BUILD_GEODESIC_TRAINING
void fMainWindow::ApplicationGeodesicTraining()
{
  updateProgress(0, "Geodesic Training Segmentation: Started");
  //m_imgGeodesicOut = NULL;
  // QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  // if (items.empty())
  // {
  //   ShowErrorMessage("Please specify an input image.", this);
  //   return;
  // }
  // int index = GetSlicerIndexFromItem(items[0]);
  // if (index < 0 || index >= (int)mSlicerManagers.size())
  // {
  //   ShowErrorMessage("Please specify an input image.", this);
  //   return;
  // }
  // VectorVectorDouble tumorPoints = FormulateDrawingPointsForTumorSegmentation();
  // if (tumorPoints.size() == 0)
  // {
  //   ShowErrorMessage("Please draw initial ROI using Label 1.", this);
  //   m_tabWidget->setCurrentIndex(2);
  //   return;
  // }
  typedef typename itk::Image<float, 3>::Pointer InputImagePointer;
  typedef typename itk::Image<int, 3>::Pointer   LabelsImagePointer;

  LabelsImagePointer mask = convertVtkToItk<int, 3>(mSlicerManagers[0]->mMask);

  std::vector<InputImagePointer> inputImages;

  for (SlicerManager* sm : mSlicerManagers)
  {
    //inputImages.push_back(convertVtkToItk<float, 3>(sm->mITKImage));
    inputImages.push_back(sm->mITKImage);
  }

  GeodesicTrainingSegmentation::Coordinator<float, 3> geodesicTraining;

  geodesicTraining.SetInputImages(inputImages);
  geodesicTraining.SetLabels(mask);

  auto executeResult = geodesicTraining.Execute();
  
  if (executeResult->ok) {
    LabelsImagePointer labelsRenamedPtr = executeResult->labelsImage; // The result segmantation mask
    
    //BOOK
    typedef  itk::ImageFileWriter< itk::Image<int, 3> > WriterType;
    WriterType::Pointer writer = WriterType::New();
    writer->SetFileName("out.nii.gz");
    writer->SetInput(labelsRenamedPtr);
    writer->Update();
    //EO BOOK

    typedef itk::CastImageFilter<itk::Image<int,3>, itk::Image<short, 3>> CastFilterType;
    CastFilterType::Pointer castFilter = CastFilterType::New();
    castFilter->SetInput(labelsRenamedPtr);
    castFilter->Update();
    m_imgGeodesicOut = castFilter->GetOutput();
    ApplicationGeodesicTreshold();
    updateProgress(0, "Geodesic Training Segmentation: Finished");
    presetComboBox->setCurrentIndex(PRESET_GEODESIC);
  }
  else {
    updateProgress(0, "Geodesic Training Segmentation: Something went wrong");
    //std::cerr << executeResult->errorMessage << "\n";
  }

  // updateProgress(5, "Running Geodesic Segmentation");
  // typedef ImageTypeFloat3D ImageType;
  // //typedef ImageTypeShort3D ImageTypeGeodesic;
  // updateProgress(10, "Running Geodesic Segmentation");
  // ImageTypeGeodesic::Pointer Inp = ImageTypeGeodesic::New();
  // typedef itk::CastImageFilter<ImageType, ImageTypeGeodesic> CastFilterType;
  // CastFilterType::Pointer castFilter = CastFilterType::New();
  // castFilter->SetInput(/*convertVtkToItk<float, 3>*/(mSlicerManagers[index]->mITKImage));
  // Inp = castFilter->GetOutput();
  // auto filter = itk::RescaleIntensityImageFilter< ImageTypeGeodesic, ImageTypeGeodesic >::New();
  // filter->SetInput(Inp);
  // filter->SetOutputMinimum(0);
  // filter->SetOutputMaximum(255);
  // filter->Update();
  // Inp = filter->GetOutput();
  // updateProgress(15, "Running Geodesic Segmentation");

  // GeodesicSegmentation< ImageTypeGeodesic > geodesicSegmentor;
  // Inp = geodesicSegmentor.Run/*<ImageTypeGeodesic>*/(Inp, tumorPoints);

  // updateProgress(85, "Running Geodesic Segmentation");
  // filter->SetInput(Inp);
  // filter->SetOutputMinimum(0);
  // filter->SetOutputMaximum(255);
  // filter->Update();
  // Inp = filter->GetOutput();
  // m_imgGeodesicOut = Inp;
  // updateProgress(90, "Displaying Geodesic Segmentation");
  // ApplicationGeodesicTreshold();
  // updateProgress(0, "Geodesic Segmentation Finished!");
  // presetComboBox->setCurrentIndex(PRESET_GEODESIC);

}
//#endif

#ifdef BUILD_GEODESIC
void fMainWindow::ApplicationGeodesic()
{
  m_imgGeodesicOut = NULL;
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    ShowErrorMessage("Please specify an input image.", this);
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);
  if (index < 0 || index >= (int)mSlicerManagers.size())
  {
    ShowErrorMessage("Please specify an input image.", this);
    return;
  }
  VectorVectorDouble tumorPoints = FormulateDrawingPointsForTumorSegmentation();
  if (tumorPoints.size() == 0)
  {
    ShowErrorMessage("Please draw initial ROI using Label 1.", this);
    m_tabWidget->setCurrentIndex(2);
    return;
  }

  updateProgress(5, "Running Geodesic Segmentation");
  typedef ImageTypeFloat3D ImageType;
  typedef ImageTypeShort3D ImageTypeGeodesic;
  updateProgress(10, "Running Geodesic Segmentation");
  ImageTypeGeodesic::Pointer Inp = ImageTypeGeodesic::New();
  typedef itk::CastImageFilter<ImageType, ImageTypeGeodesic> CastFilterType;
  CastFilterType::Pointer castFilter = CastFilterType::New();
  castFilter->SetInput(/*convertVtkToItk<float, 3>*/(mSlicerManagers[index]->mITKImage));
  Inp = castFilter->GetOutput();
  auto filter = itk::RescaleIntensityImageFilter< ImageTypeGeodesic, ImageTypeGeodesic >::New();
  filter->SetInput(Inp);
  filter->SetOutputMinimum(0);
  filter->SetOutputMaximum(255);
  filter->Update();
  Inp = filter->GetOutput();
  updateProgress(15, "Running Geodesic Segmentation");

  GeodesicSegmentation< ImageTypeGeodesic > geodesicSegmentor;
  Inp = geodesicSegmentor.Run/*<ImageTypeGeodesic>*/(Inp, tumorPoints);

  updateProgress(85, "Running Geodesic Segmentation");
  filter->SetInput(Inp);
  filter->SetOutputMinimum(0);
  filter->SetOutputMaximum(255);
  filter->Update();
  Inp = filter->GetOutput();
  m_imgGeodesicOut = Inp;
  updateProgress(90, "Displaying Geodesic Segmentation");
  ApplicationGeodesicTreshold();
  updateProgress(0, "Geodesic Segmentation Finished!");
  presetComboBox->setCurrentIndex(PRESET_GEODESIC);

}
#endif
void fMainWindow::ApplicationGeodesicTreshold()
{
  if (m_imgGeodesicOut.IsNull())
  {
    return;
  }
  itk::ImageRegionIterator<ImageTypeShort3D> imageIterator(m_imgGeodesicOut, m_imgGeodesicOut->GetLargestPossibleRegion());
  while (!imageIterator.IsAtEnd())
  {
    auto currentIndex = imageIterator.GetIndex();
    float val = imageIterator.Get();
    float* pData = (float*)this->mSlicerManagers[0]->GetSlicer(0)->mMask->GetScalarPointer((int)currentIndex[0], (int)currentIndex[1], (int)currentIndex[2]);
    if (val < thresholdSpinBox->value())
    {
      *pData = 1.0;
    }
    else
    {
      *pData = 0.0;
    }
    ++imageIterator;
  }
  this->mSlicerManagers[0]->GetSlicer(0)->mMask->Modified();
  this->mSlicerManagers[0]->Render();
}
void fMainWindow::ImageDenoising()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    ShowErrorMessage("Please load the image you would like to de-noise", this);
    return;
  }

  int index = GetSlicerIndexFromItem(items[0]);
  if (index < 0 || index >= (int)mSlicerManagers.size())
    return;

  QString saveFileName = getSaveFile(this, mInputPathName, mInputPathName + "denoise.nii.gz");
  if (!saveFileName.isEmpty())
  {
    auto saveFileName_string = saveFileName.toStdString();
    //Job TBD replace with app name
    typedef itk::ImageDuplicator<ImageTypeFloat3D> DuplicatorType;
    DuplicatorType::Pointer duplicator = DuplicatorType::New();
    duplicator->SetInputImage(mSlicerManagers[index]->mITKImage);
    duplicator->Update();
    ImageTypeFloat3D::Pointer inputImage = duplicator->GetOutput();
    updateProgress(5, "Susan noise removal in process");
    SusanDenoising denoising /*= SusanDenoising()*/;
    ImageTypeFloat3D::Pointer outputImage = denoising.Run<ImageTypeFloat3D>(inputImage);
    if (outputImage.IsNotNull())
    {
      updateProgress(80, "Saving file");
      cbica::WriteImage< ImageTypeFloat3D >(outputImage, saveFileName_string);
      if (cbica::fileExists(saveFileName_string))
      {
        updateProgress(90, "Displaying output");
        LoadSlicerImages(saveFileName_string, CAPTK::ImageExtension::NIfTI);

      }
      updateProgress(0, "Susan noise removal finished");
    }
    else
    {
      updateProgress(0, "Error in Susan noise removal!!");
    }
  }
}
void fMainWindow::ImageBiasCorrection()
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    ShowErrorMessage("Please load an image to run bias correction on", this);
    return;
  }

  int index = GetSlicerIndexFromItem(items[0]);
  if (index < 0 || index >= (int)mSlicerManagers.size())
    return;

  QString saveFileName = getSaveFile(this, mInputPathName, mInputPathName + "biasCorrect.nii.gz");
  if (!saveFileName.isEmpty())
  {
    auto saveFileName_string = saveFileName.toStdString();
    typedef itk::ImageDuplicator<ImageType> DuplicatorType;
    DuplicatorType::Pointer duplicator = DuplicatorType::New();
    duplicator->SetInputImage(mSlicerManagers[index]->mITKImage);
    duplicator->Update();
    ImageType::Pointer inputImage = duplicator->GetOutput();
    updateProgress(5, "Bias correction in process");
    N3BiasCorrection biasCorrecter /*= N3BiasCorrection()*/;
    ImageType::Pointer outputImage = biasCorrecter.Run<ImageTypeFloat3D>(inputImage);
    if (outputImage.IsNotNull())
    {
      updateProgress(80, "Saving file");
      cbica::WriteImage< ImageTypeFloat3D >(outputImage, saveFileName_string);
      if (cbica::fileExists(saveFileName_string))
      {
        updateProgress(90, "Displaying output");
        LoadSlicerImages(saveFileName_string, CAPTK::ImageExtension::NIfTI);
      }
      updateProgress(0, "Bias correction finished");
    }
    else
    {
      updateProgress(0, "Error in Bias correction!");
    }
  }
}
void fMainWindow::ImageRegistration()
{
  registrationPanel.mInputPathName = mInputPathName;
  registrationPanel.exec();
}

void fMainWindow::ImageHistogramMatching()
{
  // open a simple dialog box with reference image, input and output
  histoMatchPanel.exec();
}

void fMainWindow::ImageDeepMedicNormalizer()
{
#ifndef WIN32
  ShowErrorMessage("DeepMedic is currently not available for your platform but will be soon.", this);
  return;
#endif
  // open a simple dialog box with reference image, input and output
  deepMedicNormPanel.exec();
}

void fMainWindow::ImageSkullStripping()
{
  // open a simple dialog box with reference image, input and output
  skullStrippingPanel.exec();
}
void fMainWindow::ApplicationPCA()
{
  typedef ImageTypeFloat3D ImageType;
  typedef ImageTypeFloat4D PerfusionImageType;
  ImageTypeFloat4D::Pointer perfusionImage = ImageTypeFloat4D::New();
  std::string msg = "";


  bool perfusionDataPresent = false;
  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION)
    {
      perfusionDataPresent = true;
      perfusionImage = mSlicerManagers[index]->mPerfusionImagePointer;
    }
  }
  if (perfusionDataPresent == false)
    msg = msg + "\n DSC-MRI scan";


  if (mSlicerManagers.size() > 0)
  {
    ImageTypeShort3D::Pointer img = convertVtkToItk<short, 3>(mSlicerManagers[0]->mMask);
    int  mask_counter = 0;

    typedef itk::ImageRegionIteratorWithIndex <ImageTypeShort3D> IteratorType;
    IteratorType maskIt(img, img->GetLargestPossibleRegion());
    maskIt.GoToBegin();
    while (!maskIt.IsAtEnd())
    {
      if (maskIt.Get() == CAPTK::GLISTR_OUTPUT_LABELS::NONENHANCING)
        mask_counter++;
      ++maskIt;
    }
    if (mask_counter == 0)
      msg = msg + "\n" + "Segmentation Label: 1.";
  }
  else
  {
    msg = msg + "\n" + "Segmentation Label: 1.";
  }

  if (!msg.empty())
  {
    ShowErrorMessage(msg, this);
  }
  
  pcaPanel.exec();
}
void fMainWindow::PerfusionMeasuresCalculation()
{
  perfmeasuresPanel.exec();
}
void fMainWindow::DiffusionMeasuresCalculation()
{
  //open a simple dialog box with input and output images
  diffmeasuresPanel.exec();
}

void fMainWindow::ClassifierTraining()
{
  //open a simple dialog box with input and output images
  trainingPanel.exec();
}

void fMainWindow::ApplicationTheia()
{
  if (!mSlicerManagers.empty())
  {
    std::string maskFile = m_tempFolderLocation + "/theia_mask.nii.gz";
    cbica::WriteImage< ImageTypeFloat3D >(getMaskImage(), maskFile);

    auto items = m_imagesTable->selectedItems();
    auto index = GetSlicerIndexFromItem(items[0]);

    QStringList args;
    args << "-i" << mSlicerManagers[index]->GetFileName().c_str() << "-m" << maskFile.c_str();

    startExternalProcess(getApplicationPath("Theia").c_str(), args);
  }
  else
  {
    ShowErrorMessage("Please load at least a single image before trying 3D Visualizer", this);
    return;
  }
}

void fMainWindow::ApplicationDeepMedicSegmentation()
{
  if (mSlicerManagers.size() < 4)
  {
    ShowErrorMessage("DeepMedic needs the following images to work: T1CE, T1, T2, FLAIR", this);
    return;
  }
  deepMedicDialog.SetCurrentImagePath(mInputPathName);
  deepMedicDialog.exec();
}

void fMainWindow::CallDeepMedicSegmentation(const std::string outputDirectory)
{
  std::string file_t1ce, file_t1, file_flair, file_t2;
  auto statsCalculator = itk::StatisticsImageFilter< ImageTypeFloat3D >::New();
  bool noMaskNorm = false;

  cbica::createDir(outputDirectory);

  auto file_mask = outputDirectory + "/dm_mask.nii.gz";
  if (!isMaskDefined())
  {
    file_mask = "";
  }
  else
  {
    cbica::WriteImage< TImageType >(getMaskImage(), file_mask);
  }

  int progressBar = 0;
  for (size_t i = 0; i < mSlicerManagers.size(); i++)
  {
    switch (mSlicerManagers[i]->mImageSubType)
    {
    case CAPTK::ImageModalityType::IMAGE_TYPE_T1CE:
      file_t1ce = mSlicerManagers[i]->GetPathFileName();
      break;
    case CAPTK::ImageModalityType::IMAGE_TYPE_T1:
      file_t1 = mSlicerManagers[i]->GetPathFileName();
      break;
    case CAPTK::ImageModalityType::IMAGE_TYPE_T2:
      file_t2 = mSlicerManagers[i]->GetPathFileName();
      break;
    case CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR:
      file_flair = mSlicerManagers[i]->GetPathFileName();
      break;
    default:
      ShowErrorMessage("DeepMedic needs the following images to work: T1CE, T1, T2, FLAIR", this);
      break;
    }
  }

  std::string dataDir = getCaPTkDataDir() + "deepMedic/";

  QStringList args;
  args /*<< "-model" << (dataDir + "to_use/modelConfig.txt").c_str()*/
    << "-t1" << file_t1.c_str() << "-t1c" << file_t1ce.c_str() << "-t2" << file_t2.c_str() << "-fl" << file_flair.c_str() << "-o" << outputDirectory.c_str();
  
  if (!file_mask.empty())
  {
    args << "-m" << file_mask.c_str();
  }
  updateProgress(5, "Starting DeepMedic Segmentation");

  auto dmExe = getApplicationPath("DeepMedic");
  auto temp = args.join(" ").toStdString();
  if (!cbica::exists(dmExe))
  {
    ShowErrorMessage("DeepMedic executable doesn't exist; can't run");
    updateProgress(0, "");
    return;
  }

   if (std::system((dmExe + " " + temp).c_str()) != 0)
  {
    ShowErrorMessage("DeepMedic returned with exit code != 0");
    updateProgress(0, "");
    return;
  }

  auto output = outputDirectory + "/segm.nii.gz";
  if (cbica::exists(output))
  {
    readMaskFile(output);
    updateProgress(100, "Completed.");
  }
  else
  {
    ShowErrorMessage("DeepMedic failed to generate results");
    updateProgress(0, "");
  }

  return;
}

void fMainWindow::DCM2NIfTIConversion()
{
  dcmConverter.exec();
}

void fMainWindow::CallDCM2NIfTIConversion(const std::string firstImageInSeries, bool loadAsImage)
{
  std::string saveFolder = m_tempFolderLocation + "/dcmConv/";
  CallDCM2NIfTIConversion(firstImageInSeries, saveFolder);

  auto vectorOfFiles = cbica::filesInDirectory(saveFolder);

  if (loadAsImage)
  {
    LoadSlicerImages(vectorOfFiles[0], CAPTK::ImageExtension::NIfTI);
  }
  else
  {
    readMaskFile(vectorOfFiles[0]);
  }

}

void fMainWindow::CallDCM2NIfTIConversion(const std::string firstImageInSeries, const std::string outputFileName)
{
  const std::string inputDataDir = cbica::getFilenamePath(firstImageInSeries);

  auto vectorOfFiles = cbica::filesInDirectory(inputDataDir);

  std::string filesInDir = "" /*+ vectorOfFiles[0]*/;

  for (size_t i = 0; i < vectorOfFiles.size(); i++)
  {
    if ((vectorOfFiles[i] != ".") || (vectorOfFiles[i] != ".."))
    {
      filesInDir += " " + vectorOfFiles[i];
    }
  }

  std::string outputDir = cbica::getFilenamePath(outputFileName, false);
  outputDir = outputDir.substr(0, outputDir.size() - 1);

  if (!cbica::isDir(outputDir))
  {
    cbica::createDir(outputDir);
  }

  std::string fullCommandToRun = cbica::normPath(dcmConverter.m_exe.toStdString()) + " -a Y -r N -o " + outputDir + filesInDir;

  if (startExternalProcess(fullCommandToRun.c_str(), QStringList()) != 0)
  {
    ShowErrorMessage("Couldn't convert the DICOM with the default parameters; please use command line functionality");
    return;
  }
  else
  {
    ShowMessage("Saved in:\n\n " + outputDir, this, "DICOM Conversion Success");
  }

}

void fMainWindow::CallImageSkullStripping(const std::string referenceAtlas, const std::string referenceMask,
  const std::string inputImageFile, const std::string outputImageFile)
{
  ShowErrorMessage("Skull Stripping takes a long time to run, during which CaPTk will not be responsive.", this, "Long Running Application");
  auto referenceAtlasImage = cbica::ReadImage< ImageTypeFloat3D >(referenceAtlas);
  auto referenceAtlasMaskImage = cbica::ReadImage< ImageTypeFloat3D >(referenceMask);
  auto inputImageImage = cbica::ReadImage< ImageTypeFloat3D >(inputImageFile);

  auto outputImage = cbica::GetSkullStrippedImage< ImageTypeFloat3D >(inputImageImage, referenceAtlasImage, referenceAtlasMaskImage);

  if ((cbica::getFilenameExtension(outputImageFile) != ".nii") && (cbica::getFilenameExtension(outputImageFile) != ".nii.gz"))
  {
    std::string path, base, ext;
    cbica::splitFileName(outputImageFile, path, base, ext);
    cbica::WriteImage< ImageTypeFloat3D >(outputImage, path + base + ".nii.gz");
    LoadSlicerImages(path + base + ".nii.gz", CAPTK::ImageExtension::NIfTI);
  }
  else
  {
    cbica::WriteImage< ImageTypeFloat3D >(outputImage, outputImageFile);
    LoadSlicerImages(outputImageFile, CAPTK::ImageExtension::NIfTI);
  }
}

void fMainWindow::CallDirectionalityEstimator(const std::string roi1File, const std::string roi2File, const std::string outputDir)
{
  std::string errorMsg = "Directionality Estimation requires at least one TU Tissue Point to be seeded on a label map";
  if (!mTissuePoints)
  {
    ShowErrorMessage(errorMsg);
    m_tabWidget->setCurrentIndex(1);
    tumorPanel->SetTissueType(TU);
    return;
  }
  auto numberOfSeeds = mTissuePoints->GetNumberOfPoints();
  if (numberOfSeeds == 0)
  {
    ShowErrorMessage(errorMsg);
    m_tabWidget->setCurrentIndex(1);
    tumorPanel->SetTissueType(TU);
    return;
  }

  auto roi1Image = cbica::ReadImage< ImageTypeFloat3D >(roi1File);
  auto roi2Image = cbica::ReadImage< ImageTypeFloat3D >(roi2File);
  auto newROIImage = roi1Image;
  newROIImage->DisconnectPipeline();

  ImageTypeFloat3D::Pointer octantImage = ImageTypeFloat3D::New();
  octantImage->SetRegions(roi1Image->GetLargestPossibleRegion());
  octantImage->Allocate();
  octantImage->SetSpacing(roi1Image->GetSpacing());
  octantImage->SetOrigin(roi1Image->GetOrigin());
  octantImage->SetDirection(roi1Image->GetDirection());

  auto size_1 = roi1Image->GetLargestPossibleRegion().GetSize();
  auto size_2 = roi2Image->GetLargestPossibleRegion().GetSize();
  auto origin_1 = roi1Image->GetOrigin();
  auto origin_2 = roi2Image->GetOrigin();
  auto spacing_1 = roi1Image->GetSpacing();
  auto spacing_2 = roi2Image->GetSpacing();

  for (size_t i = 0; i < ImageTypeFloat3D::ImageDimension; i++)
  {
    if (size_1[i] != size_2[i])
    {
      ShowErrorMessage("Dimensions of the 2 ROIs do not match");
      return;
    }
    if (origin_1[i] != origin_2[i])
    {
      ShowErrorMessage("Origins of the 2 ROIs do not match");
      return;
    }
    if (spacing_1[i] != spacing_2[i])
    {
      ShowErrorMessage("Spacings of the 2 ROIs do not match");
      return;
    }
  }

  // visualizing ROI_post with 2 colors to highlight the post-injection region
  ImageTypeFloat3DIterator roi1It(roi1Image, roi1Image->GetLargestPossibleRegion()), roi2It(roi2Image, roi2Image->GetLargestPossibleRegion()), roiNew(newROIImage, newROIImage->GetLargestPossibleRegion()), octantIt(octantImage, octantImage->GetLargestPossibleRegion());

  for (octantIt.GoToBegin(); !octantIt.IsAtEnd(); ++octantIt)
    octantIt.Set(0);


  for (roi2It.GoToBegin(); !roi2It.IsAtEnd(); ++roi2It)
  {
    if (roi2It.Get() > 0)
    {
      auto currentIndex = roi2It.GetIndex();
      float* pData = (float*)this->mSlicerManagers[0]->GetSlicer(0)->mMask->GetScalarPointer((int)currentIndex[0], (int)currentIndex[1], (int)currentIndex[2]);

      roi1It.SetIndex(currentIndex);

      if (roi1It.Get() > 0) // if pre-injection ROI is defined, then the mask to be displayed is under label 1
      {
        *pData = 1.0;
      }
      else // this is for the section of the ROI which is post-injection
      {
        *pData = 2.0;
      }
    }
  }

  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    ShowErrorMessage("Please specify an input image.");
    help_contextual("Glioblastoma_Directionality.html");
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);

  auto minMaxCalc = itk::MinimumMaximumImageCalculator< ImageType >::New();
  minMaxCalc->SetImage(newROIImage);
  minMaxCalc->Compute();
  auto maxVal = minMaxCalc->GetMaximum();

  if (maxVal == 0)
  {
    ShowErrorMessage("Please specify an ROI. See documentation for details");
    help_contextual("Glioblastoma_Directionality.html");
    return;
  }

  QString output_msg = "";
  QString outputPoints = (m_tempFolderLocation + "/directionalityOutput_points.txt").c_str();

  bool singleIteration = false; // this to check if tissue points other than TU has been initialized

  auto imageOrigin = mSlicerManagers[index]->mOrigin /*labelMap->GetOrigin()*/;
  auto imageSpacing = roi2Image->GetSpacing();
  auto volumeMultiplier = imageSpacing[0] * imageSpacing[1] * imageSpacing[2];

  DirectionalityEstimate< ImageTypeFloat3D > directionalityEstimatorObj;
  directionalityEstimatorObj.SetInputMask(roi2Image);

  QString volumeString;

  for (size_t i = 0; i < numberOfSeeds; i++)
  {
    if (mTissuePoints->mLandmarks[i].id == TU)
    {
      singleIteration = true;
      ImageTypeFloat3D::IndexType currentIndex;
      itk::Point< float, 3 > pointFromTumorPanel = mTissuePoints->mLandmarks[i].coordinates;
      auto output_point = pointFromTumorPanel, output_xy_point = pointFromTumorPanel,
        output_yz_point = pointFromTumorPanel, output_xz_point = pointFromTumorPanel;

      double x_index = (/*-*/pointFromTumorPanel[0] - imageOrigin[0]) / imageSpacing[0];
      double y_index = (/*-*/pointFromTumorPanel[1] - imageOrigin[1]) / imageSpacing[1];
      double z_index = (pointFromTumorPanel[2] - imageOrigin[2]) / imageSpacing[2];

      // round up pixel values
      currentIndex[0] = ROUND(x_index /*+ pointFromTumorPanel[0]*/);
      currentIndex[1] = ROUND(y_index /*+ pointFromTumorPanel[1]*/);
      currentIndex[2] = ROUND(z_index);

      directionalityEstimatorObj.SetInputSeed(currentIndex);
      directionalityEstimatorObj.Update();
      auto tempOutput = directionalityEstimatorObj.GetOutputFull();// std::make_pair(directionalityEstimator.GetOutputMaxDistance(), directionalityEstimator.GetOutputIndex());

      auto output_full_index = tempOutput["ALL"].first;
      auto output_xy_index = tempOutput["XY"].first;
      auto output_yz_index = tempOutput["YZ"].first;
      auto output_xz_index = tempOutput["XZ"].first;

      for (size_t j = 0; j < 3; j++)
      {
        output_point[j] = output_full_index[j] * imageSpacing[j] + imageOrigin[j];
      }

      // volumes for
      std::map< int, float > octrantVolume_1, octrantVolume_2, octrantVolume_ratio, octrantVolume_percent;
      float volume_1 = 0, volume_2 = 0;

      // ensure everything is initialized
      for (size_t o = 0; o < 8; o++)
      {
        octrantVolume_1[o] = 0;
        octrantVolume_2[o] = 0;
        octrantVolume_ratio[o] = 0;
        octrantVolume_percent[o] = 0;
      }

      // initialize a lambda that takes the octCounter and roiValue from inside the mask iterator so that this step isn't repeated
      auto octFunc = [&](int counter, float roiValue)
      {
        octrantVolume_1[counter]++;
        octrantVolume_2[counter]++;

        if (roiValue == 2)
        {
          octrantVolume_2[counter]++;
        }
      };

      //-----------------------------------------------
      // mask iterator start
      for (roiNew.GoToBegin(); !roiNew.IsAtEnd(); ++roiNew)
      {
        if (roiNew.Get() > 0)
        {
          auto currentItrIndex = roiNew.GetIndex();
          roi2It.SetIndex(currentItrIndex);

          volume_1++;
          if (roi2It.Get() > 0)
          {
            volume_2++;
          }

          bool x = false, y = false, z = false;

          // initialize flags for where in the octrant space the current index is
          (currentItrIndex[0] > currentIndex[0]) ? x = true : x = false;
          (currentItrIndex[1] > currentIndex[1]) ? y = true : z = false;
          (currentItrIndex[2] > currentIndex[2]) ? z = true : z = false;

          if (!x && !y && !z) // 000
          {
            int counter = 0;
            octrantVolume_1[counter]++;

            if (roi2It.Get() > 0)
            {
              octrantVolume_2[counter]++;
            }
          }

          if (!x && !y && z) // 001
          {
            int counter = 1;
            octrantVolume_1[counter]++;

            if (roi2It.Get() > 0)
            {
              octrantVolume_2[counter]++;
            }
          }

          if (!x && y && !z) // 010
          {
            int counter = 2;
            octrantVolume_1[counter]++;

            if (roi2It.Get() > 0)
            {
              octrantVolume_2[counter]++;
            }
          }

          if (!x && y && z) // 011
          {
            int counter = 3;
            octrantVolume_1[counter]++;

            if (roi2It.Get() > 0)
            {
              octrantVolume_2[counter]++;
            }
          }

          if (x && !y && !z) // 100
          {
            int counter = 4;
            octrantVolume_1[counter]++;

            if (roi2It.Get() > 0)
            {
              octrantVolume_2[counter]++;
            }
          }

          if (x && !y && z) // 101
          {
            int counter = 5;
            octrantVolume_1[counter]++;

            if (roi2It.Get() > 0)
            {
              octrantVolume_2[counter]++;
            }
          }

          if (x && y && !z) // 110
          {
            int counter = 6;
            octrantVolume_1[counter]++;

            if (roi2It.Get() > 0)
            {
              octrantVolume_2[counter]++;
            }
          }

          if (x && y && z) // 111
          {
            int counter = 7;
            octrantVolume_1[counter]++;

            if (roi2It.Get() > 0)
            {
              octrantVolume_2[counter]++;
            }
          }

          // visualize the octrant where the dye moves
          // the following if loop is a collection of 3 X-NOR gates; see https://en.wikipedia.org/wiki/XNOR_gate
          if (!((output_full_index[0] > currentIndex[0]) ^ (currentItrIndex[0] > currentIndex[0]))
            && !((output_full_index[1] > currentIndex[1]) ^ (currentItrIndex[1] > currentIndex[1]))
            && !((output_full_index[2] > currentIndex[2]) ^ (currentItrIndex[2] > currentIndex[2])))
          {
            float* pData = (float*)this->mSlicerManagers[0]->GetSlicer(0)->mMask->GetScalarPointer((int)currentItrIndex[0], (int)currentItrIndex[1], (int)currentItrIndex[2]);
            *pData = 3.0;
          }

        }
      }
      //----------------------------------------------------
      // volumes for
      std::map< int, float > octrantVolume_1_revised, octrantVolume_2_revised, octrantVolume_ratio_revised, octrantVolume_percent_revised;
      float volume_1_revised = 0, volume_2_revised = 0;

      // ensure everything is initialized
      for (size_t o = 0; o < 8; o++)
      {
        octrantVolume_1_revised[o] = 0;
        octrantVolume_2_revised[o] = 0;
        octrantVolume_ratio_revised[o] = 0;
        octrantVolume_percent_revised[o] = 0;
      }

      roiNew.GoToBegin();
      roi2It.GoToBegin();
      octantIt.GoToBegin();

      while (!roiNew.IsAtEnd())
      {
        if (roiNew.Get() > 0)
          volume_1_revised++;

        if (roi2It.Get() > 0)
          volume_2_revised++;

        auto currentItrIndex = roiNew.GetIndex();
        bool x = false, y = false, z = false;
        // initialize flags for where in the octrant space the current index is
        (currentItrIndex[0] > currentIndex[0]) ? x = true : x = false;
        (currentItrIndex[1] > currentIndex[1]) ? y = true : z = false;
        (currentItrIndex[2] > currentIndex[2]) ? z = true : z = false;

        if (!x && !y && !z) // 000
        {
          int count = 0;
          if (roiNew.Get() > 0)
            octrantVolume_1_revised[count]++;
          if (roi2It.Get() > 0)
          {
            octrantVolume_2_revised[count]++;
            octantIt.Set(1);
          }
        }
        if (!x && !y && z) // 001
        {
          int count = 1;
          if (roiNew.Get() > 0)
            octrantVolume_1_revised[count]++;
          if (roi2It.Get() > 0)
          {
            octrantVolume_2_revised[count]++;
            octantIt.Set(2);
          }
        }

        if (!x && y && !z) // 010
        {
          int count = 2;
          if (roiNew.Get() > 0)
            octrantVolume_1_revised[count]++;
          if (roi2It.Get() > 0)
          {
            octrantVolume_2_revised[count]++;
            octantIt.Set(3);
          }
        }

        if (!x && y && z) // 011
        {
          int count = 3;
          if (roiNew.Get() > 0)
            octrantVolume_1_revised[count]++;
          if (roi2It.Get() > 0)
          {
            octrantVolume_2_revised[count]++;
            octantIt.Set(4);
          }
        }

        if (x && !y && !z) // 100
        {
          int count = 4;
          if (roiNew.Get() > 0)
            octrantVolume_1_revised[count]++;
          if (roi2It.Get() > 0)
          {
            octrantVolume_2_revised[count]++;
            octantIt.Set(5);
          }
        }

        if (x && !y && z) // 101
        {
          int count = 5;
          if (roiNew.Get() > 0)
            octrantVolume_1_revised[count]++;
          if (roi2It.Get() > 0)
          {
            octrantVolume_2_revised[count]++;
            octantIt.Set(6);
          }
        }

        if (x && y && !z) // 110
        {
          int count = 6;
          if (roiNew.Get() > 0)
            octrantVolume_1_revised[count]++;
          if (roi2It.Get() > 0)
          {
            octrantVolume_2_revised[count]++;
            octantIt.Set(7);
          }
        }

        if (x && y && z) // 111
        {
          int count = 7;
          if (roiNew.Get() > 0)
            octrantVolume_1_revised[count]++;
          if (roi2It.Get() > 0)
          {
            octrantVolume_2_revised[count]++;
            octantIt.Set(8);
          }
        }

        ++roiNew;
        ++roi2It;
        ++octantIt;
      }


      //------------------------------------------------

      volumeString += "Total volumetric change (post/pre): " + QString::number(volume_2_revised * 100 / volume_1_revised) + "%\n\n";

      volumeString += "Volumetric change per 3D octant (post/pre):\n\n";

      // calculate the actual volume for the 8 quadrants for each of the ROIs
      octrantVolume_1 = octrantVolume_1_revised;
      octrantVolume_2 = octrantVolume_2_revised;
      octrantVolume_ratio = octrantVolume_ratio_revised;
      octrantVolume_percent = octrantVolume_percent_revised;

      for (auto it_q1 = octrantVolume_1.begin(), it_q2 = octrantVolume_2.begin(), it_qRatio = octrantVolume_ratio.begin(), it_qPercent = octrantVolume_percent.begin();
        it_q1 != octrantVolume_1.end(); ++it_q1, ++it_q2, ++it_qRatio, ++it_qPercent)
      {
        it_qPercent->second = (it_q2->second - it_q1->second) / it_q1->second; // volumeMultiplier is not needed since it will get cancelled out
        it_qRatio->second = it_q2->second / it_q1->second; // volumeMultiplier is not needed since it will get cancelled out

        volumeString += "Octant[" + QString::number(it_q1->first + 1) + "]: Change = " + QString::number(it_qPercent->second * 100 + 100) + "%\n";
      }

      volumeString += "\n\nFurther details and detailed visualization results can be found in the output folder";

      // mask iterator start
      for (roiNew.GoToBegin(); !roiNew.IsAtEnd(); ++roiNew)
      {
        if (roiNew.Get() > 0)
        {
          auto currentItrIndex = roiNew.GetIndex();

          bool x = false, y = false, z = false;

          // initialize flags for where in the octrant space the current index is
          (currentItrIndex[0] > currentIndex[0]) ? x = true : x = false;
          (currentItrIndex[1] > currentIndex[1]) ? y = true : z = false;
          (currentItrIndex[2] > currentIndex[2]) ? z = true : z = false;

          if (!x && !y && !z) // 000
          {
            roiNew.Set(octrantVolume_ratio[0]);
          }

          if (!x && !y && z) // 001
          {
            roiNew.Set(octrantVolume_ratio[1]);
          }

          if (!x && y && !z) // 010
          {
            roiNew.Set(octrantVolume_ratio[2]);
          }

          if (!x && y && z) // 011
          {
            roiNew.Set(octrantVolume_ratio[3]);
          }

          if (x && !y && !z) // 100
          {
            roiNew.Set(octrantVolume_ratio[4]);
          }

          if (x && !y && z) // 101
          {
            roiNew.Set(octrantVolume_ratio[5]);
          }

          if (x && y && !z) // 110
          {
            roiNew.Set(octrantVolume_ratio[6]);
          }

          if (x && y && z) // 111
          {
            roiNew.Set(octrantVolume_ratio[7]);
          }
        }
      }

      output_xy_point = output_point;
      output_xy_point[2] = pointFromTumorPanel[2];
      output_yz_point = output_point;
      output_yz_point[0] = pointFromTumorPanel[0];
      output_xz_point = output_point;
      output_xz_point[1] = pointFromTumorPanel[1];

      auto dist_00 = pointFromTumorPanel.EuclideanDistanceTo(output_point);
      auto dist_xy = pointFromTumorPanel.EuclideanDistanceTo(output_xy_point);
      auto dist_yz = pointFromTumorPanel.EuclideanDistanceTo(output_yz_point);
      auto dist_xz = pointFromTumorPanel.EuclideanDistanceTo(output_xz_point);

      // place estimations back on the seed point map
      mSlicerManagers[index]->mTissuePoints->AddLandmark(output_point[0], output_point[1], output_point[2], 0.0, 0.0, RTN); // actual output
      mSlicerManagers[index]->mTissuePoints->AddLandmark(output_xy_point[0], output_xy_point[1], output_xy_point[2], 0.0, 0.0, CSF); // per-slice output
      mSlicerManagers[index]->mTissuePoints->AddLandmark(output_yz_point[0], output_yz_point[1], output_yz_point[2], 0.0, 0.0, CSF); // per-slice output
      mSlicerManagers[index]->mTissuePoints->AddLandmark(output_xz_point[0], output_xz_point[1], output_xz_point[2], 0.0, 0.0, CSF); // per-slice output

      tumorPanel->tSave(outputPoints);
      tumorPanel->tLoad(outputPoints);

      output_msg +=
        ":::Real World Coordinates:::\n\nSeedPoint: [" + QString::number(-pointFromTumorPanel[0]) + "," + QString::number(-pointFromTumorPanel[1]) + "," + QString::number(pointFromTumorPanel[2]) + "]\n" +
        "Actual Output {RTN}:\nCoordinate = [" + QString::number(output_point[0]) + "," + QString::number(output_point[1]) + "," + QString::number(output_point[2]) + "];\t Distance = " + QString::number(dist_00) + "\n" +
        " Center_panel {CSF}:\nCoordinate = [" + QString::number(-output_xy_point[0]) + "," + QString::number(-output_xy_point[1]) + "," + QString::number(output_xy_point[2]) + "];\t Distance = " + QString::number(dist_xy) + "\n" +
        " Right_panel  {CSF}:\nCoordinate = [" + QString::number(-output_yz_point[0]) + "," + QString::number(-output_yz_point[1]) + "," + QString::number(output_yz_point[2]) + "];\t Distance = " + QString::number(dist_yz) + "\n" +
        " Left_panel   {CSF}:\nCoordinate = [" + QString::number(-output_xz_point[0]) + "," + QString::number(-output_xz_point[1]) + "," + QString::number(output_xz_point[2]) + "];\t Distance = " + QString::number(dist_xz) + "\n\n"
        ;

      output_msg +=
        ":::Image Indeces [Rounded]:::\n\nSeedPoint: [" + QString::number(currentIndex[0]) + "," + QString::number(currentIndex[1]) + "," + QString::number(currentIndex[2]) + "]\n" +
        "Actual Output {RTN}:\nCoordinate = [" + QString::number(output_full_index[0]) + "," + QString::number(output_full_index[1]) + "," + QString::number(output_full_index[2]) + "];\t Distance = " + QString::number(tempOutput["ALL"].second) + "\n" +
        " Center_panel {CSF}:\nCoordinate = [" + QString::number(output_xy_index[0]) + "," + QString::number(output_xy_index[1]) + "," + QString::number(output_xy_index[2]) + "];\t Distance = " + QString::number(tempOutput["XY"].second) + "\n" +
        " Right_panel  {CSF}:\nCoordinate = [" + QString::number(output_yz_index[0]) + "," + QString::number(output_yz_index[1]) + "," + QString::number(output_yz_index[2]) + "];\t Distance = " + QString::number(tempOutput["YZ"].second) + "\n" +
        " Left_panel   {CSF}:\nCoordinate = [" + QString::number(output_xz_index[0]) + "," + QString::number(output_xz_index[1]) + "," + QString::number(output_xz_index[2]) + "];\t Distance = " + QString::number(tempOutput["XZ"].second) + "\n\n\n" +
        volumeString + // this adds the volume-related information to the pop-up box
        "---------------------------------------\n";
    }
  }

  if (!singleIteration)
  {
    ShowErrorMessage(errorMsg);
    return;
  }

  QMessageBox *box = new QMessageBox(QMessageBox::Question, "Directionality Results", volumeString, QMessageBox::Ok | QMessageBox::Cancel);
  box->setAttribute(Qt::WA_DeleteOnClose); //makes sure the msgbox is deleted automatically when closed
  box->setWindowModality(Qt::NonModal);
  QCoreApplication::processEvents();
  if (box->exec() == QMessageBox::Ok)
  {
    std::string saveFileName = outputDir + "/directionalityOutput.txt";

    std::ofstream myFile;
    myFile.open(saveFileName);
    myFile << output_msg.toStdString();
    myFile.close();

    updateProgress(100, "Output saved to '" + saveFileName + "'");

    auto currentROI = convertVtkToItk<ImageTypeFloat3D::PixelType, ImageTypeFloat3D::ImageDimension>(mSlicerManagers[index]->mMask);
    cbica::WriteImage< ImageTypeFloat3D >(currentROI, outputDir + "/roi_DE_visualizationIncrease.nii.gz");
    cbica::WriteImage< ImageTypeFloat3D >(octantImage, outputDir + "/roi_DE_octantImage.nii.gz");
    cbica::WriteImage< ImageTypeFloat3D >(newROIImage, outputDir + "/roi_DE_visualizationRatio.nii.gz");

    minMaxCalc->SetImage(newROIImage);
    minMaxCalc->Compute();

    auto bMin = minMaxCalc->GetMinimum();
    auto bMax = minMaxCalc->GetMaximum();

    // A = (B - Bmin) / (Bmax � Bmin)
    for (roiNew.GoToBegin(); !roiNew.IsAtEnd(); ++roiNew)
    {
      if (roiNew.Get() > 0)
      {
        roiNew.Set((roiNew.Get() - bMin) / (bMax - bMin));
      }
    }

    cbica::WriteImage< ImageTypeFloat3D >(newROIImage, outputDir + "/roiDE_visualizationProbability.nii.gz");

    LoadSlicerImages(outputDir + "/roiDE_visualizationProbability.nii.gz", CAPTK::ImageExtension::NIfTI);
  }
}

void fMainWindow::CallImageHistogramMatching(const std::string referenceImage, const std::string inputImageFile, const std::string outputImageFile)
{
  auto referenceAtlasImage = cbica::ReadImage< ImageTypeFloat3D >(referenceImage);
  auto inputImageImage = cbica::ReadImage< ImageTypeFloat3D >(inputImageFile);

  auto outputImage = cbica::GetHistogramMatchedImage< ImageTypeFloat3D >(inputImageImage, referenceAtlasImage);

  cbica::WriteImage< ImageTypeFloat3D >(outputImage, outputImageFile);

  LoadSlicerImages(outputImageFile, CAPTK::ImageExtension::NIfTI);
}

void fMainWindow::CallImageDeepMedicNormalizer(const std::string inputImage, const std::string maskImage, const std::string outputImageFile,
  const std::string quantLower, const std::string quantUpper,
  const std::string cutoffLower, const std::string cutoffUpper, bool wholeImageMeanThreshold)
{
  auto input = cbica::ReadImage< ImageTypeFloat3D >(inputImage);
  auto mask = cbica::ReadImage< ImageTypeFloat3D >(maskImage);

  auto qLower = std::atof(quantLower.c_str());
  auto qUpper = std::atof(quantUpper.c_str());
  auto cLower = std::atof(cutoffLower.c_str());
  auto cUpper = std::atof(cutoffUpper.c_str());

  updateProgress(5, "Starting Normalization");

  DeepMedicNormalizer< ImageTypeFloat3D > normalizer;
  normalizer.SetInputImage(input);
  normalizer.SetInputMask(mask);
  normalizer.SetQuantiles(qLower, qUpper);
  normalizer.SetCutoffs(cLower, cUpper);
  normalizer.Update();

  updateProgress(100, "Normalization Done");

  cbica::WriteImage< ImageTypeFloat3D >(normalizer.GetOutput(), outputImageFile);

  LoadSlicerImages(outputImageFile, CAPTK::ImageExtension::NIfTI);
}

void fMainWindow::CallDiffusionMeasuresCalculation(const std::string inputImage, const std::string maskImage, const std::string BValFile, const std::string BVecFile, const bool ax, const bool fa, const bool rad, const bool tr, const std::string outputFolder)
{
  DiffusionDerivatives m_diffusionderivatives;
  typedef itk::Image<float, 3> ScalarImageType;
  std::vector<ScalarImageType::Pointer> diffusionDerivatives;

  diffusionDerivatives = m_diffusionderivatives.Run(inputImage, maskImage, BValFile, BVecFile, outputFolder);
  //fa,tr, rad , ax
  if (fa == true)
    cbica::WriteImage< ImageTypeFloat3D >(diffusionDerivatives[0], outputFolder + "/FractionalAnisotropy.nii.gz");
  if (tr == true)
    cbica::WriteImage< ImageTypeFloat3D >(diffusionDerivatives[1], outputFolder + "/ApparentDiffusionCoefficient.nii.gz");
  if (rad == true)
    cbica::WriteImage< ImageTypeFloat3D >(diffusionDerivatives[2], outputFolder + "/RadialDiffusivity.nii.gz");
  if (ax == true)
    cbica::WriteImage< ImageTypeFloat3D >(diffusionDerivatives[3], outputFolder + "/AxialDiffusivity.nii.gz");

  QString msg;
  msg = "Diffusion derivatives have been saved at the specified locations.";
  ShowMessage(msg.toStdString(), this);
}
void fMainWindow::CallPerfusionMeasuresCalculation(const double TE, const bool rcbv, const bool  psr, const bool ph, const std::string inputfilename, std::string outputFolder)
{
  typedef ImageTypeFloat4D PerfusionImageType;

  PerfusionDerivatives m_perfusionderivatives;
  std::vector<typename ImageTypeFloat3D::Pointer> perfusionDerivatives = m_perfusionderivatives.Run<ImageTypeFloat3D, ImageTypeFloat4D>(inputfilename, rcbv, psr, ph, TE);

  if (perfusionDerivatives.size() == 0)
  {
    std::string message;
    message = "Perfusion derivatives were not calculated as expected, please see log file for details: ";
    message = message + loggerFile;
    ShowErrorMessage(message, this);
  }
  else
  {
    if (psr == true)
      cbica::WriteImage< ImageTypeFloat3D >(perfusionDerivatives[0], outputFolder + "/PSR.nii.gz");
    if (ph == true)
      cbica::WriteImage< ImageTypeFloat3D >(perfusionDerivatives[1], outputFolder + "/PH.nii.gz"); 
    if (rcbv == true)
      cbica::WriteImage< ImageTypeFloat3D >(perfusionDerivatives[2], outputFolder + "/ap-RCBV.nii.gz");


    QString msg;
    msg = "Perfusion derivatives have been saved at the specified locations.";
    ShowMessage(msg.toStdString(), this);
  }
}

void fMainWindow::CallTrainingSimulation(const std::string featurefilename, const std::string targetfilename, std::string outputFolder, int classifier, int conf, int folds)
{
  TrainingModule m_trainingsimulator;
  if (m_trainingsimulator.Run(featurefilename, targetfilename, outputFolder, classifier, folds, conf))
  {
    QString msg;
    msg = "Training model has been saved at the specified location.";
    ShowMessage(msg.toStdString(), this);
  }
}

void fMainWindow::CallPCACalculation(const int number, const std::string outputFolder)
{
  typedef ImageTypeFloat4D PerfusionImageType;
  ImageTypeFloat4D::Pointer perfusionImage = ImageTypeFloat4D::New();

  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    if (mSlicerManagers[index]->mImageSubType == CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION)
      perfusionImage = mSlicerManagers[index]->mPerfusionImagePointer;
  }
  ImageTypeFloat3D::Pointer maskImage = convertVtkToItk<float, 3>(mSlicerManagers[0]->mMask);
  PerfusionPCA object_pca;
  QMessageBox *box = new QMessageBox(QMessageBox::Question, "Long Running Application", "This application takes some time to run (<15 minutes).", QMessageBox::Ok | QMessageBox::Cancel);
  box->setAttribute(Qt::WA_DeleteOnClose); //makes sure the msgbox is deleted automatically when closed
  box->setWindowModality(Qt::NonModal);
  QCoreApplication::processEvents();
  if (box->exec() == QMessageBox::Ok)
  {
    std::vector<ImageTypeFloat3D::Pointer> individual_pcs = object_pca.Run<ImageTypeFloat4D, ImageTypeFloat3D>(maskImage, perfusionImage);
    for (int index = 0; index < number; index++)
      cbica::WriteImage< ImageTypeFloat3D >(individual_pcs[index], outputFolder + "/pca_" + std::to_string(index) + ".nii.gz");

    QString message;
    message = "First " + QString::number(number) + " principal components have been saved at the specified locations.";
    ShowMessage(message.toStdString(), this);
  }
}


void fMainWindow::CallWhiteStripe(double twsWidth, int sliceStartZ, int sliceStopZ, int tissuesMax, double smoothMax, double smoothDelta, int histSize,
  bool T1Image, const std::string outputFileName)
{
  QList<QTableWidgetItem*> items = m_imagesTable->selectedItems();
  if (items.empty())
  {
    return;
  }
  int index = GetSlicerIndexFromItem(items[0]);

  WhiteStripe normalizer;
  normalizer.setParams(twsWidth, sliceStartZ, sliceStopZ, tissuesMax, smoothMax, smoothDelta, histSize, T1Image);

  ImageTypeFloat3D::Pointer mask;
  auto normImage = normalizer.process(mSlicerManagers[index]->mITKImage, mask);

  if (normImage.IsNotNull())
  {
    cbica::WriteImage< ImageTypeFloat3D >(normImage, outputFileName);
    LoadSlicerImages(outputFileName, CAPTK::ImageExtension::NIfTI);

    std::vector<float> mids, origHist, smoothHist;
    std::vector<int> peakIds;
    int modeId;
    normalizer.getHisInfo(mids, origHist, smoothHist, peakIds, modeId);

    auto m_hWdg = new  HistWidget(this);
    m_hWdg->setAxis(mids, 2);
    m_hWdg->addColumn(origHist, "Hist", 1, cv::Scalar(0, 255, 255, 255));
    m_hWdg->addColumn(smoothHist, "Smooth", 2);
    float height = *max_element(smoothHist.begin(), smoothHist.end());
    m_hWdg->plotVerticalLine(mids[modeId], height, "Mode");
    m_hWdg->show();
  }
  else
  {
    ShowErrorMessage("WhiteStripe did not run as expected. Please see 'Help' for assistance.");
    help_contextual("Glioblastoma_WhiteStripe.html");
    return;
  }
}

void fMainWindow::CustomPreprocessing()
{

}
void fMainWindow::ChangeBrushSize(int size)
{
  updateDrawMode();
}

void fMainWindow::ChangeMaskOpacity(int newMaskOpacity) // multiLabel uncomment this function
{
  double test = newMaskOpacity * 0.1;
  for (size_t i = 0; i < this->mSlicerManagers.size(); i++)
  {
    for (size_t j = 0; j < 3; j++)
    {
      this->mSlicerManagers[i]->GetSlicer(j)->mMaskOpacity = test;
      this->mSlicerManagers[i]->GetSlicer(j)->mMaskActor->SetOpacity(test);
      this->mSlicerManagers[i]->GetSlicer(j)->mMask->Modified();
    }
  }
  this->mSlicerManagers[0]->Render();
}

void fMainWindow::ChangeDrawingLabel(int drawingLabel) // multiLabel uncomment this function
{
  updateDrawMode();
}

/**
* Read a transform specification, format file,number
*/
TransformSpec read_transform_spec(std::string &file)
{
  std::string spec = file;
  size_t pos = spec.find_first_of(',');

  TransformSpec ts;
  ts.filename = spec.substr(0, pos);
  ts.exponent = 1.0;

  if (!itksys::SystemTools::FileExists(ts.filename.c_str()))
    throw GreedyException("File '%s' does not exist", ts.filename.c_str());

  if (pos != std::string::npos)
  {
    errno = 0; char *pend;
    std::string expstr = spec.substr(pos + 1);
    ts.exponent = std::strtod(expstr.c_str(), &pend);

    if (errno || *pend)
      throw GreedyException("Expected a floating point number after comma in transform specification, instead got '%s'",
        spec.substr(pos).c_str());

  }
  return ts;
}

//Reads radius for registration
std::vector<int> read_int_vector(std::string &nccRadii)
{
  std::string arg = nccRadii;
  std::istringstream f(arg);
  std::string s;
  std::vector<int> vector;
  while (getline(f, s, 'x'))
  {
    errno = 0; char *pend;
    long val = std::strtol(s.c_str(), &pend, 10);
    //std::cout << "Radii: " << val <<std::endl;
    if (errno || *pend)
      throw GreedyException("Expected an integer vector as parameter, instead got '%s'",
        arg.c_str());
    vector.push_back((int)val);
  }

  if (!vector.size())
    throw GreedyException("Expected an integer vector as parameter, instead got '%s'",
      arg.c_str());

  return vector;
}

void fMainWindow::Registration(std::string fixedFileName, std::vector<std::string> inputFileNames, std::vector<std::string> outputFileNames, std::vector<std::string> matrixFileNames, bool registrationMode, std::string metrics, bool affineMode, std::string radii, std::string iterations)
{
  std::string configPathName;
  std::string configFileName;
  std::string extn = ".txt";

  std::vector<std::string> affineMatrix;
  std::vector<std::string> outputImage;

  updateProgress(5, "Starting Registration");

  auto TargetImage = cbica::ReadImage< ImageTypeFloat3D >(fixedFileName);

  if (outputFileNames.size() != inputFileNames.size() || outputFileNames.size() != matrixFileNames.size() || matrixFileNames.size() != inputFileNames.size())
  {
    ShowErrorMessage("Number of input, matrix and output file names do not match");
    return;
  }

  configPathName = itksys::SystemTools::GetFilenamePath(matrixFileNames[0]).c_str();
  configFileName = configPathName + "/" + itksys::SystemTools::GetFilenameWithoutExtension(matrixFileNames[0]).c_str() + extn;

  for (unsigned int i = 0; i < inputFileNames.size(); i++)
  {
    if (!cbica::isFile(inputFileNames[i]))
    {
      ShowErrorMessage("Input file '" + std::to_string(i) + "' is undefined; please check");
      return;
    }
    updateProgress(static_cast<int>(100 / ((i + 1) * inputFileNames.size())), "processing Registration");

    std::string fixedFileCommand = "-f " + fixedFileName;
    std::string movingFileCommand = " -i " + inputFileNames[i];
    std::string affineMatrixCommand = " -t " + matrixFileNames[i];
    std::string outputCommand = " -o " + outputFileNames[i];
    std::string metricsCommand = " -m " + metrics;
    std::string iterationsCommand = " -n " + iterations;
    QStringList args;
    args << "-reg" << "-trf" << "-a" << "-f" << fixedFileName.c_str()
      << "-i" << inputFileNames[i].c_str() << "-t" << matrixFileNames[i].c_str() << "-o" << outputFileNames[i].c_str()
      << "-m" << metrics.c_str() << "-n" << iterations.c_str();

    if (metrics == "NCC")
      args << "-ri" << radii.c_str();
    if (affineMode)
    {
      args << "-a";
    }
    else
    {
      args << "-r";
    }
    std::string fullCommandToRun = getApplicationPath("GreedyRegistration");

    if (startExternalProcess(fullCommandToRun.c_str(), args) != 0)
    {
      ShowErrorMessage("Couldn't register with the default parameters; please use command line functionality");
      return;
    }
    else
    {
      affineMatrix.push_back(matrixFileNames[i] + ".mat");
    }


    if (matrixFileNames[i].find("remove") != std::string::npos) 
    {
      if (cbica::isFile(matrixFileNames[i]))
      {
        if (std::remove(matrixFileNames[i].c_str()) == 0) 
        {
          updateProgress(80, "Cleaning temporary files");
        }
      }

      updateProgress(static_cast<int>(100 / ((i + 1) * inputFileNames.size())), "Writing File");
    }
  
    updateProgress(100, "Registration Complete.");

    time_t t = std::time(0);
    long int now = static_cast<long int> (t);

    std::ofstream file;
    file.open(configFileName.c_str());

    std::string mode;

    if (affineMode == true)
      mode = "Affine";
    else
      mode = "Rigid";

    if (file.is_open())
    {
      if (metrics != "NCC") {
        file << fixedFileName << ","
          << metrics << ","
          << mode << ","
          << iterations << ","
          << now << "\n";
      }
      else {
        file << fixedFileName << ","
          << metrics << ","
          << radii << ","
          << mode << ","
          << iterations << ","
          << now << "\n";
      }
    }
    file.close();
  }
}


void fMainWindow::UpdateAction(std::vector<PointVal> points)
{
  mActionPoints.push_back(points);
}

void fMainWindow::FillLabel(int label)
{
  //auto orientation = mSlicerManagers[0]->mSlicers[0]->GetOrientation();
}

void fMainWindow::UndoFunctionality()
{
  if (mActionPoints.empty())
    return;
  std::vector<PointVal>  OneStrkePoints = mActionPoints.back();
  //Its important to do the undo in reverse order of what happened
  for (std::vector<PointVal>::iterator it = OneStrkePoints.end(); it != OneStrkePoints.begin();)
  {
    --it;
    PointVal pt = *it;
    float* pData = (float*)this->mSlicerManagers[0]->GetSlicer(0)->mMask->GetScalarPointer(pt.x, pt.y, pt.z);
    *pData = pt.value;
  }
  mActionPoints.pop_back();

  this->mSlicerManagers[0]->GetSlicer(0)->mMask->Modified();
  this->mSlicerManagers[0]->Render();
}


void fMainWindow::SetOpacity()
{
  for (unsigned int index = 0; index < mSlicerManagers.size(); index++)
  {
    for (int i = 0; i < 3; i++)
    {
      if (this->mSlicerManagers[index]->GetSlicer(i)->GetMaskOpacity() == 0)
        this->mSlicerManagers[index]->GetSlicer(i)->SetMaskOpacity(1);
      else
        this->mSlicerManagers[index]->GetSlicer(i)->SetMaskOpacity(0);
    }
  }
  this->mSlicerManagers[0]->GetSlicer(0)->mMask->Modified();
  this->mSlicerManagers[0]->Render();
}

void fMainWindow::closeEvent(QCloseEvent* event)
{
  if (!cbica::fileExists(closeConfirmation))
  {
    auto msgBox = new QMessageBox();
    msgBox->setWindowTitle("Close Confirmation!");
    msgBox->setText("Are you certain you would like to exit?");
    msgBox->addButton(QMessageBox::Yes);
    msgBox->addButton(QMessageBox::No);
    msgBox->setDefaultButton(QMessageBox::No);

    QCheckBox closeConfirmationBox("Never ask again");
    closeConfirmationBox.blockSignals(true);
    msgBox->addButton(&closeConfirmationBox, QMessageBox::ResetRole);
    if (msgBox->exec() == QMessageBox::Yes)
    {
      if (closeConfirmationBox.checkState() == Qt::Checked)
      {
        std::ofstream file;
        file.open(closeConfirmation.c_str());
        file << "User doesn't want close confirmation.\n";
        file.close();
      }

      //! close the help dialog forcefully as we are about to exit the application
      bool closed = mHelpDlg->close();

      event->accept();
    }
    else
    {
      event->ignore();
    }
  }
  else
  {
    //! close the help dialog forcefully as we are about to exit the application
    bool closed = mHelpDlg->close();

    event->accept();
  }
}

void fMainWindow::updateProgress(int progress, std::string message, int max)
{
#ifdef USE_PROCESSDIALOG
  m_progressBar->setMaximum(max);
  m_progressBar->setValue(progress);
  m_messageLabel->setText(QString::fromStdString(message));
  QTimer::singleShot(10000.0, m_messageLabel, SLOT(clear()));
  qApp->processEvents();

#endif
}
std::vector<std::map<CAPTK::ImageModalityType, std::string>> fMainWindow::LoadQualifiedSubjectsFromGivenDirectoryForSurvival(const std::string directoryname)
{
  std::map<CAPTK::ImageModalityType, std::string> OneQualifiedSubject;
  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects;
  std::vector<std::string> subjectNames = cbica::subdirectoriesInDirectory(directoryname);
  std::sort(subjectNames.begin(), subjectNames.end());

  for (unsigned int sid = 0; sid < subjectNames.size(); sid++)
  {
    std::string subjectPath = directoryname + "/" + subjectNames[sid];

    std::string t1ceFilePath = "";
    std::string t1FilePath = "";
    std::string t2FilePath = "";
    std::string t2FlairFilePath = "";
    std::string axFilePath = "";
    std::string faFilePath = "";
    std::string radFilePath = "";
    std::string trFilePath = "";
    std::string rcbvFilePath = "";
    std::string psrFilePath = "";
    std::string phFilePath = "";
    std::string labelPath = "";
    std::string atlasPath = "";
    std::string parametersPath = "";
    std::string featureFilePath = "";

    std::vector<std::string> files;

    if (cbica::directoryExists(subjectPath + "/SEGMENTATION"))
    {
      files = cbica::filesInDirectory(subjectPath + "/SEGMENTATION", false);
      if (files.size() == 1)
      {
        labelPath = subjectPath + "/SEGMENTATION" + "/" + files[0];
      }
      else
      {
        for (unsigned int i = 0; i < files.size(); i++)
        {
          std::string filePath = subjectPath + "/SEGMENTATION" + "/" + files[i], filePath_lower;
          std::string extension = cbica::getFilenameExtension(filePath, false);
          filePath_lower = filePath;
          std::transform(filePath_lower.begin(), filePath_lower.end(), filePath_lower.begin(), ::tolower);
          if ((filePath_lower.find("atlas") != std::string::npos || filePath_lower.find("jakob_label") != std::string::npos)
            && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
            atlasPath = subjectPath + "/SEGMENTATION" + "/" + files[i];
          else if ((filePath_lower.find("segmentation") != std::string::npos)
            && (extension == HDR_EXT || extension
              == NII_EXT || extension == NII_GZ_EXT))
            labelPath = subjectPath + "/SEGMENTATION" + "/" + files[i];
          else if ((filePath_lower.find("parameter") != std::string::npos)
            && (extension == PARAM_EXT))
            parametersPath = subjectPath + "/SEGMENTATION" + "/" + files[i];
        }
      }
    }

    if (cbica::directoryExists(subjectPath + "/CONVENTIONAL"))
    {
      files = cbica::filesInDirectory(subjectPath + "/CONVENTIONAL", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/CONVENTIONAL" + "/" + files[i];
        std::string extension = cbica::getFilenameExtension(filePath, false);

        if ((files[i].find("t1ce") != std::string::npos || files[i].find("T1CE") != std::string::npos || files[i].find("T1ce") != std::string::npos || files[i].find("T1-gd") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t1ceFilePath = subjectPath + "/CONVENTIONAL" + "/" + files[i];
        else if ((files[i].find("t1") != std::string::npos || files[i].find("T1") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t1FilePath = subjectPath + "/CONVENTIONAL" + "/" + files[i];
        else if ((files[i].find("t2") != std::string::npos || files[i].find("T2") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t2FilePath = subjectPath + "/CONVENTIONAL" + "/" + files[i];
        else if ((files[i].find("flair") != std::string::npos || files[i].find("FLAIR") != std::string::npos || files[i].find("Flair") != std::string::npos || files[i].find("T2-Flair") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t2FlairFilePath = subjectPath + "/CONVENTIONAL" + "/" + files[i];
      }
    }

    if (cbica::directoryExists(subjectPath + "/PERFUSION"))
    {
      files = cbica::filesInDirectory(subjectPath + "/PERFUSION", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/PERFUSION" + "/" + files[i], filePath_lower;
        std::string extension = cbica::getFilenameExtension(filePath, false);
        filePath_lower = filePath;
        std::transform(filePath_lower.begin(), filePath_lower.end(), filePath_lower.begin(), ::tolower);
        if ((filePath_lower.find("rcbv") != std::string::npos)
          && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          rcbvFilePath = subjectPath + "/PERFUSION" + "/" + files[i];
        else if ((filePath_lower.find("psr") != std::string::npos)
          && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          psrFilePath = subjectPath + "/PERFUSION" + "/" + files[i];
        else if ((filePath_lower.find("ph") != std::string::npos)
          && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          phFilePath = subjectPath + "/PERFUSION" + "/" + files[i];
      }
    }

    if (cbica::directoryExists(subjectPath + "/DTI"))
    {
      files = cbica::filesInDirectory(subjectPath + "/DTI", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/DTI/" + files[i];
        std::string extension = cbica::getFilenameExtension(filePath, false);

        if ((files[i].find("AX") != std::string::npos || files[i].find("axial") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          axFilePath = subjectPath + "/DTI/" + files[i];
        else if ((files[i].find("FA") != std::string::npos || files[i].find("fractional") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          faFilePath = subjectPath + "/DTI/" + files[i];
        else if ((files[i].find("RAD") != std::string::npos || files[i].find("radial") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          radFilePath = subjectPath + "/DTI/" + files[i];
        else if ((files[i].find("TR") != std::string::npos || files[i].find("trace") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          trFilePath = subjectPath + "/DTI/" + files[i];
      }
    }
    if (cbica::fileExists(subjectPath + "/features.csv"))
      featureFilePath = subjectPath + "/features.csv";

    if (labelPath.empty() || t1FilePath.empty() || t2FilePath.empty() || t1ceFilePath.empty() || t2FlairFilePath.empty() || rcbvFilePath.empty() || axFilePath.empty() || faFilePath
      == "" || radFilePath.empty() || trFilePath.empty() || psrFilePath.empty() || phFilePath.empty() || featureFilePath.empty())
      continue;

    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T1] = t1FilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T2] = t2FilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T1CE] = t1ceFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR] = t2FlairFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_AX] = axFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_FA] = faFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_RAD] = radFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_TR] = trFilePath;

    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_PSR] = psrFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_PH] = phFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_RCBV] = rcbvFilePath;

    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_SEG] = labelPath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_ATLAS] = atlasPath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_PARAMS] = parametersPath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_FEATURES] = featureFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_SUDOID] = subjectNames[sid];

    QualifiedSubjects.push_back(OneQualifiedSubject);
  }
  return QualifiedSubjects;
}
std::vector<std::map<CAPTK::ImageModalityType, std::string>>  fMainWindow::LoadQualifiedSubjectsFromGivenDirectoryForRecurrence(const CAPTK::MachineLearningApplicationSubtype type, const std::string &directoryname, const bool &useConventionalData, const bool &useDTIData, const bool &usePerfData, const bool &useDistData)
{
  std::map<CAPTK::ImageModalityType, std::string> OneQualifiedSubject;
  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects;
  std::vector<std::string> subjectNames = cbica::subdirectoriesInDirectory(directoryname);
  std::sort(subjectNames.begin(), subjectNames.end());

  for (unsigned int sid = 0; sid < subjectNames.size(); sid++)
  {
    std::string subjectPath = directoryname + "/" + subjectNames[sid];

    std::string t1ceFilePath = "";
    std::string t1FilePath = "";
    std::string t2FilePath = "";
    std::string t2FlairFilePath = "";
    std::string axFilePath = "";
    std::string faFilePath = "";
    std::string radFilePath = "";
    std::string trFilePath = "";
    std::string perfFilePath = "";
    std::string labelPath = "";
    std::string nearFilePath = "";
    std::string farFilePath = "";

    std::vector<std::string> files;


    if (cbica::directoryExists(subjectPath + "/DRAWING"))
    {
      files = cbica::filesInDirectory(subjectPath + "/DRAWING", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/DRAWING/" + files[i];
        std::string extension = cbica::getFilenameExtension(filePath, false);
        if ((files[i].find("near") != std::string::npos || files[i].find("Infiltrated") != std::string::npos) && (extension == IMG_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          nearFilePath = subjectPath + "/DRAWING/" + files[i];

        if ((files[i].find("far") != std::string::npos || files[i].find("Pure") != std::string::npos) && (extension == IMG_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          farFilePath = subjectPath + "/DRAWING/" + files[i];
      }
    }

    if (cbica::directoryExists(subjectPath + "/SEGMENTATION"))
    {
      files = cbica::filesInDirectory(subjectPath + "/SEGMENTATION", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/SEGMENTATION/" + files[i];
        std::string extension = cbica::getFilenameExtension(filePath, false);
        if ((files[i].find("label-map") != std::string::npos || files[i].find("label") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          labelPath = subjectPath + "/SEGMENTATION/" + files[i];
      }
    }
    if (cbica::directoryExists(subjectPath + "/PERFUSION"))
    {
      files = cbica::filesInDirectory(subjectPath + "/PERFUSION", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/PERFUSION/" + files[i];
        std::string extension = cbica::getFilenameExtension(filePath, false);
        if ((files[i].find("perf") != std::string::npos || files[i].find("Perf") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          perfFilePath = subjectPath + "/PERFUSION/" + files[i];
      }
    }
    if (useConventionalData && cbica::directoryExists(subjectPath + "/CONVENTIONAL"))
    {
      files = cbica::filesInDirectory(subjectPath + "/CONVENTIONAL", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/CONVENTIONAL/" + files[i];
        std::string extension = cbica::getFilenameExtension(filePath, false);

        if ((files[i].find("t1ce") != std::string::npos || files[i].find("T1CE") != std::string::npos || files[i].find("T1ce") != std::string::npos || files[i].find("T1-gd") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t1ceFilePath = subjectPath + "/CONVENTIONAL/" + files[i];
        else if ((files[i].find("t1") != std::string::npos || files[i].find("T1") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t1FilePath = subjectPath + "/CONVENTIONAL/" + files[i];
        else if ((files[i].find("t2") != std::string::npos || files[i].find("T2") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t2FilePath = subjectPath + "/CONVENTIONAL/" + files[i];
        else if ((files[i].find("flair") != std::string::npos || files[i].find("FLAIR") != std::string::npos || files[i].find("Flair") != std::string::npos || files[i].find("T2-Flair") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t2FlairFilePath = subjectPath + "/CONVENTIONAL/" + files[i];
      }
    }


    if (useDTIData && cbica::directoryExists(subjectPath + "/DTI"))
    {
      files = cbica::filesInDirectory(subjectPath + "/DTI", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/DTI/" + files[i];
        std::string extension = cbica::getFilenameExtension(filePath, false);

        if ((files[i].find("Axial") != std::string::npos || files[i].find("axial") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          axFilePath = subjectPath + "/DTI/" + files[i];
        else if ((files[i].find("Fractional") != std::string::npos || files[i].find("fractional") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          faFilePath = subjectPath + "/DTI/" + files[i];
        else if ((files[i].find("Radial") != std::string::npos || files[i].find("radial") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          radFilePath = subjectPath + "/DTI/" + files[i];
        else if ((files[i].find("Trace") != std::string::npos || files[i].find("trace") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          trFilePath = subjectPath + "/DTI/" + files[i];
      }
    }


    if ((useConventionalData && t1FilePath.empty()) || (useConventionalData && t2FilePath.empty()) || (useConventionalData && t1ceFilePath.empty()) || (useConventionalData && t2FlairFilePath.empty()) ||
      (usePerfData && perfFilePath.empty()) || (useDTIData && axFilePath.empty()) || (useDTIData && faFilePath.empty()) || (useDTIData && radFilePath.empty()) || (useDTIData && trFilePath.empty()))
      continue;

    if ((nearFilePath.empty() || farFilePath.empty()) && type == CAPTK::MachineLearningApplicationSubtype::TRAINING)
      continue;

    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T1] = t1FilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T2] = t2FilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T1CE] = t1ceFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR] = t2FlairFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_AX] = axFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_FA] = faFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_RAD] = radFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_TR] = trFilePath;

    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_SEG] = labelPath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_SUDOID] = subjectNames[sid];
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION] = perfFilePath;

    if (type == CAPTK::MachineLearningApplicationSubtype::TRAINING)
    {
      OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_NEAR] = nearFilePath;
      OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_FAR] = farFilePath;
    }
    QualifiedSubjects.push_back(OneQualifiedSubject);

  }
  return QualifiedSubjects;
}


std::vector<std::map<CAPTK::ImageModalityType, std::string>>  fMainWindow::LoadQualifiedSubjectsFromGivenDirectoryForPseudoProgression(const CAPTK::MachineLearningApplicationSubtype type, const std::string &directoryname, const bool &useConventionalData, const bool &useDTIData, const bool &usePerfData, const bool &useDistData)
{
  std::map<CAPTK::ImageModalityType, std::string> OneQualifiedSubject;
  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects;
  std::vector<std::string> subjectNames= cbica::subdirectoriesInDirectory(directoryname);

  //subjectNames.clear();
  //subjectNames.push_back("AAMA");
  //subjectNames.push_back("AAMG");
  //subjectNames.push_back("AAMJ");
  //subjectNames.push_back("AAMP");
  //subjectNames.push_back("AAMQ");
  //subjectNames.push_back("ABEM");

  std::sort(subjectNames.begin(), subjectNames.end());

  for (unsigned int sid = 0; sid < subjectNames.size(); sid++)
  {
    std::string subjectPath = directoryname + "/" + subjectNames[sid];

    std::string t1ceFilePath = "";
    std::string t1FilePath = "";
    std::string t2FilePath = "";
    std::string t2FlairFilePath = "";

    //std::string t1t1ceFilePath    = "";
    //std::string t2t2FlairFilePath = "";

    std::string axFilePath = "";
    std::string faFilePath = "";
    std::string radFilePath = "";
    std::string trFilePath = "";
    std::string phFilePath = "";
    std::string psrFilePath = "";
    std::string rcbvFilePath = "";
    std::string perfFilePath = "";
    std::string featuresFilePath = "";

    std::string labelPath = "";
    std::string atlasPath = "";

    std::vector<std::string> files;


    if (cbica::directoryExists(subjectPath + "/SEGMENTATION"))
    {
      files = cbica::filesInDirectory(subjectPath + "/SEGMENTATION", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/SEGMENTATION/" + files[i];
        std::string extension = cbica::getFilenameExtension(filePath, false);
        if ((files[i].find("label-map") != std::string::npos || files[i].find("label") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          labelPath = subjectPath + "/SEGMENTATION/" + files[i];
        else if ((files[i].find("atlas") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          atlasPath = subjectPath + "/SEGMENTATION/" + files[i];
      }
    }
    if (cbica::directoryExists(subjectPath + "/PERFUSION"))
    {
      files = cbica::filesInDirectory(subjectPath + "/PERFUSION", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/PERFUSION" + "/" + files[i], filePath_lower;
        std::string extension = cbica::getFilenameExtension(filePath, false);
        filePath_lower = filePath;
        std::transform(filePath_lower.begin(), filePath_lower.end(), filePath_lower.begin(), ::tolower);
        if ((filePath_lower.find("rcbv") != std::string::npos)
          && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          rcbvFilePath = subjectPath + "/PERFUSION" + "/" + files[i];
        else if ((filePath_lower.find("psr") != std::string::npos)
          && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          psrFilePath = subjectPath + "/PERFUSION" + "/" + files[i];
        else if ((filePath_lower.find("ph") != std::string::npos)
          && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          phFilePath = subjectPath + "/PERFUSION" + "/" + files[i];
        else if ((filePath_lower.find("perf") != std::string::npos)
          && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          perfFilePath = subjectPath + "/PERFUSION" + "/" + files[i];
      }
    }
    if (useConventionalData && cbica::directoryExists(subjectPath + "/CONVENTIONAL"))
    {
      files = cbica::filesInDirectory(subjectPath + "/CONVENTIONAL", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/CONVENTIONAL/" + files[i];
        std::string extension = cbica::getFilenameExtension(filePath, false);

        //if ((files[i].find("t1t1ce") != std::string::npos || files[i].find("T1T1CE") != std::string::npos || files[i].find("T1T1ce") != std::string::npos || files[i].find("T1-T1-gd") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
        //  t1t1ceFilePath = subjectPath + "/CONVENTIONAL/" + files[i];
        //else if ((files[i].find("t2t2flair") != std::string::npos || files[i].find("T2T2FLAIR") != std::string::npos || files[i].find("T2T2Flair") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
        //  t2t2FlairFilePath = subjectPath + "/CONVENTIONAL/" + files[i];
        //else
        if ((files[i].find("t1ce") != std::string::npos || files[i].find("T1CE") != std::string::npos || files[i].find("T1ce") != std::string::npos || files[i].find("T1-gd") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t1ceFilePath = subjectPath + "/CONVENTIONAL/" + files[i];
        else if ((files[i].find("t1") != std::string::npos || files[i].find("T1") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t1FilePath = subjectPath + "/CONVENTIONAL/" + files[i];
        else if ((files[i].find("t2") != std::string::npos || files[i].find("T2") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t2FilePath = subjectPath + "/CONVENTIONAL/" + files[i];
        else if ((files[i].find("flair") != std::string::npos || files[i].find("FLAIR") != std::string::npos || files[i].find("Flair") != std::string::npos || files[i].find("T2-Flair") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          t2FlairFilePath = subjectPath + "/CONVENTIONAL/" + files[i];
      }
    }


    if (useDTIData && cbica::directoryExists(subjectPath + "/DTI"))
    {
      files = cbica::filesInDirectory(subjectPath + "/DTI", false);
      for (unsigned int i = 0; i < files.size(); i++)
      {
        std::string filePath = subjectPath + "/DTI/" + files[i];
        std::string extension = cbica::getFilenameExtension(filePath, false);

        if ((files[i].find("Axial") != std::string::npos || files[i].find("axial") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          axFilePath = subjectPath + "/DTI/" + files[i];
        else if ((files[i].find("Fractional") != std::string::npos || files[i].find("fractional") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          faFilePath = subjectPath + "/DTI/" + files[i];
        else if ((files[i].find("Radial") != std::string::npos || files[i].find("radial") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          radFilePath = subjectPath + "/DTI/" + files[i];
        else if ((files[i].find("Trace") != std::string::npos || files[i].find("trace") != std::string::npos) && (extension == HDR_EXT || extension == NII_EXT || extension == NII_GZ_EXT))
          trFilePath = subjectPath + "/DTI/" + files[i];
      }
    }
    if (cbica::fileExists(subjectPath + "/features.csv"))
      featuresFilePath = subjectPath + "/features.csv";

    if (labelPath.empty() || t1FilePath.empty() || t2FilePath.empty() || t1ceFilePath.empty() || t2FlairFilePath.empty() || rcbvFilePath.empty() || axFilePath.empty() || faFilePath.empty() || radFilePath.empty() || trFilePath.empty() || psrFilePath.empty() || phFilePath.empty())
      continue;

    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T1] = t1FilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T2] = t2FilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T1CE] = t1ceFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_T2FLAIR] = t2FlairFilePath;
    //OneQualifiedSubject[IMAGE_TYPE_T1T1CE]    = t1t1ceFilePath; 
    //OneQualifiedSubject[IMAGE_TYPE_T2FL]      = t2t2FlairFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_AX] = axFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_FA] = faFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_RAD] = radFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_TR] = trFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_PH] = phFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_PSR] = psrFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_RCBV] = rcbvFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_PERFUSION] = perfFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_FEATURES] = featuresFilePath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_SEG] = labelPath;
    OneQualifiedSubject[CAPTK::ImageModalityType::IMAGE_TYPE_SUDOID] = subjectNames[sid];
    QualifiedSubjects.push_back(OneQualifiedSubject);

    std::cout << subjectNames[sid] << std::endl;
  }
  return QualifiedSubjects;
}

void fMainWindow::CallForMolecularSubtypePredictionOnExistingModelFromMain(const std::string modeldirectory, const std::string inputdirectory, const std::string outputdirectory)
{
  if (modeldirectory == "")
  {
    ShowErrorMessage("Please provide path of a directory having SVM model");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }
  if (!cbica::isDir(modeldirectory))
  {
    ShowErrorMessage("The given SVM model directory does not exist");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }
  if (!cbica::fileExists(modeldirectory + "/ProneuralModelFile.xml") || !cbica::fileExists(modeldirectory + "/NeuralModelFile.xml") ||
    !cbica::fileExists(modeldirectory + "/MessenchymalModelFile.xml") || !cbica::fileExists(modeldirectory + "/ClassicalModelFile.xml") ||
    !cbica::fileExists(modeldirectory + "/Molecular_ZScore_Std.csv") || !cbica::fileExists(modeldirectory + "/Molecular_ZScore_Mean.csv"))
  {
    ShowErrorMessage("The given SVM model directory does not have all the model files");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }

  if (inputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having input images");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }
  if (!cbica::isDir(inputdirectory))
  {
    ShowErrorMessage("The given input directory does not exist");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }


  if (outputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory to save output");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }
  if (!cbica::isDir(outputdirectory))
  {
    if (!cbica::createDir(outputdirectory))
    {
      ShowErrorMessage("Unable to create the output directory");
      help_contextual("Glioblastoma_MolecularSubtype.html");
      return;
    }
  }

  std::vector<double> finalresult;
  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects = LoadQualifiedSubjectsFromGivenDirectoryForSurvival(inputdirectory);
  if (QualifiedSubjects.size() == 0)
  {
    ShowErrorMessage("No patient inside the given input directory has required scans");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }

  VectorDouble result = mMolecularSubtype.MolecularSubtypePredictionOnExistingModel(modeldirectory, inputdirectory, QualifiedSubjects, outputdirectory);
  QString msg;
  if (result.size() == 0)
  {
    msg = "Molecular subtype model did not finish as expected, please see log file for details: ";
    msg = msg + QString::fromStdString(loggerFile);
  }
  else
  {
    msg = "A Molecular Subtype has been calculated for the given subjects by applying the specified model. \n\n";
    if (result[0] == 1)
      msg = msg + "Molecular Subtype = Proneural \n\n";
    else if (result[0] == 2)
      msg = msg + "Molecular Subtype = Neural \n\n";
    else if (result[0] == 3)
      msg = msg + "Molecular Subtype = Mesenchymal \n\n";
    else if (result[0] == 4)
      msg = msg + "Molecular Subtype = Classical \n\n";
    else
      msg = msg + "Molecular Subtype = Unknown \n\n";
    msg = msg + "Molecular subtype saved in 'results.csv' file in the output directory. \n\n";
    msg = msg + "Input Directory = " + QString::fromStdString(inputdirectory) + "\nOutput Directory = " + QString::fromStdString(outputdirectory) + "\nModel Directory = " + QString::fromStdString(modeldirectory);
  }
  ShowMessage(msg.toStdString(), this);
}

void fMainWindow::CallForNewMolecularSubtypePredictionModelFromMain(const std::string inputdirectory, const std::string outputdirectory)
{
  std::vector<double> finalresult;


  if (inputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory having input images");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }
  if (!cbica::isDir(inputdirectory))
  {
    ShowErrorMessage("The given input directory does not exist");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }


  if (outputdirectory.empty())
  {
    ShowErrorMessage("Please provide path of a directory to save output");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }
  if (!cbica::isDir(outputdirectory))
  {
    if (!cbica::createDir(outputdirectory))
    {
      ShowErrorMessage("Unable to create the output directory");
      help_contextual("Glioblastoma_MolecularSubtype.html");
      return;
    }
  }

  std::vector<std::map<CAPTK::ImageModalityType, std::string>> QualifiedSubjects = LoadQualifiedSubjectsFromGivenDirectoryForSurvival(inputdirectory);
  if (QualifiedSubjects.size() == 0)
  {
    ShowErrorMessage("No patient inside the given input directory has required scans");
    help_contextual("Glioblastoma_MolecularSubtype.html");
    return;
  }


  if (mMolecularSubtype.PrepareNewMolecularPredictionModel(inputdirectory, QualifiedSubjects, outputdirectory) == false)
  {
    std::string message;
    message = "Molecular Subtype Training did not finish as expected, please see log file for details: ";
    message = message + loggerFile;
    ShowErrorMessage(message);
  }
  else
  {
    ShowMessage("A Molecular Subtype Prediction model has been prepared and saved. \n\nInput Directory = " + inputdirectory + "\nOutput Directory = " + outputdirectory, this);
  }
}

bool fMainWindow::isMaskDefined()
{
  if (!mSlicerManagers.empty())
  {
    auto minMaxComputer = itk::MinimumMaximumImageCalculator<ImageTypeFloat3D>::New();
    minMaxComputer->SetImage(convertVtkToItk<float, 3>(mSlicerManagers[0]->mMask));
    minMaxComputer->Compute();
    if (minMaxComputer->GetMaximum() > 0)
    {
      return true;
    }
  }

  return false;
}

void fMainWindow::help_contextual(const std::string startPage)
{
  mHelpDlg->setNewStartPage(startPage);
  mHelpDlg->show();
}

std::vector< fMainWindow::ActionAndName >fMainWindow::populateStringListInMenu(const std::string &inputList, QMainWindow* inputFMainWindow, QMenu* menuToPopulate, std::string menuAppSubGroup, bool ExcludeGeodesic)
{
  if (!inputList.empty())
  {
    std::string inputList_wrap = inputList;
    if (inputList_wrap[0] == ' ')
    {
      inputList_wrap.erase(0, 1);
    }
    std::vector< std::string > vectorOfInputs = cbica::stringSplit(inputList_wrap, " ");
    return populateStringListInMenu(vectorOfInputs, inputFMainWindow, menuToPopulate, menuAppSubGroup, ExcludeGeodesic);
  }
  else
  {
    return std::vector< fMainWindow::ActionAndName >{};
  }
}

std::vector< fMainWindow::ActionAndName >fMainWindow::populateStringListInMenu(const std::vector< std::string > &vectorOfInputs, QMainWindow* inputFMainWindow, QMenu* menuToPopulate, std::string menuAppSubGroup, bool ExcludeGeodesic)
{
  std::vector< fMainWindow::ActionAndName > returnVector;
  if (ExcludeGeodesic)
  {
    returnVector.resize(vectorOfInputs.size());
  }
  else
  {
    returnVector.resize(vectorOfInputs.size() + 1);
  }
  size_t returnVecCounter = 0;

  if (!menuAppSubGroup.empty())
  {
    returnVector[returnVecCounter].action = new QAction(inputFMainWindow);
    returnVector[returnVecCounter].action->setObjectName(QString::fromUtf8(std::string("action" + menuAppSubGroup).c_str()));
    returnVector[returnVecCounter].action->setIconText(QString(menuAppSubGroup.c_str()));
    returnVector[returnVecCounter].action->setText(QString(menuAppSubGroup.c_str()));
    returnVector[returnVecCounter].action->setEnabled(false);
    returnVector[returnVecCounter].name = menuAppSubGroup;
    returnVector[returnVecCounter].action->setEnabled(false);
    menuToPopulate->addAction(returnVector[returnVecCounter].action);
    returnVecCounter++;
  }

  for (size_t i = 0; i < vectorOfInputs.size(); i++)
  {
    if ((vectorOfInputs[i] != "FeatureExtraction"))
    {
      returnVector[returnVecCounter].action = new QAction(inputFMainWindow);
      returnVector[returnVecCounter].action->setObjectName(QString::fromUtf8(std::string("action" + vectorOfInputs[i]).c_str()));
      returnVector[returnVecCounter].action->setIconText(QString(vectorOfInputs[i].c_str()));
      returnVector[returnVecCounter].name = vectorOfInputs[i];
#ifdef CAPTK_BUILD_CONSOLE_ONLY
      returnVector[returnVecCounter].action->setEnabled(false);
#endif
      menuToPopulate->addAction(returnVector[returnVecCounter].action);
      returnVecCounter++;
    }
  }
  //}

  return returnVector;
}