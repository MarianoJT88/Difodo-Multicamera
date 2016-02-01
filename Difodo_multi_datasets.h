/* +---------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)               |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2015, Individual contributors, see AUTHORS file        |
   | See: http://www.mrpt.org/Authors - All rights reserved.                   |
   | Released under BSD License. See details in http://www.mrpt.org/License    |
   +---------------------------------------------------------------------------+ */

#include "CDifodo_multi.h"
#include <mrpt/utils/types_math.h> // Eigen (with MRPT "plugin" in BaseMatrix<>)
#include <mrpt/utils/CConfigFileBase.h>
#include <mrpt/utils/CImage.h>
#include <mrpt/obs/CRawlog.h>
#include <mrpt/obs/CObservation3DRangeScan.h>
#include <mrpt/opengl/COpenGLScene.h>
#include <mrpt/gui.h>
#include <iostream>

class CDifodoDatasets : public CDifodo {
public:

	mrpt::opengl::COpenGLScenePtr scene;	//!< Opengl scene
	mrpt::gui::CDisplayWindow3D	window;
	mrpt::obs::CRawlog	dataset;
	std::ifstream		f_gt;
	std::ofstream		f_res;

	unsigned int repr_level;
	unsigned int rawlog_count;
	bool first_pose;
	bool save_results;
	bool dataset_finished;

	/** Constructor. */
	CDifodoDatasets() : CDifodo()
	{
		save_results = 0;
		first_pose = false;
		dataset_finished = false;
	}

	/** Initialize the visual odometry method and loads the rawlog file */
	void loadConfiguration( const mrpt::utils::CConfigFileBase &ini );

	/** Load the depth image and the corresponding groundtruth pose */
	void loadFrame();

	/** Create a file to save the trajectory estimates */
	void CreateResultsFile();

	/** Initialize the opengl scene */
	void initializeScene();

	/** Update the opengl scene */
	void updateScene();

	/** A pre-step that should be performed before starting to estimate the camera speed
	  * As a couple of frames are necessary to estimate the camera motion, this methods loads the first frame
	  * before any motion can be estimated.*/
	void reset();

	/** Save the pose estimation following the format of the TUM datasets:
	  * 
	  * 'timestamp tx ty tz qx qy qz qw'
	  *
	  * Please visit http://vision.in.tum.de/data/datasets/rgbd-dataset/file_formats for further details.*/  
	void writeTrajectoryFile();
};
