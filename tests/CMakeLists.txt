# the suite is static_asserts: compiling a test IS running it
add_custom_target(ctcss-test ALL)

file(GLOB test-sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# tests with a main() run; the rest are static_assert suites where
# compiling is the test
set(runtime-tests "")

foreach(source ${test-sources})
  get_filename_component(test "${source}" NAME_WE)
  if(test IN_LIST runtime-tests)
    add_executable(ctcss-test-${test} "${source}")
    target_link_libraries(ctcss-test-${test} PRIVATE ctcss::ctcss)
    if(CTCSS_PCH AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
      target_precompile_headers(ctcss-test-${test} PRIVATE
        "${PROJECT_SOURCE_DIR}/include/ctcss.hpp")
    endif()
    add_dependencies(ctcss-test ctcss-test-${test})
    add_test(NAME run-${test} COMMAND ctcss-test-${test})
  else()
    add_library(ctcss-test-${test} OBJECT EXCLUDE_FROM_ALL "${source}")
    target_link_libraries(ctcss-test-${test} PRIVATE ctcss::ctcss)
    if(CTCSS_PCH AND CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
      target_precompile_headers(ctcss-test-${test} PRIVATE
        "${PROJECT_SOURCE_DIR}/include/ctcss.hpp")
    endif()
    add_dependencies(ctcss-test ctcss-test-${test})
    add_test(NAME compile-${test}
      COMMAND "${CMAKE_COMMAND}" --build "${CMAKE_BINARY_DIR}" --target ctcss-test-${test})
  endif()
endforeach()
