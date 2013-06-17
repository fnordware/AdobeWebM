
#ifndef WEBP_UI_H
#define WEBP_UI_H


typedef enum {
	DIALOG_ALPHA_NONE = 0,
	DIALOG_ALPHA_TRANSPARENCY,
	DIALOG_ALPHA_CHANNEL
} DialogAlpha;

typedef struct {
	DialogAlpha		alpha;
	bool			mult;
} WebP_InUI_Data;

typedef struct {
	bool				lossless;
	int					quality;
	DialogAlpha			alpha;
} WebP_OutUI_Data;

// WebP UI
//
// return true if user hit OK
// if user hit OK, params block will have been modified
//
// plugHndl is bundle identifier string on Mac, hInstance on win
// mwnd is the main window for Windows

bool
WebP_InUI(
	WebP_InUI_Data		*params,
	bool				have_alpha,
	const void			*plugHndl,
	const void			*mwnd);

bool
WebP_OutUI(
	WebP_OutUI_Data		*params,
	bool				have_transparency,
	const char			*alpha_name,
	const void			*plugHndl,
	const void			*mwnd);

void
WebP_About(
	const char		*plugin_version_string,
	const char		*WebP_version_string,
	const void		*plugHndl,
	const void		*mwnd);


// Mac prefs keys
#define WEBP_PREFS_ID		"com.fnordware.Photoshop.WebP"
#define WEBP_PREFS_ALPHA	"Alpha Mode"
#define WEBP_PREFS_MULT		"Mult"
#define WEBP_PREFS_AUTO		"Auto"


// Windows registry keys
#define WEBP_PREFIX		 "Software\\fnord\\WebP"
#define WEBP_ALPHA_KEY	"Alpha"
#define WEBP_MULT_KEY	"Mult"
#define WEBP_AUTO_KEY	"Augo"


#endif // WEBP_UI_H