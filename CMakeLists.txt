cmake_minimum_required(VERSION 3.20)
project(ChunkVault VERSION 0.1.0 LANGUAGES C)

add_library(cv_storage OBJECT src/publish.c src/sha256.c src/chunk.c src/cdc.c src/fs.c)
target_include_directories(cv_storage PUBLIC src)
add_executable(cv src/main.c src/repo.c)
if(WIN32)
    target_link_libraries(cv PRIVATE shell32)
endif()
target_link_libraries(cv PRIVATE cv_storage)
set_target_properties(cv cv_storage PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_compile_definitions(cv PRIVATE CV_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(cv PRIVATE /W4)
    target_compile_options(cv_storage PRIVATE /W4)
else()
    target_compile_options(cv PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cv_storage PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS cv RUNTIME DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    add_executable(test_source tests/source.c)
    target_link_libraries(test_source PRIVATE cv_storage)
    set_target_properties(test_source PROPERTIES
        C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME storage.source COMMAND test_source)

    add_executable(test_repo_lock tests/repo_lock.c)
    target_link_libraries(test_repo_lock PRIVATE cv_storage)
    set_target_properties(test_repo_lock PROPERTIES
        C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME storage.repo_lock COMMAND test_repo_lock)

    add_executable(test_manifest_fixture tests/manifest_fixture.c)
    set_target_properties(test_manifest_fixture PROPERTIES
        C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME cli.lifecycle COMMAND "${CMAKE_COMMAND}"
        "-DCV=$<TARGET_FILE:cv>" "-DFIXTURE=$<TARGET_FILE:test_manifest_fixture>"
        "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/lifecycle-$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/lifecycle.cmake")

    add_executable(test_cdc tests/cdc.c)
    target_link_libraries(test_cdc PRIVATE cv_storage)
    set_target_properties(test_cdc PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME storage.cdc COMMAND test_cdc)

    add_executable(test_chunk tests/chunk.c)
    target_link_libraries(test_chunk PRIVATE cv_storage)
    set_target_properties(test_chunk PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME storage.chunk COMMAND test_chunk)

    add_executable(test_sha256 tests/sha256.c)
    target_link_libraries(test_sha256 PRIVATE cv_storage)
    set_target_properties(test_sha256 PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME storage.sha256 COMMAND test_sha256)

    add_executable(test_publish tests/publish.c)
    target_link_libraries(test_publish PRIVATE cv_storage)
    set_target_properties(test_publish PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    add_test(NAME storage.publish COMMAND "${CMAKE_COMMAND}"
        "-DPUBLISH=$<TARGET_FILE:test_publish>"
        "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/publish-test-$<CONFIG>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/publish.cmake")

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        add_executable(test_cv_fsync src/main.c src/repo.c tests/fsync_fault.c)
        target_link_libraries(test_cv_fsync PRIVATE cv_storage)
        target_compile_definitions(test_cv_fsync PRIVATE CV_VERSION="${PROJECT_VERSION}")
        target_link_options(test_cv_fsync PRIVATE -Wl,--wrap=fsync)
        set_target_properties(test_cv_fsync PROPERTIES
            C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME cli.fsync_failure COMMAND "${CMAKE_COMMAND}"
            "-DCV=$<TARGET_FILE:test_cv_fsync>"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/fsync-fault-$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/fsync_fault.cmake")

        add_executable(test_publish_fallback tests/publish.c)
        target_link_libraries(test_publish_fallback PRIVATE cv_storage)
        target_compile_definitions(test_publish_fallback PRIVATE CV_TEST_LINK_FALLBACK)
        target_link_options(test_publish_fallback PRIVATE -Wl,--wrap=renameat2 -Wl,--wrap=unlink -Wl,--wrap=fsync)
        set_target_properties(test_publish_fallback PROPERTIES
            C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
        add_test(NAME storage.publish_fallback COMMAND "${CMAKE_COMMAND}"
            "-DPUBLISH=$<TARGET_FILE:test_publish_fallback>" -DCHECK_CLEANUP=ON
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/publish-fallback-$<CONFIG>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/publish.cmake")
    endif()

    foreach(case IN ITEMS help version no_args unknown extra)
        add_test(NAME cli.${case} COMMAND "${CMAKE_COMMAND}"
            "-DCV=$<TARGET_FILE:cv>" "-DCASE=${case}" "-DVERSION=${PROJECT_VERSION}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli.cmake")
    endforeach()
endif()
