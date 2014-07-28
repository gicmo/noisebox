# - Try to find BerkeleyDB
# Once done this will define
#  BDB_FOUND - System has BDB
#  BDB_INCLUDE_DIRS - The BDB include directories
#  BDB_LIBRARIES - The libraries needed to use BDB


find_path(BDB_INCLUDE_DIR db_cxx.h
          HINTS /usr/local/include
          /usr/include
          $ENV{BDB_ROOT}/include)

find_library(BDB_C_LIBRARY db libdb
             HINTS ${BDB_INCLUDE_DIR}/../lib
             /usr/local/lib
             /usr/lib)

find_library(BDB_CXX_LIBRARY db_cxx libdb_cxx
             HINTS ${BDB_INCLUDE_DIR}/../lib
             /usr/local/lib
             /usr/lib)

set(BDB_LIBRARIES ${BDB_C_LIBRARY} ${BDB_CXX_LIBRARY})
set(BDB_INCLUDE_DIRS ${BDB_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set BDB_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(BerkeleyDB DEFAULT_MSG
                                  BDB_C_LIBRARY BDB_CXX_LIBRARY BDB_INCLUDE_DIR)

mark_as_advanced(BDB_INCLUDE_DIR BDB_C_LIBRARY BDB_CXX_LIBRARY)