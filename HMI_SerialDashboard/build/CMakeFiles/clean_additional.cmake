# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\HMI_SerialDashboard_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\HMI_SerialDashboard_autogen.dir\\ParseCache.txt"
  "HMI_SerialDashboard_autogen"
  )
endif()
