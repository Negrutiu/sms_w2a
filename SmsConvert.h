
// Marius Negrutiu (marius.negrutiu@gmail.com)
// 2017/03/10

#pragma once

#include <list>
#include <vector>
#include <string>

//+ class utf8string
class utf8string: public std::string
{
public:
	utf8string() { }

	utf8string( const char* psz ): std::string( psz ) { }
	operator const char*() const { return c_str(); }

	void StripLF()
	{
		for (int i = (int)size() - 1; i >= 0; i--)
			if (at( i ) == '\r')
				erase( begin() + i );
	}

	DWORD SaveToFile( _In_ LPCTSTR pszFile ) const
	{
		DWORD err = ERROR_SUCCESS;
		if (pszFile && *pszFile) {
			HANDLE h = CreateFile( pszFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
			if (h != INVALID_HANDLE_VALUE) {
				DWORD iBytes;
				if (WriteFile( h, c_str(), (DWORD)size(), &iBytes, NULL )) {
					assert( iBytes == size() );
				} else {
					err = GetLastError();
				}
				CloseHandle( h );
			} else {
				err = GetLastError();
			}
		} else {
			err = ERROR_INVALID_PARAMETER;
		}
		return err;
	}

	DWORD LoadFromFile( _In_ LPCTSTR pszFile )
	{
		DWORD err = ERROR_SUCCESS;
		if (pszFile && *pszFile) {
			HANDLE h = CreateFile( pszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
			if (h != INVALID_HANDLE_VALUE) {
				DWORD iSize = GetFileSize( h, NULL );
				if (iSize != INVALID_FILE_SIZE) {
					resize( iSize );		/// Make room for the file content
					DWORD iBytes;
					if (ReadFile( h, (LPBYTE)c_str(), iSize, &iBytes, NULL )) {		/// Read directly to string's buffer ;)
						assert( iBytes == iSize );
						assert( iBytes == size() );
					} else {
						err = GetLastError();
					}
				} else {
					err = GetLastError();
				}
				CloseHandle( h );
			} else {
				err = GetLastError();
			}
		} else {
			err = ERROR_INVALID_PARAMETER;
		}
		return err;
	}
};


//+ SMS
/// Generic SMS structure
struct SMS {

	FILETIME Timestamp;
	bool IsIncoming;
	bool IsRead;
	utf8string Text;
	std::vector<utf8string> PhoneNo;

	void clear()
	{
		Timestamp.dwHighDateTime = Timestamp.dwLowDateTime = 0;
		IsIncoming = IsRead = true;
		Text.clear();
		PhoneNo.clear();
	}

	bool operator==( const SMS& second ) const {
		return
			Timestamp.dwHighDateTime == second.Timestamp.dwHighDateTime &&
			Timestamp.dwLowDateTime == second.Timestamp.dwLowDateTime &&
			IsIncoming == second.IsIncoming &&
			IsRead == second.IsRead &&
			Text == second.Text &&
			PhoneNo == second.PhoneNo
			;
	}

	bool operator<( const SMS& second ) const {
		return *((PULONG64)&Timestamp) < *((PULONG64)&second.Timestamp);
	}

	bool operator>(const SMS& second) const {
		return *((PULONG64)&Timestamp) > *((PULONG64)&second.Timestamp);
	}
};
typedef std::list<SMS> SMS_LIST;


//+ SmsSetAppName
/// Configure the app name, written to .xml comments. Default is "sms_w2a"
VOID SmsSetAppName( _In_ LPCSTR pszName );


//+ SmsCount
/// Compute message count, aware of messages sent to multiple contacts
ULONG SmsCount( const SMS_LIST &SmsList );


//+ SmsGetFileSummary
ULONG SmsGetFileSummary(
	_In_ LPCTSTR pszFile,
	_Out_ ULONG &iType,					/// 0=Unknown, 1=CMBK, 2=SMSBR, 3=NOKIA
	_Out_ ULONG &iMessageCount,
	_Out_ std::string &sComments
);

//+ SmsFormatStr
LPCSTR SmsFormatStr( _In_ ULONG iType );

//+ contacts+message backup (Windows Phone)
/// https://www.microsoft.com/en-us/store/p/contacts-message-backup/9nblgggz57gm

ULONG Read_CMBK( _In_ LPCTSTR pszFile, _Out_ SMS_LIST& SmsList );
ULONG Write_CMBK( _In_ LPCTSTR pszFile, _In_ const SMS_LIST& SmsList );
ULONG Compute_CMBK_Hash( _In_ LPCTSTR pszFile, _Out_ utf8string &Hash );


//+ SMS Backup & Restore (Android)
/// https://play.google.com/store/apps/details?id=com.riteshsahu.SMSBackupRestore

ULONG Read_SMSBR( _In_ LPCTSTR pszFile, _Out_ SMS_LIST& SmsList );
ULONG Write_SMSBR( _In_ LPCTSTR pszFile, _In_ const SMS_LIST& SmsList );

//+ Nokia Suite exported messages (Symbian)
/// https://en.wikipedia.org/wiki/Nokia_Suite

ULONG Read_NOKIA( _In_ LPCTSTR pszFile, _Out_ SMS_LIST& SmsList );
ULONG Write_NOKIA( _In_ LPCTSTR pszFile, _In_ const SMS_LIST& SmsList );
