#----------------------------------------------------------------------------
# Setup the project
cmake_minimum_required(VERSION 3.8...3.18)
if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()
project(analysis)

set(CMAKE_BUILD_TYPE Release)

#----------------------------------------------------------------------------
# Find ROOT (required package)
#
find_package(ROOT REQUIRED COMPONENTS Geom EG RIO) 
include(${ROOT_USE_FILE})
message(STATUS "Set ROOT : ${ROOT_USE_FILE}")
message(STATUS "ROOT : ${ROOT_LIBRARIES}")
#----------------------------------------------------------------------------
# Find nlohmann_json (required package)
find_package( nlohmann_json )

#----------------------------------------------------------------------------
# Locate sources and headers for this project
#
include_directories(${PROJECT_SOURCE_DIR}/include 
                    ${ROOT_INCLUDE_DIR}
                    )

file(GLOB_RECURSE sources ${PROJECT_SOURCE_DIR}/source/*.cxx)
file(GLOB_RECURSE headers ${PROJECT_SOURCE_DIR}/include/*.h)
file(GLOB_RECURSE configs RELATIVE ${PROJECT_SOURCE_DIR} config/*.json)
file(GLOB_RECURSE definitions ${PROJECT_SOURCE_DIR}/include/RDFDefines.h)

#----------------------------------------------------------------------------
# Copy all configs to the build directory, i.e. the directory in which we
# build the analysis. This is so that we can run the executable directly because it
# relies on these scripts being in the current working directory.
#
# Enable config for out-of-source build
foreach(_file ${configs})
  configure_file(
    ${_file}
    ${PROJECT_BINARY_DIR}/${_file}
    COPYONLY
    )
    #message(STATUS "Copying ${_file} in ${PROJECT_BINARY_DIR}/${_file}")
endforeach()


#----------------------------------------------------------------------------
# Add the executable, and link it to the Geant4 libraries
#
add_executable(analysis analysis.cxx ${sources})

target_compile_definitions(analysis PRIVATE
    PROJECT_INCLUDE_DIR="${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(analysis
                      nlohmann_json::nlohmann_json
                      ${ROOT_LIBRARIES}
                      )

target_compile_features(analysis PRIVATE cxx_std_17)
#----------------------------------------------------------------------------
# Install the executable to 'bin' directory under CMAKE_INSTALL_PREFIX
#
install(TARGETS analysis DESTINATION bin)