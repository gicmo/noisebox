# - Try to find Libdaemon
# Once done this will define
#  LIBDAEMON_FOUND - System has Libdaemon
#  LIBDAEMON_INCLUDE_DIRS - The Libdaemon include directories
#  LIBDAEMON_LIBRARIES - The libraries needed to use Libdaemon


find_path(LIBDAEMON_INCLUDE_DIR libdaemon/daemon.h
          HINTS /usr/local/include
          /usr/include
          $ENV{LIBDAEMON_ROOT})

find_library(LIBDAEMON_LIBRARY NAMES daemon libdaemon
             HINTS ${LIBDAEMON_INCLUDE_DIR}/../lib
             /usr/local/lib
             /usr/lib
             $ENV{LIBDAEMON_ROOT})

set(LIBDAEMON_LIBRARIES ${LIBDAEMON_LIBRARY} )
set(LIBDAEMON_INCLUDE_DIRS ${LIBDAEMON_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBDAEMON_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Libdaemon DEFAULT_MSG
                                  LIBDAEMON_LIBRARY LIBDAEMON_INCLUDE_DIR)

mark_as_advanced(LIBDAEMON_INCLUDE_DIR LIBDAEMON_LIBRARY)