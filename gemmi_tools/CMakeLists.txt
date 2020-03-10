# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("gemmi_tools")


# INCLUDES
# include_directories("${CMAKE_SOURCE_DIR}/include")
# include_directories("${CMAKE_SOURCE_DIR}/include/pybind11")
# include_directories("${CMAKE_SOURCE_DIR}/include/gemmi")


# Include sub-projects.
# add_subdirectory ("gemmi_tools")
add_subdirectory ("include/gemmi")
add_subdirectory ("include/pybind11")



# PYBIND MODULE
pybind11_add_module(gemmi_tools_python python/sample.cpp)

if (NOT DEFINED PYTHON_INSTALL_DIR)
  # set(PYTHON_INSTALL_DIR ${PYTHON_SITE_PACKAGES})
  set(PYTHON_INSTALL_DIR "${CMAKE_SOURCE_DIR}/install")
endif()

message("${PYTHON_INSTALL_DIR} ===python install ===dir.")


# INSTALL
install(TARGETS gemmi_tools_python DESTINATION ${PYTHON_INSTALL_DIR})