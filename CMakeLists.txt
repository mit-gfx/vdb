cmake_minimum_required(VERSION 2.8)
project(OpenFabExternal)

############################################################################
# Set up common build definitions and rules
############################################################################
set(OPEN_FAB_DIR "../..")
set(OPEN_FAB_BUILD_DIR "${OPEN_FAB_DIR}/OpenFabBuild")
include("${OPEN_FAB_BUILD_DIR}/src/build/CommonRules.cmake")

############################################################################
# Subdirs
############################################################################
add_subdirectory(lib)
