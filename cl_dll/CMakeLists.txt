cmake_minimum_required(VERSION 3.10)
project(client)

set(CLIENT_LIB client)

file(GLOB CS_CLIENT_SRC "*.cpp")
file(GLOB CS_EV_SRC "events/*.cpp")
file(GLOB CS_WPN_SRC "cs_wpn/*.cpp")
file(GLOB CS_WPNSH_SRC "../dlls/wpn_shared/*.cpp")
file(GLOB CS_HUD_SRC "hud/*.cpp")
file(GLOB CS_STUDIO_SRC "studio/*.cpp")
file(GLOB CS_INPUT_SRC "input/*.cpp")
file(GLOB CS_PM_SRC "../pm_shared/*.cpp")
file(GLOB CS_GAMSH_SRC "../game_shared/*.cpp")
file(GLOB MINIUTL_SRC "../3rdparty/mainui_cpp/miniutl/*.cpp")
file(GLOB CS_PARTICLEMAN_SRC "particleman/*.cpp")

list(REMOVE_ITEM CS_CLIENT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/ev_hldm.cpp")
list(REMOVE_ITEM CS_INPUT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/input/input_sdl.cpp")
list(REMOVE_ITEM CS_CLIENT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/inputw32.cpp")
list(REMOVE_ITEM CS_WPN_SRC "${CMAKE_CURRENT_SOURCE_DIR}/cs_wpn/cs_baseentity.cpp")
list(APPEND CS_CLIENT_SRC ../common/interface.cpp)
list(APPEND CS_CLIENT_SRC ../public/utflib.cpp)

if(HAVE_STRLCPY)
	add_definitions(-DHAVE_STRLCPY)
else()
	list(APPEND CS_CLIENT_SRC ../public/strlcpy.c)
endif()

if(HAVE_STRLCAT)
	add_definitions(-DHAVE_STRLCAT)
else()
	list(APPEND CS_CLIENT_SRC ../public/strlcat.c)
endif()

list(APPEND CS_CLIENT_SRC ${CS_EV_SRC})
list(APPEND CS_CLIENT_SRC ${CS_WPN_SRC})
list(APPEND CS_CLIENT_SRC ${CS_WPNSH_SRC})
list(APPEND CS_CLIENT_SRC ${CS_HUD_SRC})
list(APPEND CS_CLIENT_SRC ${CS_STUDIO_SRC})
list(APPEND CS_CLIENT_SRC ${CS_PM_SRC})
list(APPEND CS_CLIENT_SRC ${CS_INPUT_SRC})
list(APPEND CS_CLIENT_SRC ${CS_GAMSH_SRC})
list(APPEND CS_CLIENT_SRC ${MINIUTL_SRC})
list(APPEND CS_CLIENT_SRC ${CS_PARTICLEMAN_SRC})

include_directories(
	include
	include/hud
	include/studio
	include/math
	../cl_dll
	../common
	../engine
	../pm_shared
	../dlls
	../game_shared
	../public/
	../public/cl_dll
	../3rdparty/mainui_cpp
	../3rdparty/mainui_cpp/controls
	../3rdparty/mainui_cpp/menus
	../3rdparty/mainui_cpp/miniutl
	particleman)

add_definitions(-DCLIENT_WEAPONS -DCLIENT_DLL -DSTDINT_H=<cstdint>)

if(NOT MSVC)
	add_definitions(-DLINUX -D_LINUX)
	add_definitions(-Dstricmp=strcasecmp -D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp -fms-extensions)
else()
	add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
endif()

add_library(${CLIENT_LIB} SHARED ${CS_CLIENT_SRC})

set_target_postfix(${CLIENT_LIB})

install(TARGETS ${CLIENT_LIB}
	DESTINATION "${GAME_DIR}/${CLIENT_INSTALL_DIR}/"
	PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
	    GROUP_READ GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE)

# Install PDB file on Windows
if(MSVC)
	install(FILES $<TARGET_PDB_FILE:${CLIENT_LIB}>
		DESTINATION "${GAME_DIR}/${CLIENT_INSTALL_DIR}/" OPTIONAL)
endif()

if(WIN32)
	# shell32: opening the MOTD in the browser of the system (hud/MOTD_browser.cpp)
	target_link_libraries(${CLIENT_LIB} shell32)
endif()
