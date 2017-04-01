#pragma once
#ifndef MAIN_H
#define MAIN_H

//+ Definitions
#define REGKEY							_T( "Software\\Marius Negrutiu\\sms_w2a" )
#define WM_TRANSLATE_ACCELERATOR_KEY	0xFF01	/// (WM_APP + 0x7F01) wParam==Unused, lParam==(LPMSG)msg
#define WM_TRANSLATE_DIALOG_KEY			0xFF02	/// (WM_APP + 0x7F02) wParam==Unused, lParam==(LPMSG)msg

//+ Global variables
extern HINSTANCE g_hInst;

#endif ///MAIN_H
