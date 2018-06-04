
// Marius Negrutiu (marius.negrutiu@gmail.com)
// 2017/03/10

#include "StdAfx.h"
#include "SmsConvert.h"
#include <algorithm>
#include "rapidxml\rapidxml.hpp"
#include "rapidxml\rapidxml_utils.hpp"
#include "rapidxml\rapidxml_print.hpp"
#include <wincrypt.h>
#include "libcsv\csv.h"


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
	_Out_ ULONG &iType,					/// 0=Unknown, 1=CMBK, 2=SMSBR, 3=NOKIA
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

	// XML types
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

	// CSV types
	if (iType == 0) {

		utf8string s;
		if (s.LoadFromFile( pszFile ) == ERROR_SUCCESS) {

			typedef struct {
				int iCurFields;
				ULONG iRecords;
			} CTX;
			CTX ctx = {0};

			struct csv_parser csv;
			csv_init( &csv, 0 );
			if (csv_parse(
				&csv,
				s.c_str(), s.size(),
				[]( void *s, size_t len, void *pParam )
				{
					CTX *pctx = (CTX*)pParam;
					if (pctx->iCurFields == 0) {
						if (len == 3 && EqualStrNA( (LPCSTR)s, "sms", 3 )) {
							/// First field OK ("sms")
						} else {
							pctx->iCurFields = -1;			/// Invalidate current record
						}
					}
					if (pctx->iCurFields >= 0)
						pctx->iCurFields++;
				},
				[]( int ch, void *pParam )
				{
					CTX *pctx = (CTX*)pParam;
					if (pctx->iCurFields == 8)			/// Valid records only
						pctx->iRecords++;
					pctx->iCurFields = 0;
				},
				&ctx ) == s.size())
			{
				if (ctx.iRecords > 0) {
					iType = 3;		/// Nokia Suite CSV
					iMessageCount = ctx.iRecords;
				}

			} else {
				csv_strerror( csv_error( &csv ) );
			}
			csv_fini( &csv, NULL, NULL, NULL );
		}
	}

	return err;
}


//++ SmsFormatStr
LPCSTR SmsFormatStr( _In_ ULONG iType )
{
	switch (iType) {
		case 1: return "contact+messages backup";
		case 2: return "SMS Backup & Restore";
		case 3: return "Nokia Suite (exported messages)";
	}
	return NULL;
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

			rapidxml::print( std::ostream_iterator<char>( fout ), Doc, rapidxml::print_no_surrogate_expansion );
			fout.close();

			// Generate the hash file (.hsh) required by "contacts+message backup"
			utf8string sHsh;
			err = Compute_CMBK_Hash( pszFile, sHsh );
			if (err == ERROR_SUCCESS) {
				TCHAR szHshFile[MAX_PATH];
				StringCchCopy( szHshFile, ARRAYSIZE( szHshFile ), pszFile );
				PathRenameExtension( szHshFile, _T( ".hsh" ) );
				err = sHsh.SaveToFile( szHshFile );
				//+ Done
			}
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


//++ Compute_CMBK_Hash
//?+ https://github.com/gpailler/Android2Wp_SMSConverter/blob/master/converter.py
ULONG Compute_CMBK_Hash( _In_ LPCTSTR pszFile, _Out_ utf8string &Hash )
{
	DWORD err = ERROR_SUCCESS;

	CHAR base64_sha2[50];
	base64_sha2[0] = ANSI_NULL;

	Hash.clear();

	//! sha256(file)
	HANDLE h = CreateFile( pszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
	if (h != INVALID_HANDLE_VALUE) {

		ULONG iBufSize = 1024 * 16;		/// 16 KiB
		LPBYTE pBuf = (LPBYTE)HeapAlloc( GetProcessHeap(), 0, iBufSize );
		if (pBuf) {

			HCRYPTPROV hCryptProv = NULL;
			if (CryptAcquireContext( &hCryptProv, NULL, MS_ENH_RSA_AES_PROV_W, PROV_RSA_AES, CRYPT_VERIFYCONTEXT | CRYPT_SILENT )) {

				HCRYPTHASH hCryptHash = NULL;
				if (CryptCreateHash( hCryptProv, CALG_SHA_256, NULL, 0, &hCryptHash )) {

					DWORD iBytesRead;
					while (err == ERROR_SUCCESS) {
						if (ReadFile( h, pBuf, iBufSize, &iBytesRead, NULL )) {
							if (iBytesRead > 0) {
								if (CryptHashData( hCryptHash, pBuf, iBytesRead, 0 )) {
								} else {
									err = GetLastError();		/// CryptHashData
								}
							} else {
								break;	/// EOF
							}
						} else {
							err = GetLastError();		/// ReadFile
						}
					}
					if (err == ERROR_SUCCESS) {

						BYTE pHash[32];
						DWORD iHashSize = sizeof( pHash );
						if (CryptGetHashParam( hCryptHash, HP_HASHVAL, pHash, &iHashSize, 0 )) {

							//! base64(sha256(file))
							DWORD n = ARRAYSIZE( base64_sha2 );
							if (CryptBinaryToStringA( pHash, iHashSize, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64_sha2, &n )) {
								//+ Success
							} else {
								err = GetLastError();		/// CryptBinaryToStringA
							}
						} else {
							err = GetLastError();		/// CryptGetHashParam
						}
					}
					CryptDestroyHash( hCryptHash );

				} else {
					err = GetLastError();		/// CryptCreateHash
				}
				CryptReleaseContext( hCryptProv, 0 );

			} else {
				err = GetLastError();		/// CryptAquireContext
			}
			HeapFree( GetProcessHeap(), 0, pBuf );

		} else {
			err = GetLastError();		/// HeapAlloc
		}
		CloseHandle( h );
	} else {
		err = GetLastError();		/// CreateFile
	}

	//! aes128(base64(sha256(file)))
	if (err == ERROR_SUCCESS) {

		const ULONG AES128_BLOCK_SIZE = 16;		/// 16 bytes == 128 bits

		const GUID AesKey = {0xD86B2FDE, 0xC318, 0x4DD2,{0x8C, 0x9E, 0xEB, 0x3F, 0x1A, 0x24, 0x4D, 0xF8}};	/// {D86B2FDE-C318-4DD2-8C9E-EB3F1A244DF8}
		const GUID AesIV = {0x089B6AEC, 0xE81D, 0x49AC,{0x91, 0xDF, 0xAD, 0x07, 0x14, 0x18, 0xE7, 0xA3}};	/// {089B6AEC-E81D-49AC-91DF-AD071418E7A3}
		assert( sizeof( GUID ) == AES128_BLOCK_SIZE );

		typedef struct _AES_KEY_BLOB {
			BLOBHEADER hdr;
			DWORD keySize;
			BYTE bytes[AES128_BLOCK_SIZE];
		} AES_KEY_BLOB;

		AES_KEY_BLOB key;
		HCRYPTPROV hProv;
		HCRYPTKEY hKey;

		key.hdr.bType = PLAINTEXTKEYBLOB;
		key.hdr.bVersion = CUR_BLOB_VERSION;
		key.hdr.reserved = 0;
		key.hdr.aiKeyAlg = CALG_AES_128;
		key.keySize = AES128_BLOCK_SIZE;
		CopyMemory( key.bytes, &AesKey, AES128_BLOCK_SIZE );

		if (CryptAcquireContext( &hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT )) {
			if (CryptImportKey( hProv, (LPBYTE)&key, sizeof( key ), 0, 0, &hKey )) {

				DWORD iParam = CRYPT_MODE_CBC;
				CryptSetKeyParam( hKey, KP_MODE, (LPBYTE)&iParam, 0 );
				//? NOTE: WinCrypt is already using PKCS#1 padding by default (which is functionally equivalent to PKCS#7 (and PKCS#5) for AES128)
				///iParam = PKCS5_PADDING;
				///CryptSetKeyParam( hKey, KP_PADDING, (LPBYTE)&iParam, 0 );

				if (CryptSetKeyParam( hKey, KP_IV, (LPBYTE)&AesIV, 0 )) {		/// Initialization vector

					BYTE aes128[64];
					ULONG aes128size = lstrlenA( base64_sha2 );
					CopyMemory( aes128, base64_sha2, aes128size );

					if (CryptEncrypt( hKey, NULL, TRUE, 0, (LPBYTE)aes128, &aes128size, sizeof( aes128 ) )) {

						//! base64(aes128(base64(sha256(file))))
						CHAR sz[72];
						DWORD n = ARRAYSIZE( sz );
						if (CryptBinaryToStringA( aes128, aes128size, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, sz, &n )) {

							//+ Success
							Hash = sz;		/// Return value

						} else {
							err = GetLastError();	/// CryptBinaryToStringA
						}
					} else {
						err = GetLastError();	/// CryptEncrypt
					}
				} else {
					err = GetLastError();	/// CryptSetKeyParam
				}
				CryptDestroyKey( hKey );

			} else {
				err = GetLastError();	/// CryptImportKey
			}
			CryptReleaseContext( hProv, 0 );

		} else {
			err = GetLastError();	/// CryptAcquireContext
		}
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


//++ Read_NOKIA
ULONG Read_NOKIA( _In_ LPCTSTR pszFile, _Out_ SMS_LIST& SmsList )
{
	ULONG err = ERROR_SUCCESS;

	if (!pszFile || !*pszFile)
		return ERROR_INVALID_PARAMETER;

	utf8string s;
	if (s.LoadFromFile( pszFile ) == ERROR_SUCCESS) {

		//? Layout:
		/// Type,Action,From,To,?,Timestamp,?,Text
		/// "sms","RECEIVED","+0000000000","","","YYYY.MM.DD HH:mm","","Text1"
		/// "sms","READ,RECEIVED","+0000000000","","","YYYY.MM.DD HH:mm","","Text2"
		/// "sms","SENT","","+0000000000","","YYYY.MM.DD HH:mm","","Text3"

		typedef struct {
			int iCurFields;
			SMS sms;
			SMS_LIST *pSmsList;
		} CTX;

		CTX ctx;
		ctx.iCurFields = 0;
		ctx.pSmsList = &SmsList;

		struct csv_parser csv;
		csv_init( &csv, 0 );
		csv_set_delim( &csv, ',' );

		if (csv_parse(
			&csv,
			s.c_str(), s.size(),
			//+ CSV field callback
			[]( void *s, size_t len, void *pParam )
			{
				CTX *pctx = (CTX*)pParam;
				if (pctx->iCurFields >= 0) {		/// Record is still valid
					switch (pctx->iCurFields) {
						case 0:
							// "sms"
							if (len != 3 || !EqualStrNA( (LPCSTR)s, "sms", 3 ))
								pctx->iCurFields = -1;		/// Invalidate this record
							break;
						case 1:
						{
							// "RECEIVED", "RECEIVED,READ", "SENT"
							utf8string s2;
							s2.assign( (LPCSTR)s, (LPCSTR)s + len );

							pctx->sms.IsRead = (StrStrA( s2.c_str(), "READ" ) != NULL);
							pctx->sms.IsIncoming = (StrStrA( s2.c_str(), "RECEIVED" ) != NULL);
							///pctx->sms.IsIncoming = !(StrStrA( s2.c_str(), "SENT" ) != NULL);
							break;
						}
						case 2:
						{
							// From
							if (pctx->sms.IsIncoming) {
								utf8string s2;
								s2.assign( (LPCSTR)s, (LPCSTR)s + len );

								pctx->sms.PhoneNo.push_back( s2 );
							}
							break;
						}
						case 3:
						{
							// To
							if (!pctx->sms.IsIncoming) {
								utf8string s2;
								s2.assign( (LPCSTR)s, (LPCSTR)s + len );

								pctx->sms.PhoneNo.push_back( s2 );
							}
							break;
						}
						case 5:
						{
							// "YYYY.MM.DD HH:MM"
							LPCSTR psz = (LPCSTR)s;
							if (len == 16 && psz[4] == '.' && psz[7] == '.' && psz[10] == ' ' && psz[13] == ':') {

								utf8string s2;
								SYSTEMTIME st = {0};
								st.wYear   = StrToIntA( s2.assign( psz, psz + 4 ).c_str() );
								st.wMonth  = StrToIntA( s2.assign( psz + 5, psz + 7 ).c_str() );
								st.wDay    = StrToIntA( s2.assign( psz + 8, psz + 10 ).c_str() );
								st.wHour   = StrToIntA( s2.assign( psz + 11, psz + 13 ).c_str() );
								st.wMinute = StrToIntA( s2.assign( psz + 14, psz + 16 ).c_str() );

								SYSTEMTIME st2;
								TzSpecificLocalTimeToSystemTime( NULL, &st, &st2 );

								SystemTimeToFileTime( &st2, &pctx->sms.Timestamp );

							} else {
								pctx->iCurFields = -1;		/// Invalidate this record
							}
							break;
						}
						case 7:
							// Message text
							pctx->sms.Text.assign( (LPCSTR)s, (LPCSTR)s + len );
							break;
					}
					pctx->iCurFields++;
				}
			},
			//+ CSV record callback
			[]( int ch, void *pParam )
			{
				CTX *pctx = (CTX*)pParam;

				// Store this SMS
				if (pctx->iCurFields == 8)
					pctx->pSmsList->push_back( pctx->sms );

				// Context cleanup
				pctx->sms.clear();
				pctx->iCurFields = 0;
			},
			&ctx ) == s.size())
		{
			//? Success
		} else {
			csv_strerror( csv_error( &csv ) );
		}
		csv_fini( &csv, NULL, NULL, NULL );
	}

	return err;
}


//++ Write_NOKIA
ULONG Write_NOKIA( _In_ LPCTSTR pszFile, _In_ const SMS_LIST& SmsList )
{
	//TODO: ?
	return ERROR_NOT_SUPPORTED;
}
