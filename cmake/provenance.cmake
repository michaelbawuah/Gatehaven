# Record the checkout revision, including worktree and detached-HEAD builds.
set(GATEHAVEN_SOURCE_REVISION "unknown")
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --verify HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE revision OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    string(LENGTH "${revision}" revision_length)
    if(revision MATCHES "^[0-9a-f]+$" AND revision_length EQUAL 40)
        set(GATEHAVEN_SOURCE_REVISION "${revision}")
        execute_process(COMMAND "${GIT_EXECUTABLE}" symbolic-ref -q HEAD
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE branch_ref OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        foreach(ref HEAD "${branch_ref}")
            if(NOT ref STREQUAL "")
                execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${ref}"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                    OUTPUT_VARIABLE ref_path OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
                if(NOT IS_ABSOLUTE "${ref_path}")
                    set(ref_path "${CMAKE_CURRENT_SOURCE_DIR}/${ref_path}")
                endif()
                if(EXISTS "${ref_path}")
                    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ref_path}")
                endif()
            endif()
        endforeach()
    endif()
endif()
