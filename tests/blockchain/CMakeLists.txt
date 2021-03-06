
set(name unittests-opentxs-blockchain)

set(cxx-sources
        main.cpp
        Test_NewAccount.cpp
        Test_AccountList.cpp
        Test_AllocateAddress.cpp
        Test_AssignAddress.cpp
        Test_StoreIncoming.cpp
        Test_StoreOutgoing.cpp
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
