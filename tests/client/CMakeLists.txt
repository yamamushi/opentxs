
set(name unittests-opentxs-client)

set(cxx-sources
  ${PROJECT_SOURCE_DIR}/tests/main.cpp
  Test_CreateNymHD.cpp
  Test_NymData.cpp
  ${PROJECT_SOURCE_DIR}/tests/OTTestEnvironment.cpp
)

include_directories(
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/tests
  ${GTEST_INCLUDE_DIRS}
)

add_executable(${name} ${cxx-sources})
target_link_libraries(${name} opentxs opentxs-proto ${PROTOBUF_LITE_LIBRARIES} ${GTEST_LIBRARY})
set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/tests)
add_test(${name} ${PROJECT_BINARY_DIR}/tests/${name} --gtest_output=xml:gtestresults.xml)
