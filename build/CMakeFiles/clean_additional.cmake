# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  [[CMakeFiles\QtVsCodeApp_autogen.dir\AutogenUsed.txt]]
  [[CMakeFiles\QtVsCodeApp_autogen.dir\ParseCache.txt]]
  [[CMakeFiles\ZenPlayer_autogen.dir\AutogenUsed.txt]]
  [[CMakeFiles\ZenPlayer_autogen.dir\ParseCache.txt]]
  "QtVsCodeApp_autogen"
  "ZenPlayer_autogen"
  )
endif()
