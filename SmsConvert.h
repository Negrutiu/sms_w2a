
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
		for (size_t i = 0; i < size(); i++)
			if (at( i ) == '\r')
				erase( begin() + i );
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
	_Out_ ULONG &iType,					/// 0=Unknown, 1=CMBK, 2=SMSBR
	_Out_ ULONG &iMessageCount,
	_Out_ std::string &sComments
);


//+ contacts+message backup (Windows Phone)
/// https://www.microsoft.com/en-us/store/p/contacts-message-backup/9nblgggz57gm

ULONG Read_CMBK( _In_ LPCTSTR pszFile, _Out_ SMS_LIST& SmsList );
ULONG Write_CMBK( _In_ LPCTSTR pszFile, _In_ const SMS_LIST& SmsList );


//+ SMS Backup & Restore (Android)
/// https://play.google.com/store/apps/details?id=com.riteshsahu.SMSBackupRestore

ULONG Read_SMSBR( _In_ LPCTSTR pszFile, _Out_ SMS_LIST& SmsList );
ULONG Write_SMSBR( _In_ LPCTSTR pszFile, _In_ const SMS_LIST& SmsList );
