file(GLOB example-sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

foreach(source ${example-sources})
  get_filename_component(example "${source}" NAME_WE)
  add_executable(ctcss-example-${example} "${source}")
  target_link_libraries(ctcss-example-${example} PRIVATE ctcss::ctcss)
  if(CTCSS_PCH AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    target_precompile_headers(ctcss-example-${example} PRIVATE
      "${PROJECT_SOURCE_DIR}/include/ctcss.hpp")
  endif()
  add_test(NAME example-${example} COMMAND ctcss-example-${example})
endforeach()
