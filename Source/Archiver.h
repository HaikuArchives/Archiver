/*

Copyright (c) 2002 Marcin 'Shard' Konicki

Permission is hereby granted, free of charge, to any person obtaining a copy 
of this software and associated documentation files (the "Software"), to deal 
in the Software without restriction, including without limitation the rights 
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
copies of the Software, and to permit persons to whom the Software is 
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE

*/

#ifndef __ARCHIVER_H_
#define __ARCHIVER_H_

//----------------------------------------------------------------------------
//
//	Include
//
//----------------------------------------------------------------------------

#include <Alert.h>
#include <Application.h>
#include <Bitmap.h>
#include <Box.h>
#include <Button.h>
#include <CheckBox.h>
#include <Entry.h>
#include <GraphicsDefs.h>
#include <Menu.h>
#include <MenuBar.h>
#include <MenuItem.h>
#include <MenuField.h>
#include <MessageQueue.h>
#include <Path.h>
#include <RadioButton.h>
#include <Roster.h>
#include <StorageKit.h>
#include <StringView.h>
#include <View.h>
#include <Window.h>

#include <be/add-ons/tracker/TrackerAddon.h>

//----------------------------------------------------------------------------
//
//	Define
//
//----------------------------------------------------------------------------

#define	ARCHIVER_MIME_TYPE				"application/Archiver"

#define	ARCHIVER_SETTINGS_FILE_PATH		"/boot/home/config/settings/"
#define	ARCHIVER_SETTINGS_FILE			"archiver.settings"
#define	ARCHIVER_RULES_FILE_PATH		"/boot/home/config/etc/"
#define	ARCHIVER_RULES_FILE				"archiver.rules"

#define	ARCHIVER_SETTINGS_FILE_DESC		"file description"				// "ZIP compressed file"
#define	ARCHIVER_SETTINGS_FILE_DESC2	"file variation"				// "maximum compression"
#define	ARCHIVER_SETTINGS_FILE_MIME		"file mime type"				// "application/x-zip-compressed"
#define	ARCHIVER_SETTINGS_FILE_EXT		"file extension"				// ".zip"
#define	ARCHIVER_SETTINGS_OPTION		"arg_v"							// "-9", "-r", "-y"...
#define	ARCHIVER_SETTINGS_FILENAME		"FILENAME"						// this will be replaced by generated name for archive file (i.e. "Archive.zip")
#define	ARCHIVER_SETTINGS_PRIORITY		"compression thread priority"	// speaks for itself ;]
#define	ARCHIVER_SETTINGS_WIN_POS		"windowPosition"				// keeps Archiver's window's position on screen
#define	ARCHIVER_SETTINGS_CLOSE_WIN		"closeWindow"					// close window after comression?

#define	ARCHIVER_REFS_DIR_REF			"dir_ref"
#define	ARCHIVER_REFS_ARCHIVE_NAME		"ArchiveName"

#define	ARCHIVER_MSG_CHANGE_RULE		'ARCG'	// Archiver - Rule ChanGe
#define ARCHIVER_MSG_CHANGE_CLOSE_WIN	'ACCW'	// Archiver - Change Close Window after compression
#define	ARCHIVER_MSG_ACCEPT				'AACC'	// Archiver - ACCept
#define	ARCHIVER_MSG_COMPRESS_THREAD_ID	'ACTI'	// Archiver - CompressThreadId
#define	ARCHIVER_MSG_COMPRESS_END		'ACHF'	// Archiver - Compression Has been Finished
#define ARCHIVER_MSG_STOP				'ASTC'	// Archiver - STop Compression
#define ARCHIVER_MSG_REMOVE_AVIEW		'ARAV'	// Archiver - Remove AView


//----------------------------------------------------------------------------
//
//	Classes
//
//----------------------------------------------------------------------------

//---------------------------------------------------
//	Archiver View (base class to inherit from)
//---------------------------------------------------
class AView : public BView
{
	public:
							AView( BMessage *settings);
							AView();
							~AView();

		void				Draw( BRect updateRect);
		bool				LoadIconForMime( char *mime);

		inline void			GetPreferredSize( float *width, float *height) { *width = aWidth; *height = aHeight; };
		inline void			ResizeToPreferred() { ResizeTo( aWidth, aHeight); };

		BButton				*aButton;
		BStringView			*aTitle;

		BMessage			*aSettings;
		BBitmap				*aIcon;

		int32				aWidth;
		int32				aHeight;

		int32				aLeftMargin;
		static const int32	aTopMargin = 8;

	private:
		typedef BView _inherited;
};

//---------------------------------------------------
//	Archiver Comression view
//---------------------------------------------------
class ACompressView : public AView
{
	public:
							ACompressView( BMessage *refs, BMessage *settings);
							~ACompressView();
		void				AttachedToWindow();
		void				DetachedFromWindow();
		void				MessageReceived( BMessage *msg);
		void				GenerateAName( BPath *result);
		thread_id			GetCompressThread();
		bool				Stop();

		BStringView			*aText;
		BMessage			*aRefs;
		int32				aRefsCount;
		BPath				aPath;

		thread_id			aCompressThread;
		thread_id			aCompressWatcherThread;
};

//---------------------------------------------------
//	Archiver Settings View
//---------------------------------------------------

class ASettingsView : public AView
{
	public:
							ASettingsView( BMessage *settings);
							~ASettingsView();
		void				AttachedToWindow();
		void				FrameResized( float width, float height);
		void				MessageReceived( BMessage *msg);

		BMessage			*LoadRules();
		void				ChangeSettingsRule();
		void				RedrawIcon();

		BMessage			*aRules;
		int32				aSelectedRule;

		BBox				*aRulesBox;
		BCheckBox			*aCheckBox;
};

//---------------------------------------------------
//	Archiver Settings View switch (show/hide settings)
//---------------------------------------------------

class ASettingsSwitch : public AView
{
	public:
							ASettingsSwitch();
							~ASettingsSwitch();
		void				Invoke();
		void				MouseDown( BPoint point);
		void				MouseUp( BPoint point);
		void				MouseMoved( BPoint where, uint32 code, const BMessage *a_message);
		void				FrameResized( float width, float height);
		void				Draw( BRect updateRect);
		void				DrawArrow();

		bool				aState;
		int32				aArrowState;
		int32				aArrowVMiddle;
		BRect				aArrowRect;

		ASettingsView		*aSettingsView;
};

//---------------------------------------------------
//	Archiver Main Window
//---------------------------------------------------
class ArchiverWindow : public BWindow
{
	public:
						ArchiverWindow( BMessage *refs = NULL);
						~ArchiverWindow();
		void			MessageReceived( BMessage *msg);
		bool			QuitRequested();
		void			Quit();
		void			ChangeSettings( BMessage *settings);
		void			Compress( BMessage *refs);
		void			Reorganize();
		
		BMessage		*GetSettings() { return aSettings; };
		
	private:
		void			LoadSettings();
		void			LoadDefaultSettings();
		void			SaveSettings();

		ASettingsSwitch	*aSettingsSwitch;

		BMessage		*aSettings;

		typedef BWindow _inherited;
};

//---------------------------------------------------
//	Archiver BApplication
//---------------------------------------------------
class ArchiverApp : public BApplication
{
	public:
						ArchiverApp();
						~ArchiverApp();
		void			RefsReceived( BMessage *msg);
};

//----------------------------------------------------------------------------
//
//	Functions
//
//----------------------------------------------------------------------------

int32	Compress( void *Data);

#endif /*__ARCHIVER_H_*/