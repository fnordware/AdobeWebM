
#ifndef WEBP_UI_H
#define WEBP_UI_H


typedef enum {
	DIALOG_COMPRESSION_NONE = 0,
	DIALOG_COMPRESSION_LOW,
	DIALOG_COMPRESSION_NORMAL,
	DIALOG_COMPRESSION_HIGH
} DialogProfile;

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
	int					quality;
	int					strength;
	int					sharpness;
	bool				lossless;
	DialogProfile		profile;
	DialogAlpha			alpha;
} WebP_OutUI_Data;

// WebP UI
//
// return true if user hit OK
// if user hit OK, params block will have been modified
//
// send in block of parameters, names for profile menu, and weather to show subsample menu
// plugHndl is bundle identifier string on Mac, hInstance on win
// mwnd is the main windowfor Windows, ADM pointers on Mac

bool
WebP_InUI(
	WebP_InUI_Data		*params,
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
	const void		*plugHndl,
	const void		*mwnd);


// Mac prefs keys
#define WEBP_PREFS_ID		"com.fnordware.Photoshop.WebP"
#define WEBP_PREFS_ALPHA	"Alpha Mode"
#define WEBP_PREFS_MULT		"Mult"
#define WEBP_PREFS_ALWAYS	"Do Dialog"


// Windows registry keys
#define WEBP_PREFIX		 "Software\\fnord\\WebP"
#define WEBP_ALPHA_KEY "Alpha"
#define WEBP_MULT_KEY "Mult"
#define WEBP_ALWAYS_KEY "Always"


#endif // WEBP_UI_H