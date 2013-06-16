

#include "WebP_UI.h"

#define gStuff				(globals->formatParamBlock)



// ==============
// Only compiled on 32-bit
// ==============
#if !__LP64__


bool
WebP_InUI(
	WebP_InUI_Data		*params,
	const void			*plugHndl,
	const void			*mwnd)
{
	bool result = true;
	
	return result;
}


bool
WebP_OutUI(
	WebP_OutUI_Data		*params,
	bool				have_transparency,
	const char			*alpha_name,
	const void			*plugHndl,
	const void			*mwnd)
{
	bool result = true;
	
	return result;
}


void
WebP_About(
	const void		*plugHndl,
	const void		*mwnd)
{

}

#endif //!__LP64__
