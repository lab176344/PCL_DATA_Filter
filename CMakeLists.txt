cmake_minimum_required(VERSION 2.8 FATAL_ERROR)

project(PCL_DATA)

find_package(PCL 1.2 REQUIRED)

include_directories(${PCL_INCLUDE_DIRS})
link_directories(${PCL_LIBRARY_DIRS})
add_definitions(${PCL_DEFINITIONS})

# Find Package PCAP
set( CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" ${CMAKE_MODULE_PATH} )
set( PCAP_DIR "C:/Program Files/WpdPack" )
find_package( PCAP )

find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

# If Capture from PCAP Files, VelodyneCapture are required PCAP.
# Please Define HAVE_PCAP in Preprocessor.
set( HAVE_PCAP )
if( PCAP_FOUND )
  set( HAVE_PCAP "-DHAVE_PCAP" )
endif()

add_executable (PCL_DATA pcl_data_processing.cpp)
target_link_libraries (PCL_DATA  ${PCL_COMMON_LIBRARIES} ${PCL_IO_LIBRARIES} ${PCL_LIBRARIES})
target_link_libraries( PCL_DATA ${OpenCV_LIBS} )
# Precompile for a minimal set of point types instead of all.
option(PCL_NO_PRECOMPILE "Do not precompile PCL code for any point types at all." OFF)
mark_as_advanced(PCL_NO_PRECOMPILE)







