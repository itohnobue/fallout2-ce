# Get author name
execute_process(
    COMMAND git log -1 --pretty=format:%an
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE AUTHOR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_AUTHOR_RESULT
    ERROR_QUIET
)

# Get current branch
execute_process(
    COMMAND git branch --show-current
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_BRANCH_RESULT
    ERROR_QUIET
)

# Get hash of the latest commit
execute_process(
    COMMAND git log --pretty=format:%h -n 1
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_HASH_RESULT
    ERROR_QUIET
)

# Get last tag
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE LATEST_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_TAG_RESULT
    ERROR_QUIET
)

# Get date of the latest commit
execute_process(
    COMMAND git log -1 --date=format:"%b %d %Y %H:%M:%S" --pretty=format:%cd
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE GIT_DATE_RESULT
    ERROR_QUIET
)

# Fallback for CI/Docker environments without git
if(NOT GIT_AUTHOR_RESULT EQUAL 0)
    set(AUTHOR "unknown")
endif()
if(NOT GIT_BRANCH_RESULT EQUAL 0)
    set(BRANCH "unknown")
endif()
if(NOT GIT_HASH_RESULT EQUAL 0)
    set(HASH "unknown")
endif()
if(NOT GIT_TAG_RESULT EQUAL 0)
    set(LATEST_TAG "")
endif()
if(NOT GIT_DATE_RESULT EQUAL 0)
    set(DATE 0)
endif()

# Set CI_BUILD based on git availability
if(GIT_HASH_RESULT EQUAL 0)
    set(CI_BUILD 0)
else()
    set(CI_BUILD 1)
endif()

# Create git_version.h file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/git_version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/git_version.h
)
