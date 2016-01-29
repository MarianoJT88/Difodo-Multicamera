PROJECT(Odometry-Multicamera)

CMAKE_MINIMUM_REQUIRED(VERSION 2.4)
if(COMMAND cmake_policy)
      cmake_policy(SET CMP0003 NEW)  # Required by CMake 2.7+
endif(COMMAND cmake_policy)


FIND_PACKAGE(MRPT REQUIRED base gui opengl slam maps obs)
#FIND_PACKAGE(OpenCV REQUIRED )
#FIND_PACKAGE(PCL REQUIRED) 

#INCLUDE_DIRECTORIES(${PCL_INCLUDE_DIRS})
#LINK_DIRECTORIES(${PCL_LIBRARY_DIRS})
#ADD_DEFINITIONS(${PCL_DEFINITIONS})
	
		
ADD_EXECUTABLE(Difodo-Multi-Datasets  
	main_Difodo_multi_datasets.cpp
	difodo_multi_datasets.cpp
	difodo_multi_datasets.h
	CDifodo_multi.h
	CDifodo_multi.cpp
	)
	
TARGET_LINK_LIBRARIES(Difodo-Multi-Datasets
		${MRPT_LIBS}
		#${PCL_LIBRARIES} 
		#${OpenCV_LIBS}
)		

 
# Set optimized building:
IF(CMAKE_COMPILER_IS_GNUCXX AND NOT CMAKE_BUILD_TYPE MATCHES "Debug")
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -mtune=native")
ENDIF(CMAKE_COMPILER_IS_GNUCXX AND NOT CMAKE_BUILD_TYPE MATCHES "Debug")

