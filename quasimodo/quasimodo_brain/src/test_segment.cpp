#include "ros/ros.h"
#include "std_msgs/String.h"
#include <sensor_msgs/PointCloud2.h>
#include <string.h>

#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>

#include "eigen_conversions/eigen_msg.h"
#include "tf_conversions/tf_eigen.h"

#include "metaroom_xml_parser/simple_xml_parser.h"
#include "metaroom_xml_parser/simple_summary_parser.h"

#include <tf_conversions/tf_eigen.h>

#include "ros/ros.h"
#include <metaroom_xml_parser/load_utilities.h>
#include <pcl_ros/point_cloud.h>
#include <cv_bridge/cv_bridge.h>

#include "metaroom_xml_parser/load_utilities.h"
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl_ros/transforms.h>

#include <tf_conversions/tf_eigen.h>

#include "quasimodo_msgs/model.h"
#include "quasimodo_msgs/rgbd_frame.h"
#include "quasimodo_msgs/model_from_frame.h"
#include "quasimodo_msgs/index_frame.h"
#include "quasimodo_msgs/fuse_models.h"
#include "quasimodo_msgs/get_model.h"
#include "quasimodo_msgs/segment_model.h"

#include "ros/ros.h"
#include <quasimodo_msgs/query_cloud.h>
#include <quasimodo_msgs/visualize_query.h>
#include <metaroom_xml_parser/load_utilities.h>
#include <pcl_ros/point_cloud.h>
#include <cv_bridge/cv_bridge.h>

#include "metaroom_xml_parser/load_utilities.h"
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl_ros/transforms.h>

#include "quasimodo_msgs/model.h"
#include "quasimodo_msgs/rgbd_frame.h"
#include "quasimodo_msgs/model_from_frame.h"
#include "quasimodo_msgs/index_frame.h"
#include "quasimodo_msgs/fuse_models.h"
#include "quasimodo_msgs/get_model.h"

#include "metaroom_xml_parser/simple_xml_parser.h"

#include <image_geometry/pinhole_camera_model.h>
#include <sensor_msgs/CameraInfo.h>

#include "modelupdater/ModelUpdater.h"
#include "core/RGBDFrame.h"
#include "Util/Util.h"



using namespace std;

typedef pcl::PointXYZRGB PointType;
typedef pcl::PointCloud<PointType> Cloud;
typedef typename Cloud::Ptr CloudPtr;
typedef pcl::search::KdTree<PointType> Tree;
typedef semantic_map_load_utilties::DynamicObjectData<PointType> ObjectData;

using pcl::visualization::PointCloudColorHandlerCustom;

std::vector< std::vector<cv::Mat> > rgbs;
std::vector< std::vector<cv::Mat> > depths;
std::vector< std::vector<cv::Mat> > masks;
std::vector< std::vector<tf::StampedTransform > >tfs;
std::vector< std::vector<Eigen::Matrix4f> > initposes;
std::vector< std::vector< image_geometry::PinholeCameraModel > > cams;

pcl::visualization::PCLVisualizer* pg;
boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer;



std::vector<Eigen::Matrix4f> getRegisteredViewPoses(const std::string& poses_file, const int& no_transforms){
	std::vector<Eigen::Matrix4f> toRet;
	ifstream in(poses_file);
	if (!in.is_open()){
		cout<<"ERROR: cannot find poses file "<<poses_file<<endl;
		return toRet;
	}
	cout<<"Loading additional view registered poses from "<<poses_file<<endl;

	for (int i=0; i<no_transforms+1; i++){
		Eigen::Matrix4f transform;
		float temp;
		for (size_t j=0; j<4; j++){
			for (size_t k=0; k<4; k++){
				in >> temp;
				transform(j,k) = temp;
			}
		}
		toRet.push_back(transform);
	}
	return toRet;
}

void chatterCallback(const std_msgs::String::ConstPtr& msg)
{
	ROS_INFO("I heard: [%s]", msg->data.c_str());

	for(unsigned int i = 1; i < rgbs.size(); i++){
		std::vector<int> fid;
		std::vector<int> fadded;
		for(unsigned int j = 0; j < rgbs[i].size(); j++){
			printf("%i %i\n",i,j);

			//cv::Mat maskimage			= masks[i][j];
			cv::Mat rgbimage			= rgbs[i][j];
			cv::Mat depthimage			= depths[i][j];
			tf::StampedTransform tf		= tfs[i][j];
			Eigen::Matrix4f m			= initposes[i][j];
			geometry_msgs::TransformStamped tfstmsg;
			tf::transformStampedTFToMsg (tf, tfstmsg);

			geometry_msgs::Transform tfmsg = tfstmsg.transform;

			printf("start adding frame %i\n",i);

			Eigen::Quaternionf q = Eigen::Quaternionf(Eigen::Affine3f(m).rotation());

			geometry_msgs::Pose		pose;
//			pose.orientation		= tfmsg.rotation;
//			pose.position.x		= tfmsg.translation.x;
//			pose.position.y		= tfmsg.translation.y;
//			pose.position.z		= tfmsg.translation.z;
					  pose.orientation.x	= q.x();
					  pose.orientation.y	= q.y();
					  pose.orientation.z	= q.z();
					  pose.orientation.w	= q.w();
					  pose.position.x		= m(0,3);
					  pose.position.y		= m(1,3);
					  pose.position.z		= m(2,3);


			cv_bridge::CvImage rgbBridgeImage;
			rgbBridgeImage.image = rgbimage;
			rgbBridgeImage.encoding = "bgr8";

			cv_bridge::CvImage depthBridgeImage;
			depthBridgeImage.image = depthimage;
			depthBridgeImage.encoding = "mono16";
/*
			quasimodo_msgs::index_frame ifsrv;
			ifsrv.request.frame.capture_time = ros::Time();
			ifsrv.request.frame.pose		= pose;
			ifsrv.request.frame.frame_id	= -1;
			ifsrv.request.frame.rgb		= *(rgbBridgeImage.toImageMsg());
			ifsrv.request.frame.depth		= *(depthBridgeImage.toImageMsg());


			//314.458000 242.038000 536.458000 537.422000
			ifsrv.request.frame.camera.K[0] = 536.458000;//cams[i][j].fx();
			ifsrv.request.frame.camera.K[4] = 537.422000;//cams[i][j].fy();
			ifsrv.request.frame.camera.K[2] = 314.458000;//cams[i][j].cx();
			ifsrv.request.frame.camera.K[5] = 242.038000;//cams[i][j].cy();

			printf("camera: %f %f %f %f\n",cams[i][j].cx(),cams[i][j].cy(),cams[i][j].fx(),cams[i][j].fy());

			if (index_frame_client.call(ifsrv)){//Add frame to model server
				int frame_id = ifsrv.response.frame_id;
				fadded.push_back(j);
				fid.push_back(frame_id);
				ROS_INFO("frame_id%i", frame_id );
			}else{ROS_ERROR("Failed to call service index_frame");}
*/
			printf("stop adding frame %i\n",i);
		}

		for(unsigned int j = 0; j < fadded.size(); j++){
			printf("start adding mask %i\n",i);
			cv::Mat mask	= masks[i][fadded[j]];

			cv_bridge::CvImage maskBridgeImage;
			maskBridgeImage.image = mask;
			maskBridgeImage.encoding = "mono8";
/*
			quasimodo_msgs::model_from_frame mff;
			mff.request.mask		= *(maskBridgeImage.toImageMsg());
			mff.request.isnewmodel	= (j == (fadded.size()-1));
			mff.request.frame_id	= fid[j];

			if (model_from_frame_client.call(mff)){//Build model from frame
				int model_id = mff.response.model_id;
				if(model_id > 0){
					ROS_INFO("model_id%i", model_id );
				}
			}else{ROS_ERROR("Failed to call service index_frame");}
*/
			printf("stop adding mask %i\n",i);

		}
	}
}

pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr transformCloud(pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cloud, Eigen::Matrix4d p){
	pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr ret (new pcl::PointCloud<pcl::PointXYZRGBNormal>);
	ret->points.resize(cloud->points.size());

	const double & m00 = p(0,0); const double & m01 = p(0,1); const double & m02 = p(0,2); const double & m03 = p(0,3);
	const double & m10 = p(1,0); const double & m11 = p(1,1); const double & m12 = p(1,2); const double & m13 = p(1,3);
	const double & m20 = p(2,0); const double & m21 = p(2,1); const double & m22 = p(2,2); const double & m23 = p(2,3);

	for(unsigned int i = 0; i < cloud->points.size(); i++){
		pcl::PointXYZRGBNormal & p1 = cloud->points[i];
		pcl::PointXYZRGBNormal & p2 = ret->points[i];

		p2.r = p1.r;
		p2.g = p1.g;
		p2.b = p1.b;

		const double & src_x = p1.x;
		const double & src_y = p1.y;
		const double & src_z = p1.z;

		const double & src_nx = p1.normal_x;
		const double & src_ny = p1.normal_y;
		const double & src_nz = p1.normal_z;

		p2.x = m00*src_x + m01*src_y + m02*src_z + m03;
		p2.y = m10*src_x + m11*src_y + m12*src_z + m13;
		p2.z = m20*src_x + m21*src_y + m22*src_z + m23;

		p2.normal_x = m00*src_nx + m01*src_ny + m02*src_nz;
		p2.normal_y = m10*src_nx + m11*src_ny + m12*src_nz;
		p2.normal_z = m20*src_nx + m21*src_ny + m22*src_nz;

		p2.r = p1.r;
		p2.g = p1.g;
		p2.b = p1.b;
	}

	return ret;
}

pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr getNC(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud, int nr_data = 5000000){
	int MaxDepthChangeFactor = 20;
	int NormalSmoothingSize = 7;
	int depth_dependent_smoothing = 1;

	pcl::IntegralImageNormalEstimation<pcl::PointXYZRGB, pcl::Normal> ne;
	ne.setMaxDepthChangeFactor(0.001*double(MaxDepthChangeFactor));
	ne.setNormalSmoothingSize(NormalSmoothingSize);
	ne.setDepthDependentSmoothing (depth_dependent_smoothing);
	ne.setNormalEstimationMethod (ne.COVARIANCE_MATRIX);

	pcl::PointCloud<pcl::Normal>::Ptr	normals (new pcl::PointCloud<pcl::Normal>);
	ne.setInputCloud(cloud);
	ne.compute(*normals);

	int rc = rand()%256;
	int gc = rand()%256;
	int bc = rand()%256;

	pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr ret (new pcl::PointCloud<pcl::PointXYZRGBNormal>);
	pcl::concatenateFields(*cloud,*normals,*ret);

	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer (new pcl::visualization::PCLVisualizer ("3D Viewer"));
	viewer->setBackgroundColor (0.5, 0, 0.5);
	viewer->addCoordinateSystem (1.0);
	viewer->initCameraParameters ();

	viewer->removeAllPointClouds();
	viewer->addPointCloud<pcl::PointXYZRGB> (cloud, pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB>(cloud), "sample cloud");
	viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
	viewer->spin();

	viewer->removeAllPointClouds();
	viewer->addPointCloud<pcl::PointXYZRGBNormal> (ret, pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBNormal>(ret), "sample cloud");
	viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
	viewer->addPointCloudNormals<pcl::PointXYZRGBNormal, pcl::PointXYZRGBNormal> (ret, ret, 15, 0.10, "normals");
	viewer->spin();


//	ret->points.resize(cloud->points.size());
//	for(int i = 0; i < cloud->points.size(); i++){
//		pcl::PointXYZRGBNormal & p1 = ret->points[i];
//		pcl::PointXYZRGB & p = cloud->points[i];
//		pcl::Normal & pn = normals->points[i];

//		p1.x = p.x;
//		p1.y = p.y;
//		p1.z = p.z;
////		p1.r = rc;
////		p1.g = gc;
////		p1.b = bc;
//		p1.r = p.r;
//		p1.g = p.g;
//		p1.b = p.b;
//		p1.normal_x = pn.normal_x;
//		p1.normal_y = pn.normal_y;
//		p1.normal_z = pn.normal_z;
//	}
/*

*/

//	for(int i = 0; i < ret->points.size(); i++){
//		pcl::PointXYZRGBNormal & p1 = ret->points[i];
//		p1.r = rc;
//		p1.g = gc;
//		p1.b = bc;
//	}
	return ret;
}

int nr_vAdditionalViews = 4;

std::vector<reglib::Model * > models;

reglib::Model * load2(std::string sweep_xml){
	int slash_pos = sweep_xml.find_last_of("/");
	std::string sweep_folder = sweep_xml.substr(0, slash_pos) + "/";
	printf("folder: %s\n",sweep_folder.c_str());

	SimpleXMLParser<PointType> parser;
	SimpleXMLParser<PointType>::RoomData roomData  = parser.loadRoomFromXML(sweep_folder+"/room.xml");

	reglib::Camera * cam		= new reglib::Camera();//TODO:: ADD TO CAMERAS
	cam->fx = 532.158936;
	cam->fy = 533.819214;
	cam->cx = 310.514310;
	cam->cy = 236.842039;

/*
	projection
	532.158936 0.000000 310.514310 0.000000
	0.000000 533.819214 236.842039 0.000000
	0.000000 0.000000 1.000000 0.000000
*/

/*
	[image]

	width
	640

	height
	480

	[narrow_stereo]

	camera matrix
	534.192642 0.000000 311.485658
	0.000000 533.870030 237.668175
	0.000000 0.000000 1.000000

	distortion
	0.030388 -0.100645 -0.000995 -0.000366 0.000000

	rectification
	1.000000 0.000000 0.000000
	0.000000 1.000000 0.000000
	0.000000 0.000000 1.000000

	projection
	532.158936 0.000000 310.514310 0.000000
	0.000000 533.819214 236.842039 0.000000
	0.000000 0.000000 1.000000 0.000000
*/

	cv::Mat fullmask;
	fullmask.create(480,640,CV_8UC1);
	unsigned char * maskdata = (unsigned char *)fullmask.data;
	for(int j = 0; j < 480*640; j++){maskdata[j] = 255;}

	reglib::Model * sweepmodel = 0;
	std::vector<reglib::RGBDFrame * > current_room_frames;
	for (size_t i=0; i<roomData.vIntermediateRoomClouds.size(); i++)
	{
		cout<<"Intermediate cloud size "<<roomData.vIntermediateRoomClouds[i]->points.size()<<endl;

		printf("%i / %i\n",i,roomData.vIntermediateRoomClouds.size());

		//Transform
		tf::StampedTransform tf	= roomData.vIntermediateRoomCloudTransformsRegistered[i];
		geometry_msgs::TransformStamped tfstmsg;
		tf::transformStampedTFToMsg (tf, tfstmsg);
		geometry_msgs::Transform tfmsg = tfstmsg.transform;
		geometry_msgs::Pose		pose;
		pose.orientation		= tfmsg.rotation;
		pose.position.x		= tfmsg.translation.x;
		pose.position.y		= tfmsg.translation.y;
		pose.position.z		= tfmsg.translation.z;
		Eigen::Affine3d epose;
		tf::poseMsgToEigen(pose, epose);

		reglib::RGBDFrame * frame = new reglib::RGBDFrame(cam,roomData.vIntermediateRGBImages[i],5.0*roomData.vIntermediateDepthImages[i],0, epose.matrix());
		current_room_frames.push_back(frame);
		if(i == 0){
			sweepmodel = new reglib::Model(frame,fullmask);
		}else{
			sweepmodel->frames.push_back(frame);
			sweepmodel->relativeposes.push_back(current_room_frames.front()->pose.inverse() * frame->pose);
			sweepmodel->modelmasks.push_back(new reglib::ModelMask(fullmask));
		}
	}
	sweepmodel->recomputeModelPoints();
	models.push_back(sweepmodel);
	printf("nr points: %i\n",sweepmodel->points.size());

/*
	reglib::RegistrationRandom *	reg	= new reglib::RegistrationRandom();
	reglib::ModelUpdaterBasicFuse * mu	= new reglib::ModelUpdaterBasicFuse( objectModel, reg);
	mu->occlusion_penalty               = 15;
	mu->massreg_timeout                 = 60*4;
	mu->viewer							= viewer;

	objectModel->print();
	mu->makeInitialSetup();
	objectModel->print();
	delete mu;

	objectModel->recomputeModelPoints();
*/
//	reglib::Model * newmodelHolder = new reglib::Model();
//	newmodelHolder->submodels.push_back(sweepmodel);
//	newmodelHolder->submodels_relativeposes.push_back(Eigen::Matrix4d::Identity());
//	newmodelHolder->recomputeModelPoints();
/*
	pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cn1 = objectModel->getPCLnormalcloud(1, false);
	viewer->removeAllPointClouds();
	viewer->addPointCloud<pcl::PointXYZRGBNormal> (cn1, pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBNormal>(cn1), "sample cloud");
	viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
	viewer->spin();
*/

//	pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cn = sweepmodel->getPCLnormalcloud(1, false);
//	viewer->removeAllPointClouds();
//	viewer->addPointCloud<pcl::PointXYZRGBNormal> (cn, pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBNormal>(cn), "sample cloud");
//	viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
//	viewer->spin();

	return sweepmodel;
}

void load(std::string sweep_xml){

	int slash_pos = sweep_xml.find_last_of("/");
	std::string sweep_folder = sweep_xml.substr(0, slash_pos) + "/";
	printf("folder: %s\n",sweep_folder.c_str());

	SimpleXMLParser<PointType> parser;
	SimpleXMLParser<PointType>::RoomData roomData  = parser.loadRoomFromXML(sweep_folder+"/room.xml");

	QStringList objectFiles = QDir(sweep_folder.c_str()).entryList(QStringList("*object*.xml"));
	vector<ObjectData> objects = semantic_map_load_utilties::loadAllDynamicObjectsFromSingleSweep<PointType>(sweep_folder+"room.xml");

	int object_id;
	for (unsigned int i=0; i< objects.size(); i++){
		if (objects[i].objectScanIndices.size() != 0){object_id = i;}
	}
	int index = objectFiles[object_id].toStdString().find_last_of(".");
	string object_root_name = objectFiles[object_id].toStdString().substr(0,index);

	for (auto object : objects){
		if (!object.objectScanIndices.size()){continue;}

		nr_vAdditionalViews = 1 + rand()%5;

		std::vector<cv::Mat> viewrgbs;
		std::vector<cv::Mat> viewdepths;
		std::vector<cv::Mat> viewmasks;
		std::vector<tf::StampedTransform > viewtfs;
		std::vector<Eigen::Matrix4f> viewposes;
		std::vector< image_geometry::PinholeCameraModel > viewcams;
		char buf [1024];
		sprintf(buf,"%s/object_%i/poses.txt",sweep_folder.c_str(),object_id);
		std::vector<Eigen::Matrix4f> poses = getRegisteredViewPoses(std::string(buf), object.vAdditionalViews.size());
		cout<<"Loaded "<<poses.size()<<" registered poses."<<endl;

		for (unsigned int i=0; i < nr_vAdditionalViews && i<object.vAdditionalViews.size(); i++){
			CloudPtr cloud = object.vAdditionalViews[i];

			cv::Mat mask;
			mask.create(cloud->height,cloud->width,CV_8UC1);
			unsigned char * maskdata = (unsigned char *)mask.data;

			cv::Mat rgb;
			rgb.create(cloud->height,cloud->width,CV_8UC3);
			unsigned char * rgbdata = (unsigned char *)rgb.data;

			cv::Mat depth;
			depth.create(cloud->height,cloud->width,CV_16UC1);
			unsigned short * depthdata = (unsigned short *)depth.data;

			unsigned int nr_data = cloud->height * cloud->width;
			for(unsigned int j = 0; j < nr_data; j++){
				maskdata[j] = 0;
				PointType p = cloud->points[j];
				rgbdata[3*j+0]	= p.b;
				rgbdata[3*j+1]	= p.g;
				rgbdata[3*j+2]	= p.r;
				depthdata[j]	= short(5000.0 * p.z);
			}

			cout<<"Processing AV "<<i<<endl;

			stringstream av_ss; av_ss<<object_root_name; av_ss<<"_additional_view_"; av_ss<<i; av_ss<<".txt";
			string complete_av_mask_name = sweep_folder + av_ss.str();
			ifstream av_mask_in(complete_av_mask_name);
			if (!av_mask_in.is_open()){
				cout<<"COULD NOT FIND AV MASK FILE "<<complete_av_mask_name<<endl;
				continue;
			}

			CloudPtr av_mask(new Cloud);

			int av_index;
			while (av_mask_in.is_open() && !av_mask_in.eof()){
				av_mask_in>>av_index;
				av_mask->push_back(object.vAdditionalViews[i]->points[av_index]);
				maskdata[av_index] = 255;
			}

			viewrgbs.push_back(rgb);
			viewdepths.push_back(depth);
			viewmasks.push_back(mask);
			viewtfs.push_back(object.vAdditionalViewsTransforms[i]);
			viewposes.push_back(poses[i+1]);
			viewcams.push_back(roomData.vIntermediateRoomCloudCamParams.front());

			cv::namedWindow("rgbimage",     cv::WINDOW_AUTOSIZE);
			cv::imshow(		"rgbimage",     rgb);
			cv::namedWindow("depthimage",	cv::WINDOW_AUTOSIZE);
			cv::imshow(		"depthimage",	depth);
			cv::namedWindow("mask",         cv::WINDOW_AUTOSIZE);
			cv::imshow(		"mask",         mask);
			cv::waitKey(30);
		}

		if(viewrgbs.size() > 0){

			rgbs.push_back(viewrgbs);
			depths.push_back(viewdepths);
			masks.push_back(viewmasks);
			tfs.push_back(viewtfs);
			initposes.push_back(viewposes);
			cams.push_back(viewcams);
/*
			tf::StampedTransform tf	= roomData.vIntermediateRoomCloudTransforms.front();
			geometry_msgs::TransformStamped tfstmsg;
			tf::transformStampedTFToMsg (tf, tfstmsg);
			geometry_msgs::Transform tfmsg = tfstmsg.transform;
			geometry_msgs::Pose		pose;
			pose.orientation		= tfmsg.rotation;
			pose.position.x		= tfmsg.translation.x;
			pose.position.y		= tfmsg.translation.y;
			pose.position.z		= tfmsg.translation.z;
			Eigen::Affine3d localthing;
			tf::poseMsgToEigen(pose, localthing);

			reglib::Camera * cam		= new reglib::Camera();//TODO:: ADD TO CAMERAS
			cam->fx = viewcams.front().fx();
			cam->fy = viewcams.front().fy();
			cam->cx = viewcams.front().cx();
			cam->cy = viewcams.front().cy();


			cv::Mat fullmask;
			fullmask.create(480,640,CV_8UC1);
			unsigned char * maskdata = (unsigned char *)fullmask.data;
			for(int j = 0; j < 480*640; j++){maskdata[j] = 0;}


			reglib::Model * objectModel = 0;//new reglib::Model(frames[frame_id],mask);

			std::vector<reglib::RGBDFrame * > current_frames;
			for (unsigned int i=0;i < nr_vAdditionalViews && i<object.vAdditionalViews.size(); i++){
				tf::StampedTransform tf	= object.vAdditionalViewsTransforms[i];
				geometry_msgs::TransformStamped tfstmsg;
				tf::transformStampedTFToMsg (tf, tfstmsg);
				geometry_msgs::Transform tfmsg = tfstmsg.transform;
				geometry_msgs::Pose		pose;
				pose.orientation		= tfmsg.rotation;
				pose.position.x		= tfmsg.translation.x;
				pose.position.y		= tfmsg.translation.y;
				pose.position.z		= tfmsg.translation.z;
				Eigen::Affine3d epose;
				tf::poseMsgToEigen(pose, epose);
				epose = localthing.inverse()*epose;

				reglib::RGBDFrame * frame = new reglib::RGBDFrame(cam,viewrgbs[i],viewdepths[i],0, epose.matrix());
				current_frames.push_back(frame);

				if(i == 0){
					objectModel = new reglib::Model(frame,viewmasks[i]);
				}else{
					objectModel->frames.push_back(frame);
					objectModel->relativeposes.push_back(current_frames.front()->pose.inverse() * frame->pose);
					objectModel->modelmasks.push_back(new reglib::ModelMask(viewmasks[i]));//fullmask));
				}
			}

			reglib::Model * sweepmodel = 0;
			std::vector<reglib::RGBDFrame * > current_room_frames;
			for (size_t i=0; i<roomData.vIntermediateRoomClouds.size(); i++)
			{
				cout<<"Intermediate cloud size "<<roomData.vIntermediateRoomClouds[i]->points.size()<<endl;

				//Transform
				tf::StampedTransform tf	= roomData.vIntermediateRoomCloudTransformsRegistered[i];
				geometry_msgs::TransformStamped tfstmsg;
				tf::transformStampedTFToMsg (tf, tfstmsg);
				geometry_msgs::Transform tfmsg = tfstmsg.transform;
				geometry_msgs::Pose		pose;
				pose.orientation		= tfmsg.rotation;
				pose.position.x		= tfmsg.translation.x;
				pose.position.y		= tfmsg.translation.y;
				pose.position.z		= tfmsg.translation.z;
				Eigen::Affine3d epose;
				tf::poseMsgToEigen(pose, epose);

				reglib::RGBDFrame * frame = new reglib::RGBDFrame(cam,roomData.vIntermediateRGBImages[i],5.0*roomData.vIntermediateDepthImages[i],0, epose.matrix());
				current_room_frames.push_back(frame);
				if(i == 0){
					sweepmodel = new reglib::Model(frame,fullmask);
				}else{
					sweepmodel->frames.push_back(frame);
					sweepmodel->relativeposes.push_back(current_room_frames.front()->pose.inverse() * frame->pose);
					sweepmodel->modelmasks.push_back(new reglib::ModelMask(fullmask));
				}
			}
			sweepmodel->recomputeModelPoints();

//            objectModel->submodels.push_back(sweepmodel);
//            objectModel->submodels_relativeposes.push_back(objectModel->frames.front()->pose.inverse() * sweepmodel->frames.front()->pose);


			reglib::RegistrationRandom *	reg	= new reglib::RegistrationRandom();
			reglib::ModelUpdaterBasicFuse * mu	= new reglib::ModelUpdaterBasicFuse( objectModel, reg);
			mu->occlusion_penalty               = 15;
			mu->massreg_timeout                 = 60*4;
			mu->viewer							= viewer;

			objectModel->print();
			mu->makeInitialSetup();
			objectModel->print();
			delete mu;

			objectModel->recomputeModelPoints();

			reglib::Model * newmodelHolder = new reglib::Model();
			newmodelHolder->submodels.push_back(objectModel);
			newmodelHolder->submodels_relativeposes.push_back(Eigen::Matrix4d::Identity());
			newmodelHolder->recomputeModelPoints();

			pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cn1 = objectModel->getPCLnormalcloud(1, false);
			viewer->removeAllPointClouds();
			viewer->addPointCloud<pcl::PointXYZRGBNormal> (cn1, pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBNormal>(cn1), "sample cloud");
			viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
			viewer->spin();

			pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr cn = sweepmodel->getPCLnormalcloud(1, false);
			viewer->removeAllPointClouds();
			viewer->addPointCloud<pcl::PointXYZRGBNormal> (cn, pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGBNormal>(cn), "sample cloud");
			viewer->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 3, "sample cloud");
			viewer->spin();
*/
//            rgbs.push_back(viewrgbs);
//            depths.push_back(viewdepths);
//            masks.push_back(viewmasks);
//            tfs.push_back(viewtfs);
//            initposes.push_back(viewposes);
//            cams.push_back(viewcams);
		}
	}
}

int main(int argc, char** argv){

	ros::init(argc, argv, "test_segment");
	ros::NodeHandle n;
//	ros::Subscriber sub = n.subscribe("modelserver", 1000, chatterCallback);

	viewer = boost::shared_ptr<pcl::visualization::PCLVisualizer>(new pcl::visualization::PCLVisualizer ("3D Viewer"));
	viewer->setBackgroundColor (0.5, 0, 0.5);
	viewer->addCoordinateSystem (1.0);
	viewer->initCameraParameters ();

	ros::ServiceClient segmentation_client = n.serviceClient<quasimodo_msgs::segment_model>("segment_model");

	for(int ar = 1; ar < argc; ar++){
		string overall_folder = std::string(argv[ar]);
		vector<string> sweep_xmls = semantic_map_load_utilties::getSweepXmls<PointType>(overall_folder);
		printf("sweep_xmls\n");
		for (auto sweep_xml : sweep_xmls) {
			printf("sweep_xml: %s\n",sweep_xml.c_str());
			load2(sweep_xml);
		}
	}

	//Not needed if metaroom well calibrated
	reglib::RegistrationRandom *	reg	= new reglib::RegistrationRandom();
	reglib::ModelUpdaterBasicFuse * mu	= new reglib::ModelUpdaterBasicFuse( models.front(), reg);
	mu->occlusion_penalty               = 15;
	mu->massreg_timeout                 = 60*4;
	mu->viewer							= viewer;

	reglib::MassRegistrationPPR2 * bgmassreg = new reglib::MassRegistrationPPR2(0.0);
	bgmassreg->timeout = 1200;
	bgmassreg->viewer = viewer;
	bgmassreg->use_surface = true;
	bgmassreg->use_depthedge = true;
	bgmassreg->visualizationLvl = 1;
	bgmassreg->maskstep = 10;
	bgmassreg->nomaskstep = 10;
	bgmassreg->nomask = true;
	bgmassreg->stopval = 0.0005;
	bgmassreg->setData(models.front()->frames,models.front()->modelmasks);
	reglib::MassFusionResults bgmfr = bgmassreg->getTransforms(models.front()->relativeposes);



	for(int j = 0; j < models.size(); j++){
		models[j]->relativeposes	= bgmfr.poses;
		models[j]->points			= mu->getSuperPoints(models[j]->relativeposes,models[j]->frames,models[j]->modelmasks,1,false);
	}

//	for(int i = 0; i < models.front()->relativeposes.size(); i++){
//		models.front()->frames[i]->camera->print();
//		std::cout << models.front()->relativeposes[i] << std::endl << std::endl;
//	}


	for(unsigned int i = 1; i < models.size(); i++){
		quasimodo_msgs::segment_model sm;
		sm.request.models.push_back(quasimodo_brain::getModelMSG(models[i]));
		if(i > 0){
			sm.request.backgroundmodel = quasimodo_brain::getModelMSG(models[i-1]);
		}
		if (segmentation_client.call(sm)){//Build model from frame
			//int model_id = mff.response.model_id;
			printf("segmented: %i\n",i);
		}else{ROS_ERROR("Failed to call service segment_model");}
	}


//	mff.request.mask		= *(maskBridgeImage.toImageMsg());
//	mff.request.isnewmodel	= (j == (fadded.size()-1));
//	mff.request.frame_id	= fid[j];

//	if (model_from_frame_client.call(mff)){//Build model from frame
//		int model_id = mff.response.model_id;
//		if(model_id > 0){
//			ROS_INFO("model_id%i", model_id );
//		}
//	}else{ROS_ERROR("Failed to call service index_frame");}
/*
	ros::NodeHandle pn("~");


	for(unsigned int i = 0; i < models.size(); i++){
		printf("%i -> %i\n",i,models[i]->frames.size());

		vector<Eigen::Matrix4d> cp;
		vector<reglib::RGBDFrame*> cf;
		vector<reglib::ModelMask*> mm;

		vector<Eigen::Matrix4d> cp_front;
		vector<reglib::RGBDFrame*> cf_front;
		vector<reglib::ModelMask*> mm_front;
		for(int j = 0; j <= i; j++){
			cp_front.push_back(models.front()->relativeposes.front().inverse() * models[j]->relativeposes.front());
			cf_front.push_back(models[j]->frames.front());
			mm_front.push_back(models[j]->modelmasks.front());
		}

		if(i > 0){
			reglib::MassRegistrationPPR2 * massreg = new reglib::MassRegistrationPPR2(0.05);
			massreg->timeout = 1200;
			massreg->viewer = viewer;
			massreg->visualizationLvl = 0;

			massreg->maskstep = 5;//std::max(1,int(0.4*double(models[i]->frames.size())));
			massreg->nomaskstep = 5;//std::max(3,int(0.5+0.*double(models[i]->frames.size())));//std::max(1,int(0.5+1.0*double(model->frames.size())));
			massreg->nomask = true;
			massreg->stopval = 0.0005;

			//		massreg->setData(models[i]->frames,models[i]->modelmasks);
			//		reglib::MassFusionResults mfr = massreg->getTransforms(models[i]->relativeposes);

			massreg->setData(cf_front,mm_front);


			reglib::MassFusionResults mfr_front = massreg->getTransforms(cp_front);

			for(int j = i; j >= 0; j--){
				Eigen::Matrix4d change = mfr_front.poses[j] * cp_front[j].inverse();
				for(unsigned int k = 0; k < models[j]->relativeposes.size(); k++){
					cp.push_back(change * models.front()->relativeposes.front().inverse() * models[j]->relativeposes[k]);
					cf.push_back(models[j]->frames[k]);
					mm.push_back(models[j]->modelmasks[k]);
				}
			}
		}else{
			for(int j = i; j >= 0; j--){
				Eigen::Matrix4d change = Eigen::Matrix4d::Identity();//mfr_front.poses[j] * cp_front[j].inverse();
				for(unsigned int k = 0; k < models[j]->relativeposes.size(); k++){
					cp.push_back(change * models.front()->relativeposes.front().inverse() * models[j]->relativeposes[k]);
					cf.push_back(models[j]->frames[k]);
					mm.push_back(models[j]->modelmasks[k]);
				}
			}
		}




		reglib::MassRegistrationPPR2 * massreg2 = new reglib::MassRegistrationPPR2(0.0);
		massreg2->timeout = 1200;
		massreg2->viewer = viewer;
		massreg2->visualizationLvl = 1;

		massreg2->maskstep = 10;//std::max(1,int(0.4*double(models[i]->frames.size())));
		massreg2->nomaskstep = 10;//std::max(3,int(0.5+0.*double(models[i]->frames.size())));//std::max(1,int(0.5+1.0*double(model->frames.size())));
		massreg2->nomask = true;
		massreg2->stopval = 0.0005;

		massreg2->setData(cf,mm);
		reglib::MassFusionResults mfr2 = massreg2->getTransforms(cp);
		cp = mfr2.poses;

		//massreg->setData(models[i]->frames,models[i]->modelmasks);
		//reglib::MassFusionResults mfr = massreg->getTransforms(models[i]->relativeposes);

//		massreg->setData(cf,mm);
//		reglib::MassFusionResults mfr = massreg->getTransforms(cp);
//		cp = mfr.poses;



		reglib::RegistrationRandom *	reg	= new reglib::RegistrationRandom();
		reglib::ModelUpdaterBasicFuse * mu	= new reglib::ModelUpdaterBasicFuse( models[i], reg);
		mu->occlusion_penalty               = 15;
		mu->massreg_timeout                 = 60*4;
		mu->viewer							= viewer;

		vector<cv::Mat> mats;
		mu->computeDynamicObject(cp,cf,mats);//models[i]->relativeposes, models[i]->frames, mats );

//		models[i]->print();
//		mu->show_init_lvl = 2;
//		mu->makeInitialSetup();
//		models[i]->print();

		//delete mu;
	}
*/
	ros::spin();
}
