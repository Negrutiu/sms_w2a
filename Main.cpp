#include "StdAfx.h"
#include "Main.h"
#include "Resource.h"
#include "SmsConvert.h"
#include <functional>


HINSTANCE g_hInst = NULL;


//+ Definitions
#define PROP_HACCEL _T( "m_hAccel" )

void OnButtonAbout( _In_ HWND hDlg );
void OnButtonBrowseCMBK( _In_ HWND hDlg );
void OnButtonConvertW2A( _In_ HWND hDlg );


//++ TranslateAcceleratorKey
BOOL TranslateAcceleratorKey( __in LPMSG msg )
{
	/// Walk the chain of parent dialogs
	/// Stop when any of them translates the message
	HWND hWnd;
	for (hWnd = msg->hwnd; hWnd; hWnd = GetAncestor( hWnd, GA_PARENT )) {
		WNDPROC fnDlgProc = (WNDPROC)GetWindowLongPtr( hWnd, DWLP_DLGPROC );
		if (fnDlgProc && CallWindowProc( fnDlgProc, hWnd, WM_TRANSLATE_ACCELERATOR_KEY, 0, (LPARAM)msg ))
			return TRUE;
	}
	return FALSE;
}


//++ TranslateDialogKey
BOOL TranslateDialogKey( __in LPMSG msg )
{
	/// Walk the chain of parent dialogs
	/// Stop when any of them translates the message
	/// NOTE: IsDialogMessage seems to always return TRUE
	HWND hWnd;
	for (hWnd = msg->hwnd; hWnd; hWnd = GetAncestor( hWnd, GA_PARENT )) {
		WNDPROC fnDlgProc = (WNDPROC)GetWindowLongPtr( hWnd, DWLP_DLGPROC );
		if (fnDlgProc && CallWindowProc( fnDlgProc, hWnd, WM_TRANSLATE_DIALOG_KEY, 0, (LPARAM)msg ))
			return TRUE;
	}
	return FALSE;
}


//++ DialogTitle
LPCTSTR DialogTitle( _In_ HWND hDlg )
{
	static TCHAR szTitle[255];
	szTitle[0] = _T( '\0' );
	GetWindowText( hDlg, szTitle, ARRAYSIZE( szTitle ) );
	return szTitle;
}


//++ GetInputFile
void GetInputFile( _In_ HWND hDlg, _Out_ TCHAR szFile[MAX_PATH] )
{
	assert( hDlg && IsWindow( hDlg ) );
	assert( szFile );

	GetDlgItemText( hDlg, IDC_EDIT_CMBK, szFile, MAX_PATH );

	StrTrim( szFile, _T( " \r\n" ) );
	PathUnquoteSpaces( szFile );
	StrTrim( szFile, _T( " " ) );
}


//++ SetOutputFile
void SetOutputFile( _In_ HWND hDlg )
{
	TCHAR szInput[MAX_PATH], szOutput[MAX_PATH];

	szOutput[0] = 0;
	GetInputFile( hDlg, szInput );
	if (PathFileExists( szInput )) {

		StringCchCopy( szOutput, ARRAYSIZE( szOutput ), szInput );
		PathRemoveExtension( szOutput );

		SYSTEMTIME st;
		GetLocalTime( &st );

		TCHAR szSuffix[30];
		StringCchPrintf( szSuffix, ARRAYSIZE( szSuffix ), _T( "-%hu%02hu%02hu" ), st.wYear, st.wMonth, st.wDay );
		StringCchCat( szOutput, ARRAYSIZE( szOutput ), szSuffix );

		PathAddExtension( szOutput, _T( ".xml" ) );
	}

	SetDlgItemText( hDlg, IDC_EDIT_SMSBR, szOutput );
}


//++ DialogProc
INT_PTR CALLBACK DialogProc( __in HWND hDlg, __in UINT iMsg, __in WPARAM wParam, __in LPARAM lParam )
{
	switch (iMsg)
	{
		/// Custom message called to translate accelerator keys
		case WM_TRANSLATE_ACCELERATOR_KEY:
			return TranslateAccelerator( hDlg, (HACCEL)GetProp( hDlg, PROP_HACCEL ), (LPMSG)lParam );

		/// Custom message called to translate dialog keys (such as Tab, Escape, etc.)
		case WM_TRANSLATE_DIALOG_KEY:
			return IsDialogMessage( hDlg, (LPMSG)lParam );

		case WM_INITDIALOG:
		{
			// Icons
			HICON hIco;
			hIco = (HICON)LoadImage( g_hInst, MAKEINTRESOURCE( IDI_MAIN ), IMAGE_ICON, GetSystemMetrics( SM_CXSMICON ), GetSystemMetrics( SM_CYSMICON ), 0 );
			assert( hIco && "Small icon" );
			SendMessage( hDlg, WM_SETICON, ICON_SMALL, (LPARAM)hIco );

			hIco = (HICON)LoadImage( g_hInst, MAKEINTRESOURCE( IDI_MAIN ), IMAGE_ICON, GetSystemMetrics( SM_CXICON ), GetSystemMetrics( SM_CYICON ), 0 );
			assert( hIco && "Big icon" );
			SendMessage( hDlg, WM_SETICON, ICON_BIG, (LPARAM)hIco );

			// Accelerators
			HACCEL hAccel = LoadAccelerators( g_hInst, MAKEINTRESOURCE( IDR_ACCEL ) );
			assert( hAccel );
			if (hAccel)
				SetProp( hDlg, PROP_HACCEL, hAccel );

			// About box
			HMENU hMenu = GetSystemMenu( hDlg, FALSE );
			if (hMenu) {
				InsertMenu( hMenu, 0, MF_BYCOMMAND | MF_ENABLED | MF_SEPARATOR, -1, NULL );
				InsertMenu( hMenu, 0, MF_BYCOMMAND | MF_ENABLED | MF_STRING, IDM_ABOUT, _T( "About\tF1" ) );	/// TODO: Move to string table
				DestroyMenu( hMenu );
			}

			// Placement
			HKEY hKey;
			if (RegOpenKeyEx( HKEY_CURRENT_USER, REGKEY, 0, KEY_READ, &hKey ) == ERROR_SUCCESS) {
				WINDOWPLACEMENT wp;
				DWORD dwType, dwSize = sizeof( wp );
				if (RegQueryValueEx( hKey, _T( "Position" ), NULL, &dwType, (LPBYTE)&wp, &dwSize ) == ERROR_SUCCESS && (dwType == REG_BINARY) && (dwSize == sizeof( wp ))) {
					///SetWindowPlacement( hDlg, &wp );
					SetWindowPos( hDlg, NULL, wp.rcNormalPosition.left, wp.rcNormalPosition.top, 0, 0, SWP_NOZORDER | SWP_NOSIZE );
				}
				RegCloseKey( hKey );
			}

			TCHAR szVersion[50];
			if (UtlReadVersionString( NULL, _T( "FileVersion" ), szVersion, ARRAYSIZE( szVersion ) ) == ERROR_SUCCESS) {
				
				// Display the version in the title bar
				TCHAR szTitle[255];
				int iTitleLen = GetWindowText( hDlg, szTitle, ARRAYSIZE( szTitle ) );
				if (iTitleLen > 0) {
					StringCchPrintf( szTitle + iTitleLen, ARRAYSIZE( szTitle ) - iTitleLen, _T( " (v%s)" ), szVersion );
					SetWindowText( hDlg, szTitle );
				}

				// Initialize the app name written in .xml comments
				CHAR szAppName[50];
				szAppName[0] = ANSI_NULL;
				StringCchPrintfA( szAppName, ARRAYSIZE( szAppName ), "sms_w2a %ws", szVersion );
				SmsSetAppName( szAppName );
			}

			// Arrange controls
			///RECT rc;
			///GetClientRect( hDlg, &rc );
			///SendMessage( hDlg, WM_SIZE, SIZE_RESTORED, MAKELPARAM( rc.right - rc.left, rc.bottom - rc.top ) );

			return (INT_PTR)TRUE;
		}

		case WM_DESTROY:
		{
			// Accelerators
			HACCEL hAccel = (HACCEL)RemoveProp( hDlg, PROP_HACCEL );
			if (hAccel)
				DestroyAcceleratorTable( hAccel );

			// Icons
			assert( SendMessage( hDlg, WM_GETICON, ICON_SMALL, 0 ) );
			assert( SendMessage( hDlg, WM_GETICON, ICON_BIG, 0 ) );
			DestroyIcon( (HICON)SendMessage( hDlg, WM_GETICON, ICON_SMALL, 0 ) );
			DestroyIcon( (HICON)SendMessage( hDlg, WM_GETICON, ICON_BIG, 0 ) );

			// Placement
			HKEY hKey;
			WINDOWPLACEMENT wp = {sizeof( wp ), 0};
			GetWindowPlacement( hDlg, &wp );
			if (RegCreateKeyEx( HKEY_CURRENT_USER, REGKEY, 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL ) == ERROR_SUCCESS) {
				RegSetValueEx( hKey, _T( "Position" ), 0, REG_BINARY, (LPBYTE)&wp, (DWORD)sizeof( wp ) );
				RegCloseKey( hKey );
			}

			break;
		}

		case WM_SIZE:
			if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
				//! TODO: Arrange controls
				///int w = GET_X_LPARAM( lParam );
				///int h = GET_Y_LPARAM( lParam );
			}
			break;

		case WM_COMMAND:
		{
			switch (LOWORD( wParam ))
			{
				case IDOK:
				case IDCANCEL:
				case IDM_EXIT:
				{
					/// Destroy the dialog
					DestroyWindow( hDlg );

					/// Terminate app
					PostQuitMessage( LOWORD( wParam ) );
					break;
				}

				case IDM_ABOUT:
					OnButtonAbout( hDlg );
					break;

				case IDC_BUTTON_CMBK:
					OnButtonBrowseCMBK( hDlg );
					break;

				case IDC_BUTTON_W2A:
					OnButtonConvertW2A( hDlg );
					break;

				case IDC_EDIT_CMBK:
					if (HIWORD( wParam ) == EN_KILLFOCUS)
						SetOutputFile( hDlg );		/// Input filename -> Output filename
					break;
			}
			break;
		}

		case WM_SYSCOMMAND:
		{
			if (wParam == IDM_ABOUT)
				SendMessage( hDlg, WM_COMMAND, IDM_ABOUT, 0 );
			break;
		}
	}

	return (INT_PTR)FALSE;
}


//++ OnButtonAbout
void OnButtonAbout( _In_ HWND hDlg )
{
	TCHAR szVersion[50];
	TCHAR szDescription[255];
	TCHAR szComments[255];
	TCHAR szCopyright[255];
	
	UtlReadVersionString( NULL, _T( "FileVersion" ), szVersion, ARRAYSIZE( szVersion ) );
	UtlReadVersionString( NULL, _T( "FileDescription" ), szDescription, ARRAYSIZE( szDescription ) );
	UtlReadVersionString( NULL, _T( "CompanyName" ), szComments, ARRAYSIZE( szComments ) );
	UtlReadVersionString( NULL, _T( "LegalCopyright" ), szCopyright, ARRAYSIZE( szCopyright ) );
	
	UtlMessageBox( hDlg, MB_OK, MAKEINTRESOURCE( IDI_MAIN ), DialogTitle( hDlg ), _T( "%s\n%s\n\n%s\n%s" ), g_hInst, szDescription, szVersion, szComments, szCopyright );
}


//++ OnButtonBrowseCMBK
void OnButtonBrowseCMBK( _In_ HWND hDlg )
{
	TCHAR szInput[MAX_PATH];
	GetInputFile( hDlg, szInput );

	TCHAR szFilter[50];
	memcpy( szFilter, _T( "*.msg, *.xml\0*.msg;*.xml\0*.*\0*.*\0\0" ), 34 * sizeof( TCHAR ) );

	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof( ofn );
	ofn.hwndOwner = hDlg;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = szInput;
	ofn.nMaxFile = ARRAYSIZE( szInput );
	ofn.Flags = OFN_ENABLESIZING | OFN_FILEMUSTEXIST | OFN_LONGNAMES;

	if (GetOpenFileName( &ofn )) {
		SetDlgItemText( hDlg, IDC_EDIT_CMBK, szInput );
		SetOutputFile( hDlg );		/// Input filename -> Output filename
	}
}


//++ OnButtonConvertW2A
void OnButtonConvertW2A( _In_ HWND hDlg )
{
	HRESULT hr;

	TCHAR szInput[MAX_PATH], szOutput[MAX_PATH];
	szOutput[0] = 0;

	GetInputFile( hDlg, szInput );
	GetDlgItemText( hDlg, IDC_EDIT_SMSBR, szOutput, ARRAYSIZE( szOutput ) );

	if (!*szInput) {
		UtlMessageBox( hDlg, MB_OK, MAKEINTRESOURCE( IDI_MAIN ), DialogTitle( hDlg ), _T( ":/\nHow about choosing a file..." ), g_hInst );
		return;
	}

	if (!PathFileExists( szOutput ) ||
		UtlMessageBox( hDlg, MB_YESNO | MB_ICONQUESTION, NULL, DialogTitle( hDlg ), _T( "\"%s\" already exists\nOverwrite?" ), PathFindFileName( szOutput ) ) == IDYES)
	{

		SMS_LIST SmsList;
		hr = Read_CMBK( szInput, SmsList );
		//hr = Read_SMSBR( szInput, SmsList );
		if (SUCCEEDED( hr )) {

			/// Sort chronologically
			//SmsList.sort( std::greater<SMS>() );		/// Newest messages first (CMBK)
			SmsList.sort( std::less<SMS>() );			/// Oldest messages first (SMSBR)

			///for (auto it = SmsList.begin(); it != SmsList.end(); it++) {
			///	SYSTEMTIME st;
			///	FileTimeToSystemTime( &it->Timestamp, &st );
			///	UtlDebugString(
			///		_T( "[%hu/%02hu/%02hu %02hu:%02hu:%02hu.%03hu] %s %s %ws \"%ws\"\n" ),
			///		st.wYear, st.wMonth, st.wDay,
			///		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
			///		it->IsIncoming ? _T( "IN" ) : _T( "OUT" ),
			///		it->IsRead ? _T( "Read" ) : _T( "Unread" ),
			///		it->PhoneNo.GetBSTR(),
			///		it->Text.GetBSTR()
			///	);
			///}

			//hr = Write_CMBK( szOutput, SmsList );
			hr = Write_SMSBR( szOutput, SmsList );
		}

		// Message
		if (SUCCEEDED( hr )) {
			UtlMessageBox( hDlg, MB_OK, MAKEINTRESOURCE( IDI_MAIN ), DialogTitle( hDlg ), _T( "Successfully converted %u messages\nEnjoy!" ), g_hInst, SmsCount( SmsList ) );	// SmsCount() is aware of multiple contacts
		} else {
			TCHAR szErr[128];
			UtlMessageBox( hDlg, MB_OK | MB_ICONSTOP, NULL, DialogTitle( hDlg ), _T( "%s\nError 0x%x" ), UtlFormatError( hr, szErr, ARRAYSIZE( szErr ) ), hr );
		}
	}
}


//++ WinMain
int APIENTRY _tWinMain( __in HINSTANCE hInstance, __in HINSTANCE hPrevInstance, __in LPTSTR lpCmdLine, __in int nCmdShow )
{
	DWORD err = ERROR_SUCCESS;

	UNREFERENCED_PARAMETER( hInstance );
	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( lpCmdLine );

	/// Instance
	g_hInst = GetModuleHandle( NULL );

	/// Common controls
	InitCommonControls();

	/// Main dialog (modeless)
	HWND hDlg = CreateDialog( g_hInst, MAKEINTRESOURCE( IDD_MAIN ), NULL, DialogProc );
	if (hDlg) {

		/// Show
		if (!IsWindowVisible( hDlg ))
			ShowWindow( hDlg, nCmdShow );

		/// Message loop
		MSG msg;
		while (GetMessage( &msg, NULL, 0, 0 )) {

			if ((msg.message < WM_KEYFIRST || msg.message > WM_KEYLAST) ||
				(!TranslateAcceleratorKey( &msg ) && !TranslateDialogKey( &msg )))
			{
				TranslateMessage( &msg );
				DispatchMessage( &msg );
			}
		}
		err = (DWORD)msg.wParam;		/// Exit code (WM_QUIT parameter)

		assert( !IsWindow( hDlg ) );

	} else {
		err = GetLastError();
	}

	return (int)err;
}
