AUTO_FILES("." "src" "\\.(cpp|h)$")
AUTO_FILES("${ROOT_SOURCE_DIR}/dep/libsundaowen" "src" "\\.(cpp|h)$")
include_directories(${DEP_INCLUDE_DIR})
link_directories(${DEP_LIBRARY_DIR})
add_definitions(-DSDW_MAIN)
if(MSVC)
  string(REPLACE "/MDd" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
  string(REPLACE "/MD" "" CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
  set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
  string(REPLACE "/MD" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  string(REPLACE "/MD" "" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
endif()
ADD_EXE(bflimtool "${src}")
if(WIN32)
  if(MSVC)
    target_link_libraries(bflimtool libpng16_static zlibstatic PVRTexLib)
    set_target_properties(bflimtool PROPERTIES LINK_FLAGS_DEBUG "/NODEFAULTLIB:LIBCMT")
  else()
    target_link_libraries(bflimtool png16 z)
  endif()
else()
  target_link_libraries(bflimtool png16 z PVRTexLib pthread)
  if(CYGWIN)
    target_link_libraries(bflimtool iconv)
  endif()
endif()
GET_CURRENT_DEP_LIBRARY_PREFIX("${ROOT_SOURCE_DIR}/dep/PVRTexTool/Library")
if(WIN32)
  add_custom_command(TARGET bflimtool POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ROOT_SOURCE_DIR}/dep/PVRTexTool/Library/${CURRENT_DEP_LIBRARY_PREFIX}/PVRTexLib.dll" $<TARGET_FILE_DIR:bflimtool>)
elseif(APPLE)
  add_custom_command(TARGET bflimtool POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ROOT_SOURCE_DIR}/dep/PVRTexTool/Library/${CURRENT_DEP_LIBRARY_PREFIX}/libPVRTexLib.dylib" $<TARGET_FILE_DIR:bflimtool> COMMAND install_name_tool -change @executable_path/../lib/libPVRTexLib.dylib @executable_path/libPVRTexLib.dylib $<TARGET_FILE:bflimtool>)
else()
  add_custom_command(TARGET bflimtool POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ROOT_SOURCE_DIR}/dep/PVRTexTool/Library/${CURRENT_DEP_LIBRARY_PREFIX}/libPVRTexLib.so" $<TARGET_FILE_DIR:bflimtool>)
endif()
install(TARGETS bflimtool DESTINATION bin)
if(WIN32)
  install(FILES $<TARGET_FILE_DIR:bflimtool>/PVRTexLib.dll DESTINATION bin)
elseif(APPLE)
  install(FILES $<TARGET_FILE_DIR:bflimtool>/libPVRTexLib.dylib DESTINATION bin)
else()
  install(FILES $<TARGET_FILE_DIR:bflimtool>/libPVRTexLib.so DESTINATION bin)
endif()
