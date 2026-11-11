//
// MOTD_browser.cpp
//
// OS specific helpers for opening server MOTD in the system web browser.
// Kept in a separate file because <windows.h> conflicts with engine headers.
// All strings here are UTF-8 (the addresses are plain ASCII after MotdUtil::MakeSafeUrl).
//

#include <stddef.h>

bool MOTD_OpenInBrowser( const char *target );
bool MOTD_SaveTempHtml( const char *html, size_t len, char *outPath, size_t outSize );
bool MOTD_BrowserOpensFiles( void );

#if defined( _WIN32 )

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <wchar.h>

static bool Utf8ToWide( const char *s, wchar_t *out, int outCount )
{
	return MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, out, outCount ) > 0;
}

bool MOTD_OpenInBrowser( const char *target )
{
	wchar_t wide[4096];

	if( !target || !Utf8ToWide( target, wide, (int)( sizeof( wide ) / sizeof( wide[0] ))))
		return false;

	HINSTANCE result = ShellExecuteW( NULL, L"open", wide, NULL, NULL, SW_SHOWNORMAL );

	return (INT_PTR)result > 32;
}

// The page is saved to %TEMP%\cs16_motd.html. The folder of a user can have any letters in its
// name (C:\Users\Иван\...), so the wide functions of Windows are used and the path is given back in UTF-8.
bool MOTD_SaveTempHtml( const char *html, size_t len, char *outPath, size_t outSize )
{
	wchar_t dir[MAX_PATH + 1];
	const DWORD n = GetTempPathW( MAX_PATH + 1, dir );

	if( n == 0 || n > MAX_PATH )
		return false;

	wchar_t path[MAX_PATH + 32];

	if( _snwprintf( path, sizeof( path ) / sizeof( path[0] ), L"%lscs16_motd.html", dir ) <= 0 )
		return false;

	path[sizeof( path ) / sizeof( path[0] ) - 1] = 0;

	FILE *f = _wfopen( path, L"wb" );

	if( !f )
		return false;

	const size_t written = fwrite( html, 1, len, f );
	fclose( f );

	if( written != len )
		return false;

	return WideCharToMultiByte( CP_UTF8, 0, path, -1, outPath, (int)outSize, NULL, NULL ) > 0;
}

#elif defined( __ANDROID__ )

// Android: the address is opened by SDL_OpenURL of the SDL the engine runs on (an ACTION_VIEW intent: the browser of
// the device or a Custom Tab over the game). The client is not linked with SDL, so the function is looked up in the
// library the engine has already loaded. Only addresses: a file of the game can't be given to another app
// (Android 7+ refuses file:// addresses), so an HTML MOTD stays in the game as text.

#include <dlfcn.h>

typedef int ( *pfnSDL_OpenURL )( const char *url ); // SDL2 returns 0 on success, SDL3 true: the result is not used

static pfnSDL_OpenURL FindSDLOpenURL( void )
{
	static pfnSDL_OpenURL func = nullptr;
	static bool tried = false;

	if( tried )
		return func;
	tried = true;

	static const char *const libs[] = { "libSDL2.so", "libSDL3.so" };
	for( const char *lib : libs )
	{
		void *handle = dlopen( lib, RTLD_NOW | RTLD_NOLOAD );
		if( !handle )
			continue;

		func = (pfnSDL_OpenURL)dlsym( handle, "SDL_OpenURL" );
		if( func )
			break;
	}

	return func;
}

bool MOTD_OpenInBrowser( const char *target )
{
	pfnSDL_OpenURL openUrl = FindSDLOpenURL();

	if( !target || !target[0] || !openUrl )
		return false;

	openUrl( target );
	return true;
}

bool MOTD_SaveTempHtml( const char *html, size_t len, char *outPath, size_t outSize )
{
	(void)html; (void)len; (void)outPath; (void)outSize;
	return false;
}

bool MOTD_BrowserOpensFiles( void )
{
	return false;
}

#elif ( defined( __linux__ ) && !defined( __ANDROID__ )) || defined( __FreeBSD__ ) || defined( __NetBSD__ ) || defined( __OpenBSD__ )

// Linux and the BSDs: the browser of the desktop is opened by xdg-open (xdg-utils), in a process of its own

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

bool MOTD_OpenInBrowser( const char *target )
{
	if( !target || !target[0] )
		return false;

	const pid_t child = fork();

	if( child < 0 )
		return false;

	if( child == 0 )
	{
		// the child starts the opener and leaves at once, so the game never waits for the browser and no
		// zombie process is left: the opener is adopted by init
		if( fork() == 0 )
		{
			setsid();

			// the libraries of the game must not be loaded into the browser
			unsetenv( "LD_PRELOAD" );
			unsetenv( "LD_LIBRARY_PATH" );

			const int null = open( "/dev/null", O_RDWR );
			if( null >= 0 )
			{
				dup2( null, 0 );
				dup2( null, 1 );
				dup2( null, 2 );
				if( null > 2 )
					close( null );
			}

			execlp( "xdg-open", "xdg-open", target, (char *)NULL );
			_exit( 127 ); // no xdg-open
		}
		_exit( 0 );
	}

	int status = 0;
	waitpid( child, &status, 0 );
	return true;
}

// The page is saved to $XDG_RUNTIME_DIR/cs16_motd.html (a folder of the user only), or to a new file in the folder
// of the temporary files ($TMPDIR or /tmp) made by mkstemps, so another user can't put a link there instead
bool MOTD_SaveTempHtml( const char *html, size_t len, char *outPath, size_t outSize )
{
	char path[4096];
	int fd = -1;
	const char *runtime = getenv( "XDG_RUNTIME_DIR" );

	if( runtime && runtime[0] == '/' && snprintf( path, sizeof( path ), "%s/cs16_motd.html", runtime ) < (int)sizeof( path ))
		fd = open( path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600 );

	if( fd < 0 )
	{
		// the file of the previous MOTD is removed, the folder is not filled with them
		static char previous[4096];
		if( previous[0] )
		{
			unlink( previous );
			previous[0] = 0;
		}

		const char *tmp = getenv( "TMPDIR" );
		if( !tmp || tmp[0] != '/' )
			tmp = "/tmp";

		if( snprintf( path, sizeof( path ), "%s/cs16_motd_XXXXXX.html", tmp ) >= (int)sizeof( path ))
			return false;

		fd = mkstemps( path, 5 );
		if( fd < 0 )
			return false;

		snprintf( previous, sizeof( previous ), "%s", path );
	}

	size_t written = 0;
	while( written < len )
	{
		const ssize_t n = write( fd, html + written, len - written );
		if( n <= 0 )
			break;
		written += (size_t)n;
	}
	close( fd );

	if( written != len || strlen( path ) + 1 > outSize )
		return false;

	memcpy( outPath, path, strlen( path ) + 1 );
	return true;
}

#else // not implemented on other platforms yet

bool MOTD_OpenInBrowser( const char *target )
{
	(void)target;
	return false;
}

bool MOTD_SaveTempHtml( const char *html, size_t len, char *outPath, size_t outSize )
{
	(void)html; (void)len; (void)outPath; (void)outSize;
	return false;
}

#endif

#if !defined( __ANDROID__ )
// an HTML MOTD can be saved to a file and opened (everywhere but Android)
bool MOTD_BrowserOpensFiles( void )
{
	return true;
}
#endif
