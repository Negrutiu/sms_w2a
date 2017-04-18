
// Marius Negrutiu (marius.negrutiu@gmail.com)
// 2017/03/10

#pragma once

#include <list>
#include <vector>

// MSXML
#import <msxml3.dll> ///raw_interfaces_only
using namespace MSXML2;


//+ SMS
/// Generic SMS structure
struct SMS {

	FILETIME Timestamp;
	bool IsIncoming;
	bool IsRead;
	_bstr_t Text;
	std::vector<_bstr_t> PhoneNo;

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


//+ contacts+message backup (Windows Phone)
/// https://www.microsoft.com/en-us/store/p/contacts-message-backup/9nblgggz57gm
HRESULT Read_CMBK(
	_In_ LPCTSTR pszFile,
	_Out_ SMS_LIST& SmsList
	);


//+ SMS Backup & Restore (Android)
/// https://play.google.com/store/apps/details?id=com.riteshsahu.SMSBackupRestore
HRESULT Write_SMSBR(
	_In_ LPCTSTR pszFile,
	_In_ const SMS_LIST& SmsList,
	_In_opt_ WCHAR **ppszComment			/// List of LPCWSTR ended with a NULL pointer
	);
