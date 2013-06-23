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
#include "WebP_version.h"

#include <Windows.h>
#include <commctrl.h>

#include <stdio.h>

extern HINSTANCE hDllInstance;

enum {
	OUT_noUI = -1,
	OUT_OK = IDOK,
	OUT_Cancel = IDCANCEL,
	OUT_Picture,
	OUT_Lossless_Radio,
	OUT_Lossy_Radio,
	OUT_Quality_Field,
	OUT_Quality_Slider,
	OUT_Alpha_Radio_None,
	OUT_Alpha_Radio_Transparency,
	OUT_Alpha_Radio_Channel,
	OUT_Alpha_Cleanup_Check,
	OUT_Lossy_Alpha_Check,
	OUT_Save_Metadata_Check
};

// sensible Win macros
#define GET_ITEM(ITEM)	GetDlgItem(hwndDlg, (ITEM))

#define SET_CHECK(ITEM, VAL)	SendMessage(GET_ITEM(ITEM), BM_SETCHECK, (WPARAM)(VAL), (LPARAM)0)
#define GET_CHECK(ITEM)			SendMessage(GET_ITEM(ITEM), BM_GETCHECK, (WPARAM)0, (LPARAM)0)

#define ENABLE_ITEM(ITEM, ENABLE)	EnableWindow(GetDlgItem(hwndDlg, (ITEM)), (ENABLE));



static bool					g_lossless = TRUE;
static int					g_quality = 50;
static DialogAlpha			g_alpha = DIALOG_ALPHA_NONE;
static bool					g_alpha_cleanup = TRUE;
static bool					g_lossy_alpha = FALSE;
static bool					g_save_metadata = TRUE;

static bool					g_have_transparency = false;
static const char			*g_alpha_name = NULL;

static WORD	g_item_clicked = 0;


static void TrackLossless(HWND hwndDlg)
{
	bool lossy = GET_CHECK(OUT_Lossy_Radio);

	ENABLE_ITEM(OUT_Quality_Field, lossy);
	ENABLE_ITEM(OUT_Quality_Slider, lossy);

	bool alpha = !GET_CHECK(OUT_Alpha_Radio_None);

	ENABLE_ITEM(OUT_Lossy_Alpha_Check, (lossy && alpha));
}


static void TrackSlider(HWND hwndDlg)
{
	int val = SendMessage(GET_ITEM(OUT_Quality_Slider), TBM_GETPOS, (WPARAM)0, (LPARAM)0 );

	char txt[5];
	sprintf_s(txt, 4, "%d", val);

	SetDlgItemText(hwndDlg, OUT_Quality_Field, txt);
}


static void TrackField(HWND hwndDlg)
{
	char txt[5];

	UINT chars = GetDlgItemText(hwndDlg, OUT_Quality_Field, txt, 4);

	if(chars)
	{
		int val = atoi(txt);

		if(val >= 0 && val <= 100)
		{
			SendMessage(GET_ITEM(OUT_Quality_Slider),(UINT)TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)val);
		}
	}
}


static void TrackAlpha(HWND hwndDlg)
{
	if( GET_CHECK(OUT_Alpha_Radio_None) )
	{
		ENABLE_ITEM(OUT_Alpha_Cleanup_Check, FALSE);
		ENABLE_ITEM(OUT_Lossy_Alpha_Check, FALSE);
	}
	else
	{
		ENABLE_ITEM(OUT_Alpha_Cleanup_Check, GET_CHECK(OUT_Alpha_Radio_Transparency));
		ENABLE_ITEM(OUT_Lossy_Alpha_Check, GET_CHECK(OUT_Lossy_Radio));
	}
}


static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 
 
    switch(message) 
    { 
		case WM_INITDIALOG:
			SET_CHECK(OUT_Lossless_Radio, g_lossless);
			SET_CHECK(OUT_Lossy_Radio, !g_lossless);

			SendMessage(GET_ITEM(OUT_Quality_Slider),(UINT)TBM_SETRANGEMIN, (WPARAM)(BOOL)FALSE, (LPARAM)0);
			SendMessage(GET_ITEM(OUT_Quality_Slider),(UINT)TBM_SETRANGEMAX, (WPARAM)(BOOL)FALSE, (LPARAM)100);
			SendMessage(GET_ITEM(OUT_Quality_Slider),(UINT)TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)g_quality);

			if(!g_have_transparency)
			{
				ENABLE_ITEM(OUT_Alpha_Radio_Transparency, FALSE);

				if(g_alpha == DIALOG_ALPHA_TRANSPARENCY)
				{
					g_alpha = (g_alpha_name != NULL ? DIALOG_ALPHA_CHANNEL : DIALOG_ALPHA_NONE);
				}
			}

			if(g_alpha_name == NULL)
			{
				ENABLE_ITEM(OUT_Alpha_Radio_Channel, FALSE);

				if(g_alpha == DIALOG_ALPHA_CHANNEL)
				{
					g_alpha = (g_have_transparency ? DIALOG_ALPHA_TRANSPARENCY : DIALOG_ALPHA_NONE);
				}
			}
			else
			{
				SetDlgItemText(hwndDlg, OUT_Alpha_Radio_Channel, g_alpha_name);
			}

			SET_CHECK( (g_alpha == DIALOG_ALPHA_NONE ? OUT_Alpha_Radio_None :
						g_alpha == DIALOG_ALPHA_TRANSPARENCY ? OUT_Alpha_Radio_Transparency :
						g_alpha == DIALOG_ALPHA_CHANNEL ? OUT_Alpha_Radio_Channel :
						OUT_Alpha_Radio_None), TRUE);

			SET_CHECK(OUT_Alpha_Cleanup_Check, g_alpha_cleanup);
			SET_CHECK(OUT_Lossy_Alpha_Check, g_lossy_alpha);
			SET_CHECK(OUT_Save_Metadata_Check, g_save_metadata);

			TrackLossless(hwndDlg);
			TrackSlider(hwndDlg);
			TrackAlpha(hwndDlg);

			return TRUE;
 
		case WM_NOTIFY:
			switch(LOWORD(wParam))
			{
				case OUT_Quality_Slider:
					TrackSlider(hwndDlg);
				return TRUE;
			}
		return FALSE;

        case WM_COMMAND: 
			g_item_clicked = LOWORD(wParam);

            switch(g_item_clicked)
            { 
                case OUT_OK: 
				case OUT_Cancel:  // do the same thing, but g_item_clicked will be different
					g_lossless = GET_CHECK(OUT_Lossless_Radio);
					g_quality = SendMessage(GET_ITEM(OUT_Quality_Slider), TBM_GETPOS, (WPARAM)0, (LPARAM)0 );

					g_alpha =	GET_CHECK(OUT_Alpha_Radio_None) ? DIALOG_ALPHA_NONE :
								GET_CHECK(OUT_Alpha_Radio_Transparency) ? DIALOG_ALPHA_TRANSPARENCY :
								GET_CHECK(OUT_Alpha_Radio_Channel) ? DIALOG_ALPHA_CHANNEL :
								DIALOG_ALPHA_TRANSPARENCY;

					g_alpha_cleanup = GET_CHECK(OUT_Alpha_Cleanup_Check);
					g_lossy_alpha = GET_CHECK(OUT_Lossy_Alpha_Check);
					g_save_metadata = GET_CHECK(OUT_Save_Metadata_Check);

					EndDialog(hwndDlg, 0);
					return TRUE;


				case OUT_Lossless_Radio:
				case OUT_Lossy_Radio:
					TrackLossless(hwndDlg);
					return TRUE;

				case OUT_Alpha_Radio_None:
				case OUT_Alpha_Radio_Transparency:
				case OUT_Alpha_Radio_Channel:
					TrackAlpha(hwndDlg);

				case OUT_Quality_Field:
					TrackField(hwndDlg);
					return TRUE;
            } 
    } 
    return FALSE; 
} 

bool
WebP_OutUI(
	WebP_OutUI_Data		*params,
	bool				have_transparency,
	const char			*alpha_name,
	const void			*plugHndl,
	const void			*mwnd)
{
	g_lossless		= params->lossless;
	g_quality		= params->quality;
	g_alpha			= params->alpha;
	g_alpha_cleanup	= params->alpha_cleanup;
	g_lossy_alpha	= params->lossy_alpha;
	g_save_metadata	= params->save_metadata;
	
	g_have_transparency = have_transparency;
	g_alpha_name = alpha_name;


	int status = DialogBox(hDllInstance, (LPSTR)"OUT_DIALOG", (HWND)mwnd, (DLGPROC)DialogProc);


	if(g_item_clicked == OUT_OK)
	{
		params->lossless		= g_lossless;
		params->quality			= g_quality;
		params->alpha			= g_alpha;
		params->alpha_cleanup	= g_alpha_cleanup;
		params->lossy_alpha		= g_lossy_alpha;
		params->save_metadata	= g_save_metadata;

		return true;
	}
	else
		return false;
}


enum {
	ABOUT_noUI = -1,
	ABOUT_OK = IDOK,
	ABOUT_Cancel = IDCANCEL,
	ABOUT_Picture,
	ABOUT_Plugin_Version_String,
	ABOUT_WebP_Version_String
};

static const char *g_plugin_version_string = NULL;
static const char *g_WebP_version_string = NULL;

static BOOL CALLBACK AboutProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 
 
    switch(message) 
    { 
		case WM_INITDIALOG:
				SetDlgItemText(hwndDlg, ABOUT_Plugin_Version_String, g_plugin_version_string);
				SetDlgItemText(hwndDlg, ABOUT_WebP_Version_String, g_WebP_version_string);

			return TRUE;
 
		case WM_NOTIFY:
			return FALSE;

        case WM_COMMAND: 
            switch(LOWORD(wParam))
            { 
                case OUT_OK: 
				case OUT_Cancel:
					EndDialog(hwndDlg, 0);
					return TRUE;
            } 
    } 
    return FALSE; 
} 

void
WebP_About(
	const char		*plugin_version_string,
	const char		*WebP_version_string,
	const void		*plugHndl,
	const void		*mwnd)
{
	g_plugin_version_string = plugin_version_string;
	g_WebP_version_string = WebP_version_string;

	int status = DialogBox(hDllInstance, (LPSTR)"ABOUT_DIALOG", (HWND)mwnd, (DLGPROC)AboutProc);
}

