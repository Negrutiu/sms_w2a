/*
	Utility functions and macros

	If building without CRT (/NODEFAULTLIBS) make sure you add "/Gs999999"
	as compiler (command line) option, 	otherwise you'll get linker error.

	Marius Negrutiu (Feb 2015)
	marius.negrutiu@gmail.com
*/

#pragma once
#ifndef _UTILS_UTILS_H_
#define _UTILS_UTILS_H_

#include <tchar.h>
#include <strsafe.h>
#include <assert.h>
#if !defined(VS_FILE_INFO)
	#include <VerRsrc.h>
#endif


//++ Strings

/// Compare strings, case insensitive
#define EqualStr(psz1, psz2) \
	(CompareString( 0, NORM_IGNORECASE, (psz1), -1, (psz2), -1 ) == CSTR_EQUAL)

#define EqualStrA(psz1, psz2) \
	(CompareStringA( 0, NORM_IGNORECASE, (psz1), -1, (psz2), -1 ) == CSTR_EQUAL)

/// Compare strings, case insensitive, specified length
#define EqualStrN(psz1, psz2, len) \
	(CompareString( 0, NORM_IGNORECASE, (psz1), (int)(len), (psz2), (int)(len)) == CSTR_EQUAL)


//++ Dynamic functions

// Declare a function prototype
// Example:
//    FUNC_DECLARE(BOOL, WINAPI, IsWow64Process, (HANDLE hProcess, PBOOL Wow64Process));
// Will expand to:
//    typedef BOOL( WINAPI *TfnIsWow64Process )(HANDLE hProcess, PBOOL Wow64Process));
//    TfnIsWow64Process fnIsWow64Process = NULL;
#define FUNC_DECLARE( ReturnVal, CallConv, Name, ArgList ) \
	typedef ReturnVal( CallConv *Tfn##Name ) ArgList; \
	Tfn##Name pfn##Name = NULL

// Call GetProcAddress() to retrieve function's address
#define FUNC_LOAD( ModuleName, Name ) \
	(pfn##Name ? pfn##Name : (pfn##Name = (Tfn##Name)GetProcAddress( GetModuleHandle(_T( #ModuleName )), #Name )))

#define FUNC_LOADH( ModuleHandle, Name ) \
	(pfn##Name ? pfn##Name : (pfn##Name = (Tfn##Name)GetProcAddress( ModuleHandle, #Name )))

#ifdef _UNICODE
	/// Load functions adding Unicode suffix "W" (Ex: FUNC_LOAD_UNICODESUFFIX(LoadLibrary) would load LoadLibraryW)
	#define FUNC_LOAD_UNICODESUFFIX( ModuleName, Name ) \
		(pfn##Name ? pfn##Name : (pfn##Name = (Tfn##Name)GetProcAddress( GetModuleHandle(_T( #ModuleName )), #Name "W" )))

	#define FUNC_LOADH_UNICODESUFFIX( ModuleHandle, Name ) \
		(pfn##Name ? pfn##Name : (pfn##Name = (Tfn##Name)GetProcAddress( ModuleHandle, #Name "W" )))
#else
	/// Load functions adding ANSI suffix "A" (Ex: UNC_LOAD_UNICODESUFFIX(LoadLibrary) would load LoadLibraryA)
	#define FUNC_LOAD_UNICODESUFFIX( ModuleName, Name ) \
		(pfn##Name ? pfn##Name : (pfn##Name = (Tfn##Name)GetProcAddress( GetModuleHandle(_T( #ModuleName )), #Name "A" )))

	#define FUNC_LOADH_UNICODESUFFIX( ModuleHandle, Name ) \
		(pfn##Name ? pfn##Name : (pfn##Name = (Tfn##Name)GetProcAddress( ModuleHandle, #Name "A" )))
#endif

// Function pointer
// Example: FUNC_PTR(LoadLibrary)( _T("my.dll") );
#define FUNC_PTR( Name ) \
	pfn##Name


//++ Output

//+ verify
#if !defined(verify)
#if _DEBUG || DBG
	#define verify(expr) assert(expr)
#else
	#define verify(expr) ((void)(expr))
#endif
#endif

//+ UtlDebugString
/// OutputDebugString with formatted text
/// pszFormat can be a null-terminated string, or the identifier of a string resource
/// If it's a string ID, the first variable argument is expected to be the HINSTANCE that contains the string resource
/// Examples:
///   UtlDebugString( _T("My string %d, %d"), 111, 222 );
///   UtlDebugString( MAKEINTRESOURCE(IDS_MYSTR), hMyResModule, 111, 222 );
static VOID UtlDebugString( _In_ LPCTSTR pszFormat, _In_opt_ ... )
{
	if (pszFormat && (IS_INTRESOURCE( pszFormat ) || *pszFormat)) {

		TCHAR szStr[1024];		/// Enough? Dynamic?
		LPTSTR psz = szStr;
		size_t len = ARRAYSIZE( szStr );

		va_list args;
		va_start( args, pszFormat );

		szStr[0] = _T( '\0' );
		StringCchPrintfEx( psz, len, &psz, &len, 0, _T( "[Th:%04x] " ), GetCurrentThreadId() );
		if (IS_INTRESOURCE( pszFormat )) {

			// Resource string
			int iLen;
			LPCTSTR pszResStr = NULL;

			HINSTANCE hResModule = *(HINSTANCE*)args;
			if (!hResModule)
				hResModule = GetModuleHandle( NULL );
			args += sizeof( hResModule );	/// Skip the first argument (HINSTANCE)

			iLen = LoadString( hResModule, PtrToUint( pszFormat ), (LPTSTR)&pszResStr, 0 );
			if (iLen > 0) {
				/// If the specified length is 0, then pszResStr receives a read-only pointer to the resource itself
				/// However, the string is not null terminated so we must extract it to a temporary string
				LPTSTR pszResFmt = (LPTSTR)LocalAlloc( LMEM_FIXED, (iLen + 1) * sizeof( TCHAR ) );
				if (pszResFmt) {
					StringCchCopyN( pszResFmt, iLen + 1, pszResStr, iLen );
					StringCchVPrintfEx( psz, len, &psz, &len, STRSAFE_IGNORE_NULLS | STRSAFE_FILL_ON_FAILURE, pszResFmt, args );
					LocalFree( pszResFmt );
				}

			} else {
				/// I don't know, do something?...
			}

		} else {
			// Explicit string
			StringCchVPrintfEx( psz, len, &psz, &len, STRSAFE_IGNORE_NULLS | STRSAFE_FILL_ON_FAILURE, pszFormat, args );
		}

		va_end( args );

		// Output
		OutputDebugString( szStr );
	}
}

#if _DEBUG || DBG
	#define DebugUtlDebugString UtlDebugString
#else
	#define DebugUtlDebugString(...)
#endif


//+ UtlMessageBox
/// MessageBox with formatted text
/// Both lpCaption and lpTextFmt can be null-terminated strings, or the identifier of a string resource
/// lpIcon can be the path to an icon file, or the identifier of an icon resource
/// If any of them are IDs, the first variable argument is expected to be the HINSTANCE that contains the string/icon resources
/// Examples:
///   UtlMessageBox( hParent, _T("MyCaption"), MB_ICONINFORMATION, NULL, _T("MyText: %d, %d"), 111, 222 );
///   UtlMessageBox( hParent, MAKEINTRESOURCE(IDS_MYCAPTION), MB_ICONINFORMATION, MAKEINTRESOURCE(IDI_MYICON), MAKEINTRESOURCE(IDS_MYTEXT_FMT), hResModule, 111, 222 );
static int WINAPI UtlMessageBox(
	_In_opt_ HWND hWnd,
	_In_ UINT uType,
	_In_opt_ LPCTSTR lpIcon,
	_In_opt_ LPCTSTR lpCaption,
	_In_opt_ LPCTSTR lpTextFmt, _In_opt_ ...
	)
{
	if (lpTextFmt && (IS_INTRESOURCE( lpTextFmt ) || *lpTextFmt)) {

		HINSTANCE hResModule = NULL;

		TCHAR szStr[1024];		/// Enough? Dynamic?
		LPTSTR psz = szStr;
		size_t len = ARRAYSIZE( szStr );

		va_list args;
		va_start( args, lpTextFmt );

		// Any resources?
		if ((lpCaption && IS_INTRESOURCE( lpCaption )) || IS_INTRESOURCE( lpTextFmt ) || (lpIcon && IS_INTRESOURCE( lpIcon ))) {
			hResModule = *(HINSTANCE*)args;
			if (!hResModule)
				hResModule = GetModuleHandle( NULL );
			args += sizeof( hResModule );		/// Skip the first argument (HINSTANCE)
#if _DEBUG || DBG
			{
				TCHAR szModule[MAX_PATH];
				assert( GetModuleFileName( hResModule, szModule, ARRAYSIZE( szModule ) > 0 ) );
			}
#endif
		}

		// Text
		szStr[0] = _T( '\0' );
		if (IS_INTRESOURCE( lpTextFmt )) {

			/// Resource string
			LPCTSTR pszResStr = NULL;
			int iLen = LoadString( hResModule, PtrToUint( lpTextFmt ), (LPTSTR)&pszResStr, 0 );
			if ((iLen > 0) && pszResStr) {

				/// If the specified length is 0, then pszResStr receives a read-only pointer to the resource itself
				/// However, the string is not null terminated so we must extract it to a temporary string
				LPTSTR pszTextFmt = (LPTSTR)LocalAlloc( LMEM_FIXED, (iLen + 1) * sizeof( TCHAR ) );
				if (pszTextFmt) {
					StringCchCopyN( pszTextFmt, iLen + 1, pszResStr, iLen );
					StringCchVPrintfEx( psz, len, &psz, &len, STRSAFE_IGNORE_NULLS | STRSAFE_FILL_ON_FAILURE, pszTextFmt, args );
					LocalFree( pszTextFmt );
				}
			}

		} else {
			/// Explicit string
			StringCchVPrintfEx( psz, len, &psz, &len, STRSAFE_IGNORE_NULLS | STRSAFE_FILL_ON_FAILURE, lpTextFmt, args );
		}

		va_end( args );

		// MessageBox
		{
			MSGBOXPARAMS mbp;
			ZeroMemory( &mbp, sizeof( mbp ) );
			mbp.cbSize = sizeof( mbp );
			mbp.hwndOwner = hWnd;
			mbp.hInstance = hResModule;
			mbp.lpszText = szStr;
			mbp.lpszCaption = lpCaption;
			mbp.lpszIcon = lpIcon;
			mbp.dwStyle = uType;
			if (lpIcon) {
				mbp.dwStyle &= ~MB_ICONMASK;		/// Remove any other icon flags
				mbp.dwStyle |= MB_USERICON;
			}
			return MessageBoxIndirect( &mbp );
		}

	}
	return -1;
}

#if _DEBUG || DBG
	#define DebugUtlMessageBox UtlMessageBox
#else
	#define DebugUtlMessageBox(...)
#endif


//++ GDI utils

// Rectangle
#define RCW(rc)			((rc).right - (rc).left)
#define RCH(rc)			((rc).bottom - (rc).top)
#define RCLTWH(rc)		(rc).left, (rc).top, RCW(rc), RCH(rc)

// Standard DPI -> Current DPI
static LONG UtlDpiX( _In_ HWND hWnd, _In_ LONG x )
{
	if (hWnd) {
		HDC dc = GetDC( hWnd );
		if (dc) {
			x = MulDiv( x, GetDeviceCaps( dc, LOGPIXELSX ), 96 );
			ReleaseDC( hWnd, dc );
		}
	}
	return x;
}

// Standard DPI -> Current DPI
static LONG UtlDpiY( _In_ HWND hWnd, _In_ LONG y )
{
	if (hWnd) {
		HDC dc = GetDC( hWnd );
		if (dc) {
			y = MulDiv( y, GetDeviceCaps( dc, LOGPIXELSY ), 96 );
			ReleaseDC( hWnd, dc );
		}
	}
	return y;
}


//++ Others


//+ UtlFormatError
/// Converts error code to string message
/// An empty string is returned if the error code is unknown
static LPTSTR UtlFormatError( _In_ DWORD err, _Out_ LPTSTR pszError, _In_ ULONG iErrorLen )
{
	if (pszError && iErrorLen) {

		DWORD iLen;
		ULONG_PTR pArgs[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};	/// Fake arguments. Some messages need them (ex: ERROR_WRONG_DISK(34))

		/// System
		if ((iLen = FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ARGUMENT_ARRAY, NULL, err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), pszError, iErrorLen, (va_list*)pArgs )) == 0) {
			pszError[0] = _T( '\0' );

			// ntdll.dll
			if ((iLen = FormatMessage( FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY, GetModuleHandle( _T( "ntdll" ) ), err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), pszError, iErrorLen, (va_list*)pArgs )) == 0) {
				pszError[0] = _T( '\0' );

				/// wininet.dll
				#define MY_INTERNET_ERROR_BASE  12000
				#define MY_INTERNET_ERROR_LAST  MY_INTERNET_ERROR_BASE + 500
				if (err >= MY_INTERNET_ERROR_BASE && err < MY_INTERNET_ERROR_LAST) {
					if ((iLen = FormatMessage( FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_ARGUMENT_ARRAY, GetModuleHandle( _T( "wininet" ) ), err, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), pszError, iErrorLen, (va_list*)pArgs )) == 0)
						pszError[0] = _T( '\0' );
				} else {
					/// Try others...
				}
			}
		}
		if (iLen > 0) {
			/// Trim trailing noise
			for (iLen--; (iLen > 0) && (pszError[iLen] == _T( '.' ) || pszError[iLen] == _T( ' ' ) || pszError[iLen] == _T( '\r' ) || pszError[iLen] == _T( '\n' )); iLen--)
				pszError[iLen] = _T( '\0' );
			iLen++;
		}
	}
	return pszError;	/// Return input buffer
}


//+ UtlReadVersionString
/// If you need to call this routine repeatedly it's advisable to preload Version.dll yourself, then unload it when the job is done
/// Otherwise, each call will load and unload it...
static DWORD UtlReadVersionString(
	_In_opt_ LPCTSTR szFile,			/// If NULL, we'll read from the current executable
	_In_     LPCTSTR szStringName,		/// "FileVersion", "FileDescription", etc.
	_Out_    LPTSTR szStringValue,
	_In_     UINT iStringValueLen
	)
{
	DWORD err = ERROR_SUCCESS;
	if (szStringValue)
		szStringValue[0] = _T( '\0' );
	if (szStringName && *szStringName && szStringValue && (iStringValueLen > 0)) {

		TCHAR szPath[MAX_PATH];
		HMODULE hVersionDll = NULL;

		szPath[0] = 0;
		if (GetSystemDirectory( szPath, ARRAYSIZE( szPath ) ) > 0) {
			/// Specify version.dll full path, to avoid malware DLL injection
			StringCchCat( szPath, ARRAYSIZE( szPath ), _T( "\\Version.dll" ) );
			hVersionDll = LoadLibrary( szPath );
		}

		if (hVersionDll) {

			DWORD dwVerInfoSize;

			FUNC_DECLARE( DWORD, APIENTRY, GetFileVersionInfoSize, (_In_ LPCTSTR lptstrFilename, _Out_opt_ LPDWORD lpdwHandle) );
			FUNC_DECLARE( BOOL, APIENTRY, GetFileVersionInfo, (_In_ LPCTSTR lptstrFilename, _In_ DWORD dwHandle, _In_ DWORD dwLen, _Out_ LPVOID lpData) );
			FUNC_DECLARE( BOOL, APIENTRY, VerQueryValue, (_In_ VOID *pBlock, _In_ LPCTSTR lpSubBlock, _Out_ LPVOID * lplpBuffer, _Out_ PUINT puLen) );

			FUNC_LOADH_UNICODESUFFIX( hVersionDll, GetFileVersionInfoSize );
			FUNC_LOADH_UNICODESUFFIX( hVersionDll, GetFileVersionInfo );
			FUNC_LOADH_UNICODESUFFIX( hVersionDll, VerQueryValue );

			/// If no file was specified, use the current executable
			if (szFile && *szFile) {
				StringCchCopy( szPath, ARRAYSIZE( szPath ), szFile );
			} else {
				GetModuleFileName( NULL, szPath, ARRAYSIZE( szPath ) );
			}

			/// Read the version information to memory
			dwVerInfoSize = FUNC_PTR(GetFileVersionInfoSize)(szPath, NULL );
			if (dwVerInfoSize > 0) {

				HANDLE hMem = GlobalAlloc( GMEM_MOVEABLE, dwVerInfoSize );
				if (hMem) {

					LPBYTE pMem = (LPBYTE)GlobalLock( hMem );
					if (pMem) {

						if (FUNC_PTR(GetFileVersionInfo)(szPath, 0, dwVerInfoSize, pMem )) {

							typedef struct _LANGANDCODEPAGE {
								WORD wLanguage;
								WORD wCodePage;
							} LANGANDCODEPAGE;

							LANGANDCODEPAGE *pCodePage;
							UINT iCodePageSize = sizeof( *pCodePage );

							/// Determine the language
							if (FUNC_PTR(VerQueryValue)( pMem, _T( "\\VarFileInfo\\Translation" ), (LPVOID*)&pCodePage, &iCodePageSize )) {

								TCHAR szTemp[255];
								LPCTSTR szValue = NULL;
								UINT iValueLen = 0;

								/// Read version info string
								StringCchPrintf( szTemp, ARRAYSIZE( szTemp ), _T( "\\StringFileInfo\\%04x%04x\\%s" ), pCodePage->wLanguage, pCodePage->wCodePage, szStringName );
								if (FUNC_PTR(VerQueryValue)( pMem, szTemp, (LPVOID*)&szValue, &iValueLen )) {

									/// Output
									if (*szValue) {
										StringCchCopy( szStringValue, iStringValueLen, szValue );
										if (iValueLen > iStringValueLen) {
											/// The output buffer is too small
											/// We'll return the string truncated, and ERROR_MORE_DATA error code
											err = ERROR_MORE_DATA;
										}

									} else {
										err = ERROR_NOT_FOUND;
									}
								} else {
									err = ERROR_NOT_FOUND;
								}
							} else {
								err = ERROR_NOT_FOUND;
							}
						} else {
							err = GetLastError();	/// GetFileVersionInfo
						}

						GlobalUnlock( hMem );

					} else {
						err = GetLastError();	/// GlobalLock
					}

					GlobalFree( hMem );

				} else {
					err = GetLastError();	/// GlobalAlloc
				}
			} else {
				err = GetLastError();	/// GetFileVersionInfoSize
			}

			FreeLibrary( hVersionDll );

		} else {
			err = GetLastError();	/// LoadLibrary
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}


//++ UtlReadFixedVersionInfo
/// If you need to call this routine repeatedly it's advisable to preload Version.dll yourself, then unload it when the job is done
/// Otherwise, each call will load and unload it...
static DWORD UtlReadFixedVersionInfo(
	_In_opt_ LPCTSTR szFile,
	_Out_ VS_FIXEDFILEINFO *pFixedInfo
	)
{
	DWORD err = ERROR_SUCCESS;

	// Validate parameters
	if (pFixedInfo) {

		TCHAR szPath[MAX_PATH];
		HMODULE hVersionDll = NULL;

		szPath[0] = 0;
		if (GetSystemDirectory( szPath, ARRAYSIZE( szPath ) ) > 0) {
			/// Specify version.dll full path, to avoid malware DLL injection
			StringCchCat( szPath, ARRAYSIZE( szPath ), _T( "\\Version.dll" ) );
			hVersionDll = LoadLibrary( szPath );
		}

		if (hVersionDll) {

			DWORD dwVerInfoSize;

			FUNC_DECLARE( DWORD, APIENTRY, GetFileVersionInfoSize, (_In_ LPCTSTR lptstrFilename, _Out_opt_ LPDWORD lpdwHandle) );
			FUNC_DECLARE( BOOL, APIENTRY, GetFileVersionInfo, (_In_ LPCTSTR lptstrFilename, _In_ DWORD dwHandle, _In_ DWORD dwLen, _Out_ LPVOID lpData) );
			FUNC_DECLARE( BOOL, APIENTRY, VerQueryValue, (_In_ VOID *pBlock, _In_ LPCTSTR lpSubBlock, _Out_ LPVOID * lplpBuffer, _Out_ PUINT puLen) );

			FUNC_LOADH_UNICODESUFFIX( hVersionDll, GetFileVersionInfoSize );
			FUNC_LOADH_UNICODESUFFIX( hVersionDll, GetFileVersionInfo );
			FUNC_LOADH_UNICODESUFFIX( hVersionDll, VerQueryValue );

			/// If no file was specified, use the current executable
			if (szFile && *szFile) {
				StringCchCopy( szPath, ARRAYSIZE( szPath ), szFile );
			} else {
				GetModuleFileName( NULL, szPath, ARRAYSIZE( szPath ) );
			}

			/// Read the version information to memory
			dwVerInfoSize = FUNC_PTR( GetFileVersionInfoSize )(szPath, NULL);
			if (dwVerInfoSize > 0) {

				HANDLE hMem = GlobalAlloc( GMEM_MOVEABLE, dwVerInfoSize );
				if (hMem) {

					LPBYTE pMem = (LPBYTE)GlobalLock( hMem );
					if (pMem) {

						if (FUNC_PTR( GetFileVersionInfo )( szPath, 0, dwVerInfoSize, pMem )) {

							/// Query
							VS_FIXEDFILEINFO *pMemFfi = NULL;
							UINT iSize = sizeof( pMemFfi );	/// size of pointer!
							if (FUNC_PTR( VerQueryValue )( pMem, _T( "\\" ), (LPVOID*)&pMemFfi, &iSize )) {

								/// Output
								CopyMemory( pFixedInfo, pMemFfi, iSize );

							} else {
								err = ERROR_NOT_FOUND;
							}
						} else {
							err = GetLastError();		/// GetFileVersionInfo
						}

#if defined (_DEBUG)
						ZeroMemory( pMem, dwVerInfoSize );
						pMem = NULL;
#endif
						GlobalUnlock( hMem );

					} else {
						err = GetLastError();	/// GlobalLock
					}

					GlobalFree( hMem );

				} else {
					err = GetLastError();	/// GlobalAlloc
				}
			} else {
				err = GetLastError();	/// GetFileVersionInfoSize
			}

			FreeLibrary( hVersionDll );

		} else {
			err = GetLastError();	/// LoadLibrary
		}
	} else {
		err = ERROR_INVALID_PARAMETER;
	}
	return err;
}

#endif	/// _UTILS_UTILS_H_