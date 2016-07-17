cmake_minimum_required(VERSION 2.8.3)
project(simple_planar_segmentator)

set(CMAKE_CXX_FLAGS   "${CMAKE_CXX_FLAGS} -std=c++11") 
set(CMAKE_CXX_FLAGS   "${CMAKE_CXX_FLAGS} -march=native")
set(CMAKE_BUILD_TYPE  Release)


find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWITH_PCL")

find_package(PCL 1.2 REQUIRED)
find_package(OpenCV     REQUIRED)

find_package(catkin REQUIRED COMPONENTS
  pcl_conversions
  pcl_ros
  roscpp
  rospy
  sensor_msgs
  std_msgs
  pcl_msgs
)

add_message_files(
 FILES
 Plane.msg
 )
 
 add_service_files(
         FILES
         PlaneDetection.srv
         )

 generate_messages( DEPENDENCIES std_msgs
 )



include_directories(${catkin_INCLUDE_DIRS} ${PCL_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
link_directories(${PCL_LIBRARY_DIRS})
add_definitions(${PCL_DEFINITIONS})

catkin_package()

add_executable (simple_planar_segmentator src/planar_segmentator.cpp)
add_dependencies(simple_planar_segmentator ${${PROJECT_NAME}_EXPORTED_TARGETS})
target_link_libraries (simple_planar_segmentator  ${catkin_LIBRARIES} ${PCL_LIBRARIES} ${OpenCV_LIBS})




