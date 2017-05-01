
// Marius Negrutiu (marius.negrutiu@gmail.com)
// 2017/03/10

#include "StdAfx.h"
#include "SmsConvert.h"
#include <algorithm>
#include "rapidxml\rapidxml.hpp"
#include "rapidxml\rapidxml_utils.hpp"
#include "rapidxml\rapidxml_print.hpp"


#define SMS_APP_NAME "sms_w2a"
#define SMS_APP_LINK "https://github.com/negrutiu/sms_w2a"
CHAR g_szAppName[50] = SMS_APP_NAME;


//++ SmsSetAppName
VOID SmsSetAppName( _In_ LPCSTR pszName )
{
	if (pszName && *pszName) {
		StringCchCopyA( g_szAppName, ARRAYSIZE( g_szAppName ), pszName );
	} else {
		StringCchCopyA( g_szAppName, ARRAYSIZE( g_szAppName ), SMS_APP_NAME );
	}
}


//++ SmsCount
ULONG SmsCount( const SMS_LIST &SmsList )
{
	ULONG n = 0;
	for (auto it = SmsList.begin(); it != SmsList.end(); ++it)
		n += (ULONG)it->PhoneNo.size();
	return n;
}


//++ SmsGetFileSummary
ULONG SmsGetFileSummary(
	_In_ LPCTSTR pszFile,
	_Out_ ULONG &iType,					/// 0=Unknown, 1=CMBK, 2=SMSBR
	_Out_ ULONG &iMessageCount,
	_Out_ std::string &sComments
)
{
	ULONG err = ERROR_SUCCESS;

	if (!pszFile || !*pszFile)
		return ERROR_INVALID_PARAMETER;

	iType = 0;
	iMessageCount = 0;
	sComments.clear();

	try {

		CHAR szFileA[MAX_PATH];
		WideCharToMultiByte( CP_ACP, 0, pszFile, -1, szFileA, ARRAYSIZE( szFileA ), NULL, NULL );
		rapidxml::file<> FileObj( szFileA );

		rapidxml::xml_document<> Doc;
		Doc.parse<rapidxml::parse_comment_nodes>( FileObj.data() );

		rapidxml::xml_node<> *Root;
		if ((Root = Doc.first_node( "ArrayOfMessage" ))) {

			/// "contacts+message backup" format
			iType = 1;
			for (auto n = Doc.first_node(); n; n = n->next_sibling()) {
				if (n->type() == rapidxml::node_comment) {
					if (!sComments.empty())
						sComments += "\n";
					sComments += n->value();
				} else if (n->type() == rapidxml::node_element) {
					break;
				}
			}
			iMessageCount = (ULONG)rapidxml::count_children( Root );

		} else if ((Root = Doc.first_node( "smses" ))) {

			/// "SMS Backup & Restore" format
			iType = 2;
			for (auto n = Doc.first_node(); n; n = n->next_sibling()) {
				if (n->type() == rapidxml::node_comment) {
					if (!sComments.empty())
						sComments += "\n";
					sComments += n->value();
				} else if (n->type() == rapidxml::node_element) {
					break;
				}
			}
			iMessageCount = (ULONG)rapidxml::count_children( Root );
		}

	} catch (const rapidxml::parse_error &e) {
		UNREFERENCED_PARAMETER( e );
		///e.what();
		err = ERROR_INVALID_DATA;
	} catch (...) {
		err = ERROR_INVALID_DATA;
	}

	return err;
}


//!++ FILETIME <-> POSIX

//? NOTES:
//? * FILETIME is represented as 100ns intervals from 1 Jan 1601 (Windows epoch)
//? * POSIX time is usually represented as *microseconds* (see struct timeval {} in BSD headers) from 1 Jan 1970 (Unix epoch)
//? * Android messages are stored as *milliseconds* from 1 Jan 1970
//? (Marius)

//? LEGEND:
//? * Seconds       1
//? * Milliseconds  1000
//? * Microseconds  1000000
//? * Nanoseconds   1000000000

//? LINKS:
//? * https://en.wikipedia.org/wiki/Unix_time
//? * http://stackoverflow.com/questions/6161776/convert-windows-filetime-to-second-in-unix-linux
//? * http://stackoverflow.com/questions/3585583/convert-unix-linux-time-to-windows-filetime

#define EPOCH_DIFFms (11644473600LL * 1000LL)		/// Milliseconds(1/1/1970 - 1/1/1601)

time_t FILETIME_to_POSIXms( FILETIME ft )
{
	time_t tm = *(time_t*)&ft / 10000;				/// 100ns -> ms
	tm -= EPOCH_DIFFms;								/// Win epoch -> Unix epoch
	return tm;
}

FILETIME POSIXms_to_FILETIME( time_t tm )
{
	tm += EPOCH_DIFFms;								/// Unix epoch -> Win epoch
	tm *= 10000;									/// ms -> 100ns
	return *(FILETIME*)&tm;
}



//++ ReadFileToString
///  The memory buffer must be HeapFree()-d when no longer needed
LPSTR ReadFileToString( _In_ LPCTSTR pszFile )
{
	LPSTR psz = NULL;
	if (pszFile && *pszFile) {
		HANDLE h = CreateFile( pszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
		if (h != INVALID_HANDLE_VALUE) {
			ULONG iBufSize = GetFileSize( h, NULL );
			if (iBufSize != INVALID_FILE_SIZE) {
				if ((psz = (LPSTR)HeapAlloc( GetProcessHeap(), 0, iBufSize + 1 )) != NULL) {
					DWORD dwBytes;
					if (ReadFile( h, psz, iBufSize, &dwBytes, NULL )) {
						psz[dwBytes] = ANSI_NULL;
						// Success
					} else {
						HeapFree( GetProcessHeap(), 0, psz );
						psz = NULL;
					}
				}
			}
			CloseHandle( h );
		}
	}
	return psz;
}


//!++ "contacts+message backup" format
/// <ArrayOfMessage ...>
///		<Message>
///			<Recepients/>
///			<Body>Message Text</Body>
///			<IsIncoming>true</IsIncoming>
///			<IsRead>true</IsRead>
///			<Attachments/>
///			<LocalTimestamp>131329631293736951</LocalTimestamp>
///			<Sender>+00000000000</Sender>
///		</Message>
///		<Message>
///			<Recepients>
///				<string>+00000000000</string>
///				<string>+00000000001</string>
///				<string>+00000000002</string>
///			</Recepients>
///			<Body>Message Text</Body>
///			<IsIncoming>false</IsIncoming>
///			<IsRead>true</IsRead>
///			<Attachments/>
///			<LocalTimestamp>131329488070946809</LocalTimestamp>
///			<Sender/>
///		</Message>
///	</ArrayOfMessage>

//++ Read_CMBK
ULONG Read_CMBK( _In_ LPCTSTR pszFile, _Out_ SMS_LIST& SmsList )
{
	ULONG err = ERROR_SUCCESS;

	if (!pszFile || !*pszFile)
		return ERROR_INVALID_PARAMETER;

	try {

		CHAR szFileA[MAX_PATH];
		WideCharToMultiByte( CP_ACP, 0, pszFile, -1, szFileA, ARRAYSIZE( szFileA ), NULL, NULL );
		rapidxml::file<> FileObj( szFileA );

		rapidxml::xml_document<> Doc;
		Doc.parse<0>( FileObj.data() );

		rapidxml::xml_node<> *Root = Doc.first_node( "ArrayOfMessage" );
		if (Root) {

			for (auto n = Root->first_node( "Message", 0, false ); n; n = n->next_sibling( "Message", 0, false )) {

				rapidxml::xml_node<> *NodeBody = n->first_node( "Body", 0, false );
				rapidxml::xml_node<> *NodeIn = n->first_node( "IsIncoming", 0, false );
				rapidxml::xml_node<> *NodeRead = n->first_node( "IsRead", 0, false );
				rapidxml::xml_node<> *NodeTime = n->first_node( "LocalTimestamp", 0, false );
				rapidxml::xml_node<> *NodeFrom = n->first_node( "Sender", 0, false );
				rapidxml::xml_node<> *NodeTo = n->first_node( "Recepients", 0, false );

				if (NodeIn && NodeBody) {
					
					SMS sms;

					sms.IsIncoming = EqualStrA( NodeIn->value(), "true" );
					sms.Text = NodeBody->value();
					sms.Text.StripLF();
					sms.IsRead = NodeRead ? EqualStrA( NodeRead->value(), "true" ) : true;
					
					if (NodeTime) {
						StrToInt64ExA( NodeTime->value(), STIF_DEFAULT, (PLONGLONG)&sms.Timestamp );
					} else {
						sms.Timestamp.dwHighDateTime = sms.Timestamp.dwLowDateTime = 0;
					}

					if (sms.IsIncoming && NodeFrom) {

						sms.PhoneNo.push_back( NodeFrom->value() );

					} else if (!sms.IsIncoming && NodeTo) {

						/// Multiple recepients
						for (auto to = NodeTo->first_node( "string", 0, false ); to; to = to->next_sibling( "string", 0, false ))
							sms.PhoneNo.push_back( to->value() );

					} else {
						/// Malformed/Incomplete node
						continue;
					}

					SmsList.push_back( sms );
				}
			}

			// Remove duplicates
			SmsList.erase( std::unique( SmsList.begin(), SmsList.end() ), SmsList.end() );

		} else {
			err = ERROR_INVALID_DATA;
		}

	} catch (const rapidxml::parse_error &e) {
		UNREFERENCED_PARAMETER( e );
		///e.what();
		err = ERROR_INVALID_DATA;
	} catch (...) {
		err = ERROR_INVALID_DATA;
	}

	return err;
}


//++ Write_CMBK
ULONG Write_CMBK( _In_ LPCTSTR pszFile, _In_ const SMS_LIST& SmsList )
{
	ULONG err = ERROR_SUCCESS;

	if (!pszFile || !*pszFile)
		return ERROR_INVALID_PARAMETER;

	try {

		rapidxml::xml_document<> Doc;
		rapidxml::xml_node<> *Root, *Node, *SubNode;
		CHAR szBuf[255];

		Node = Doc.allocate_node( rapidxml::node_declaration );
		Node->append_attribute( Doc.allocate_attribute( "version", "1.0" ) );
		Node->append_attribute( Doc.allocate_attribute( "encoding", "UTF-8" ) );
		Node->append_attribute( Doc.allocate_attribute( "standalone", "yes" ) );
		Doc.append_node( Node );

		SYSTEMTIME st;
		GetLocalTime( &st );
		StringCchPrintfA(
			szBuf, ARRAYSIZE( szBuf ),
			" File created by %s on %hu/%02hu/%02hu %02hu:%02hu:%02hu ",
			g_szAppName,
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond
		);
		Doc.append_node( Doc.allocate_node( rapidxml::node_comment, "", Doc.allocate_string( szBuf ) ) );
		Doc.append_node( Doc.allocate_node( rapidxml::node_comment, "", " " SMS_APP_LINK " " ) );

		// Root node
		Root = Doc.allocate_node( rapidxml::node_element, "ArrayOfMessage" );
		Doc.append_node( Root );

		// SMS nodes
		for (auto it = SmsList.begin(); it != SmsList.end(); ++it) {

			Root->append_node( (Node = Doc.allocate_node( rapidxml::node_element, "Message" )) );

			Node->append_node( (SubNode = Doc.allocate_node( rapidxml::node_element, "Recepients" )) );
			if (!it->IsIncoming) {
				for (auto to = it->PhoneNo.begin(); to != it->PhoneNo.end(); to++)
					SubNode->append_node( Doc.allocate_node( rapidxml::node_element, "string", *to ) );
			}

			Node->append_node( Doc.allocate_node( rapidxml::node_element, "Body", it->Text ) );
			Node->append_node( Doc.allocate_node( rapidxml::node_element, "IsIncoming", it->IsIncoming ? "true" : "false" ) );
			Node->append_node( Doc.allocate_node( rapidxml::node_element, "IsRead", it->IsRead ? "true" : "false" ) );
			Node->append_node( Doc.allocate_node( rapidxml::node_element, "Attachments" ) );

			StringCchPrintfA( szBuf, ARRAYSIZE( szBuf ), "%I64u", it->Timestamp );
			Node->append_node( Doc.allocate_node( rapidxml::node_element, "LocalTimestamp", Doc.allocate_string( szBuf ) ) );

			Node->append_node( Doc.allocate_node( rapidxml::node_element, "Sender", it->IsIncoming ? (it->PhoneNo.empty() ? "" : it->PhoneNo.front()) : "" ) );
		}

		// xml -> File
		CHAR szFileA[MAX_PATH];
		WideCharToMultiByte( CP_UTF8, 0, pszFile, -1, szFileA, ARRAYSIZE( szFileA ), NULL, NULL );
		std::ofstream fout( szFileA );
		if (!fout.bad()) {
			rapidxml::print( std::ostream_iterator<char>( fout ), Doc, 0 );
			fout.close();
		}

	} catch (const rapidxml::parse_error &e) {
		UNREFERENCED_PARAMETER( e );
		///e.what();
		err = ERROR_INVALID_DATA;
	} catch (...) {
		err = ERROR_INVALID_DATA;
	}

	return err;
}


//!++ "SMS Backup & Restore" format
/// <smses count = "2" backup_set = "cb081d84-aca6-4a12-ab0d-30cfdcf1891f" backup_date = "1488996424918">
/// 	<sms protocol = "0" address = "+40000000000" date = "1488572656975" type = "1" subject = "null" body = "Message Text" toa = "null" sc_toa = "null" service_center = "+40770000053" read = "1" status = "-1" locked = "0" date_sent = "1488572650000" readable_date = "Mar 3, 2017 22:24:16" contact_name = "First Last Name" / >
/// 	<sms protocol = "0" address = "+40000000000" date = "1488572942710" type = "2" subject = "null" body = "Message Text" toa = "null" sc_toa = "null" service_center = "null" read = "1" status = "-1" locked = "0" date_sent = "0" readable_date = "Mar 3, 2017 22:29:02" contact_name = "First Last Name" / >
/// </smses>


//++ Read_SMSBR
ULONG Read_SMSBR( _In_ LPCTSTR pszFile, _Out_ SMS_LIST& SmsList )
{
	ULONG err = ERROR_SUCCESS;

	if (!pszFile || !*pszFile)
		return ERROR_INVALID_PARAMETER;

	try {

		CHAR szFileA[MAX_PATH];
		WideCharToMultiByte( CP_ACP, 0, pszFile, -1, szFileA, ARRAYSIZE( szFileA ), NULL, NULL );
		rapidxml::file<> FileObj( szFileA );

		rapidxml::xml_document<> Doc;
		Doc.parse<0>( FileObj.data() );

		rapidxml::xml_node<> *Root = Doc.first_node( "smses" );
		if (Root) {

			for (auto n = Root->first_node( "sms" ); n; n = n->next_sibling( "sms" )) {

				rapidxml::xml_attribute<> *AttrAddr = n->first_attribute( "address" );
				rapidxml::xml_attribute<> *AttrDate = n->first_attribute( "date" );
				rapidxml::xml_attribute<> *AttrBody = n->first_attribute( "body" );
				rapidxml::xml_attribute<> *AttrType = n->first_attribute( "type" );
				rapidxml::xml_attribute<> *AttrRead = n->first_attribute( "read" );

				if (AttrAddr && AttrDate && AttrBody && AttrType && AttrRead) {

					SMS sms;

					sms.PhoneNo.push_back( AttrAddr->value() );
					sms.IsIncoming = EqualStrA( AttrType->value(), "1" );		/// Incoming=1, Outgoing=2
					sms.IsRead = !EqualStrA( AttrRead->value(), "0" );			/// Unread=0, Read=1

					time_t tm;
					StrToInt64ExA( AttrDate->value(), STIF_DEFAULT, (PLONGLONG)&tm );
					sms.Timestamp = POSIXms_to_FILETIME( tm );
					sms.Text = AttrBody->value();
					sms.Text.StripLF();

					/// Aggregate outgoing messages sent to multiple recepients
					if (!sms.IsIncoming &&
						!SmsList.empty() &&
						!SmsList.back().IsIncoming &&
						*(PULONG64)&SmsList.back().Timestamp == *(PULONG64)&sms.Timestamp &&
						EqualStrA( SmsList.back().Text, sms.Text ))
					{
						SmsList.back().PhoneNo.push_back( sms.PhoneNo.front() );
					} else {
						SmsList.push_back( sms );
					}
				}
			}

		} else {
			err = ERROR_INVALID_DATA;
		}

	} catch (const rapidxml::parse_error &e) {
		UNREFERENCED_PARAMETER( e );
		///e.what();
		err = ERROR_INVALID_DATA;
	} catch (...) {
		err = ERROR_INVALID_DATA;
	}

	return err;
}


//++ Write_SMSBR
ULONG Write_SMSBR( _In_ LPCTSTR pszFile, _In_ const SMS_LIST& SmsList )
{
	ULONG err = ERROR_SUCCESS;

	if (!pszFile || !*pszFile)
		return ERROR_INVALID_PARAMETER;

	try {

		rapidxml::xml_document<> Doc;
		rapidxml::xml_node<> *Root, *Node;
		CHAR szBuf[128];

		//? "SMS Backup & Restore" expects a precise .xml layout in order to list it correctly
		//? If conditions are not met, the converted .xml file might be displayed at the end of the backup list, with a timestamp somewhere in the 70s
		//? However, even if the timestamp looks bad, messages *can* be restored...

		//? Layout:
		/// <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
		/// <!--File created by XXX on YYY-->
		/// <?xml-stylesheet type="text/xsl" href="sms.xsl"?>
		/// <smses count="xxx" backup_set = "GUID" backup_date = "1493574946759">
		///    <sms [...] />
		///    <sms [...] />
		/// </smses>

		//? Rules:
		//? * The nodes "xml", comment, "xml-stylesheet", "smses" must appear in this precise order
		//? * There must *not* be multiple comment nodes
		//? * The comment node must *not* have additional leading whitespaces (such as "<--   File created [...]   -->"

		Node = Doc.allocate_node( rapidxml::node_declaration );
		Node->append_attribute( Doc.allocate_attribute( "version", "1.0" ) );
		Node->append_attribute( Doc.allocate_attribute( "encoding", "UTF-8" ) );
		Node->append_attribute( Doc.allocate_attribute( "standalone", "yes" ) );
		Doc.append_node( Node );

		SYSTEMTIME st;
		GetLocalTime( &st );
		StringCchPrintfA(
			szBuf, ARRAYSIZE( szBuf ),
			"File created by %s on %hu/%02hu/%02hu %02hu:%02hu:%02hu, %s",
			g_szAppName,
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
			SMS_APP_LINK
		);
		Doc.append_node( Doc.allocate_node( rapidxml::node_comment, "", Doc.allocate_string( szBuf ) ) );

		Doc.append_node( Doc.allocate_node( rapidxml::node_pi, "xml-stylesheet", "type=\"text/xsl\" href=\"sms.xsl\"" ) );

		// Root node
		Root = Doc.allocate_node( rapidxml::node_element, "smses" );
		Doc.append_node( Root );

		StringCchPrintfA( szBuf, ARRAYSIZE( szBuf ), "%u", SmsCount( SmsList ) );	/// SmsCount() is aware of multiple contacts!
		Root->append_attribute( Doc.allocate_attribute( "count", Doc.allocate_string( szBuf ) ) );

		UUID uuid;
		RPC_WSTR szuuid = NULL;
		UuidCreate( &uuid );
		UuidToString( &uuid, &szuuid );
		WideCharToMultiByte( CP_UTF8, 0, (LPCWSTR)szuuid, -1, szBuf, ARRAYSIZE( szBuf ), NULL, NULL );
		RpcStringFree( &szuuid );
		Root->append_attribute( Doc.allocate_attribute( "backup_set", Doc.allocate_string( szBuf ) ) );

		FILETIME ft;
		GetSystemTime( &st );
		SystemTimeToFileTime( &st, &ft );
		time_t tm = FILETIME_to_POSIXms( ft );
		StringCchPrintfA( szBuf, ARRAYSIZE( szBuf ), "%I64u", tm );
		Root->append_attribute( Doc.allocate_attribute( "backup_date", Doc.allocate_string( szBuf ) ) );

		// SMS nodes
		for (auto it = SmsList.begin(); it != SmsList.end(); ++it) {

			/// Outgoing message may have multiple recepients
			/// "Clone" the same message for each contact
			for (auto itPhoneNo = it->PhoneNo.begin(); itPhoneNo != it->PhoneNo.end(); ++itPhoneNo) {

				Root->append_node( (Node = Doc.allocate_node( rapidxml::node_element, "sms" )) );

				Node->append_attribute( Doc.allocate_attribute( "protocol", "0" ) );
				Node->append_attribute( Doc.allocate_attribute( "address", *itPhoneNo ) );

				time_t tm = FILETIME_to_POSIXms( it->Timestamp );
				StringCchPrintfA( szBuf, ARRAYSIZE( szBuf ), "%I64u", tm );
				Node->append_attribute( Doc.allocate_attribute( "date", Doc.allocate_string( szBuf ) ) );

				Node->append_attribute( Doc.allocate_attribute( "type", it->IsIncoming ? "1" : "2" ) );
				Node->append_attribute( Doc.allocate_attribute( "subject", "null" ) );
				Node->append_attribute( Doc.allocate_attribute( "body", it->Text ) );
				Node->append_attribute( Doc.allocate_attribute( "read", it->IsRead ? "1" : "0" ) );
				Node->append_attribute( Doc.allocate_attribute( "date_sent", "" ) );

				FileTimeToSystemTime( &it->Timestamp, &st );
				StringCchPrintfA( szBuf, ARRAYSIZE( szBuf ), "%hu/%02hu/%02hu %02hu:%02hu:%02hu", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond );
				Node->append_attribute( Doc.allocate_attribute( "readable_date", Doc.allocate_string( szBuf ) ) );
			}
		}

		// xml -> File
		CHAR szFileA[MAX_PATH];
		WideCharToMultiByte( CP_UTF8, 0, pszFile, -1, szFileA, ARRAYSIZE( szFileA ), NULL, NULL );
		std::ofstream fout( szFileA );
		if (!fout.bad()) {
			rapidxml::print(
				std::ostream_iterator<char>( fout ),
				Doc,
				rapidxml::print_no_surrogate_expansion		/// Don't expand the special emoji characters used by "SMS Backup & Restore"
			);
			fout.close();
		}

	} catch (const rapidxml::parse_error &e) {
		UNREFERENCED_PARAMETER( e );
		///e.what();
		err = ERROR_INVALID_DATA;
	} catch (...) {
		err = ERROR_INVALID_DATA;
	}

	return err;
}
