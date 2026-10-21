# Smoke test: build a tiny synthetic Bedrock world, then export it to OBJ.
# Runs with no resource pack, so the block registry must fall back to a
# built-in "missing" texture and still produce a valid mesh.

file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}")

execute_process(
  COMMAND "${MAKEWORLD_EXE}" "${TEST_DIR}/world" 4
  RESULT_VARIABLE gen_result
)
if(NOT gen_result EQUAL 0)
  message(FATAL_ERROR "make_test_world failed")
endif()

execute_process(
  COMMAND "${MAPROCK_EXE}"
    --world "${TEST_DIR}/world"
    --whole
    --format obj
    --output "${TEST_DIR}/out.obj"
  RESULT_VARIABLE export_result
  OUTPUT_VARIABLE export_out
  ERROR_VARIABLE export_err
)
if(NOT export_result EQUAL 0)
  message(FATAL_ERROR "maprock export failed:\n${export_out}\n${export_err}")
endif()

foreach(f "${TEST_DIR}/out.obj" "${TEST_DIR}/out.mtl" "${TEST_DIR}/out.png")
  if(NOT EXISTS "${f}")
    message(FATAL_ERROR "expected output missing: ${f}")
  endif()
endforeach()

message(STATUS "smoke test passed: ${TEST_DIR}/out.obj")
