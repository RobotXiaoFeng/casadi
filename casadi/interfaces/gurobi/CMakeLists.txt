cmake_minimum_required(VERSION 2.8.6)

include_directories(../)
include_directories(${GUROBI_INCLUDE_DIRS})

casadi_plugin(Qpsol gurobi
  gurobi_interface.hpp
  gurobi_interface.cpp
  gurobi_interface_meta.cpp)

casadi_plugin_link_libraries(Qpsol gurobi ${GUROBI_LIBRARIES})
