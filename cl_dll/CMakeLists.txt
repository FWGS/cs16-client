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
file(GLOB CS_CHROME_SRC "chrome/*.cpp")
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
list(APPEND CS_CLIENT_SRC ${CS_CHROME_SRC})
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
# =========================================================
# CEF CONFIGURATION
# =========================================================

option(CS16CLIENT_ENABLE_CEF "Enable CEF integration" ON)
set(CEF_ROOT "" CACHE PATH "Path to CEF distribution root")

set(CS16CLIENT_HAS_CEF OFF CACHE BOOL "CEF enabled")
set(CS16CLIENT_CEF_ROOT_RESOLVED "" CACHE PATH "Resolved CEF root")

if(CS16CLIENT_ENABLE_CEF AND NOT "${CEF_ROOT}" STREQUAL "")

    get_filename_component(_cef_root "${CEF_ROOT}" ABSOLUTE)

    # Validation
    if(NOT EXISTS "${CEF_ROOT}/include/cef_app.h")
		message(FATAL_ERROR "Invalid CEF root (missing include/cef_app.h)")
	endif()

	if(NOT EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
		message(FATAL_ERROR "Invalid CEF root (missing cmake module)")
	endif()

    set(CS16CLIENT_HAS_CEF ON CACHE BOOL "" FORCE)
    set(CS16CLIENT_CEF_ROOT_RESOLVED "${_cef_root}" CACHE PATH "" FORCE)

    target_compile_definitions(${CLIENT_LIB} PRIVATE
        CS16CLIENT_ENABLE_CEF=1
    )

    target_include_directories(${CLIENT_LIB} PRIVATE
        "${_cef_root}"
        "${_cef_root}/include"
    )

    include(${CMAKE_SOURCE_DIR}/cmake/CEFIntegration.cmake)
	set(CS16CLIENT_CEF_ROOT "${CEF_ROOT}")
    cs16client_setup_cef(${CLIENT_LIB})

    # libcef.dll lives in <gamedir>/cef and is loaded by the client itself (Platform_LoadCEF),
    # so it must not be a hard dependency, otherwise client.dll fails to load without CEF.
    if(MSVC)
        target_link_libraries(${CLIENT_LIB} PRIVATE delayimp)
        target_link_options(${CLIENT_LIB} PRIVATE "/DELAYLOAD:libcef.dll")
    endif()

    # Helper executable for the Chromium processes (renderer etc.), it is installed next to libcef.dll
    if(WIN32)
        add_executable(cs16_cef_helper WIN32
            cef_helper/cef_helper_main.cpp
            chrome/chrome_policy.cpp)
        target_compile_features(cs16_cef_helper PRIVATE cxx_std_20)
        target_compile_definitions(cs16_cef_helper PRIVATE CS16CLIENT_ENABLE_CEF=1 NOMINMAX)
        target_include_directories(cs16_cef_helper PRIVATE
            "${_cef_root}"
            "${_cef_root}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/chrome")
        target_link_libraries(cs16_cef_helper PRIVATE libcef_dll_wrapper "${_cef_root}/Release/libcef.lib")
        install(TARGETS cs16_cef_helper RUNTIME DESTINATION "${GAME_DIR}/cef/")
    endif()

endif()


set_target_postfix(${CLIENT_LIB})

if(WIN32)
	# gdi32 and user32: the letters of the Windows fonts for the atlases (hud/glyph_gdi.cpp)
	target_link_libraries(${CLIENT_LIB} PRIVATE gdi32 user32)
endif()

# The fonts for the atlases of the texts (hud/glyph_freetype.cpp) on the systems without the fonts of Windows:
# FreeType draws the letters, fontconfig finds the font files. Without them the font of the HUD is used.
if(NOT WIN32 AND NOT ANDROID)
	find_package(PkgConfig QUIET)
	if(PkgConfig_FOUND)
		pkg_check_modules(CS16_FONTS QUIET IMPORTED_TARGET freetype2 fontconfig)
	endif()
	# required on Linux and the BSDs: without them the client would have no texts of its own but the font of the
	# HUD (CS16CLIENT_REQUIRE_FREETYPE=OFF builds such a client anyway)
	option(CS16CLIENT_REQUIRE_FREETYPE "Fail without FreeType and fontconfig on Linux and the BSDs" ON)
	if(NOT CS16_FONTS_FOUND AND CS16CLIENT_REQUIRE_FREETYPE AND CMAKE_SYSTEM_NAME MATCHES "Linux|BSD")
		message(FATAL_ERROR "The texts of the client need FreeType and fontconfig: install their development "
			"packages (Debian/Ubuntu: libfreetype-dev libfontconfig-dev; Fedora: freetype-devel fontconfig-devel), "
			"or configure with -DCS16CLIENT_REQUIRE_FREETYPE=OFF to use the font of the HUD")
	endif()
	if(CS16_FONTS_FOUND)
		target_link_libraries(${CLIENT_LIB} PRIVATE PkgConfig::CS16_FONTS)
		target_compile_definitions(${CLIENT_LIB} PRIVATE CS16CLIENT_FREETYPE=1)
		message(STATUS "Client texts: FreeType and fontconfig")
	else()
		message(STATUS "Client texts: no FreeType/fontconfig, the font of the HUD is used")
	endif()
endif()

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
	target_link_libraries(${CLIENT_LIB} PRIVATE shell32)
endif()
