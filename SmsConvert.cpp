
// Marius Negrutiu (marius.negrutiu@gmail.com)
// 2017/03/10

#include "StdAfx.h"
#include "SmsConvert.h"
#include <algorithm>
#include <time.h>


//++ Read_CMBK
HRESULT Read_CMBK( _In_ LPCTSTR pszFile, _Out_ SMS_LIST& SmsList )
{
	HRESULT hr = S_OK;

	if (!pszFile || !*pszFile)
		return E_INVALIDARG;

	try {

		IXMLDOMDocument2Ptr XmlDoc;
		hr = XmlDoc.CreateInstance( _T( "msxml2.domdocument" ) );
		if (SUCCEEDED( hr )) {

			XmlDoc->async = VARIANT_FALSE;
			XmlDoc->PutvalidateOnParse( VARIANT_TRUE );
			XmlDoc->PutresolveExternals( VARIANT_FALSE );

			if (XmlDoc->load( pszFile )) {

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
				///			</Recepients>
				///			<Body>Message Text</Body>
				///			<IsIncoming>false</IsIncoming>
				///			<IsRead>true</IsRead>
				///			<Attachments/>
				///			<LocalTimestamp>131329488070946809</LocalTimestamp>
				///			<Sender/>
				///		</Message>
				///	</ArrayOfMessage>

				MSXML2::IXMLDOMElementPtr RootNode = XmlDoc->GetdocumentElement();
				if (RootNode) {
					if (EqualStr( RootNode->tagName, _T( "ArrayOfMessage" ) )) {

						MSXML2::IXMLDOMNodeListPtr NodeList = RootNode->GetchildNodes();
						if (NodeList) {

							for (long i = 0; i < NodeList->length; i++) {

								MSXML2::IXMLDOMElementPtr AppNode = NodeList->item[i];
								if (AppNode) {

									if (EqualStr( AppNode->tagName, _T( "Message" ) )) {

										MSXML2::IXMLDOMNodeListPtr nodes;
										FILETIME Timestamp;
										bool bIncoming = false, bRead = false;
										BSTR bstrBody = NULL, bstrPhoneNo = NULL;

										// IsIncoming
										nodes = AppNode->getElementsByTagName( L"IsIncoming" );
										assert( nodes );
										if (nodes) {
											assert( nodes->length == 1 );
											if (nodes->length > 0) {
												BSTR bstr = L"";
												nodes->item[0]->get_text( &bstr );
												bIncoming = EqualStr( bstr, L"true" );
											}
											nodes.Release();
										}

										// IsRead
										nodes = AppNode->getElementsByTagName( L"IsRead" );
										assert( nodes );
										if (nodes) {
											assert( nodes->length == 1 );
											if (nodes->length > 0) {
												BSTR bstr = L"";
												nodes->item[0]->get_text( &bstr );
												bRead = EqualStr( bstr, L"true" );
											}
											nodes.Release();
										}

										// Body
										nodes = AppNode->getElementsByTagName( L"Body" );
										assert( nodes );
										if (nodes) {
											assert( nodes->length == 1 );
											if (nodes->length > 0)
												nodes->item[0]->get_text( &bstrBody );
											nodes.Release();
										}

										// Timestamp
										nodes = AppNode->getElementsByTagName( L"LocalTimestamp" );
										assert( nodes );
										if (nodes) {
											assert( nodes->length == 1 );
											if (nodes->length == 1) {
												BSTR bstr = L"";
												nodes->item[0]->get_text( &bstr );
												StrToInt64Ex( bstr, STIF_DEFAULT, (PLONGLONG)&Timestamp );
											}
											nodes.Release();
										}

										// PhoneNo
										if (bIncoming) {

											nodes = AppNode->getElementsByTagName( L"Sender" );
											assert( nodes );
											if (nodes) {
												assert( nodes->length == 1 );
												if (nodes->length == 1)
													nodes->item[0]->get_text( &bstrPhoneNo );
												nodes.Release();
											}

										} else {

											nodes = AppNode->getElementsByTagName( L"Recepients" );
											assert( nodes );
											if (nodes) {
												assert( nodes->length == 1 );
												if (nodes->length == 1) {
													assert( nodes->item[0]->GetchildNodes()->length == 1 );
													MSXML2::IXMLDOMNodePtr node2 = nodes->item[0]->GetfirstChild();
													if (node2)
														node2->get_text( &bstrPhoneNo );
												}
												nodes.Release();
											}
										}

										// Collect
										if (bstrBody && *bstrBody && bstrPhoneNo && *bstrPhoneNo) {
											SMS sms;
											sms.Timestamp = Timestamp;
											sms.IsIncoming = bIncoming;
											sms.IsRead = bRead;
											sms.Text = bstrBody;
											sms.PhoneNo = bstrPhoneNo;
											SmsList.push_back( sms );
										}
									}
								}
							}

							SmsList.erase( std::unique( SmsList.begin(), SmsList.end() ), SmsList.end() );
						}

					} else {
						hr = RPC_E_INVALID_DATA;
					}
				}
			} else {
				hr = HRESULT_FROM_WIN32( ERROR_INVALID_DATA );
			}
		}

	} catch (HRESULT hr2) {
		hr = hr2;
	} catch (...) {
		hr = E_UNEXPECTED;
	}

	return hr;
}


// http://stackoverflow.com/questions/6161776/convert-windows-filetime-to-second-in-unix-linux
///#define TICKS_PER_SECOND 10000000
///#define EPOCH_DIFFERENCE 11644473600LL
///time_t convertWindowsTimeToUnixTime( long long int input ) {
///	long long int temp;
///	temp = input / TICKS_PER_SECOND; //convert from 100ns intervals to seconds;
///	temp = temp - EPOCH_DIFFERENCE;  //subtract number of seconds between epochs
///	return (time_t)temp;
///}


// http://stackoverflow.com/questions/6161776/convert-windows-filetime-to-second-in-unix-linux
time_t FileTime_to_POSIX( FILETIME ft )
{
	FILETIME localFileTime;
	FileTimeToLocalFileTime( &ft, &localFileTime );
	SYSTEMTIME sysTime;
	FileTimeToSystemTime( &localFileTime, &sysTime );
	struct tm tmtime = {0};
	tmtime.tm_year = sysTime.wYear - 1900;
	tmtime.tm_mon = sysTime.wMonth - 1;
	tmtime.tm_mday = sysTime.wDay;
	tmtime.tm_hour = sysTime.wHour;
	tmtime.tm_min = sysTime.wMinute;
	tmtime.tm_sec = sysTime.wSecond;
	tmtime.tm_wday = 0;
	tmtime.tm_yday = 0;
	tmtime.tm_isdst = -1;
	time_t ret = mktime( &tmtime );
	ret *= 1000;	/// Marius!
	return ret;
}


//++ Write_SMSBR
HRESULT Write_SMSBR( _In_ LPCTSTR pszFile, _In_ const SMS_LIST& SmsList, _In_opt_ LPCWSTR pszComment )
{
	HRESULT hr = S_OK;

	if (!pszFile || !*pszFile)
		return E_INVALIDARG;

	try {

		/// <smses count = "2" backup_set = "cb081d84-aca6-4a12-ab0d-30cfdcf1891f" backup_date = "1488996424918">
		/// 	<sms protocol = "0" address = "+40744791245" date = "1488572656975" type = "1" subject = "null" body = "Message Text" toa = "null" sc_toa = "null" service_center = "+40770000053" read = "1" status = "-1" locked = "0" date_sent = "1488572650000" readable_date = "Mar 3, 2017 22:24:16" contact_name = "First Last Name" / >
		/// 	<sms protocol = "0" address = "+40744791245" date = "1488572942710" type = "2" subject = "null" body = "Message Text" toa = "null" sc_toa = "null" service_center = "null" read = "1" status = "-1" locked = "0" date_sent = "0" readable_date = "Mar 3, 2017 22:29:02" contact_name = "First Last Name" / >
		/// </smses>

		IXMLDOMDocument2Ptr XmlDoc;
		hr = XmlDoc.CreateInstance( _T( "msxml2.domdocument" ) );
		if (SUCCEEDED( hr )) {

			WCHAR szBuf[128];
			VARIANT vtTemp;
			vtTemp.vt = VT_I2;
			vtTemp.iVal = MSXML2::NODE_ELEMENT;

			XmlDoc->appendChild( XmlDoc->createProcessingInstruction( L"xml", L"version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"" ) );
			XmlDoc->appendChild( XmlDoc->createProcessingInstruction( L"xml-stylesheet", L"type=\"text/xsl\" href=\"sms.xsl\"" ) );
			XmlDoc->appendChild( XmlDoc->createComment( pszComment ? pszComment : L"Generated by sms_w2a" ) );

			// Root node
			MSXML2::IXMLDOMNodePtr RootNode;
			RootNode = XmlDoc->createNode( vtTemp, L"smses", L"" );

			SYSTEMTIME st;
			FILETIME ft;
			GetSystemTime( &st );
			SystemTimeToFileTime( &st, &ft );
			time_t tm = FileTime_to_POSIX( ft );
			StringCchPrintfW( szBuf, ARRAYSIZE( szBuf ), L"%I64u", tm );
			RootNode->attributes->setNamedItem( XmlDoc->createAttribute( L"backup_date" ) );
			RootNode->attributes->getNamedItem( L"backup_date" )->text = szBuf;

			UUID uuid;
			RPC_WSTR szuuid = NULL;
			UuidCreate( &uuid );
			UuidToString( &uuid, &szuuid );
			RootNode->attributes->setNamedItem( XmlDoc->createAttribute( L"backup_set" ) );
			RootNode->attributes->getNamedItem( L"backup_set" )->text = (LPCWSTR)szuuid;
			RpcStringFree( &szuuid );

			StringCchPrintfW( szBuf, ARRAYSIZE( szBuf ), L"%u", (ULONG)SmsList.size() );
			RootNode->attributes->setNamedItem( XmlDoc->createAttribute( L"count" ) );
			RootNode->attributes->getNamedItem( L"count" )->text = szBuf;

			XmlDoc->appendChild( RootNode );

			// SMS nodes
			for (auto it = SmsList.begin(); it != SmsList.end(); ++it) {
				MSXML2::IXMLDOMNodePtr SmsNode = XmlDoc->createNode( vtTemp, L"sms", L"" );

				SmsNode->attributes->setNamedItem( XmlDoc->createAttribute( L"protocol" ) );
				SmsNode->attributes->getNamedItem( L"protocol" )->text = L"0";

				SmsNode->attributes->setNamedItem( XmlDoc->createAttribute( L"address" ) );
				SmsNode->attributes->getNamedItem( L"address" )->text = it->PhoneNo;

				time_t tm = FileTime_to_POSIX( it->Timestamp );
				StringCchPrintfW( szBuf, ARRAYSIZE( szBuf ), L"%I64u", tm );
				SmsNode->attributes->setNamedItem( XmlDoc->createAttribute( L"date" ) );
				SmsNode->attributes->getNamedItem( L"date" )->text = szBuf;

				SmsNode->attributes->setNamedItem( XmlDoc->createAttribute( L"type" ) );
				SmsNode->attributes->getNamedItem( L"type" )->text = it->IsIncoming ? L"1" : L"2";

				///SmsNode->attributes->setNamedItem( XmlDoc->createAttribute( L"subject" ) );
				///SmsNode->attributes->getNamedItem( L"subject" )->text = L"null";

				SmsNode->attributes->setNamedItem( XmlDoc->createAttribute( L"body" ) );
				SmsNode->attributes->getNamedItem( L"body" )->text = it->Text;

				SmsNode->attributes->setNamedItem( XmlDoc->createAttribute( L"read" ) );
				SmsNode->attributes->getNamedItem( L"read" )->text = it->IsRead ? L"1" : L"0";

				///SmsNode->attributes->setNamedItem( XmlDoc->createAttribute( L"date_sent" ) );
				///SmsNode->attributes->getNamedItem( L"date_sent" )->text = L"";

				FileTimeToSystemTime( &it->Timestamp, &st );
				StringCchPrintf( szBuf, ARRAYSIZE( szBuf ), _T( "%hu/%02hu/%02hu %02hu:%02hu:%02hu" ), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond );
				SmsNode->attributes->setNamedItem( XmlDoc->createAttribute( L"readable_date" ) );
				SmsNode->attributes->getNamedItem( L"readable_date" )->text = szBuf;

				///SmsNode->attributes->setNamedItem( XmlDoc->createAttribute( L"contact_name" ) );
				///SmsNode->attributes->getNamedItem( L"contact_name" )->text = L"";

				RootNode->appendChild( SmsNode );
			}

			// Make sure the existing xml file is not read-only
			DWORD attr = GetFileAttributes( pszFile );
			if ((attr & FILE_ATTRIBUTE_READONLY) != 0)
				SetFileAttributes( pszFile, attr & ~FILE_ATTRIBUTE_READONLY );

			// Save
			_variant_t varXml( pszFile );
			hr = XmlDoc->save( varXml );
		}

	} catch (HRESULT hr2) {
		hr = hr2;
	} catch (...) {
		hr = E_UNEXPECTED;
	}

	return hr;
}