set(git_info "unknown")

if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C ${REPO_DIR} branch --show-current
        OUTPUT_VARIABLE git_branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(git_branch STREQUAL "")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} -C ${REPO_DIR} rev-parse --abbrev-ref HEAD
            OUTPUT_VARIABLE git_branch
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C ${REPO_DIR} rev-parse --short HEAD
        OUTPUT_VARIABLE git_hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C ${REPO_DIR} status --porcelain --untracked-files=no
        OUTPUT_VARIABLE git_status
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    set(git_dirty "")
    if(NOT git_status STREQUAL "")
        set(git_dirty "-dirty")
    endif()

    if(NOT git_branch STREQUAL "" AND NOT git_hash STREQUAL "")
        set(git_info "${git_branch}@${git_hash}${git_dirty}")
    endif()
endif()

# Function to get git info for a library
function(get_lib_git_info lib_name lib_path out_var)
    set(${out_var} "unknown" PARENT_SCOPE)
    if(NOT GIT_EXECUTABLE OR NOT EXISTS "${lib_path}/.git" AND NOT EXISTS "${lib_path}/../.git/modules/${lib_name}")
        # Check if it's a git repo at all
        execute_process(
            COMMAND ${GIT_EXECUTABLE} -C ${lib_path} rev-parse --git-dir
            OUTPUT_QUIET
            ERROR_QUIET
            RESULT_VARIABLE git_check_result
        )
        if(NOT git_check_result EQUAL 0)
            return()
        endif()
    endif()

    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C ${lib_path} branch --show-current
        OUTPUT_VARIABLE lib_branch
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(lib_branch STREQUAL "")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} -C ${lib_path} rev-parse --abbrev-ref HEAD
            OUTPUT_VARIABLE lib_branch
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} -C ${lib_path} rev-parse --short HEAD
        OUTPUT_VARIABLE lib_hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(NOT lib_branch STREQUAL "" AND NOT lib_hash STREQUAL "")
        set(${out_var} "${lib_branch}@${lib_hash}" PARENT_SCOPE)
    elseif(NOT lib_hash STREQUAL "")
        set(${out_var} "${lib_hash}" PARENT_SCOPE)
    endif()
endfunction()

# Collect third-party library git info
set(lib_git_defines "")

if(LIB_LIST_STR)
    # Split the '|' separated string into a list
    string(REPLACE "|" ";" LIB_LIST "${LIB_LIST_STR}")
    foreach(lib_entry ${LIB_LIST})
        # Parse "NAME=PATH" format
        string(REGEX MATCH "^([^=]+)=(.+)$" _match "${lib_entry}")
        if(_match)
            set(lib_name "${CMAKE_MATCH_1}")
            set(lib_path "${CMAKE_MATCH_2}")
            get_lib_git_info(${lib_name} ${lib_path} lib_info)
            # Convert lib_name to uppercase for the macro
            string(TOUPPER "${lib_name}" lib_name_upper)
            string(APPEND lib_git_defines "#define EOS_GIT_${lib_name_upper} \"${lib_info}\"\n")
        endif()
    endforeach()
endif()

set(content "#ifndef EOS_GIT_INFO_H\n#define EOS_GIT_INFO_H\n\n#define EOS_GIT_INFO \"${git_info}\"\n\n/* Third-party library git info */\n${lib_git_defines}\n#endif /* EOS_GIT_INFO_H */\n")

if(EXISTS ${OUT_FILE})
    file(READ ${OUT_FILE} old_content)
else()
    set(old_content "")
endif()

if(NOT old_content STREQUAL content)
    file(WRITE ${OUT_FILE} "${content}")
endif()
