# - Try to find zmq
# Once done this will define
#  ZMQ_FOUND - System has zmq
#  ZMQ_INCLUDE_DIRS - The zmq include directories
#  ZMQ_LIBRARIES - The libraries needed to use zmq
#  ZMQ_DEFINITIONS - Compiler switches required for using zmq

find_package(PkgConfig)
pkg_check_modules(PC_ZMQ QUIET libzmq)
set(ZMQ_DEFINITIONS ${PC_ZMQ_CFLAGS_OTHER})

find_path(ZMQ_INCLUDE_DIR zmq.h
          HINTS ${PC_ZMQ_INCLUDEDIR} ${PC_ZMQ_INCLUDE_DIRS})

find_library(ZMQ_LIBRARY NAMES zmq libzmq
             HINTS ${PC_ZMQ_LIBDIR} ${PC_ZMQ_LIBRARY_DIRS} )

set(ZMQ_LIBRARIES ${ZMQ_LIBRARY} )
set(ZMQ_INCLUDE_DIRS ${ZMQ_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set ZMQ_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(ZMQ DEFAULT_MSG
                                  ZMQ_LIBRARY ZMQ_INCLUDE_DIR)

mark_as_advanced(ZMQ_INCLUDE_DIR ZMQ_LIBRARY)