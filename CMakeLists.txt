cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
cmake_policy(VERSION 3.12)

project(NANCONVERT CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMake)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${PROJECT_SOURCE_DIR}/CMake)
include(CheckCXXCompilerFlag)
include(CMakeToolsHelpers OPTIONAL)
include(CMakePrintHelpers)

include(${PROJECT_SOURCE_DIR}/cmake/BuildType.cmake)

find_package(args CONFIG REQUIRED)
find_package(fmt REQUIRED CONFIG)
find_package(ITK 5.0.0
              COMPONENTS
                ITKCommon
                ITKIOImageBase
                ITKIOBruker
                ITKIOGDCM
                ITKIONIFTI
                ITKImageCompose
                ITKImageGrid
              REQUIRED)
include(${ITK_USE_FILE})

# Main Library
set(SRC_DIR "${PROJECT_SOURCE_DIR}/Source")
add_library(Convert STATIC ${SRC_DIR}/IO.cpp ${SRC_DIR}/Util.cpp)
target_link_libraries(Convert ${ITK_LIBRARIES})

add_executable(nanconvert_bruker ${SRC_DIR}/nanconvert_bruker.cpp)
target_link_libraries(nanconvert_bruker
  Convert
  taywee::args
  fmt::fmt
  ${ITK_LIBRARIES}
)
install(TARGETS nanconvert_bruker RUNTIME DESTINATION bin)

add_executable(nanconvert_dicom ${SRC_DIR}/nanconvert_dicom.cpp)
target_link_libraries(nanconvert_dicom 
  Convert
  taywee::args
  fmt::fmt
  ${ITK_LIBRARIES}
)
install(TARGETS nanconvert_dicom RUNTIME DESTINATION bin)

set(SCRIPTS_DIR Scripts)
set(SCRIPTS nanbruker nanbruker_sge.qsub nandicom nandicom_sge.qsub)
foreach(SCRIPT ${SCRIPTS})
    INSTALL(FILES ${SCRIPTS_DIR}/${SCRIPT} 
             DESTINATION bin 
             PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                         GROUP_READ GROUP_EXECUTE
                         WORLD_READ WORLD_EXECUTE)
endforeach(SCRIPT)