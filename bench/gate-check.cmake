# Runs the gate on bench/gate-fixture.csv and checks every expected verdict.
execute_process(COMMAND "${EXE}" "--gate-files=${FIXTURE}"
    --gate-snapshot=qt-loose-sort --gate-persistent=qt-loose --gate-ordered=qt-refs-ordered:qt-refs
    RESULT_VARIABLE result OUTPUT_VARIABLE output)
set(expected
    "G0 +hard +snapshot int32 pile n=1000 sweep MISMATCH .* FAIL"
    "G1 +hard +snapshot int32 uniform n=8 vs brute force \\(brute\\) .* pass"
    "G2 +hard +snapshot int32 uniform n=100000 vs best baseline \\(grid\\) .* pass"
    "G2 +hard +snapshot int32 clustered n=100000 vs best baseline \\(grid\\) .* FAIL"
    "G2 +hard +snapshot int32 split-line n=1000 vs best baseline \\(grid\\) .* inconclusive"
    "G3 +hard +snapshot int32 split-line n=10000 candidates/\\(pairs\\+n\\) vs n=1000 .* pass"
    "G3 +hard +snapshot int32 split-line n=100000 candidates/\\(pairs\\+n\\) vs n=10000 .* FAIL"
    "G4 +hard +snapshot int32 split-line-translated n=10000 candidates vs split-line .* pass"
    "G4 +hard +snapshot int32 split-line-translated n=10000 time vs split-line .* pass"
    "G6 +hard +snapshot int32 mixed-size n=100000 allocations per warm step .* FAIL"
    "G9 +hard +snapshot int32 uniform n=10000 vs unordered \\(qt-refs\\) .* pass"
    "G9 +hard +snapshot int32 clustered n=10000 vs unordered \\(qt-refs\\) .* FAIL"
    "G7 +hard +persistent float uniform n=10000 static/full vs dynamic tree \\(b2tree\\) .* pass"
    "G7 +soft +persistent float uniform n=10000 static/full vs grid \\(grid\\) .* FAIL"
    "G8a +hard +persistent float uniform n=10000 static/moved ns per step .* pass"
    "G8b +hard +persistent float uniform n=100000 small-single/moved vs n=1000 .* FAIL"
    "G8c +hard +persistent float uniform n=10000 small-1%/moved vs dynamic tree \\(b2tree-fat\\) .* pass"
    "G8c +soft +persistent float uniform n=10000 small-1%/moved vs grid \\(grid\\) .* pass"
    "gates: FAIL")
foreach (pattern IN LISTS expected)
    if (NOT output MATCHES "${pattern}")
        message(FATAL_ERROR "missing gate verdict: ${pattern}\n${output}")
    endif()
endforeach()
if (NOT result EQUAL 3)
    message(FATAL_ERROR "expected exit code 3 (hard gate failure), got ${result}")
endif()
