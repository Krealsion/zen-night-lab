# The scenarios.
#
# One sitting is one process. The question this application exists to ask is
# what a committee knows when it reconvenes, so the only honest way to ask it
# is to let the first process EXIT and start a second one with nothing between
# them but a file. That cannot be expressed inside the program, so it lives
# here.
#
# Every scenario destroys its work directory first. A county list that survived
# from the previous run would make several of these pass for the wrong reason,
# which is exactly the failure the application is about.

if(NOT DEFINED SITTING OR NOT DEFINED WORK OR NOT DEFINED SCENARIO)
    message(FATAL_ERROR "scenarios.cmake: SITTING, WORK and SCENARIO are required")
endif()

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(LIST_FILE "${WORK}/marchfield.list")

function(run_sitting)
    cmake_parse_arguments(S "FOUND;CONTROLS" "YEAR;SEATS;EXPECT;LABEL" "" ${ARGN})
    set(args --archive "${LIST_FILE}" --year "${S_YEAR}")
    if(S_FOUND)
        list(APPEND args --found)
    endif()
    if(S_CONTROLS)
        list(APPEND args --controls)
    endif()
    if(DEFINED S_SEATS)
        list(APPEND args --seats "${S_SEATS}")
    endif()
    list(APPEND args "${A1}" "${A2}" "${A3}" "${A4}" "${A5}")

    execute_process(COMMAND "${SITTING}" ${args} RESULT_VARIABLE rc)
    if(NOT rc EQUAL S_EXPECT)
        message(FATAL_ERROR "${S_LABEL}: expected exit ${S_EXPECT}, got ${rc}")
    endif()
    message(STATUS "-- ok: ${S_LABEL} (exit ${rc})")
endfunction()

function(list_must_contain needle what)
    file(READ "${LIST_FILE}" content)
    string(FIND "${content}" "${needle}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "${what}: the county list does not contain '${needle}'")
    endif()
    message(STATUS "-- ok: ${what}")
endfunction()

# ---------------------------------------------------------------------------

if(SCENARIO STREQUAL "founding-sitting")
    # The county has never kept records. The committee founds a list, sits, and
    # the three labelled controls run inside the sitting.
    run_sitting(YEAR 1979 FOUND CONTROLS EXPECT 0 LABEL "the 1979 sitting")
    list_must_contain("list  Little Bunting|1979-017" "the county list gained the Little Bunting")
    list_must_contain("held  1979-044" "the late record is waiting in the file")

elseif(SCENARIO STREQUAL "two-sittings")
    # THE QUESTION. Two processes, a year apart, and a file.
    run_sitting(YEAR 1979 FOUND EXPECT 0 LABEL "the 1979 sitting")
    list_must_contain("det   1979-011|Pallid Harrier|T. Bewick|NOT ACCEPTED"
                      "the 1979 minutes are in the file")
    run_sitting(YEAR 1980 EXPECT 0 LABEL "the 1980 sitting, reading what 1979 wrote")
    list_must_contain("det   1980-012|Pallid Harrier|T. Bewick|ACCEPTED"
                      "the 1980 minutes joined them")
    list_must_contain("sittings 1979,1980" "the file records both sittings")

elseif(SCENARIO STREQUAL "control-no-list")
    # There is no file. A committee that quietly starts from nothing is the
    # whole failure this application is about, so it must refuse instead.
    run_sitting(YEAR 1980 EXPECT 2 LABEL "1980 with no county list at all")
    if(EXISTS "${LIST_FILE}")
        message(FATAL_ERROR "the refused sitting wrote a county list anyway")
    endif()
    message(STATUS "-- ok: the refused sitting wrote nothing")

elseif(SCENARIO STREQUAL "control-another-county")
    run_sitting(YEAR 1979 FOUND EXPECT 0 LABEL "the 1979 sitting")
    file(SHA256 "${LIST_FILE}" before)
    file(READ "${LIST_FILE}" content)
    string(REPLACE "county   Marchfield" "county   Barrowdale" content "${content}")
    file(WRITE "${LIST_FILE}" "${content}")
    run_sitting(YEAR 1980 EXPECT 2 LABEL "1980 handed another county's list")
    file(SHA256 "${LIST_FILE}" after)
    file(READ "${LIST_FILE}" now)
    string(REPLACE "county   Barrowdale" "county   Marchfield" now "${now}")
    file(WRITE "${WORK}/restored" "${now}")
    file(SHA256 "${WORK}/restored" restored)
    if(NOT before STREQUAL restored)
        message(FATAL_ERROR "the refused sitting altered the file it refused to read")
    endif()
    message(STATUS "-- ok: the refused sitting left the other county's list alone")

elseif(SCENARIO STREQUAL "control-half-a-list")
    # A partial write: the file stops before it says how long it is.
    run_sitting(YEAR 1979 FOUND EXPECT 0 LABEL "the 1979 sitting")
    file(STRINGS "${LIST_FILE}" lines)
    list(POP_BACK lines)
    string(JOIN "\n" truncated ${lines})
    file(WRITE "${LIST_FILE}" "${truncated}\n")
    file(SHA256 "${LIST_FILE}" before)
    run_sitting(YEAR 1980 EXPECT 2 LABEL "1980 handed a list that stops in the middle")
    file(SHA256 "${LIST_FILE}" after)
    if(NOT before STREQUAL after)
        message(FATAL_ERROR "the refused sitting overwrote the half-written list")
    endif()
    message(STATUS "-- ok: the refused sitting left the half-written list alone")

elseif(SCENARIO STREQUAL "control-inquorate")
    # Two seats unfilled. Three members is not a committee, however obvious the
    # records look, and the sitting's own checks assert that nothing was
    # decided and the county list did not move.
    run_sitting(YEAR 1979 FOUND SEATS 3 EXPECT 0 LABEL "an inquorate 1979 sitting")
    list_must_contain("held  1979-004" "the inquorate sitting held its records over")

elseif(SCENARIO STREQUAL "control-lost-list")
    # THE FALSE GREEN. The 1979 file is lost and somebody founds a new list;
    # the 1980 sitting then runs perfectly happily and announces a first county
    # record for a bird the county has had since 1979. Nothing inside the
    # sitting can tell. The sitting's own checks assert that it DOES go wrong,
    # because a control that cannot fail proves nothing about the one that can.
    run_sitting(YEAR 1979 FOUND EXPECT 0 LABEL "the 1979 sitting")
    file(REMOVE "${LIST_FILE}")
    run_sitting(YEAR 1980 FOUND EXPECT 0 LABEL "1980 on a re-founded, empty list")
    list_must_contain("list  Little Bunting|1980-006"
                      "the re-founded list credits the wrong record with the first")

else()
    message(FATAL_ERROR "unknown scenario: ${SCENARIO}")
endif()

message(STATUS "SCENARIO ${SCENARIO}: all expectations held")
