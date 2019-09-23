cmake_minimum_required( VERSION 3.2 FATAL_ERROR )

project( NANCONVERT CXX )
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED on )
add_compile_options("-Wall" "-Wextra" "-Wshadow" "-ftemplate-backtrace-limit=30")
aux_source_directory(. SRC_LIST )
include( CheckCXXCompilerFlag )
include( CMakeToolsHelpers OPTIONAL )
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/CMake)
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${PROJECT_SOURCE_DIR}/CMake)
include( ${PROJECT_SOURCE_DIR}/CMake/BuildType.cmake )

set( External_Include_DIR "${PROJECT_SOURCE_DIR}/include"
     CACHE PATH "Path to vendored includes (usually /include" )
include_directories( SYSTEM ${External_Include_DIR} )

# fmtlib: Sane output
find_package(fmtlib REQUIRED CONFIG)
if(fmtlib_FOUND)
    add_library(fmtlib STATIC ${FMTLIB_SOURCES})
    target_include_directories(fmtlib SYSTEM PUBLIC ${FMTLIB_INCLUDE_DIR})
endif()

find_package(ITK 5.0.0
              COMPONENTS
                ITKCommon
                ITKIOGDCM
                ITKIOImageBase
                ITKImageCompose
                ITKImageGrid
                ITKImageIO
              REQUIRED )
include( ${ITK_USE_FILE} )

# Main Library
set( SRC_DIR "${PROJECT_SOURCE_DIR}/Source" )
add_library( Convert STATIC ${SRC_DIR}/IO.cpp ${SRC_DIR}/Util.cpp )

add_executable( nanconvert_bruker ${SRC_DIR}/nanconvert_bruker.cpp )
target_link_libraries( nanconvert_bruker Convert ${ITK_LIBRARIES} fmtlib )
install( TARGETS nanconvert_bruker RUNTIME DESTINATION bin )

add_executable( nanconvert_dicom ${SRC_DIR}/nanconvert_dicom.cpp )
target_link_libraries( nanconvert_dicom Convert ${ITK_LIBRARIES} fmtlib )
install( TARGETS nanconvert_dicom RUNTIME DESTINATION bin )

set( SCRIPTS_DIR Scripts )
set( SCRIPTS nanbruker nanbruker_sge.qsub nandicom )
foreach( SCRIPT ${SCRIPTS} )
    INSTALL( FILES ${SCRIPTS_DIR}/${SCRIPT} 
             DESTINATION bin 
             PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                         GROUP_READ GROUP_EXECUTE
                         WORLD_READ WORLD_EXECUTE )
endforeach( SCRIPT )