cmake_minimum_required( VERSION 3.2 FATAL_ERROR )
project( NANCONVERT CXX )
set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED on )
aux_source_directory(. SRC_LIST )
include( CheckCXXCompilerFlag )
include( CMakeToolsHelpers OPTIONAL )

find_package( ITK 4.10.0 REQUIRED )
include( ${ITK_USE_FILE} )
set( Args_DIR "${PROJECT_SOURCE_DIR}/External/args"
     CACHE PATH "Path to Args library (usually External/args" )
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

add_executable( nanconvert_bruker Source/nanconvert_bruker.cpp Source/IO.cpp )
add_dependencies( nanconvert_bruker nan_version )
target_link_libraries( nanconvert_bruker ${ITK_LIBRARIES} )
install( TARGETS nanconvert_bruker RUNTIME DESTINATION bin )

add_executable( nanconvert_dicom Source/nanconvert_dicom.cpp Source/IO.cpp )
add_dependencies( nanconvert_dicom nan_version )
target_link_libraries( nanconvert_dicom ${ITK_LIBRARIES} )
install( TARGETS nanconvert_dicom RUNTIME DESTINATION bin )

set( SCRIPTS_DIR Scripts )
set( SCRIPTS nanbruker nanbruker_sge.qsub )
foreach( SCRIPT ${SCRIPTS} )
    INSTALL( FILES ${SCRIPTS_DIR}/${SCRIPT} 
             DESTINATION bin 
             PERMISSIONS OWNER_READ GROUP_READ WORLD_READ
                         OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE )
endforeach( SCRIPT )