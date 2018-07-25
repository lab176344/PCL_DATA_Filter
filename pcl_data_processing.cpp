// Standard libraries
#include <iostream>
#include <string>
#include <vector>
#include <time.h>
// Point Cloud I/O related
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <pcl/io/hdl_grabber.h>
#include <pcl/console/parse.h>
#include <pcl/visualization/pcl_visualizer.h>
// For PCL Filtering
#include <pcl/point_types.h>
#include <pcl/filters/passthrough.h>
// Outline removal
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/radius_outlier_removal.h>
// For Ground Estimation
#include <pcl/io/io.h>
#include <pcl/io/pcd_io.h>
#include <pcl/ModelCoefficients.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/common/pca.h>
// Downsampling
#include <pcl/filters/voxel_grid.h>
#include <pcl/common/common.h>
// Kd Tree
#include <pcl/features/normal_3d.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/segmentation/extract_clusters.h>
#include <pcl/segmentation/conditional_euclidean_clustering.h>
#include <pcl/segmentation/region_growing.h>
// Bounding box
#include <boost/thread/thread.hpp>
#include <pcl/features/moment_of_inertia_estimation.h>
// Kalman Filter


//---------------------------------------------------------------------------------------------------------------------------------------------------

/* GLOBAL VARIABLE DECLARATIONS */

// Structure for bounding box
struct measurement
{
	int ID;
	float x;
	float y;
	float z;
	float azimuth;
	uint32_t utc;
	pcl::PointXYZI max, min;

};



// Namespaces for PCL and OpenCV

using namespace std;
// Point Type (pcl::PointXYZ, pcl::PointXYZI, pcl::PointXYZRGBA)
typedef pcl::PointXYZI PointType;
// For Ground plane estimation
typedef pcl::PointCloud<PointType> PointCloudT;
// For clustering
typedef pcl::PointXYZINormal PointTypeFull;

pcl::PointCloud<PointType>::Ptr cloud_filtered(new pcl::PointCloud<PointType>);

//---------------------------------------------------------------------------------------------------------------------------------------------------
// For ground plane estimation
PointCloudT::Ptr	cloud_inliers(new PointCloudT), cloud_outliers(new PointCloudT), cloud_seg(new PointCloudT);

// Segment the ground
pcl::ModelCoefficients::Ptr plane(new pcl::ModelCoefficients);
pcl::PointIndices::Ptr 		inliers_plane(new pcl::PointIndices);
PointCloudT::Ptr 			cloud_plane(new PointCloudT);


//---------------------------------------------------------------------------------------------------------------------------------------------------

/* GLOBAL VARIABLE DECLARATIONS */

std::vector<measurement> meas_snd(30);
std::vector<std::vector<measurement>> matlabsend;
bool first_frame = false; uint64_t time_frame; float current_time = 0; uint32_t last_time = 0, ls_t = 0; uint32_t n, u;
bool send_measurement = false;
/* GLOBAL FUNCTION DECLARATIONS */
std::ofstream wrte("test.csv");

/* GLOBAL FUNCTION DECLARATIONS */
void vector_clear()
{

	for (int i = 0; i < 30; i++)
	{
		meas_snd[i].x = 0; meas_snd[i].y = 0; meas_snd[i].z = 0; meas_snd[i].utc = 0; meas_snd[i].azimuth = 0;
	}
}

uint32_t calculate_utc(uint32_t time, uint32_t tim_gap, float azimuth)
{
	uint32_t time_calcualte = 0;
	uint32_t time_degree = 0;
	time_degree = tim_gap * ((6.28319 - azimuth) / 6.28319);
	time_calcualte = time - time_degree;


	return time_calcualte;

}

void pass_inte(pcl::PointCloud<PointType>::ConstPtr ptr)
{
	pcl::PassThrough<PointType> pass;
	pass.setInputCloud(ptr);
	pass.setFilterFieldName("z");
	pass.setFilterLimits(2.0f, 25.0f);
	pass.setFilterLimitsNegative(true);
	pass.filter(*cloud_seg);
}
/**
* +-> Function for ground plane estimation
*/
void segmentcloud()
{
	// Segment the ground
	pcl::ModelCoefficients::Ptr plane(new pcl::ModelCoefficients);
	pcl::PointIndices::Ptr 		inliers_plane(new pcl::PointIndices);
	PointCloudT::Ptr 			cloud_plane(new PointCloudT);
	pcl::ExtractIndices<PointType> extract;

	plane->values.resize(4);                            // 4 points to form a plane
														// Make room for a plane equation (ax+by+cz+d=0)
	pcl::SACSegmentation<PointType> seg;				// Create the segmentation object
	seg.setOptimizeCoefficients(true);				// Optional
	seg.setMethodType(pcl::SAC_RANSAC);
	seg.setModelType(pcl::SACMODEL_PLANE);
	seg.setDistanceThreshold(0.4f);
	seg.setInputCloud(cloud_seg);
	seg.segment(*inliers_plane, *plane);

	if (inliers_plane->indices.size() == 0) {
		PCL_ERROR("Could not estimate a planar model for the given dataset.\n");
	}

	// Extract Inliers

	extract.setInputCloud(cloud_seg);
	extract.setIndices(inliers_plane);
	extract.setNegative(false);			// Extract the inliers
	extract.filter(*cloud_inliers);		// cloud_inliers contains the plane

										// Extract Outliers
	extract.setNegative(true);
	extract.filter(*cloud_outliers);		// cloud_outliers contains everything but the plane


}


void write()
{
	for (int i = 0; i < 30; i++)
	{
		if (meas_snd[i].x != 0)
		{
			wrte << meas_snd[i].x << ";" << meas_snd[i].y << ";" << meas_snd[i].utc << ";" << endl;
		}
	}
}


void cluster(uint32_t n)
{
	cout << "UTC" << n << endl;
	/* CLUSTERING */
	uint32_t time_s = 0;
	time_s = n - last_time;
	last_time = n;
	// Search tree
	pcl::search::KdTree<PointType>::Ptr search_tree(new pcl::search::KdTree<PointType>);
	search_tree->setInputCloud(cloud_outliers);			// Kd Tree Data Structure
	std::vector<pcl::PointIndices> cluster_indices;		// Extraxt indices here
														// Extract the indices of the cloud
	std::vector<pcl::PointCloud<PointType>::Ptr> cloud_cluster_vector;
	// Cluster algorithm

	// Eucledian
	if (1)
	{
		// Eucledian
		pcl::EuclideanClusterExtraction<PointType> ec;
		ec.setClusterTolerance(2); // 2cm
		ec.setMinClusterSize(1);
		ec.setMaxClusterSize(100);
		ec.setSearchMethod(search_tree);
		ec.setInputCloud(cloud_outliers);
		ec.extract(cluster_indices);
	}

	//---------------------------------------------------------------------------------------------------------------------------------------

	/* BOUNDING BOX */

	if (1)
	{


		// (new pcl::PointCloud<PointType>);
		int j = 0;

		for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin(); it != cluster_indices.end(); ++it)
		{
			if (j > 29)
			{
				break;
			}

			pcl::PointCloud<PointType>::Ptr cloud_cluster(new pcl::PointCloud<PointType>);
			for (std::vector<int>::const_iterator pit = it->indices.begin(); pit != it->indices.end(); ++pit)
				cloud_cluster->points.push_back(cloud_outliers->points[*pit]); //*
			cloud_cluster->width = cloud_cluster->points.size();
			cloud_cluster->height = 1;
			cloud_cluster->is_dense = true;

			cloud_cluster_vector.push_back(cloud_cluster);
			j++;
		}

		// Bounding box creation
		for (int i = 0; i < cloud_cluster_vector.size(); i++)
		{

			/*+++++++++++++++
			* Outliner removal in each cluster
			* +++++++++++++++++*/

			std::stringstream ss1, cluster, ss2;

			/**+++++++++++++++
			* Bounidng box generation
			* ++++++++++++++++++*/

			pcl::MomentOfInertiaEstimation <PointType> feature_extractor;
			feature_extractor.setInputCloud(cloud_cluster_vector[i]);
			feature_extractor.compute();

			pcl::PointXYZI min_point_AABB;
			pcl::PointXYZI max_point_AABB;
			feature_extractor.getAABB(min_point_AABB, max_point_AABB);
			ss1 << "line_1" << i;
			ss2 << "s2" << i;

			float cluster_x = 0, cluster_y = 0, cluster_angle = 0;
			uint32_t time_cluster = 0;
			cluster_x = (max_point_AABB.x + min_point_AABB.x) / 2;
			cluster_y = (max_point_AABB.y + min_point_AABB.y) / 2;
			float distance;
			distance = sqrt(pow(cluster_x, 2.0) + pow(cluster_y, 2.0));
			if (distance < 0.8)
			{
				continue;
			}
			float length = std::sqrt((max_point_AABB.x - min_point_AABB.x)*(max_point_AABB.x - min_point_AABB.x));
			float width = std::sqrt((max_point_AABB.y - min_point_AABB.y)*(max_point_AABB.y - min_point_AABB.y));
			if (length < 0.9 && width < 0.9)
			{

				meas_snd[i].x = cluster_x * cos(-3.14159) + cluster_y * sin(-3.14159);
				meas_snd[i].y = cluster_y * cos(-3.14159) - cluster_x * sin(-3.14159);
				meas_snd[i].max = max_point_AABB;
				meas_snd[i].min = min_point_AABB;
				cluster_angle = std::atan2(cluster_x, cluster_y);
				if (cluster_angle < 0.0)
				{
					cluster_angle += 6.28319;
				}
				meas_snd[i].azimuth = cluster_angle;
				uint32_t ut = calculate_utc(n, time_s, cluster_angle);
				meas_snd[i].utc = ut;

				//time_cluster = calculate_utc(n, time_gap, meas_snd[i].azimuth);
				send_measurement = true;
			}
		}
	}
}
void vector_send(bool send)
{
	if (send)
		matlabsend.push_back(meas_snd);
}
//---------------------------------------------------------------------------------------------------------------------------------------------------

/* MAIN FUNCTION */

int main(int argc, char *argv[])
{
	/* COMMAND LINE ARGPARSE */

	if (pcl::console::find_switch(argc, argv, "-help")) {
		std::cout << "usage: " << argv[0]
			<< " [-ipaddress <192.168.1.70>]"
			<< " [-port <2368>]"
			<< " [-pcap <*.pcap>]"
			<< " [-clbr <*.xml>]"
			<< " [-help]"
			<< std::endl;
		return 0;
	}

	// Default values
	std::string ipaddress("192.168.1.112");
	std::string port("2368");
	std::string pcap("All3Static2.pcap");
	std::string clbr("HDL-32.xml");
	int shae = 0;
	pcl::console::parse_argument(argc, argv, "-ipaddress", ipaddress);
	pcl::console::parse_argument(argc, argv, "-port", port);
	pcl::console::parse_argument(argc, argv, "-pcap", pcap);
	pcl::console::parse_argument(argc, argv, "-clbr", clbr);

	std::cout << "-ipadress : " << ipaddress << std::endl;
	std::cout << "-port : " << port << std::endl;
	std::cout << "-pcap : " << pcap << std::endl;
	std::cout << "-clbr : " << clbr << std::endl;

	//-----------------------------------------------------------------------------------------------------------------------------------------------

	/* VARIABLE DECLARATIONS */



	// Trial parameters
	bool downsample = false,
		eucledian = true,
		region_growing = false,
		bounding = true,
		bounding_alt = false,
		debug = false;

	// Point Cloud
	pcl::PointCloud<PointType>::ConstPtr cloud;

	

	

	// Search tree 
	pcl::search::KdTree<PointType>::Ptr search_tree(new pcl::search::KdTree<PointType>);

	//-----------------------------------------------------------------------------------------------------------------------------------------------

	/* OUTPUT VIEWER WINDOW SETUP */

	// PCL Visualizer
	boost::shared_ptr<pcl::visualization::PCLVisualizer> viewer(new pcl::visualization::PCLVisualizer("Velodyne Viewer"));
	viewer->addCoordinateSystem(3.0, "coordinate");
	viewer->setBackgroundColor(0.0, 0.0, 0.0, 0);
	viewer->initCameraParameters();
	viewer->setCameraPosition(0.0, 0.0, 30.0, 0.0, 1.0, 0.0, 0);

	// Point Cloud Color handler
	pcl::visualization::PointCloudColorHandler<PointType>::Ptr handler;

	const std::type_info& type = typeid(PointType);
	if (type == typeid(pcl::PointXYZ)) {
		std::vector<double> color = { 0, 255.0, 0 };
		boost::shared_ptr<pcl::visualization::PointCloudColorHandlerCustom<PointType>> color_handler(new pcl::visualization::PointCloudColorHandlerCustom<PointType>(color[0], color[1], color[2]));
		handler = color_handler;
	}
	else if (type == typeid(pcl::PointXYZI)) {
		std::vector<double> color = { 255.0, 255.0, 255.0 };
		boost::shared_ptr<pcl::visualization::PointCloudColorHandlerCustom<PointType>> color_handler(new pcl::visualization::PointCloudColorHandlerCustom<PointType>(color[0], color[1], color[2]));
		handler = color_handler;
	}
	else if (type == typeid(pcl::PointXYZRGBA)) {
		boost::shared_ptr<pcl::visualization::PointCloudColorHandlerRGBField<PointType>> color_handler(new pcl::visualization::PointCloudColorHandlerRGBField<PointType>());
		handler = color_handler;
	}
	else {
		throw std::runtime_error("This PointType is unsupported.");
	}

	//-----------------------------------------------------------------------------------------------------------------------------------------------

	/* POINT CLOUD EXTRACTION FROM PCAP FILE */

	// Retrieved Point Cloud Callback Function
	boost::mutex mutex;
	boost::function<void(const pcl::PointCloud<PointType>::ConstPtr&)> function =
		[&cloud, &mutex](const pcl::PointCloud<PointType>::ConstPtr& ptr) {
		boost::mutex::scoped_lock lock(mutex);
		uint64_t time_frame = ptr->header.stamp;
		uint32_t n_t = static_cast<uint32_t>(time_frame);
		
		// Point Cloud Processing
		vector_clear();
		
		pass_inte(ptr);
		segmentcloud();
		cluster(n_t);
		vector_send(send_measurement);
		write();
		// Point Cloud Processing
		cloud = ptr;
	};


	// HDL Grabber
	boost::shared_ptr<pcl::HDLGrabber> grabber;
	if (!pcap.empty()) {
		std::cout << "Capture from PCAP..." << std::endl;
		grabber = boost::shared_ptr<pcl::HDLGrabber>(new pcl::HDLGrabber(clbr, pcap));
	}
	else if (!ipaddress.empty() && !port.empty()) {
		std::cout << "Capture from Sensor..." << std::endl;
		grabber = boost::shared_ptr<pcl::HDLGrabber>(new pcl::HDLGrabber(boost::asio::ip::address::from_string(ipaddress), boost::lexical_cast<unsigned short>(port)));

	}
	// Register Callback Function
	boost::signals2::connection connection = grabber->registerCallback(function);

	// Start Grabber
	grabber->start();
	std::stringstream s;
	//-----------------------------------------------------------------------------------------------------------------------------------------------
	while (!viewer->wasStopped())
	{
		
		
		viewer->spinOnce(); // Update Viewer
		boost::mutex::scoped_try_lock lock(mutex);
		unsigned int lastangle = grabber->last_azimuth_;
		if (lock.owns_lock() && cloud)
		{
			handler->setInputCloud(cloud_outliers);
			viewer->removeAllShapes();
			/*Get time stamp data*/
			
			//---------------------------------------------------------------------------------------------------------------------------------------

			for (int i = 0; i < 30; i++)
			{
				if (meas_snd[i].x != 0)
				{
					viewer->addCube(meas_snd[i].min.x, meas_snd[i].max.x, meas_snd[i].min.y, meas_snd[i].max.y, meas_snd[i].min.z, meas_snd[i].max.z, 1.0, 1.0, 1.0, s.str());
				
				s << "l" << shae;
				shae = shae + 1;
				cout << meas_snd[i].utc << endl;
				}
			}
			viewer->setRepresentationToWireframeForAllActors();
			if ((!viewer->updatePointCloud(cloud_outliers, *handler, "cloud outs")))
			{
				viewer->addPointCloud(cloud_outliers, *handler, "cloud outs");

			}

		}
		
	}

	//-----------------------------------------------------------------------------------------------------------------------------------------------

	/* WINDING THINGS DOWN */

	// Stop Grabber
	grabber->stop();

	// Disconnect Callback Function
	if (connection.connected()) {
		connection.disconnect();
	}

	return 0;
}





