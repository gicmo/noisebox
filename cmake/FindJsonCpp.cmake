# - Try to find JsonCpp
# Once done this will define
#  JSONCPP_FOUND - System has JsonCpp
#  JSONCPP_INCLUDE_DIRS - The JsonCpp include directories
#  JSONCPP_LIBRARIES - The libraries needed to use JsonCpp


find_path(JSONCPP_INCLUDE_DIR json/json.h
          HINTS /usr/local/include
          /usr/include
          $ENV{JSONCPP_ROOT}/include
          PATH_SUFFIXES jsoncpp)

find_library(JSONCPP_LIBRARY NAMES jsoncpp libjsoncpp
             HINTS ${JSONCPP_INCLUDE_DIR}/../lib
             /usr/local/lib
             /usr/lib)

set(JSONCPP_LIBRARIES ${JSONCPP_LIBRARY} )
set(JSONCPP_INCLUDE_DIRS ${JSONCPP_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set JSONCPP_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(JsonCpp DEFAULT_MSG
                                  JSONCPP_LIBRARY JSONCPP_INCLUDE_DIR)

mark_as_advanced(JSONCPP_INCLUDE_DIR JSONCPP_LIBRARY)