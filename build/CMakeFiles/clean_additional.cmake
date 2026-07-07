# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  [[CMakeFiles\ZenPlayer_autogen.dir\AutogenUsed.txt]]
  [[CMakeFiles\ZenPlayer_autogen.dir\ParseCache.txt]]
  "ZenPlayer_autogen"
  )
endif()
