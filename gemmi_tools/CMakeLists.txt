# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("gemmi_tools")

set(PYTHON_INSTALL_PATH ${PYTHON_SITE_PACKAGES})


# INCLUDES
# include_directories("${CMAKE_SOURCE_DIR}/include")
# include_directories("${CMAKE_SOURCE_DIR}/include/pybind11")
# include_directories("${CMAKE_SOURCE_DIR}/include/gemmi")


# Include sub-projects.
# add_subdirectory ("gemmi_tools")
include_directories ("include")

# PYBIND MODULE
find_package(pybind11)
pybind11_add_module(gemmi_tools_python python/sample.cpp)

# target_link_libraries(gemmi_tools_python PUBLIC /dls/science/groups/i04-1/conor_dev/gemmi/libgemmi_lib.a)
# SET_TARGET_PROPERTIES( gemmi_tools_python
# 	PROPERTIES
#     PREFIX ""
#     LIBRARY_OUTPUT_DIRECTORY "${PYTHON_SITE_PACKAGES}"
# )


if (NOT DEFINED PYTHON_INSTALL_DIR)
  set(PYTHON_INSTALL_DIR ${PYTHON_SITE_PACKAGES})
  # set(PYTHON_INSTALL_DIR "${CMAKE_SOURCE_DIR}/install")
endif()

message("${PYTHON_INSTALL_DIR} ===python install ===dir.")


# INSTALL
install(TARGETS gemmi_tools_python DESTINATION ${PYTHON_INSTALL_DIR})