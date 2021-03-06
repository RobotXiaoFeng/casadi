cmake_minimum_required(VERSION 2.8.6)
include_directories(${IPOPT_INCLUDE_DIRS})
link_directories(${IPOPT_LIBRARY_DIRS})

set(NLPSOL_IPOPT_SRCS
  ipopt_interface.hpp
  ipopt_interface.cpp
  ipopt_nlp.hpp
  ipopt_nlp.cpp
  ipopt_interface_meta.cpp)
  
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
else()
  add_definitions(-DHAVE_CSTDDEF)
endif()

if(WITH_IPOPT_CALLBACK)
  add_definitions(-DWITH_IPOPT_CALLBACK)
endif()

if(IPOPT_FOUND)
  add_definitions(${IPOPT_CFLAGS_OTHER})
endif()

if(WITH_SIPOPT)
  add_definitions(-DWITH_SIPOPT)
endif()

casadi_plugin(Nlpsol ipopt ${NLPSOL_IPOPT_SRCS})
casadi_plugin_link_libraries(Nlpsol ipopt ${IPOPT_LIBRARIES})

set_property(TARGET casadi_nlpsol_ipopt PROPERTY PKG_CONFIG_REQUIRES ipopt)
