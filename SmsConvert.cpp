
// Marius Negrutiu (marius.negrutiu@gmail.com)
// 2017/03/10

#include "StdAfx.h"
#include "SmsConvert.h"
#include <algorithm>
#include <time.h>


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



//++ Read_CMBK
HRESULT Read_CMBK( _In_ LPCTSTR pszFile, _Out_ SMS_LIST& SmsList )
{
	HRESULT hr = S_OK;

	if (!pszFile || !*pszFile)
		return E_INVALIDARG;

	try {

		IXMLDOMDocument2Ptr XmlDoc;
		hr = XmlDoc.CreateInstance( L"msxml2.domdocument" );
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

				MSXML2::IXMLDOMElementPtr RootNode = XmlDoc->GetdocumentElement();
				if (RootNode) {
					if (EqualStr( RootNode->tagName, L"ArrayOfMessage" )) {

						MSXML2::IXMLDOMNodeListPtr NodeList = RootNode->GetchildNodes();
						if (NodeList) {

							for (long i = 0; i < NodeList->length; i++) {

								MSXML2::IXMLDOMElementPtr MsgNode = NodeList->item[i];
								if (MsgNode) {

									if (EqualStr( MsgNode->tagName, L"Message" )) {

										MSXML2::IXMLDOMNodeListPtr nodes;
										FILETIME Timestamp;
										bool bIncoming = false, bRead = false;
										BSTR bstrBody = NULL;
										std::vector<_bstr_t> PhoneNoList;

										// IsIncoming
										nodes = MsgNode->getElementsByTagName( L"IsIncoming" );
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
										nodes = MsgNode->getElementsByTagName( L"IsRead" );
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
										nodes = MsgNode->getElementsByTagName( L"Body" );
										assert( nodes );
										if (nodes) {
											assert( nodes->length == 1 );
											if (nodes->length > 0)
												nodes->item[0]->get_text( &bstrBody );
											nodes.Release();
										}

										// Timestamp
										nodes = MsgNode->getElementsByTagName( L"LocalTimestamp" );
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

											nodes = MsgNode->getElementsByTagName( L"Sender" );
											assert( nodes );
											if (nodes) {
												assert( nodes->length == 1 );						/// Always one sender
												if (nodes->length == 1) {
													BSTR bstrPhoneNo = NULL;
													nodes->item[0]->get_text( &bstrPhoneNo );
													PhoneNoList.push_back( bstrPhoneNo );
												}
												nodes.Release();
											}

										} else {

											nodes = MsgNode->getElementsByTagName( L"Recepients" );
											assert( nodes );
											if (nodes) {
												MSXML2::IXMLDOMNodeListPtr nodes2 = nodes->item[0]->GetchildNodes();
												assert( nodes2 );
												for (long j = 0; j < nodes2->length; j++) {			/// Multiple receipients
													BSTR bstrPhoneNo;
													nodes2->item[j]->get_text( &bstrPhoneNo );
													PhoneNoList.push_back( bstrPhoneNo );
												}
												nodes.Release();
											}
										}

										// Collect
										if (bstrBody && *bstrBody && !PhoneNoList.empty()) {
											SMS sms;
											sms.Timestamp = Timestamp;
											sms.IsIncoming = bIncoming;
											sms.IsRead = bRead;
											sms.Text = bstrBody;
											sms.PhoneNo = PhoneNoList;
											SmsList.push_back( sms );
										}
									}
								}
							}

							// Remove duplicates
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



//++ Write_SMSBR
HRESULT Write_SMSBR( _In_ LPCTSTR pszFile, _In_ const SMS_LIST& SmsList, _In_opt_ WCHAR **ppszComment )
{
	HRESULT hr = S_OK;

	if (!pszFile || !*pszFile)
		return E_INVALIDARG;

	try {

		/// <smses count = "2" backup_set = "cb081d84-aca6-4a12-ab0d-30cfdcf1891f" backup_date = "1488996424918">
		/// 	<sms protocol = "0" address = "+40000000000" date = "1488572656975" type = "1" subject = "null" body = "Message Text" toa = "null" sc_toa = "null" service_center = "+40770000053" read = "1" status = "-1" locked = "0" date_sent = "1488572650000" readable_date = "Mar 3, 2017 22:24:16" contact_name = "First Last Name" / >
		/// 	<sms protocol = "0" address = "+40000000000" date = "1488572942710" type = "2" subject = "null" body = "Message Text" toa = "null" sc_toa = "null" service_center = "null" read = "1" status = "-1" locked = "0" date_sent = "0" readable_date = "Mar 3, 2017 22:29:02" contact_name = "First Last Name" / >
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
			if (ppszComment) {
				for (int i = 0; ppszComment[i]; i++)
					XmlDoc->appendChild( XmlDoc->createComment( ppszComment[i] ) );
			} else {
				XmlDoc->appendChild( XmlDoc->createComment( L" Generated by sms_w2a " ) );
				XmlDoc->appendChild( XmlDoc->createComment( L" https://github.com/negrutiu/sms_w2a " ) );
			}

			// Root node
			MSXML2::IXMLDOMNodePtr RootNode;
			RootNode = XmlDoc->createNode( vtTemp, L"smses", L"" );

			SYSTEMTIME st;
			FILETIME ft;
			GetSystemTime( &st );
			SystemTimeToFileTime( &st, &ft );
			time_t tm = FILETIME_to_POSIXms( ft );
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

			/// NOTES
			///   Outgoing messages may have multiple recipients!
			///   E.g. a message sent to two contacts will count as two messages
			ULONG iCount = 0;
			for (auto it = SmsList.begin(); it != SmsList.end(); ++it)
				iCount += (ULONG)it->PhoneNo.size();
			StringCchPrintfW( szBuf, ARRAYSIZE( szBuf ), L"%u", iCount );
			RootNode->attributes->setNamedItem( XmlDoc->createAttribute( L"count" ) );
			RootNode->attributes->getNamedItem( L"count" )->text = szBuf;

			XmlDoc->appendChild( RootNode );

			// SMS nodes
			for (auto it = SmsList.begin(); it != SmsList.end(); ++it) {

				/// Outgoing message may have multiple recipients
				/// "Clone" the same message for each contact
				for (auto itPhoneNo = it->PhoneNo.begin(); itPhoneNo != it->PhoneNo.end(); ++itPhoneNo) {

					MSXML2::IXMLDOMNodePtr MsgNode = XmlDoc->createNode( vtTemp, L"sms", L"" );

					MsgNode->attributes->setNamedItem( XmlDoc->createAttribute( L"protocol" ) );
					MsgNode->attributes->getNamedItem( L"protocol" )->text = L"0";

					MsgNode->attributes->setNamedItem( XmlDoc->createAttribute( L"address" ) );
					MsgNode->attributes->getNamedItem( L"address" )->text = *itPhoneNo;

					time_t tm = FILETIME_to_POSIXms( it->Timestamp );
					StringCchPrintfW( szBuf, ARRAYSIZE( szBuf ), L"%I64u", tm );
					MsgNode->attributes->setNamedItem( XmlDoc->createAttribute( L"date" ) );
					MsgNode->attributes->getNamedItem( L"date" )->text = szBuf;

					MsgNode->attributes->setNamedItem( XmlDoc->createAttribute( L"type" ) );
					MsgNode->attributes->getNamedItem( L"type" )->text = it->IsIncoming ? L"1" : L"2";

					///MsgNode->attributes->setNamedItem( XmlDoc->createAttribute( L"subject" ) );
					///MsgNode->attributes->getNamedItem( L"subject" )->text = L"null";

					MsgNode->attributes->setNamedItem( XmlDoc->createAttribute( L"body" ) );
					MsgNode->attributes->getNamedItem( L"body" )->text = it->Text;

					MsgNode->attributes->setNamedItem( XmlDoc->createAttribute( L"read" ) );
					MsgNode->attributes->getNamedItem( L"read" )->text = it->IsRead ? L"1" : L"0";

					///MsgNode->attributes->setNamedItem( XmlDoc->createAttribute( L"date_sent" ) );
					///MsgNode->attributes->getNamedItem( L"date_sent" )->text = L"";

					FileTimeToSystemTime( &it->Timestamp, &st );
					StringCchPrintf( szBuf, ARRAYSIZE( szBuf ), _T( "%hu/%02hu/%02hu %02hu:%02hu:%02hu" ), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond );
					MsgNode->attributes->setNamedItem( XmlDoc->createAttribute( L"readable_date" ) );
					MsgNode->attributes->getNamedItem( L"readable_date" )->text = szBuf;

					///MsgNode->attributes->setNamedItem( XmlDoc->createAttribute( L"contact_name" ) );
					///MsgNode->attributes->getNamedItem( L"contact_name" )->text = L"";

					RootNode->appendChild( MsgNode );
				}
			}

			// Make sure the output file is not read-only. MSXML won't like it..
			DWORD attr = GetFileAttributes( pszFile );
			if (attr & FILE_ATTRIBUTE_READONLY)
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