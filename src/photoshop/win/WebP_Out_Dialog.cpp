
#include "WebP.h"

#include "WebP_UI.h"
#include "WebP_version.h"

#include <Windows.h>
#include <commctrl.h>

extern HINSTANCE hDllInstance;

enum {
	OUT_noUI = -1,
	OUT_OK = IDOK,
	OUT_Cancel = IDCANCEL,
	OUT_Picture,
	OUT_Lossless_Check,
	OUT_Quality_Slider,
	OUT_Alpha_Radio_None,
	OUT_Alpha_Radio_Transparency,
	OUT_Alpha_Radio_Channel
};

// sensible Win macros
#define GET_ITEM(ITEM)	GetDlgItem(hwndDlg, (ITEM))

#define SET_CHECK(ITEM, VAL)	SendMessage(GET_ITEM(ITEM), BM_SETCHECK, (WPARAM)(VAL), (LPARAM)0)
#define GET_CHECK(ITEM)			SendMessage(GET_ITEM(ITEM), BM_GETCHECK, (WPARAM)0, (LPARAM)0)

#define ENABLE_ITEM(ITEM, ENABLE)	EnableWindow(GetDlgItem(hwndDlg, (ITEM)), (ENABLE));



static bool					g_lossless = TRUE;
static int					g_quality = 50;
static DialogAlpha			g_alpha = DIALOG_ALPHA_NONE;

static bool					g_have_transparency = false;
static const char			*g_alpha_name = NULL;

static WORD	g_item_clicked = 0;



static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) 
{ 
    BOOL fError; 
 
    switch(message) 
    { 
		case WM_INITDIALOG:
			SET_CHECK(OUT_Lossless_Check, g_lossless);

			SendMessage(GET_ITEM(OUT_Quality_Slider),(UINT)TBM_SETRANGEMIN, (WPARAM)(BOOL)FALSE, (LPARAM)1);
			SendMessage(GET_ITEM(OUT_Quality_Slider),(UINT)TBM_SETRANGEMAX, (WPARAM)(BOOL)FALSE, (LPARAM)100);
			SendMessage(GET_ITEM(OUT_Quality_Slider),(UINT)TBM_SETPOS, (WPARAM)(BOOL)TRUE, (LPARAM)g_quality);

			ENABLE_ITEM(OUT_Quality_Slider, !g_lossless);

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

			return TRUE;
 
		case WM_NOTIFY:
			return FALSE;

        case WM_COMMAND: 
			g_item_clicked = LOWORD(wParam);

            switch(g_item_clicked)
            { 
                case OUT_OK: 
				case OUT_Cancel:  // do the same thing, but g_item_clicked will be different
					g_lossless = GET_CHECK(OUT_Lossless_Check);
					g_quality = SendMessage(GET_ITEM(OUT_Quality_Slider), TBM_GETPOS, (WPARAM)0, (LPARAM)0 );

					g_alpha =	GET_CHECK(OUT_Alpha_Radio_None) ? DIALOG_ALPHA_NONE :
								GET_CHECK(OUT_Alpha_Radio_Transparency) ? DIALOG_ALPHA_TRANSPARENCY :
								GET_CHECK(OUT_Alpha_Radio_Channel) ? DIALOG_ALPHA_CHANNEL :
								DIALOG_ALPHA_TRANSPARENCY;


					EndDialog(hwndDlg, 0);
					return TRUE;


				case OUT_Lossless_Check:
					ENABLE_ITEM(OUT_Quality_Slider, !GET_CHECK(OUT_Lossless_Check));
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
	g_lossless	= params->lossless;
	g_quality	= params->quality;
	g_alpha		= params->alpha;
	
	g_have_transparency = have_transparency;
	g_alpha_name = alpha_name;


	int status = DialogBox(hDllInstance, (LPSTR)"OUT_DIALOG", (HWND)mwnd, (DLGPROC)DialogProc);


	if(g_item_clicked == OUT_OK)
	{
		params->lossless	= g_lossless;
		params->quality		= g_quality;
		params->alpha		= g_alpha;

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

