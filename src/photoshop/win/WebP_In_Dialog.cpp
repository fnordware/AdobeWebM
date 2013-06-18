///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2013, Brendan Bolles
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *	   Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *	   Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------
//
// WebP Photoshop plug-in
//
// by Brendan Bolles <brendan@fnordware.com>
//
// ------------------------------------------------------------------------

#include "WebP.h"

#include "WebP_UI.h"

#include <Windows.h>
#include <commctrl.h>

extern HINSTANCE hDllInstance;

enum {
	IN_noUI = -1,
	IN_OK = IDOK,
	IN_Cancel = IDCANCEL,
	IN_Set_Defaults_Button,
	IN_Alpha_Radio_Transparent,
	IN_Alpha_Radio_Channel,
	IN_Multiply_Checkbox,
	IN_Auto_Checkbox
};

// sensible Win macros
#define GET_ITEM(ITEM)	GetDlgItem(hwndDlg, (ITEM))

#define SET_CHECK(ITEM, VAL)	SendMessage(GET_ITEM(ITEM), BM_SETCHECK, (WPARAM)(VAL), (LPARAM)0)
#define GET_CHECK(ITEM)			SendMessage(GET_ITEM(ITEM), BM_GETCHECK, (WPARAM)0, (LPARAM)0)

#define ENABLE_ITEM(ITEM, ENABLE)	EnableWindow(GetDlgItem(hwndDlg, (ITEM)), (ENABLE));


static DialogAlpha			g_alpha = DIALOG_ALPHA_TRANSPARENCY;
static bool					g_mult = true;
static bool					g_autoD = false;


static void ReadPrefs()
{
	// read prefs from registry
	HKEY webp_hkey;
	LONG reg_error = RegOpenKeyEx(HKEY_CURRENT_USER, WEBP_PREFIX, 0, KEY_READ, &webp_hkey);

	if(reg_error == ERROR_SUCCESS)
	{
		DWORD type;
		DWORD size = sizeof(DWORD);

		DWORD alpha = g_alpha,
				mult = g_mult,
				autoD = g_autoD;

		reg_error = RegQueryValueEx(webp_hkey, WEBP_ALPHA_KEY, NULL, &type, (LPBYTE)&alpha, &size);

		if(reg_error == ERROR_SUCCESS && type == REG_DWORD)
			g_alpha = (DialogAlpha)alpha;

		reg_error = RegQueryValueEx(webp_hkey, WEBP_MULT_KEY, NULL, &type, (LPBYTE)&mult, &size);

		if(reg_error == ERROR_SUCCESS && type == REG_DWORD)
			g_mult = mult;

		reg_error = RegQueryValueEx(webp_hkey, WEBP_AUTO_KEY, NULL, &type, (LPBYTE)&autoD, &size);

		if(reg_error == ERROR_SUCCESS && type == REG_DWORD)
			g_autoD = autoD;

		reg_error = RegCloseKey(webp_hkey);
	}
}

static void WriteAlphaPrefs()
{
	HKEY webp_hkey;

	LONG reg_error = RegCreateKeyEx(HKEY_CURRENT_USER, WEBP_PREFIX, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &webp_hkey, NULL);

	if(reg_error == ERROR_SUCCESS)
	{
		DWORD alpha = g_alpha,
				mult = g_mult;

		reg_error = RegSetValueEx(webp_hkey, WEBP_ALPHA_KEY, NULL, REG_DWORD, (BYTE *)&alpha, sizeof(DWORD));
		reg_error = RegSetValueEx(webp_hkey, WEBP_MULT_KEY, NULL, REG_DWORD, (BYTE *)&mult, sizeof(DWORD));

		reg_error = RegCloseKey(webp_hkey);
	}
}

static void WriteAutoPrefs()
{
	HKEY webp_hkey;

	LONG reg_error = RegCreateKeyEx(HKEY_CURRENT_USER, WEBP_PREFIX, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &webp_hkey, NULL);

	if(reg_error == ERROR_SUCCESS)
	{
		DWORD autoD = g_autoD;

		reg_error = RegSetValueEx(webp_hkey, WEBP_AUTO_KEY, NULL, REG_DWORD, (BYTE *)&autoD, sizeof(DWORD));

		reg_error = RegCloseKey(webp_hkey);
	}
}


static WORD	g_item_clicked = 0;

static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 
 
    switch(message) 
    { 
		case WM_INITDIALOG:
			SET_CHECK( (g_alpha == DIALOG_ALPHA_TRANSPARENCY ? IN_Alpha_Radio_Transparent :
						g_alpha == DIALOG_ALPHA_CHANNEL ? IN_Alpha_Radio_Channel :
						IN_Alpha_Radio_Transparent), TRUE);

			SET_CHECK(IN_Multiply_Checkbox, g_mult);
			SET_CHECK(IN_Auto_Checkbox, g_autoD);

			ENABLE_ITEM(IN_Multiply_Checkbox, (g_alpha == DIALOG_ALPHA_CHANNEL));

			return TRUE;
 
		case WM_NOTIFY:
			return FALSE;

        case WM_COMMAND: 
			g_alpha = GET_CHECK(IN_Alpha_Radio_Transparent) ? DIALOG_ALPHA_TRANSPARENCY :
							GET_CHECK(IN_Alpha_Radio_Channel) ? DIALOG_ALPHA_CHANNEL :
							DIALOG_ALPHA_TRANSPARENCY;

			g_mult = GET_CHECK(IN_Multiply_Checkbox);
			g_autoD = GET_CHECK(IN_Auto_Checkbox);

			g_item_clicked = LOWORD(wParam);

            switch(g_item_clicked)
            { 
                case IN_OK: 
				case IN_Cancel:
					EndDialog(hwndDlg, 0);
					return TRUE;

				case IN_Set_Defaults_Button:
					WriteAlphaPrefs();
					WriteAutoPrefs();
					return TRUE;

				case IN_Alpha_Radio_Transparent:
				case IN_Alpha_Radio_Channel:
					ENABLE_ITEM(IN_Multiply_Checkbox, (g_alpha == DIALOG_ALPHA_CHANNEL));
					return TRUE;
            } 
    } 
    return FALSE; 
} 


static inline bool KeyIsDown(int vKey)
{
	return (GetAsyncKeyState(vKey) & 0x8000);
}


bool
WebP_InUI(
	WebP_InUI_Data		*params,
	bool				have_alpha,
	const void			*plugHndl,
	const void			*mwnd)
{
	bool continue_reading = true;

	g_alpha = DIALOG_ALPHA_TRANSPARENCY;
	g_mult = false;
	g_autoD = false;

	ReadPrefs();

	// check for that shift key
	bool shift_key = ( KeyIsDown(VK_LSHIFT) || KeyIsDown(VK_RSHIFT) || KeyIsDown(VK_LMENU) || KeyIsDown(VK_RMENU) );

	if((g_autoD && have_alpha) || shift_key)
	{
		int status = DialogBox(hDllInstance, (LPSTR)"IN_DIALOG", (HWND)mwnd, (DLGPROC)DialogProc);

		if(g_item_clicked == IN_OK)
		{
			WriteAutoPrefs();

			continue_reading = true;
		}
		else
			continue_reading = false;
	}

	params->alpha	= g_alpha;
	params->mult	= g_mult;

	return continue_reading;
}
