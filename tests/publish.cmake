file(MAKE_DIRECTORY "${TEST_DIR}")
file(REMOVE "${TEST_DIR}/final" "${TEST_DIR}/missing.tmp")
file(REMOVE "${TEST_DIR}/stage.tmp")
execute_process(COMMAND "${PUBLISH}" 5 WORKING_DIRECTORY "${TEST_DIR}"
    RESULT_VARIABLE staged)
file(READ "${TEST_DIR}/stage.tmp" stage_bytes HEX)
if(NOT staged STREQUAL "0" OR NOT stage_bytes STREQUAL "000aff")
    message(FATAL_ERROR "Temporary file did not preserve binary bytes")
endif()

execute_process(COMMAND "${PUBLISH}" 6 WORKING_DIRECTORY "${TEST_DIR}"
    RESULT_VARIABLE staged_publish)
file(READ "${TEST_DIR}/final" published_bytes HEX)
if(NOT staged_publish STREQUAL "0" OR NOT published_bytes STREQUAL stage_bytes
   OR EXISTS "${TEST_DIR}/stage.tmp")
    message(FATAL_ERROR "Publishing the staged file did not preserve binary bytes")
endif()
file(REMOVE "${TEST_DIR}/final")

file(WRITE "${TEST_DIR}/stage.tmp" "keep")
execute_process(COMMAND "${PUBLISH}" 5 WORKING_DIRECTORY "${TEST_DIR}"
    RESULT_VARIABLE stage_collision)
file(READ "${TEST_DIR}/stage.tmp" stage_contents)
if(NOT stage_collision STREQUAL "1" OR NOT stage_contents STREQUAL "keep")
    message(FATAL_ERROR "Staging modified a pre-existing file")
endif()

file(WRITE "${TEST_DIR}/first.tmp" "first")
file(WRITE "${TEST_DIR}/second.tmp" "second")

execute_process(COMMAND "${PUBLISH}" 3 WORKING_DIRECTORY "${TEST_DIR}"
    RESULT_VARIABLE missing_result)
if(NOT missing_result STREQUAL "2" OR EXISTS "${TEST_DIR}/final")
    message(FATAL_ERROR "Publishing a missing source returned ${missing_result}")
endif()

execute_process(COMMAND "${PUBLISH}" 1 COMMAND "${PUBLISH}" 2
    WORKING_DIRECTORY "${TEST_DIR}" RESULTS_VARIABLE results)
file(READ "${TEST_DIR}/final" published)

if(results STREQUAL "0;1")
    set(winner first)
    set(loser second)
elseif(results STREQUAL "1;0")
    set(winner second)
    set(loser first)
else()
    message(FATAL_ERROR "Expected one success and one collision, got ${results}")
endif()

file(READ "${TEST_DIR}/${loser}.tmp" remaining)
if(NOT published STREQUAL winner OR NOT remaining STREQUAL loser
   OR EXISTS "${TEST_DIR}/${winner}.tmp")
    message(FATAL_ERROR "Publication did not preserve the winner and losing source")
endif()

if(loser STREQUAL "first")
    set(losing_argument 1)
else()
    set(losing_argument 2)
endif()
execute_process(COMMAND "${PUBLISH}" "${losing_argument}"
    WORKING_DIRECTORY "${TEST_DIR}" RESULT_VARIABLE retry)
file(READ "${TEST_DIR}/final" after_retry)
file(READ "${TEST_DIR}/${loser}.tmp" after_retry_source)
if(NOT retry STREQUAL "1" OR NOT after_retry STREQUAL winner
   OR NOT after_retry_source STREQUAL loser)
    message(FATAL_ERROR "Retry changed an existing destination or its temporary source")
endif()

if(CHECK_CLEANUP)
    file(REMOVE "${TEST_DIR}/stage.tmp")
    execute_process(COMMAND "${PUBLISH}" 7 WORKING_DIRECTORY "${TEST_DIR}"
        RESULT_VARIABLE flush_result)
    if(NOT flush_result STREQUAL "2" OR EXISTS "${TEST_DIR}/stage.tmp")
        message(FATAL_ERROR "Failed flush did not remove the staged file")
    endif()
    execute_process(COMMAND "${PUBLISH}" 8 WORKING_DIRECTORY "${TEST_DIR}"
        RESULT_VARIABLE flush_cleanup)
    if(NOT flush_cleanup STREQUAL "3" OR NOT EXISTS "${TEST_DIR}/stage.tmp")
        message(FATAL_ERROR "Failed staging cleanup did not report the remaining file")
    endif()

    file(REMOVE "${TEST_DIR}/final")
    file(WRITE "${TEST_DIR}/first.tmp" "cleanup pending")
    execute_process(COMMAND "${PUBLISH}" 4 WORKING_DIRECTORY "${TEST_DIR}"
        RESULT_VARIABLE cleanup_result)
    file(READ "${TEST_DIR}/final" after_publish)
    file(READ "${TEST_DIR}/first.tmp" temporary)
    if(NOT cleanup_result STREQUAL "4" OR NOT after_publish STREQUAL "cleanup pending"
       OR NOT temporary STREQUAL after_publish)
        message(FATAL_ERROR "Unlink failure did not preserve the published and temporary names")
    endif()
endif()

file(REMOVE "${TEST_DIR}/object" "${TEST_DIR}/object.tmp")
execute_process(COMMAND "${PUBLISH}" store WORKING_DIRECTORY "${TEST_DIR}"
    RESULT_VARIABLE stored)
execute_process(COMMAND "${PUBLISH}" store WORKING_DIRECTORY "${TEST_DIR}"
    RESULT_VARIABLE reused)
file(READ "${TEST_DIR}/object" object_bytes HEX)
if(NOT stored STREQUAL "0" OR NOT reused STREQUAL "1"
   OR NOT object_bytes STREQUAL "000aff" OR EXISTS "${TEST_DIR}/object.tmp")
    message(FATAL_ERROR "Chunk storage did not reuse the existing object")
endif()

file(WRITE "${TEST_DIR}/object.tmp" "owned by someone else")
execute_process(COMMAND "${PUBLISH}" store WORKING_DIRECTORY "${TEST_DIR}"
    RESULT_VARIABLE no_stage)
file(READ "${TEST_DIR}/object.tmp" existing_temporary)
if(NOT no_stage STREQUAL "1" OR NOT existing_temporary STREQUAL "owned by someone else")
    message(FATAL_ERROR "Reusing an object unexpectedly touched the temporary path")
endif()
file(REMOVE "${TEST_DIR}/object.tmp")

file(WRITE "${TEST_DIR}/object" "bad")
execute_process(COMMAND "${PUBLISH}" store WORKING_DIRECTORY "${TEST_DIR}"
    RESULT_VARIABLE conflict)
file(READ "${TEST_DIR}/object" corrupted)
if(NOT conflict STREQUAL "2" OR NOT corrupted STREQUAL "bad"
   OR EXISTS "${TEST_DIR}/object.tmp")
    message(FATAL_ERROR "Chunk storage accepted or replaced a corrupted object")
endif()

file(REMOVE "${TEST_DIR}/object" "${TEST_DIR}/object-other.tmp")
execute_process(COMMAND "${PUBLISH}" store COMMAND "${PUBLISH}" store-other
    WORKING_DIRECTORY "${TEST_DIR}" RESULTS_VARIABLE object_race)
file(READ "${TEST_DIR}/object" raced_bytes HEX)
if((NOT object_race STREQUAL "0;1" AND NOT object_race STREQUAL "1;0")
   OR NOT raced_bytes STREQUAL "000aff" OR EXISTS "${TEST_DIR}/object.tmp"
   OR EXISTS "${TEST_DIR}/object-other.tmp")
    message(FATAL_ERROR "Concurrent chunk storage returned ${object_race}, destination bytes: ${raced_bytes}")
endif()
