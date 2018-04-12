cmake_minimum_required( VERSION 3.2 FATAL_ERROR )
project( NANCONVERT CXX )
set( CMAKE_CXX_STANDARD 11 )
set( CMAKE_CXX_STANDARD_REQUIRED on )
aux_source_directory(. SRC_LIST )
include( CheckCXXCompilerFlag )
include( CMakeToolsHelpers OPTIONAL )

find_package( ITK 4.12.0 REQUIRED )
include( ${ITK_USE_FILE} )
set( Args_DIR "${PROJECT_SOURCE_DIR}/External/args"
     CACHE PATH "Path to Args library (usually External/args" )
set( SRC_DIR "${PROJECT_SOURCE_DIR}/Source" )
set( CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/CMake" )
include_directories( "${SRC_DIR}" "${Args_DIR}" )

# Version File
include_directories( ${CMAKE_CURRENT_BINARY_DIR} ) # For version file
set( VERSION_FILE_NAME "VersionFile" )
add_custom_target( nanconvert_version ALL DEPENDS version_depend_dummy )
add_custom_command( OUTPUT version_depend_dummy
                    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_MODULE_PATH}/GetVersion.cmake )
set_source_files_properties( ${CMAKE_CURRENT_BINARY_DIR}/${VERSION_FILE_NAME} PROPERTIES GENERATED TRUE HEADER_FILE_ONLY TRUE )

# Main Library
add_library( Convert STATIC ${SRC_DIR}/IO.cpp ${SRC_DIR}/Util.cpp )

add_executable( nanconvert_bruker ${SRC_DIR}/nanconvert_bruker.cpp )
add_dependencies( nanconvert_bruker nanconvert_version )
target_link_libraries( nanconvert_bruker Convert ${ITK_LIBRARIES} )
install( TARGETS nanconvert_bruker RUNTIME DESTINATION bin )

include_directories( "External/fmt/include" )
add_executable( nanconvert_dicom ${SRC_DIR}/nanconvert_dicom.cpp External/fmt/src/format.cc )
add_dependencies( nanconvert_dicom nanconvert_version )
target_link_libraries( nanconvert_dicom Convert ${ITK_LIBRARIES} )
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