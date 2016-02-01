/* +---------------------------------------------------------------------------+
   |                     Mobile Robot Programming Toolkit (MRPT)               |
   |                          http://www.mrpt.org/                             |
   |                                                                           |
   | Copyright (c) 2005-2016, Individual contributors, see AUTHORS file        |
   | See: http://www.mrpt.org/Authors - All rights reserved.                   |
   | Released under BSD License. See details in http://www.mrpt.org/License    |
   +---------------------------------------------------------------------------+ */

#include "Difodo_multi_datasets.h"
#include <mrpt/system/filesystem.h>
#include <mrpt/opengl/CFrustum.h>
#include <mrpt/opengl/CGridPlaneXY.h>
#include <mrpt/opengl/CBox.h>
#include <mrpt/opengl/CPointCloudColoured.h>
#include <mrpt/opengl/CPointCloud.h>
#include <mrpt/opengl/CSetOfLines.h>
#include <mrpt/opengl/CEllipsoid.h>
#include <mrpt/opengl/stock_objects.h>
#include "legend.xpm"

using namespace Eigen;
using namespace std;
using namespace mrpt;
using namespace mrpt::opengl;

using namespace mrpt::gui;
using namespace mrpt::obs;
using namespace mrpt::maps;
using namespace mrpt::math;
using namespace mrpt::utils;
using namespace mrpt::poses;



void CDifodoDatasets::loadConfiguration(const utils::CConfigFileBase &ini )
{	
	fovh = M_PI*62.5/180.0;	//Larger FOV because depth is registered with color
	fovv = M_PI*48.5/180.0;
    cam_mode = ini.read_int("DIFODO_CONFIG", "cam_mode", 2, true); // The resolution of the images within the rawlog (1:640x480, 2:320x240)
	fast_pyramid = false;
    downsample = ini.read_int("DIFODO_CONFIG", "downsample", 1, true);
	rows = ini.read_int("DIFODO_CONFIG", "rows", 240, true);
	cols = ini.read_int("DIFODO_CONFIG", "cols", 320, true);
	ctf_levels = ini.read_int("DIFODO_CONFIG", "ctf_levels", 5, true);
	string filename = ini.read_string("DIFODO_CONFIG", "filename", "no file", true);
    fast_pyramid = true;

    //				Load cameras' extrinsic calibrations
    //==================================================================

    int cams_order[4] = {1,4,3,2};

    for ( int c = 1; c <= NC; c++ )
    {
        string sensorLabel = mrpt::format("RGBD_%i",cams_order[c-1]);

        double x       = ini.read_double(sensorLabel,"x",0,true);
        double y       = ini.read_double(sensorLabel,"y",0,true);
        double z       = ini.read_double(sensorLabel,"z",0,true);
        double yaw     = DEG2RAD(ini.read_double(sensorLabel,"yaw",0,true));
        double pitch   = DEG2RAD(ini.read_double(sensorLabel,"pitch",0,true));
        double roll    = DEG2RAD(ini.read_double(sensorLabel,"roll",0,true));

        cam_pose[c-1].setFromValues(x,y,z,yaw,pitch,roll);
        CMatrixDouble44 homoMatrix;
        cam_pose[c-1].getHomogeneousMatrix(homoMatrix);
        calib_mat[c-1] = (CMatrixFloat44)homoMatrix;
    }

	//						Open Rawlog File
	//==================================================================
	if (!dataset.loadFromRawLogFile(filename))
		throw std::runtime_error("\nCouldn't open rawlog dataset file for input...");

	rawlog_count = 0;

	// Set external images directory:
	const string imgsPath = CRawlog::detectImagesDirectory(filename);
	CImage::IMAGES_PATH_BASE = imgsPath;

	//			Resize matrices and adjust parameters
	//=========================================================
	width = 640/(cam_mode*downsample);
	height = 480/(cam_mode*downsample);
	repr_level = utils::round(log(float(width/cols))/log(2.f));

	//Resize pyramid
    const unsigned int pyr_levels = round(log(float(width/cols))/log(2.f)) + ctf_levels;

	for (unsigned int c=0; c<NC; c++)
	{
		for (unsigned int i = 0; i<pyr_levels; i++)
		{
			unsigned int s = pow(2.f,int(i));
			cols_i = width/s; rows_i = height/s;
			depth[c][i].resize(rows_i, cols_i);
			depth_inter[c][i].resize(rows_i, cols_i);
			depth_old[c][i].resize(rows_i, cols_i);
			depth[c][i].assign(0.0f);
			depth_old[c][i].assign(0.0f);
			xx[c][i].resize(rows_i, cols_i);
			xx_inter[c][i].resize(rows_i, cols_i);
			xx_old[c][i].resize(rows_i, cols_i);
			xx[c][i].assign(0.0f);
			xx_old[c][i].assign(0.0f);
			yy[c][i].resize(rows_i, cols_i);
			yy_inter[c][i].resize(rows_i, cols_i);
			yy_old[c][i].resize(rows_i, cols_i);
			yy[c][i].assign(0.0f);
			yy_old[c][i].assign(0.0f);
			zz_global[c][i].resize(rows_i, cols_i);
			xx_global[c][i].resize(rows_i, cols_i);
			yy_global[c][i].resize(rows_i, cols_i);

			if (cols_i <= cols)
			{
				depth_warped[c][i].resize(rows_i,cols_i);
				xx_warped[c][i].resize(rows_i,cols_i);
				yy_warped[c][i].resize(rows_i,cols_i);
			}
		}

		//Resize matrix that store the original depth image
		depth_wf[c].setSize(height,width);
	}

	//Resize the transformation matrices
	for (unsigned int l = 0; l<pyr_levels; l++)
		global_trans[l].resize(4,4);

	for (unsigned int c=0; c<NC; c++)	
		for (unsigned int l = 0; l<pyr_levels; l++)
			transformations[c][l].resize(4,4);
}

void CDifodoDatasets::CreateResultsFile()
{
	try
	{
		// Open file, find the first free file-name.
		char	aux[100];
		int     nFile = 0;
		bool    free_name = false;

		system::createDirectory("./difodo.results");

		while (!free_name)
		{
			nFile++;
			sprintf(aux, "./difodo.results/experiment_%03u.txt", nFile );
			free_name = !system::fileExists(aux);
		}

		// Open log file:
		f_res.open(aux);
		printf(" Saving results to file: %s \n", aux);
	}
	catch (...)
	{
		printf("Exception found trying to create the 'results file' !!\n");
	}

}

void CDifodoDatasets::initializeScene()
{
	CPose3D rel_lenspose(0,-0.022,0,0,0,0);
	
	global_settings::OCTREE_RENDER_MAX_POINTS_PER_NODE = 1000000;
	window.resize(1000,900);
	window.setPos(900,0);
	window.setCameraZoom(16);
	window.setCameraAzimuthDeg(0);
	window.setCameraElevationDeg(90);
	window.setCameraPointingToPoint(0,0,0);
	window.setCameraPointingToPoint(0,0,1);

	scene = window.get3DSceneAndLock();

	// Lights:
	scene->getViewport()->setNumberOfLights(1);
	CLight & light0 = scene->getViewport()->getLight(0);
	light0.light_ID = 0;
	light0.setPosition(0,0,1,1);

	//Grid (ground)
	CGridPlaneXYPtr ground = CGridPlaneXY::Create();
	scene->insert( ground );

	//Reference
	//CSetOfObjectsPtr reference = stock_objects::CornerXYZ();
	//scene->insert( reference );

	//					Cameras and points
	//------------------------------------------------------

    //Cameras
    for (unsigned int c=0; c<NC; c++)
    {
        CBoxPtr camera_odo = CBox::Create(math::TPoint3D(-0.02,-0.1,-0.01),math::TPoint3D(0.02,0.1,0.01));
        camera_odo->setPose(cam_pose[c] + rel_lenspose);
        camera_odo->setColor(0,1,0);
        scene->insert( camera_odo );

        //Frustum
        opengl::CFrustumPtr FOV = opengl::CFrustum::Create(0.3, 2, 57.3*fovh, 57.3*fovv, 1.f, true, false);
        FOV->setColor(0.7,0.7,0.7);
        FOV->setPose(cam_pose[c]);
        scene->insert( FOV );

        //Camera points
        CPointCloudColouredPtr cam_points = CPointCloudColoured::Create();
        cam_points->setColor(1,0,0);
        cam_points->setPointSize(2);
        cam_points->enablePointSmooth(1);
        cam_points->setPose(cam_pose[c]);
        scene->insert( cam_points );
    }


	//					Trajectories and covariance
	//-------------------------------------------------------------

	//Dif Odometry
	CSetOfLinesPtr traj_lines_odo = CSetOfLines::Create();
	traj_lines_odo->setLocation(0,0,0);
	traj_lines_odo->setColor(0,0.6,0);
	traj_lines_odo->setLineWidth(6);
	scene->insert( traj_lines_odo );
	CPointCloudPtr traj_points_odo = CPointCloud::Create();
	traj_points_odo->setColor(0,0.6,0);
	traj_points_odo->setPointSize(4);
	traj_points_odo->enablePointSmooth(1);
	scene->insert( traj_points_odo );

	//Ellipsoid showing covariance
	math::CMatrixFloat33 cov3d = 20.f*est_cov.topLeftCorner(3,3);
	CEllipsoidPtr ellip = CEllipsoid::Create();
	ellip->setCovMatrix(cov3d);
	ellip->setQuantiles(2.0);
	ellip->setColor(1.0, 1.0, 1.0, 0.5);
	ellip->enableDrawSolid3D(true);
    ellip->setPose(global_pose);
	scene->insert( ellip );

	//User-interface information
	utils::CImage img_legend;
	img_legend.loadFromXPM(legend_xpm);
	COpenGLViewportPtr legend = scene->createViewport("legend");
	legend->setViewportPosition(20, 20, 332, 164);
	legend->setImageView(img_legend);

	window.unlockAccess3DScene();
	window.repaint();
}

void CDifodoDatasets::updateScene()
{
	CPose3D rel_lenspose(0,-0.022,0,0,0,0);
	
    printf("\n rows, cols = %d, %d", rows, cols);

	scene = window.get3DSceneAndLock();

	//Reference gt
//	CSetOfObjectsPtr reference_gt = scene->getByClass<CSetOfObjects>(0);
//	reference_gt->setPose(gt_pose);

	//Camera points
    for (unsigned int c=0; c<NC; c++)
    {
        CPointCloudColouredPtr cam_points = scene->getByClass<CPointCloudColoured>(c);
        cam_points->clear();
        cam_points->setPose(global_pose + cam_pose[c]);

        for (unsigned int y=0; y<cols; y++)
            for (unsigned int z=0; z<rows; z++)
                cam_points->push_back(depth[c][repr_level](z,y), xx[c][repr_level](z,y), yy[c][repr_level](z,y),
//                                      0.f, 0.f, 1.f);
                                        1.f-sqrt(weights[c](z,y)), sqrt(weights[c](z,y)), 0);

        //DifOdo camera
        CBoxPtr camera_odo = scene->getByClass<CBox>(c);
        camera_odo->setPose(global_pose + cam_pose[c] + rel_lenspose);

        //Frustum
        CFrustumPtr FOV = scene->getByClass<CFrustum>(c);
        FOV->setPose(global_pose + cam_pose[c]);
    }

    if ((first_pose == true))
    {
        //Difodo traj lines
        CSetOfLinesPtr traj_lines_odo = scene->getByClass<CSetOfLines>(0);
        traj_lines_odo->appendLine(global_oldpose.x(), global_oldpose.y(), global_oldpose.z(), global_pose.x(), global_pose.y(), global_pose.z());

        //Difodo traj points
        CPointCloudPtr traj_points_odo = scene->getByClass<CPointCloud>(0);
        traj_points_odo->insertPoint(global_pose.x(), global_pose.y(), global_pose.z());
    }

    //Ellipsoid showing covariance
    math::CMatrixFloat33 cov3d = 20.f*est_cov.topLeftCorner(3,3);
    CEllipsoidPtr ellip = scene->getByClass<CEllipsoid>(0);
    ellip->setCovMatrix(cov3d);
    ellip->setPose(global_pose + rel_lenspose);

	window.unlockAccess3DScene();
	window.repaint();
}

void CDifodoDatasets::loadFrame()
{
    for (unsigned int c=0; c<NC; c++)
    {
        CObservationPtr alfa = dataset.getAsObservation(rawlog_count);

        while (!IS_CLASS(alfa, CObservation3DRangeScan))
        {
            rawlog_count++;
            if (dataset.size() <= rawlog_count)
            {
                dataset_finished = true;
                return;
            }
            alfa = dataset.getAsObservation(rawlog_count);
        }

        CObservation3DRangeScanPtr obs3D = CObservation3DRangeScanPtr(alfa);
        obs3D->load();
        const CMatrix range = obs3D->rangeImage;
        const unsigned int height = range.getRowCount();
        const unsigned int width = range.getColCount();

        for (unsigned int j = 0; j<cols; j++)
            for (unsigned int i = 0; i<rows; i++)
            {
                const float z = range(height-downsample*i-1, width-downsample*j-1);
                if (z < 4.5f)	depth_wf[c](i,j) = z;
                else			depth_wf[c](i,j) = 0.f;
            }

        obs3D->unload();
        rawlog_count++;

        if (dataset.size() <= rawlog_count)
            dataset_finished = true;
    }
}

void CDifodoDatasets::reset()
{
	loadFrame();
	if (fast_pyramid)	buildCoordinatesPyramidFast();
	else				buildCoordinatesPyramid();

    global_oldpose = global_pose;
}

void CDifodoDatasets::writeTrajectoryFile()
{	
	////Don't take into account those iterations with consecutive equal depth images
	//if (abs(dt.sumAll()) > 0)
	//{		
	//	mrpt::math::CQuaternionDouble quat;
	//	CPose3D auxpose, transf;
	//	transf.setFromValues(0,0,0,0.5*M_PI, -0.5*M_PI, 0);

	//	auxpose = cam_pose - transf;
	//	auxpose.getAsQuaternion(quat);
	//
	//	char aux[24];
	//	sprintf(aux,"%.04f", timestamp_obs);
	//	f_res << aux << " ";
	//	f_res << cam_pose[0] << " ";
	//	f_res << cam_pose[1] << " ";
	//	f_res << cam_pose[2] << " ";
	//	f_res << quat(2) << " ";
	//	f_res << quat(3) << " ";
	//	f_res << -quat(1) << " ";
	//	f_res << -quat(0) << endl;
	//}
}


