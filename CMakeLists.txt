cmake_minimum_required(VERSION 3.18)

set(SOLUTION_NAME Chess)	#Unijeti naziv solution-a

#BEGIN dio koji ide u sve solutions
project(${SOLUTION_NAME})
set(SOURCE_ROOT ${CMAKE_CURRENT_LIST_DIR})
# Locate the user's home directory. Windows normally does NOT define HOME
# (it uses USERPROFILE), so relying on HOME alone makes WORK_ROOT collapse
# to "/Work" and the include() below fails with a confusing error on any
# clean Windows machine. Fall back through the usual alternatives.
set(HOME_ROOT "$ENV{HOME}")
if (NOT HOME_ROOT)
	set(HOME_ROOT "$ENV{USERPROFILE}")
endif()
if (NOT HOME_ROOT AND DEFINED ENV{HOMEDRIVE} AND DEFINED ENV{HOMEPATH})
	set(HOME_ROOT "$ENV{HOMEDRIVE}$ENV{HOMEPATH}")
endif()
if (NOT HOME_ROOT)
	message(FATAL_ERROR
		"Could not determine your home directory: none of HOME, USERPROFILE or "
		"HOMEDRIVE+HOMEPATH is set. Set one of them, or replace HOME_ROOT below "
		"with the absolute path to the folder containing your 'Work' directory.")
endif()
if (WIN32)
	string(REPLACE "\\" "/" HOME_ROOT "${HOME_ROOT}")
endif()
set(WORK_ROOT ${HOME_ROOT}/Work)
set(NATID_SDK_ROOT ${WORK_ROOT}) 
if (NOT EXISTS "${WORK_ROOT}/DevEnv/Common.cmake")
	message(FATAL_ERROR
		"natGUI framework not found at ${WORK_ROOT}/DevEnv. This project needs "
		"the course framework installed under <home>/Work. Current home was "
		"detected as: ${HOME_ROOT}")
endif()
include(${WORK_ROOT}/DevEnv/Common.cmake) 
#END dio koji ide u sve solutions

include(${WORK_ROOT}/DevEnv/natGUI.cmake)

include(chess.cmake)