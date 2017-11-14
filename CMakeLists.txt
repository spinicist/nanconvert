cmake_minimum_required( VERSION 3.2 FATAL_ERROR )
project( NANCONVERT CXX )
set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED on )
aux_source_directory(. SRC_LIST )
include( CheckCXXCompilerFlag )
include( CMakeToolsHelpers OPTIONAL )

find_package( ITK 4.10.0 REQUIRED )
include( ${ITK_USE_FILE} )
set( Args_DIR "${PROJECT_SOURCE_DIR}/External/Args"
     CACHE PATH "Path to Args library (usually External/Args" )
set( SRC_DIR "${PROJECT_SOURCE_DIR}/Source" )
set( CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMake" )
include_directories( "${SRC_DIR}" "${Args_DIR}" )
# Setup version
include_directories( ${CMAKE_CURRENT_BINARY_DIR} ) # For version file
set( VERSION_FILE_NAME "VersionFile" )
add_custom_target( nan_version ALL DEPENDS version_depend_dummy )
add_custom_command( OUTPUT version_depend_dummy
                    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_MODULE_PATH}/GetVersion.cmake )
set_source_files_properties( ${CMAKE_CURRENT_BINARY_DIR}/${VERSION_FILE_NAME} PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE )

add_executable( nan_bruker Source/nan_bruker.cpp )
add_dependencies( nan_bruker nan_version )
target_link_libraries( nan_bruker ${ITK_LIBRARIES} )

set( PROGRAMS ${UTILS} ${QUANT} ${ELLIPSE} )
install( TARGETS ${PROGRAMS} RUNTIME DESTINATION bin )