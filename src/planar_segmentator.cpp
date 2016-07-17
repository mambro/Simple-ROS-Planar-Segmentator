#include <ros/ros.h>
#include <ros/package.h>

// PCL specific includes

#include <pcl_conversions/pcl_conversions.h>
#include <sensor_msgs/PointCloud2.h>

#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/ModelCoefficients.h>
#include <pcl/io/pcd_io.h>
#include <pcl/sample_consensus/method_types.h>
#include <pcl/sample_consensus/model_types.h>
#include <pcl/segmentation/sac_segmentation.h>
#include <pcl/ModelCoefficients.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/features/normal_3d.h>
#include <pcl/surface/concave_hull.h>
#include <pcl/visualization/cloud_viewer.h>
#include <pcl/visualization/pcl_visualizer.h>

#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>

// ROS Msgs and Srvs autogenerated headers
#include <simple_planar_segmentator/Plane.h>
#include <simple_planar_segmentator/PlaneDetection.h>


class PlanarSegmentator
{
public:
    PlanarSegmentator():
        nh_("~"),
        cloud_src_(new pcl::PointCloud<pcl::PointXYZRGB>),
        cloud_p_(new pcl::PointCloud<pcl::PointXYZRGB>),
        cloud_hull_ (new pcl::PointCloud<pcl::PointXYZRGB>),
        cloud_segmented_ (new pcl::PointCloud<pcl::PointXYZRGB>),
        savings_(0)
        //pcl_viewer_ (new pcl::visualization::PCLVisualizer ("3D Viewer"))

    {
        if (!start())
        {
           std::cerr<<"Planar Segmentator not initialized"<<std::endl;
        }
        else
        {
        std::cout <<"Planar Segmentator ready to run" << std::endl;
        }      
    }

    boost::shared_ptr<pcl::visualization::PCLVisualizer> pcl_viewer_;

private:

    // Clouds
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_src_;
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_p_, cloud_segmented_ ;
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_hull_;
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_filtered2;

    pcl::ConcaveHull<pcl::PointXYZRGB> chull_;


    // PCL Viewer
    bool refreshUI = true;
    int l_count = 0;

    // ROS Stuff
    ros::NodeHandle nh_;
    ros::Subscriber sub_;
    ros::Publisher pub_;
    ros::Publisher pub2_;
    ros::ServiceServer service_;

    // Flags
    bool filtering_pointcloud_;
    bool debug_;
    bool use_pclviewer_;

    std::string cloud_input_topic_;

    int savings_;

    bool start()
    {
        // Node params
        nh_.param("filtering_pointcloud", filtering_pointcloud_, false);
        nh_.param("debug_", debug_, false);
        nh_.param("use_pclviewer",use_pclviewer_,false);
        nh_.param<std::string>("cloud_input_topic", cloud_input_topic_, "");

        // Create a ROS subscriber for the input point cloud
        sub_ = nh_.subscribe (cloud_input_topic_, 1, &PlanarSegmentator::cloudCb, this);
        service_ = nh_.advertiseService("PlaneDetection", &PlanarSegmentator::planeDetection, this);


        // Create a ROS publisher for the output point cloud
        pub_ = nh_.advertise<sensor_msgs::PointCloud2> ("extracted_planes", 1);
        return true;
    }

    // Callback Function for the subscribed ROS topic
    void cloudCb (const pcl::PCLPointCloud2ConstPtr& input)
    {
        // Convert the sensor_msgs/PointCloud2 data to pcl/PointCloud
        pcl::fromPCLPointCloud2(*input,*cloud_src_);
    }  



    bool planeDetection(simple_planar_segmentator::PlaneDetection::Request & req,
                      simple_planar_segmentator::PlaneDetection::Response & res)
    {
        // Create the filtering object: downsample the dataset using a leaf size of 0.5cm
        if (filtering_pointcloud_)
        {
            /* TODO
             pcl::VoxelGrid<pcl::PCLPointCloud2> sor;
            sor.setInputCloud (cloud_src_);
            sor.setLeafSize (0.005f, 0.005f, 0.005f);
            pcl::PCLPointCloud2 cloud_filtered;
            sor.filter (cloud_filtered);
            pcl::fromPCLPointCloud2(cloud_filtered,*cloud_filtered2);
            */
        }

        if (cloud_src_->points.size() == 0)
        {
          std::cerr <<"Empty point cloud"<< std::endl;  
        }

        // Output Planar segmented cloud
        if (debug_)
        {
            std::cout << "points: " << cloud_src_->points.size () << std::endl;
        }

        /*cloud_src->sensor_orientation_.w() = 0.0;
        cloud_src->sensor_orientation_.x() = 1.0;
        cloud_src->sensor_orientation_.y() = 0.0;
        cloud_src->sensor_orientation_.z() = 0.0;
        */

        // Create the segmentation planar object
        pcl::ModelCoefficients::Ptr coefficients (new pcl::ModelCoefficients ());
        pcl::PointIndices::Ptr inliers (new pcl::PointIndices ());
        pcl::SACSegmentation<pcl::PointXYZRGB> seg;

        // Apply planar segmentation object
        seg.setOptimizeCoefficients(true);
        seg.setModelType(pcl::SACMODEL_PLANE);
        seg.setMethodType(pcl::SAC_RANSAC);
        seg.setMaxIterations(1000);
        seg.setDistanceThreshold(0.01);
        seg.setInputCloud(cloud_src_);
        seg.segment (*inliers, *coefficients);

        if (inliers->indices.size() == 0)
        {
            std::cerr << "Could not estimate a planar model for the given dataset." << std::endl;
            // return una stringaccia di errore
            //return -1;
        }

        if(debug_)
        {
            std::cout << "Model coefficients: " << coefficients->values[0] << " "
                                              << coefficients->values[1] << " "
                                              << coefficients->values[2] << " "
                                              << coefficients->values[3] << std::endl;

            std::cout << "Model inliers: " << inliers->indices.size () << std::endl;
        }

        // Create the filtering object
        pcl::ExtractIndices<pcl::PointXYZRGB> extract;

        extract.setInputCloud(cloud_src_);
        extract.setIndices (inliers);
        extract.setNegative (false);
        extract.filter (*cloud_p_);
        std::cout << "PointCloud representing the planar component: " << cloud_p_->width * cloud_p_->height << " data points." << std::endl;

        // Project the model inliers

        // Create a Concave Hull representation of the projected inliers
        chull_.setInputCloud (cloud_p_);
        chull_.setAlpha (0.1);
        chull_.reconstruct (*cloud_hull_);

        //cloud_hull_->sensor_orientation_.w() = 0.0;
        //cloud_hull_->sensor_orientation_.x() = 1.0;
        //cloud_hull_->sensor_orientation_.y() = 0.0;
        //cloud_hull_->sensor_orientation_.z() = 0.0;

        if(debug_)
        {
            std::cerr << "Concave hull has: " << cloud_hull_->points.size ()
            << " data points." << std::endl;
        }


        std::string file_name = "plane_"+std::to_string(savings_)+".yaml";
        std::string config_path = ros::package::getPath("simple_planar_segmentator")+"/data/"+file_name;
        std::ofstream ofs(config_path.c_str());
        cv::FileStorage fs(config_path, cv::FileStorage::WRITE);

        fs << "plane_coefficients";
        fs << "[" << coefficients->values[0] << coefficients->values[1]
               << coefficients->values[2] << coefficients->values[3] << "]" ;
        fs.release();                                       // explicit close
        
        for (size_t i = 0; i < cloud_p_->points.size(); i++)
        {
            cloud_p_->points[i].r = 255;
            cloud_p_->points[i].g = 0;
            cloud_p_->points[i].b = 0;
        }

        *cloud_segmented_ = *cloud_src_ + *cloud_p_; //+ *cloud_hull_;
        *cloud_segmented_ = *cloud_segmented_ + *cloud_hull_;

        pcl::PCDWriter writer;
        std::string pcd_filename = "plane_"+std::to_string(savings_)+".pcd";
        std::string pcd_path = ros::package::getPath("simple_planar_segmentator")+"/data/"+pcd_filename;
        writer.write (pcd_path, *cloud_p_, false);


        simple_planar_segmentator::Plane plane_tmp;

        std::vector<simple_planar_segmentator::Plane> planes;
        plane_tmp.a = coefficients->values[0];
        plane_tmp.b = coefficients->values[1];
        plane_tmp.c = coefficients->values[2];
        plane_tmp.d = coefficients->values[3];

        planes.push_back(plane_tmp);

        res.plane = planes;
        res.result = "Write Done. Yaml file and pcd created in the package folder";

        // Publish the model coefficients
        pcl::PCLPointCloud2 segmented_pcl;
        pcl::toPCLPointCloud2(*cloud_segmented_,segmented_pcl);
        sensor_msgs::PointCloud2 segmented;
        pcl_conversions::fromPCL(segmented_pcl,segmented);
        pub_.publish (segmented);

        savings_++;

        //Update PCL Viewer
        if (use_pclviewer_)
        {
            //printToPCLViewer();
        }

        return true;
    }

    /*void printToPCLViewer()
    {
        pcl_viewer_->setBackgroundColor (0.0, 0.0, 0.0);
        pcl_viewer_->removeAllPointClouds();
        pcl::visualization::PointCloudColorHandlerRGBField<pcl::PointXYZRGB> rgb(cloud_src_);
        pcl_viewer_->addPointCloud<pcl::PointXYZRGB>(cloud_src_,rgb,"source cloud");
        pcl_viewer_->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "source cloud");
        pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZRGB> red_color(cloud_p_, 255, 0, 0);
        pcl_viewer_->addPointCloud<pcl::PointXYZRGB>(cloud_p_,red_color,"segmented cloud");
        pcl_viewer_->setPointCloudRenderingProperties (pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 1, "segmented cloud");

       // pcl::visualization::PointCloudColorHandlerCustom<pcl::PointXYZRGB> green_color(cloud_hull_, 0, 255, 0);
       // viewer.addPointCloud<pcl::PointXYZRGB>(cloud_hull_, green_color, "cloud hull");

        pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud_p_rgb (new pcl::PointCloud<pcl::PointXYZRGB>);

        
      
        //pcl::copyPointCloud(*cloud_p, *cloud_p_rgb);
    }
    */
};


int
main (int argc, char** argv)
{
    // Initialize ROS
    ros::init (argc, argv, "simple_planar_segmentator");
    ros::NodeHandle n;
    PlanarSegmentator plane_segmentator;
    ros::Rate r(30);
    while (ros::ok()) // && plane_segmentator.pcl_viewer_->wasStopped())
    {
        ros::spinOnce();
        //plane_segmentator.pcl_viewer_->spinOnce (100);
        r.sleep();
    }
    return 0;
}
