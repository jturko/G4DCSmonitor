#----------------------------------------------------------------------------
# Setup the project
cmake_minimum_required(VERSION 3.16...3.27)
project(G4DCSmonitor)

#----------------------------------------------------------------------------
# Find Geant4 package, activating all available UI and Vis drivers by default
# You can set WITH_GEANT4_UIVIS to OFF via the command line or ccmake/cmake-gui
# to build a batch mode only executable
#
option(WITH_GEANT4_UIVIS "Build example with Geant4 UI and Vis drivers" ON)
if(WITH_GEANT4_UIVIS)
  find_package(Geant4 REQUIRED ui_all vis_all)
else()
  find_package(Geant4 REQUIRED)
endif()

#----------------------------------------------------------------------------
# Setup Geant4 include directories and compile definitions
#
include(${Geant4_USE_FILE})







#----------------------------------------------------------------------------
# Find ROOT (required package)
#
# You need to tell CMake where to find the ROOT installation. This can be done in a number of ways:
#   - ROOT built with classic configure/make use the provided $ROOTSYS/etc/cmake/FindROOT.cmake
#   - ROOT built with CMake. Add in CMAKE_PREFIX_PATH the installation prefix for ROOT
list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})

#---Locate the ROOT package and defines a number of variables (e.g. ROOT_INCLUDE_DIRS)
find_package(ROOT REQUIRED COMPONENTS RIO Net)

# ROOT version 6 required
if(ROOT_FOUND)
  STRING(REGEX MATCH "6.*" VERSION6MATCH ${ROOT_VERSION})
  if(NOT VERSION6MATCH)
    message(FATAL_ERROR "G4DCSmonitor requires ROOT 6")
  endif()
    message("-- Found Root-Version: " ${ROOT_VERSION})
endif()

#---Define useful ROOT functions and macros (e.g. ROOT_GENERATE_DICTIONARY)
include(${ROOT_USE_FILE})

message("-- ROOT_USE_FILE: " ${ROOT_USE_FILE})
message("-- ROOT_INCLUDE_DIRS: " ${ROOT_INCLUDE_DIRS})
message("-- GEANT4_INCLUDE_DIR: " ${Geant4_INCLUDE_DIR})

EXECUTE_PROCESS(COMMAND root-config --cflags OUTPUT_VARIABLE ROOT_CXX_FLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ROOT_CXX_FLAGS}")

#----------------------------------------------------------------------------








#----------------------------------------------------------------------------
# Locate sources and headers for this project
#
include_directories(${PROJECT_SOURCE_DIR}/include 
                    ${Geant4_INCLUDE_DIR}
                    ${ROOT_INCLUDE_DIRS})
file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cc)
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hh)

#----------------------------------------------------------------------------
# Add the executable, 
#
add_executable(G4DCSmonitor G4DCSmonitor.cc ${sources} ${headers})

#----------------------------------------------------------------------------
# Copy all scripts to the build directory, i.e. the directory in which we
# build G4DCSmonitor. This is so that we can run the executable directly because it
# relies on these scripts being in the current working directory.
#
set(G4DCSmonitor_SCRIPTS
    vis.mac vis_ogl.mac vis_vrml.mac run.mac
  )

foreach(_script ${G4DCSmonitor_SCRIPTS})
  configure_file(
    ${PROJECT_SOURCE_DIR}/macros/${_script}
    ${PROJECT_BINARY_DIR}/${_script}
    COPYONLY
    )
endforeach()

# link the libraries (geant4, root, eve)
target_link_libraries(G4DCSmonitor ${Geant4_LIBRARIES} ${ROOT_LIBRARIES} Eve)

#----------------------------------------------------------------------------
# Install the executable to 'bin' directory under CMAKE_INSTALL_PREFIX
#
install(TARGETS G4DCSmonitor DESTINATION bin)

