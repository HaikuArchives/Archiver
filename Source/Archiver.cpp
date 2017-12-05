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

//----------------------------------------------------------------------------
//
//	Include
//
//----------------------------------------------------------------------------

#include "Archiver.h"


#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <image.h>
#include <stdlib.h>

#include <signal.h>

extern char	**environ;

static char zipCmd[] = "/bin/zip";

//----------------------------------------------------------------------------
//
//	Functions :: ArchiverView
//
//----------------------------------------------------------------------------

//---------------------------------------------------
//	Constructor
//---------------------------------------------------
AView::AView( BMessage *settings)
	:BView( BRect( 0, 0, 0, 0), "AView", B_FOLLOW_LEFT_RIGHT, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	aButton( NULL),
	aTitle( NULL),
	aSettings( new BMessage( *settings)),
	aIcon( NULL),
	aWidth( 0),
	aHeight( 0),
	aLeftMargin( B_LARGE_ICON + 8)
{
	SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR));
	SetLowColor( ViewColor());
	SetDrawingMode( B_OP_OVER);
}

//---------------------------------------------------
//	Constructor - no aSettings needed
//---------------------------------------------------
AView::AView()
	:BView( BRect( 0, 0, 0, 0), "AView", B_FOLLOW_LEFT_RIGHT, B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
	aButton( NULL),
	aTitle( NULL),
	aSettings( NULL),
	aIcon( NULL),
	aWidth( 0),
	aHeight( 0),
	aLeftMargin( B_LARGE_ICON + 8)
{
	SetViewColor( ui_color( B_PANEL_BACKGROUND_COLOR));
	SetLowColor( ViewColor());
	SetDrawingMode( B_OP_OVER);
}

//---------------------------------------------------
//	Destructor
//---------------------------------------------------
AView::~AView()
{
	if( aButton != NULL)
		delete aButton;

	if( aTitle != NULL)
		delete aTitle;

	if( aSettings != NULL)
		delete aSettings;

	if( aIcon != NULL)
		delete aIcon;
}

//---------------------------------------------------
//	Draw view - margin and icon for mime type
//---------------------------------------------------
void
AView::Draw( BRect updateRect)
{
	BRect bounds = Bounds();

	// left margin background
	SetHighColor( tint_color( ViewColor(), B_HIGHLIGHT_BACKGROUND_TINT));
	FillRect( BRect( 0, 0, B_LARGE_ICON, bounds.bottom));
	
	// aIcon if any
	if( aIcon != NULL)
	{
		DrawBitmap( aIcon, BPoint( (B_LARGE_ICON / 2), aTopMargin));
	}
}

//---------------------------------------------------
//	Updates aIcon so it's icon for given mime type
//---------------------------------------------------
bool
AView::LoadIconForMime( char *mime)
{
	bool result = false;
	BMimeType mimeType( mime);
	if( mimeType.InitCheck() == B_OK)
	{
		if( aIcon == NULL)
			aIcon = new BBitmap( BRect( 0, 0, B_LARGE_ICON-1, B_LARGE_ICON-1), B_CMAP8);

		if( mimeType.GetIcon( aIcon, B_LARGE_ICON) != B_OK)
		{
			// there was no icon - free archiveIcon
			delete aIcon;
			aIcon = NULL;
		}
		else
			result = true;
	}
	return result;
}


//----------------------------------------------------------------------------
//
//	Functions :: ComressView
//
//----------------------------------------------------------------------------

//---------------------------------------------------
//	Constructor
//---------------------------------------------------
ACompressView::ACompressView( BMessage *refs, BMessage *settings)
	:AView( settings),
	aRefs( new BMessage( *refs)),
	aRefsCount( 0),
	aCompressThread( 0),
	aCompressWatcherThread( 0)
{
	// count refs
	type_code typecode;
	aRefs->GetInfo("refs", &typecode, &aRefsCount);

	// archive name
	GenerateAName( &aPath);
	aRefs->AddString( ARCHIVER_REFS_ARCHIVE_NAME, aPath.Leaf());

	// some temporary variables
	BFont		font = be_plain_font;
	float		fontsize = font.Size();
	font_height	fontheight;
	float		lineheight;

	BRect		rect;

	// set basic width and height
	aWidth		= aLeftMargin;
	aHeight		= aTopMargin;

	// icon
	const char *mime;
	aSettings->FindString( ARCHIVER_SETTINGS_FILE_MIME, &mime);
	if( LoadIconForMime( (char*)mime))
	{
		aWidth += B_LARGE_ICON / 2;
		aLeftMargin += B_LARGE_ICON / 2;
		aHeight += B_LARGE_ICON;
	}
	
	// title
	font.SetSize( fontsize + 2);
	font.SetFace( B_BOLD_FACE);
	font.GetHeight( &fontheight);
	lineheight = ceil( fontheight.ascent + fontheight.descent + fontheight.leading);
	
	aTitle = new BStringView( BRect( aLeftMargin, aTopMargin, aLeftMargin, aTopMargin), "", "Compressing files");
	aTitle->SetFont( &font, B_FONT_ALL);
	aTitle->ResizeToPreferred();
	rect = aTitle->Frame();
	
	if( aWidth < rect.right) aWidth = (int32)ceil( rect.right);
	if( aHeight < rect.bottom) aHeight = (int32)ceil( rect.bottom);
	
	// text
	char *text = new char[B_FILE_NAME_LENGTH+strlen( "Creating archive: ")];
	sprintf( text, "Creating archive: %s", aPath.Leaf());	
	
	aText = new BStringView( BRect( aLeftMargin, aTopMargin + lineheight + fontheight.leading, aLeftMargin, aTopMargin), "", text);
	delete text;

	font.SetSize( fontsize);
	font.SetFace( B_REGULAR_FACE);
	font.GetHeight( &fontheight);

	aText->SetFont( &font, B_FONT_ALL);
	aText->ResizeToPreferred();
	rect = aText->Frame();
	
	if( aWidth < rect.right) aWidth = (int32)ceil( rect.right);
	if( aHeight < rect.bottom) aHeight = (int32)ceil( rect.bottom);

	// "Stop" button
	aButton = new BButton( BRect( aWidth, ceil( rect.bottom + fontheight.leading) + 8, aWidth, aHeight), "", "Stop", new BMessage( ARCHIVER_MSG_STOP), B_FOLLOW_RIGHT | B_FOLLOW_TOP);
	aButton->SetFont( &font, B_FONT_ALL);
	aButton->ResizeToPreferred();
	rect = aButton->Frame();

	if( aWidth < rect.right) aWidth = (int32)ceil( rect.right);
	if( aHeight < rect.bottom) aHeight = (int32)ceil( rect.bottom);
	
	ResizeTo( aWidth+=2, aHeight+=2);

	AddChild( aTitle);
	AddChild( aText);
	AddChild( aButton);
}

//---------------------------------------------------
//	Destructor
//---------------------------------------------------
ACompressView::~ACompressView()
{
	delete aText;
	delete aRefs;
}


//---------------------------------------------------
//	Attached to window - start compression
//---------------------------------------------------
void
ACompressView::AttachedToWindow()
{
	aCompressWatcherThread = spawn_thread( Compress, "CompressWatcher", B_LOW_PRIORITY, (void*)this);
	resume_thread( aCompressWatcherThread);

	aButton->SetTarget( this);
}

//---------------------------------------------------
//	Detached from window - kill compression if it's still there
//---------------------------------------------------
void
ACompressView::DetachedFromWindow()
{
	thread_id threadID = GetCompressThread();
	if( threadID)
	{
		// kill watcher
		kill_thread( aCompressWatcherThread);

		// quit zip gently, so it will delete temp file
		send_signal( (pid_t)threadID, SIGTERM);

		// delete not finished file if it was left by compressing application
		BEntry entry( aPath.Path());
		if( entry.Exists())
			entry.Remove();
	}
}

//---------------------------------------------------
//	Generate ArchiveName - if name exist add counter to it
//---------------------------------------------------
void
ACompressView::MessageReceived( BMessage *msg)
{
	switch( msg->what)
	{
		case ARCHIVER_MSG_STOP:
		{
			if( Stop())
			{
				BMessage rmsg(ARCHIVER_MSG_REMOVE_AVIEW);
				rmsg.AddPointer( "view", (const void*)(AView*)this);
				Window()->PostMessage( &rmsg);
			}
			break;
		}
		case ARCHIVER_MSG_COMPRESS_END:
		{
			bool close;
			aSettings->FindBool( ARCHIVER_SETTINGS_CLOSE_WIN, &close);
			if( close)
			{
				BMessage rmsg(ARCHIVER_MSG_REMOVE_AVIEW);
				rmsg.AddPointer( "view", (const void*)(AView*)this);
				Window()->PostMessage( &rmsg);
			}
			else
			{
				aTitle->SetText("Compression finished");
				aTitle->ResizeToPreferred();
				aButton->SetLabel("OK");
			}
			break;
		}
		case ARCHIVER_MSG_COMPRESS_THREAD_ID:
		{
			msg->FindInt32( "thread_id", &aCompressThread);
			break;
		}
		default:
		{
			_inherited::MessageReceived( msg);
			break;
		}
	}
}

//---------------------------------------------------
//	Generate filename for archive
//	if name exist add counter to it
//	set result to generated full path (directory+name)
//---------------------------------------------------
void
ACompressView::GenerateAName( BPath *result)
{
	// temps
	char name[B_FILE_NAME_LENGTH];
	char path[B_PATH_NAME_LENGTH];
	const char *extension;

	// get current dir
	entry_ref dir_ref;
	aRefs->FindRef( ARCHIVER_REFS_DIR_REF, &dir_ref);
	result->SetTo( &dir_ref);

	// get file extension
	aSettings->FindString( ARCHIVER_SETTINGS_FILE_EXT, &extension);

	// there is only one file, name archive after it
	if( aRefsCount == 1)
	{
		entry_ref ref;
		aRefs->FindRef( "refs", &ref);
		strcpy( name, ref.name);
	}
	// ... else name it "Archive"
	else
		strcpy( name, "Archive");

	// now check if path already exist, if so generate original path by adding counter to it
	int32 i = 0;
	sprintf( path, "%s/%s%s", result->Path(), name, extension);
	BEntry entry;
	while( B_OK == entry.SetTo( (const char*)path))
	{
		if( entry.Exists())
		{
			sprintf( path, "%s/%s %ld%s", result->Path(), name, ++i, extension);
		}
		else
			break;
	}

	// here it goes!
	result->SetTo( path);
}

//---------------------------------------------------
//	Returns aCompressThread if it's valid, NULL if not
//---------------------------------------------------
thread_id
ACompressView::GetCompressThread()
{
	thread_info threadinfo;
	if( get_thread_info( aCompressThread, &threadinfo) == B_OK)
	{
		// aCompressThread is still running - return it's id
		return aCompressThread;
	}
	
	// there was no aCompressThread running
	return 0;
}

//---------------------------------------------------
//	If compression is still running ask to really quit it
//---------------------------------------------------

bool
ACompressView::Stop()
{
	// if CompressThread is still there, ask user if Quit it, or keeep going
	thread_id threadid = GetCompressThread();
	if( threadid)
	{
		// suspend CompressThread while asking question
		// if it can't be suspended (thread quit probably) give permission to Quit()
		if( suspend_thread( threadid) == B_BAD_THREAD_ID)
			return true;

		// ask question
		if( ((new BAlert( "", "Are You sure You want to stop creating this archve?", "Stop", "Keep going", NULL, B_WIDTH_AS_USUAL, B_STOP_ALERT))->Go()) == 0)
		{
			// quit zip gently, so it will delete temp file
			send_signal( (pid_t)threadid, SIGTERM);
			resume_thread( threadid);

			// delete not finished file if it was left by compressing application
			BEntry entry( aPath.Path());
			if( entry.Exists())
				entry.Remove();
			
			return true;
		}
		else
		{
			// user doesn't want to quit, resume Compress
			// if it can't be resumed (thread doesn't exist anymore probably) give permission to Quit()
			if( resume_thread( threadid) == B_BAD_THREAD_ID)
				return true;
			else
				return false;
		}
	}
	return true;
}


//----------------------------------------------------------------------------
//
//	Functions :: SettingsView
//
//----------------------------------------------------------------------------

//---------------------------------------------------
//	Constructor
//---------------------------------------------------
ASettingsView::ASettingsView( BMessage *settings)
	:AView( settings),
	aRules( LoadRules())
{
	// some temporary variables
	BFont		font = be_plain_font;
	float		fontsize = font.Size();
	font_height	fontheight;
	float		lineheight;

	BRect		rect;

	// set basic width and height
	aWidth		= (aLeftMargin += B_LARGE_ICON / 2);
	aHeight		= aTopMargin;
	
	// title
	font.SetSize( fontsize + 5);
	font.SetFace( B_BOLD_FACE);
	font.GetHeight( &fontheight);
	lineheight = ceil( fontheight.ascent + fontheight.descent + fontheight.leading);
	
	aTitle = new BStringView( BRect( aLeftMargin, aTopMargin, aLeftMargin, aTopMargin), "", "Archiver Settings");
	aTitle->SetFont( &font, B_FONT_ALL);
	aTitle->ResizeToPreferred();
	rect = aTitle->Frame();
	
	if( aWidth < rect.right) aWidth = (int32)ceil( rect.right);
	if( aHeight < rect.bottom) aHeight = (int32)ceil( rect.bottom);

	// rules selection - build BBox, and add BRadioButtons to it (one for each Rule)
	font.SetSize( fontsize);
	font.SetFace( B_BOLD_FACE);
	font.GetHeight( &fontheight);
	lineheight = ceil( fontheight.ascent + fontheight.descent + fontheight.leading);
	
	aHeight += 5; // some space between main title and aRulesBox title

	aRulesBox = new BBox( BRect( aLeftMargin, aHeight, aLeftMargin, aHeight));
	aRulesBox->SetLabel( "Select default compression format");
	aRulesBox->SetFont( &font, B_FONT_ALL);
	font.SetFace( B_REGULAR_FACE);

		// add RadioButtons
		int32	rindex = 0;		// rules index
		char	*rname;			// rule name
		char	*rdesc;			// rule description
		char	*rdesc2;		// rule variation
		char	*sdesc;			// settings rule description
		char	*sdesc2;		// settings rule variation
		char	*path;			// compression tool path
		bool	found = false;	// was settings rule found?
		BEntry	entry;			// to check if path exists
		aSettings->FindString( ARCHIVER_SETTINGS_FILE_DESC, (const char**)&sdesc);
		aSettings->FindString( ARCHIVER_SETTINGS_FILE_DESC2, (const char**)&sdesc2);

		BRadioButton *item;
		BMessage *imsg;					// item message
		char ilabel[1024];				// item label
		float iwidth, iheight;			// item width and height
		float bwidth = 0;				// aRulesBox width
		float bheight = lineheight;		// aRulesBox height
		
		while( aRules->FindString( "rules", rindex++, (const char**)&rname) == B_OK)
		{
			aRules->FindString( rname, 0, (const char**)&rdesc);
			aRules->FindString( rname, 1, (const char**)&rdesc2);
			
			// check if compression tool exists, if not omit rule
			aRules->FindString( rname, 4, (const char**)&path);
			entry.SetTo(path);
			if( entry.InitCheck() != B_OK || !entry.Exists())
				continue;

			imsg = new BMessage( ARCHIVER_MSG_CHANGE_RULE);
			imsg->AddInt32( "index", rindex-1);
			
			if( rdesc2[0])
			{
				sprintf( ilabel, "%s [%s]", rdesc, rdesc2);
				item = new BRadioButton( BRect( 8, bheight, 0, bheight), rname, ilabel, imsg);
			}
			else
				item = new BRadioButton( BRect( 8, bheight, 0, bheight), rname, rdesc, imsg);
				
			item->SetFont( &font, B_FONT_ALL);
			item->GetPreferredSize( &iwidth, &iheight);
			item->ResizeTo( iwidth, iheight);
			
			if( bwidth < iwidth) bwidth = iwidth;
			bheight += iheight;
			
			aRulesBox->AddChild( item);
			
			if( !found && !strcmp( sdesc, rdesc) && !strcmp( sdesc2, rdesc2))
			{
				item->SetValue( 1);
				found = true;
				aSelectedRule = rindex-1;
				
				// get icon for it
				char *mime;
				aRules->FindString( rname, 2, (const char**)&mime);
				LoadIconForMime( mime);
			}
		}
	
	aRulesBox->ResizeBy( bwidth+16, bheight+4);
	rect = aRulesBox->Frame();
	
	if( aWidth < rect.right) aWidth = (int32)ceil( rect.right);
	if( aHeight < rect.bottom) aHeight = (int32)ceil( rect.bottom);

	// "Close window" checkbox
	bool close;
	aSettings->FindBool( ARCHIVER_SETTINGS_CLOSE_WIN, &close);

	aCheckBox = new BCheckBox( BRect( aLeftMargin, aHeight, aLeftMargin, aHeight), "", "Close window after compresssion", new BMessage( ARCHIVER_MSG_CHANGE_CLOSE_WIN));
	font.SetFace( B_BOLD_FACE);
	aCheckBox->SetFont( &font, B_FONT_ALL);
	font.SetFace( B_REGULAR_FACE);
	if( close) aCheckBox->SetValue( 1);
	aCheckBox->ResizeToPreferred();
	rect = aCheckBox->Frame();

	if( aWidth < rect.right) aWidth = (int32)ceil( rect.right);
	if( aHeight < rect.bottom) aHeight = (int32)ceil( rect.bottom);

	// "OK" button
	aButton = new BButton( BRect( aWidth, ceil( rect.bottom + fontheight.leading) + 8, aWidth, aHeight), "", "Accept", new BMessage( ARCHIVER_MSG_ACCEPT));
	aButton->SetFont( &font, B_FONT_ALL);
	aButton->ResizeToPreferred();
	aButton->SetEnabled( false);
	rect = aButton->Frame();
	
	aButton->MoveBy( -rect.Width(), 0);

	if( aHeight < rect.bottom) aHeight = (int32)ceil( rect.bottom);


	// resize to let all children be visible, add some bottom and right margin
	ResizeTo( aWidth+=3, aHeight+=2);

	// add children
	AddChild( aTitle);
	AddChild( aRulesBox);
	AddChild( aCheckBox);
	AddChild( aButton);

	// FrameResized() must be called to resize aRulesBox and move aButton
	SetFlags( B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE);
}

//---------------------------------------------------
//	Destructor
//---------------------------------------------------
ASettingsView::~ASettingsView()
{
	delete aRules;
	delete aCheckBox;
	delete aRulesBox;
}

//---------------------------------------------------
//	Attached to window - change buttons target to this
//---------------------------------------------------
void
ASettingsView::AttachedToWindow()
{
	int32 index = 0;
	BRadioButton *radio;
	while( ( radio = dynamic_cast<BRadioButton*>( aRulesBox->ChildAt( index++))) != NULL)
	{
		radio->SetTarget( this);
	}
	aCheckBox->SetTarget( this);
	aButton->SetTarget( this);
}

//---------------------------------------------------
//	view was resized - resize aRulesBox and move aButton
//---------------------------------------------------
void
ASettingsView::FrameResized( float width, float height)
{
	if( width >= aWidth)
	{
		BRect rect = aRulesBox->Frame();
		aRulesBox->ResizeTo( width - aLeftMargin - 3, rect.Height());
		rect = aButton->Frame();
		aButton->MoveTo( BPoint( width - rect.Width() - 3, rect.top));
	}
}

//---------------------------------------------------
//	Messages resolver
//---------------------------------------------------
void
ASettingsView::MessageReceived( BMessage *msg)
{
	switch( msg->what)
	{
		case ARCHIVER_MSG_CHANGE_RULE:
		{
			int32 oldSelectedRule = aSelectedRule;
			msg->FindInt32( "index", &aSelectedRule);
			if( oldSelectedRule != aSelectedRule)
			{
					// change View's icon to show icon for file compressed by cosen compression method
					char *rname;
					aRules->FindString( "rules", aSelectedRule, (const char**)&rname);
					char *mime;
					aRules->FindString( rname, 2, (const char**)&mime);
					LoadIconForMime( mime);
					RedrawIcon();
					aButton->SetEnabled( true);
			}
			break;
		}
		case ARCHIVER_MSG_CHANGE_CLOSE_WIN:
		{
			int32 value;
			if( msg->FindInt32( "be:value", &value) == B_OK)
			{
				bool close = value;
				aSettings->ReplaceBool( ARCHIVER_SETTINGS_CLOSE_WIN, close);
				aButton->SetEnabled( true);
			}
			break;
		}
		case ARCHIVER_MSG_ACCEPT:
		{
			ChangeSettingsRule();
			if( Window()->CountChildren() < 2)
			{
				ASettingsSwitch *parent = dynamic_cast<ASettingsSwitch*>(Parent());
				if( parent)
					parent->Invoke();
			}
			else
				aButton->SetEnabled( false);
		}
		default:
		{
			_inherited::MessageReceived( msg);
			break;
		}
	}
}

//---------------------------------------------------
//	Load rules to target BMessage
//---------------------------------------------------
BMessage *
ASettingsView::LoadRules()
{
	BMessage *Rules = new BMessage();
		
	// make path to rules file
	BPath path;
	if( find_directory( B_SYSTEM_ETC_DIRECTORY, &path) != B_OK)
	{
		path.SetTo( ARCHIVER_RULES_FILE_PATH);
		path.Append( ARCHIVER_RULES_FILE);
	}
	else
		path.Append( ARCHIVER_RULES_FILE);
		
	// open settings file and read rules			
	BFile file;
	if( file.SetTo( path.Path(), B_READ_ONLY) == B_OK)
	{
		off_t size;
		file.GetSize( &size);
		char *data = (char*)malloc( size);

		file.Read( data, size);
		
		// read lines, one by one and tokenize them
		int32 counter = 0;
		char name[128];
		char *line = data;
		char *npos = 0;
		while( ( npos = strchr( line, 10)))// find "\n"
		{
			sprintf( name, "rule[%ld]", counter);
			Rules->AddString( "rules", name);
			counter++;
			
			*npos = 0;
			
			char *tabpos = NULL;
			while( ( tabpos = strchr( line, 9)))// find "\t"
			{
				*tabpos = 0;
				
				Rules->AddString( name, line);

				line = tabpos+1;
			}
			Rules->AddString( name, line);
			
			*npos = 10;
			
			line = npos+1;
		}

		free( data);
	}
	// couldn't open file, set rules to default, try to write new settings file
	else
	{
		// set ZIP as default
		Rules->AddString( "rules", "rule[0]");							// rule name
		Rules->AddString( "rule[0]", "ZIP compressed file");			// description
		Rules->AddString( "rule[0]", "maximum compression");			// variation
		Rules->AddString( "rule[0]", "application/x-zip-compressed");	// mime
		Rules->AddString( "rule[0]", ".zip");							// extension
		Rules->AddString( "rule[0]", zipCmd);							// executable
		Rules->AddString( "rule[0]", "-9");
		Rules->AddString( "rule[0]", "-r");
		Rules->AddString( "rule[0]", "-y");

		if( file.SetTo( path.Path(), B_READ_WRITE | B_CREATE_FILE) == B_OK)
		{
			char default_rule[] = "ZIP compressed file\tmaximum compression\tapplication/x-zip-compressed\t.zip\t";
			char zipArgs[] = "\t-9\t-r\t-y\t"ARCHIVER_SETTINGS_FILENAME"\n";
			file.Write( (char*)default_rule, sizeof( default_rule)-1);
			file.Write( (char*)zipCmd, sizeof( zipCmd)-1);
			file.Write( (char*)zipArgs, sizeof( zipArgs)-1);
		}
	}

	return Rules;
}

//---------------------------------------------------
//	Change Settings, save settings
//---------------------------------------------------
void
ASettingsView::ChangeSettingsRule()
{
	if( aRules != NULL && aSettings != NULL)
	{
		// delete old compression Settings
		aSettings->RemoveName( ARCHIVER_SETTINGS_FILE_DESC);
		aSettings->RemoveName( ARCHIVER_SETTINGS_FILE_DESC2);
		aSettings->RemoveName( ARCHIVER_SETTINGS_FILE_MIME);
		aSettings->RemoveName( ARCHIVER_SETTINGS_FILE_EXT);
		aSettings->RemoveName( ARCHIVER_SETTINGS_OPTION);

		int32 index = 0;
		char *name;
		char *temp;

		// get rule name, if ok, search for settings for it
		if( aRules->FindString( "rules", aSelectedRule, (const char**)&name) == B_OK)
		{
			// file description
			aRules->FindString( name, index++, (const char**)&temp);
			aSettings->AddString( ARCHIVER_SETTINGS_FILE_DESC, temp);

			// file description2 - for different variations (i.e. ZIP compressed file - maximum compression)				
			aRules->FindString( name, index++, (const char**)&temp);
			aSettings->AddString( ARCHIVER_SETTINGS_FILE_DESC2, temp);
				
			// file mime type
			aRules->FindString( name, index++, (const char**)&temp);
			aSettings->AddString( ARCHIVER_SETTINGS_FILE_MIME, temp);
				
			// file extension
			aRules->FindString( name, index++, (const char**)&temp);
			aSettings->AddString( ARCHIVER_SETTINGS_FILE_EXT, temp);
				
			// options (first goes compression tool path and next options for it, i.e. "-9")
			while( aRules->FindString( name, index++, (const char**)&temp) == B_OK)
			{
				aSettings->AddString( ARCHIVER_SETTINGS_OPTION, temp);
			}
		}
		((ArchiverWindow*)Window())->ChangeSettings( aSettings);
	}
}

//---------------------------------------------------
//	Draw Icon - first it's background, than icon itself
//---------------------------------------------------
void
ASettingsView::RedrawIcon()
{
	float left = B_LARGE_ICON / 2;
	float right = left + B_LARGE_ICON;
	float top = 8;
	float bottom = top + B_LARGE_ICON;
	float vmiddle = B_LARGE_ICON + 1;

	// left margin background
	SetHighColor( tint_color( ViewColor(), B_HIGHLIGHT_BACKGROUND_TINT));
	FillRect( BRect( 0, top, vmiddle, bottom));
	
	// background
	SetHighColor( ViewColor());
	FillRect( BRect( vmiddle, top, right, bottom));

	// aIcon if any
	if( aIcon != NULL)
	{
		DrawBitmap( aIcon, BPoint( left, top));
	}
}


//----------------------------------------------------------------------------
//
//	Functions :: SettingsSwitch
//
//----------------------------------------------------------------------------

//---------------------------------------------------
//	Constructor
//---------------------------------------------------
ASettingsSwitch::ASettingsSwitch()
	:AView(),
	aState( false),
	aArrowState( 0),
	aSettingsView( NULL)
{

	// some temporary variables
	BRect		rect;

	// title
	aTitle = new BStringView( BRect( aLeftMargin+10, 0, aLeftMargin+10, 0), "", "Show settings");
	aTitle->ResizeToPreferred();
	aTitle->SetHighColor( tint_color( ViewColor(), B_HIGHLIGHT_BACKGROUND_TINT));

	// set variables needed for arrow
	rect = aTitle->Frame();
	aArrowVMiddle = int32( rect.Height() / 2);
	aArrowRect = BRect( aLeftMargin-2, aArrowVMiddle-4, aLeftMargin+8, aArrowVMiddle+4);

	aWidth = (int32)ceil( rect.right) + 10;
	aHeight = (int32)ceil( rect.bottom);

	ResizeToPreferred();

	AddChild( aTitle);

	// FrameResized() must be called, because automatic resizing doesn't work too good
	SetFlags( B_WILL_DRAW | B_FRAME_EVENTS | B_FULL_UPDATE_ON_RESIZE);
}

//---------------------------------------------------
//	Constructor
//---------------------------------------------------
ASettingsSwitch::~ASettingsSwitch()
{
	if( aSettingsView != NULL)
		delete aSettingsView;
}

//---------------------------------------------------
//	Show/Hide aSettingsView, set arrow, redraw etc..
//---------------------------------------------------
void
ASettingsSwitch::Invoke()
{
	if( aState)
	{
		aArrowState = 0;
		aTitle->SetText( "Show settings");
		aState = false;
		if( aSettingsView)
		{
			BRect rect = aTitle->Frame();
			aWidth = (int32)ceil( rect.right) + 10;
			aHeight = (int32)ceil( rect.bottom);

			RemoveChild( aSettingsView);
			delete aSettingsView;
			aSettingsView = NULL;

			ResizeToPreferred();
			((ArchiverWindow*)Window())->Reorganize();
		}
	}
	else
	{
		aArrowState = 2;
		aTitle->SetText( "Hide settings");
		aState = true;
		if( aSettingsView == NULL)
		{
			aSettingsView = new ASettingsView( ((ArchiverWindow*)Window())->GetSettings());
			aSettingsView->MoveTo( BPoint( 0, aHeight));

			aHeight += aSettingsView->aHeight;
			if( aWidth < aSettingsView->aWidth) aWidth = aSettingsView->aWidth;

			ResizeToPreferred();
			((ArchiverWindow*)Window())->Reorganize();

			AddChild( aSettingsView);
		}
	}
	Invalidate( aArrowRect);
}

//---------------------------------------------------
//	MouseDown - redraw arrow if needed
//---------------------------------------------------
void
ASettingsSwitch::MouseDown( BPoint point)
{
	if( aArrowRect.Contains( point))
	{
		aArrowState = 1;
		Invalidate( aArrowRect);
	}
}

//---------------------------------------------------
//	MouseUp - Invoke and redraw if needed
//---------------------------------------------------
void
ASettingsSwitch::MouseUp( BPoint point)
{
	if( aArrowRect.Contains( point))
		Invoke();
}

//---------------------------------------------------
//	MouseMoved - if out of rect, cancel switching
//---------------------------------------------------
void
ASettingsSwitch::MouseMoved( BPoint where, uint32 code, const BMessage *a_message)
{
	if( !aArrowRect.Contains( where) && aArrowState == 1)
	{
		if( aState)
			aArrowState = 2;
		else
			aArrowState = 0;

		Invalidate( aArrowRect);
	}
}

//---------------------------------------------------
//	switch was resized - resize SettingsView to proper size
//---------------------------------------------------
void
ASettingsSwitch::FrameResized( float width, float height)
{
	if( aSettingsView)
	{
		float swidth, sheight;
		aSettingsView->GetPreferredSize( &swidth, &sheight);
		if( width > swidth) swidth = width;
		if( height > sheight) sheight = height;
		aSettingsView->ResizeTo( swidth, sheight);
	}
}

//---------------------------------------------------
//	Draw - redraw arrow, inherited draw
//---------------------------------------------------
void
ASettingsSwitch::Draw( BRect updateRect)
{
	AView::Draw( updateRect);
	DrawArrow();
}

//---------------------------------------------------
//	MouseDown - redraw arrow
//---------------------------------------------------
void
ASettingsSwitch::DrawArrow()
{
	switch( aArrowState)
	{
		case 0: // arrow turned right >
			FillTriangle( BPoint( aLeftMargin+2, aArrowVMiddle-4), BPoint( aLeftMargin+2, aArrowVMiddle+4), BPoint( aLeftMargin+6, aArrowVMiddle));
			break;
		case 1: // arrow turned down right _|
			FillTriangle( BPoint( aLeftMargin, aArrowVMiddle+2), BPoint( aLeftMargin+6, aArrowVMiddle+2), BPoint( aLeftMargin+6, aArrowVMiddle-4));
			break;
		case 2: // arrow turned down V
			FillTriangle( BPoint( aLeftMargin, aArrowVMiddle-2), BPoint( aLeftMargin+8, aArrowVMiddle-2), BPoint( aLeftMargin+4, aArrowVMiddle+2));
			break;
	}
}


//----------------------------------------------------------------------------
//
//	Functions :: ArchiverWindow
//
//----------------------------------------------------------------------------

//---------------------------------------------------
//	Constructor - build ArchiverWindow
//---------------------------------------------------
ArchiverWindow::ArchiverWindow( BMessage *refs)
	:BWindow( BRect( 0, 0, 0, 0), "Archiver", B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, B_NOT_RESIZABLE | B_NOT_ZOOMABLE | B_ASYNCHRONOUS_CONTROLS),
	aSettings( NULL)
{
	aSettingsSwitch = new ASettingsSwitch();
	AddChild( aSettingsSwitch);

	//
	LoadSettings();
	
	// restore window position on screen
	BPoint pos;
	if( aSettings->FindPoint( ARCHIVER_SETTINGS_WIN_POS, &pos) == B_OK)
		MoveTo( pos);
	else
		MoveTo( 200, 200);

	// get count of refs (files passed to compress) if launched from Tracker as Add-On
	int32 refscount = 0;
	if( refs != NULL)
	{
		type_code typecode;
		refs->GetInfo( "refs", &typecode, &refscount);
	}
	
	// if 0 files (no file was selected in Tracker or Archiver was launched as an application)
	// than show settings
	if( !refscount)
	{
		aSettingsSwitch->Invoke();
	}
	else
	{
		// run and show compression
		ACompressView *AView = new ACompressView( refs, aSettings);
		BRect rect = AView->Frame();
		ResizeTo( rect.Width(), rect.Height() + aSettingsSwitch->aHeight);
		AView->MoveBy( 0, aSettingsSwitch->aHeight);
		AddChild( AView);
	}
}

//---------------------------------------------------
//	Destructor - delete objects
//---------------------------------------------------
ArchiverWindow::~ArchiverWindow()
{
	aSettingsSwitch->RemoveSelf();
	delete aSettingsSwitch;

	if( aSettings != NULL)
		delete aSettings;
}

//---------------------------------------------------
//	Messages resolver
//---------------------------------------------------
void
ArchiverWindow::MessageReceived( BMessage *msg)
{
	if( msg->WasDropped() && msg->HasRef( "refs")) {
		be_app->LockLooper();
		be_app->RefsReceived( msg);
		be_app->UnlockLooper();
		return;
	}

	switch( msg->what)
	{
		case ARCHIVER_MSG_REMOVE_AVIEW:
		{
			AView *view;
			if( msg->FindPointer( "view", (void**)&view) == B_OK)
			{
				view->RemoveSelf();
				ASettingsView *sview;
				ACompressView *cview;
				if( ( sview = dynamic_cast<ASettingsView*>(view)) != NULL)
				{
					delete sview;
				}
				else if( ( cview = dynamic_cast<ACompressView*>(view)) != NULL)
				{
					delete cview;
				}
			}
			Reorganize();
		}
		default:
			_inherited::MessageReceived( msg);
			break;
	}
}

//---------------------------------------------------
//	Quit application - delete objects etc..
//---------------------------------------------------
bool
ArchiverWindow::QuitRequested()
{
	int32 index = 0;
	BView *view;
	ACompressView *cview;
	while(( view = ChildAt(index++)) != NULL)
	{
		if( ( cview = dynamic_cast<ACompressView*>( view)) != NULL)
		{
			if( !cview->Stop())
				return false;
		}
	}
	return true;
}

//---------------------------------------------------
//	Quit application - delete objects etc..
//---------------------------------------------------
void
ArchiverWindow::Quit()
{
	// save window position on screen and other settings
	BRect frame = Frame();
	aSettings->ReplacePoint( ARCHIVER_SETTINGS_WIN_POS, frame.LeftTop());
	SaveSettings();
	
	// quit app on window close
	be_app->PostMessage( B_QUIT_REQUESTED);
	
	// call default Quit()
	BWindow::Quit();
}

//---------------------------------------------------
//	Change Settings, save settings
//---------------------------------------------------
void
ArchiverWindow::ChangeSettings( BMessage *settings)
{
	*aSettings = *settings;
	SaveSettings();
}

//---------------------------------------------------
//	Default settings
//---------------------------------------------------
void
ArchiverWindow::LoadDefaultSettings()
{
	// there is no Settings message, create one
	if( aSettings == NULL)
		aSettings = new BMessage();

	// set default settings
	aSettings->AddInt32( ARCHIVER_SETTINGS_PRIORITY, B_LOW_PRIORITY);
	aSettings->AddPoint( ARCHIVER_SETTINGS_WIN_POS, BPoint( 200, 200));
	aSettings->AddBool( ARCHIVER_SETTINGS_CLOSE_WIN, true);

	// set default compression tool (ZIP)
	aSettings->AddString( ARCHIVER_SETTINGS_FILE_DESC, "ZIP compressed file");
	aSettings->AddString( ARCHIVER_SETTINGS_FILE_DESC2, "maximum compression");
	aSettings->AddString( ARCHIVER_SETTINGS_FILE_MIME, "application/x-zip-compressed");
	aSettings->AddString( ARCHIVER_SETTINGS_FILE_EXT, ".zip");
	aSettings->AddString( ARCHIVER_SETTINGS_OPTION, zipCmd);
	aSettings->AddString( ARCHIVER_SETTINGS_OPTION, "-9");
	aSettings->AddString( ARCHIVER_SETTINGS_OPTION, "-r");
	aSettings->AddString( ARCHIVER_SETTINGS_OPTION, "-y");
	aSettings->AddString( ARCHIVER_SETTINGS_OPTION, ARCHIVER_SETTINGS_FILENAME);
}

//---------------------------------------------------
//	Loads Settings message from file or defaults if it doesn't exist
//---------------------------------------------------
void
ArchiverWindow::LoadSettings()
{
	// if ther's no Settings message, create one
	if( aSettings == NULL)
		aSettings = new BMessage();

	// make path to settings file
	BPath path;
	if( find_directory( B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
	{
		path.SetTo( ARCHIVER_SETTINGS_FILE_PATH);
		path.Append( ARCHIVER_SETTINGS_FILE);
	}
	else
		path.Append( ARCHIVER_SETTINGS_FILE);
	
	// open settings file and read settings			
	BFile file;
	if( file.SetTo( path.Path(), B_READ_WRITE) == B_OK)
	{
		aSettings->Unflatten( &file);
	}
	// couldn't open file, set settings to default, try to write new settings file
	else
	{
		LoadDefaultSettings();

		if( file.SetTo( path.Path(), B_READ_WRITE | B_CREATE_FILE) == B_OK)
		{
			aSettings->Flatten( &file);
		}
	}	
}

//---------------------------------------------------
//	Save Settings message to file, create file if it doesn't exist
//---------------------------------------------------
void
ArchiverWindow::SaveSettings()
{
	// there is no Settings message, nothing to save, return
	if( aSettings == NULL) return;

	// make path to settings file
	BPath path;
	if( find_directory( B_USER_SETTINGS_DIRECTORY, &path) != B_OK)
	{
		path.SetTo( ARCHIVER_SETTINGS_FILE_PATH);
		path.Append( ARCHIVER_SETTINGS_FILE);
	}
	else
		path.Append( ARCHIVER_SETTINGS_FILE);
	
	// open settings file (create it if there's no file) and write settings			
	BFile file;
	if( file.SetTo( path.Path(), B_READ_WRITE | B_CREATE_FILE | B_ERASE_FILE) == B_OK)
	{
		aSettings->Flatten( &file);
	}
}

//---------------------------------------------------
//	new refs received - add new compress view
//---------------------------------------------------
void
ArchiverWindow::Compress( BMessage *refs)
{
	// get count of refs (files passed to compress) if launched from Tracker as Add-On
	int32 refscount = 0;
	if( refs != NULL)
	{
		type_code typecode;
		refs->GetInfo( "refs", &typecode, &refscount);
	}
	
	if( refscount)
	{
		ACompressView *view = new ACompressView( refs, aSettings);
		AddChild( view);
		Reorganize();
	}
}

//---------------------------------------------------
//	resize to widest View, move Views to their proper y
//---------------------------------------------------
void
ArchiverWindow::Reorganize()
{
	if( CountChildren() < 2 && !aSettingsSwitch->aState)
	{
		LockLooper();
		Quit();
		return;
	}

	int32 index = 0;
	AView *view;
	float width = 0;
	float vwidth = 0;
	float height = 0;
	float vheight = 0;

	while( ( view = (AView*)ChildAt(index++)) != NULL)
	{
		view->MoveTo( 0, height);
		view->GetPreferredSize( &vwidth, &vheight);
		if( width < vwidth) width = vwidth;
		height += vheight;
	}
	ResizeTo( width, height);
	index = 0;
	while( ( view = (AView*)ChildAt(index++)) != NULL)
	{
		view->GetPreferredSize( &vwidth, &vheight);
		view->ResizeTo( width, vheight);
	}
}


//----------------------------------------------------------------------------
//
//	Functions :: ArchiverApp
//
//----------------------------------------------------------------------------

//---------------------------------------------------
//	Constructor - build and show ArchiverWindow
//---------------------------------------------------
ArchiverApp::ArchiverApp()
	:BApplication( ARCHIVER_MIME_TYPE)
{
	// check if there is B_REFS_RECEIVED message waiting
	// there should be one if Archiver was run as Tracker's Add-On
	// or from "Open with..." and similar
	BMessageQueue *msgqueue = MessageQueue();
	if( msgqueue != NULL)
	{
		BMessage *msg = msgqueue->FindMessage( (uint32)B_REFS_RECEIVED);
		// are there any refs to comress?
		if( msg != NULL)
		{
			// check if there is ARCHIVER_REFS_DIR_REF (it will if Archiver was run as Add-On)
			type_code typecode;
			if (msg->GetInfo( ARCHIVER_REFS_DIR_REF, &typecode) != B_OK)
			{
				// it wasn't sent by Tracker Add-On, so Archiver must check dir_ref itself (by taking it from first entry_ref in refs)
				entry_ref ref;
				entry_ref dir_ref;
				msg->FindRef( "refs", &ref);
				BEntry refEntry( &ref);
				BEntry dirEntry;
				refEntry.GetParent( &dirEntry);
				dirEntry.GetRef( &dir_ref);
				msg->AddRef( ARCHIVER_REFS_DIR_REF, &dir_ref);
			}
			// uff... now run compression window :)
			ArchiverWindow *Window = new ArchiverWindow( msg);
			Window->Show();

			// remove message from queue since Archiver will parse it itself, not waiting for RefsReceived()
			msgqueue->RemoveMessage( msg);
		}
		// nope - it was run by double click probably
		else
		{
			// run settings window
			ArchiverWindow *Window = new ArchiverWindow();
			Window->Show();
		}
	}
}

//---------------------------------------------------
//	Destructor
//---------------------------------------------------
ArchiverApp::~ArchiverApp()
{
}

//---------------------------------------------------
//	New refs - add compression view
//---------------------------------------------------
void
ArchiverApp::RefsReceived( BMessage *msg)
{
	type_code typecode;
	if (msg->GetInfo( ARCHIVER_REFS_DIR_REF, &typecode) != B_OK)
	{
		// it wasn't sent by Tracker Add-On, so Archiver must check dir_ref itself (by taking it from first entry_ref in refs)
		entry_ref ref;
		entry_ref dir_ref;
		msg->FindRef( "refs", &ref);
		BEntry refEntry( &ref);
		BEntry dirEntry;
		refEntry.GetParent( &dirEntry);
		dirEntry.GetRef( &dir_ref);
		msg->AddRef( ARCHIVER_REFS_DIR_REF, &dir_ref);
	}
	// run compression
	ArchiverWindow *Window = (ArchiverWindow*)WindowAt( 0);
	Window->LockLooper();
	Window->Compress( msg);
	Window->UnlockLooper();
	Window->Activate( true);
}


//----------------------------------------------------------------------------
//
//	Functions :: Main
//
//----------------------------------------------------------------------------

//---------------------------------------------------
//	Main Archiver function
//---------------------------------------------------
int
main() 
{
	ArchiverApp *app = new ArchiverApp();
	app->Run();
	
	return( 0);
}

//---------------------------------------------------
//	Called by Tracker
//---------------------------------------------------
void process_refs( entry_ref dir_ref, BMessage *msg, void *)
{
	// add dir_ref as ref - easier way to pass it to Archiver
	msg->AddRef( ARCHIVER_REFS_DIR_REF, &dir_ref);

	// run Archiver
	be_roster->Launch( ARCHIVER_MIME_TYPE, msg);
}

//---------------------------------------------------
//	Launch Zip in new thread and return it's thread_id
//---------------------------------------------------
int32
Compress( void *Data)
{
	// get arguments from Data (which is pointer to ACompressView)
	ACompressView	*View = (ACompressView*)Data;
	BMessage		*Refs = View->aRefs;
	BMessage		*Settings = View->aSettings;

	//
	int32	ref_c = 0;	// refs count
	int32	arg_c = 0;	// args (options for compression tool) count

	// count Refs and arguments for compression tool
	type_code typecode;
	Refs->GetInfo( "refs", &typecode, &ref_c);
	Settings->GetInfo( ARCHIVER_SETTINGS_OPTION, &typecode, &arg_c);

	// if there is no refs return
	if ( !ref_c)
		return -1;
	
	// more temporary variables
	BEntry	entry;
	BPath	path;
	
	// set current directory, so compression tool will work from it
	entry_ref dir_ref;
	Refs->FindRef( ARCHIVER_REFS_DIR_REF, &dir_ref);
	entry.SetTo( &dir_ref);
	entry.GetPath( &path);
	chdir( path.Path());

	// get file name for archive to be created
	char	*filename;
	Refs->FindString( ARCHIVER_REFS_ARCHIVE_NAME, (const char**)&filename);

	// if there there is name for created file, go with compression
	if( filename[0])
	{
		int			arg_index = 0;
		int			ref_index = 0;
		entry_ref	ref;
		
		// = options + filenames
		arg_c += ref_c;

		// allocate array of arguments - they will be passed to compression tool
		// plus one - last must be NULL
		char **arg_v = (char **)malloc( sizeof(char *) * (arg_c + 1));
		
		// parse arguments
		char *temp;
		int32 index = 0;
		bool found = false; // remember if there was ARCHIVER_SETTINGS_FILENAME replaced already, so it will not compare strings in each loop
		while( Settings->FindString( ARCHIVER_SETTINGS_OPTION, index++, (const char**)&temp) == B_OK)
		{
			// if it's special arg ("FILENAME") then put filename of created archive instead of it
			if( !found && !strcmp( temp, ARCHIVER_SETTINGS_FILENAME))
			{
				arg_v[arg_index++] = strdup( filename);
				found = true;
			}
			else
				arg_v[arg_index++] = strdup( temp);
		}
				
		// parse filenames
		while( Refs->FindRef( "refs", ref_index++, &ref) == B_OK)
		{
			// if user wants to store file paths there should be such option for compression tool :)
			arg_v[arg_index++] = strdup( ref.name);
		}
		
		// last argument must be NULL
		arg_v[arg_index] = NULL;

		// launch compression tool in new thread
		thread_id	exec_thread;
		status_t	exec_thread_return_value;
		int32		exec_thread_priotity = B_NORMAL_PRIORITY;
		
		exec_thread = load_image( arg_c, (const char**) arg_v, (const char**) environ); 

		rename_thread( exec_thread, "Archiver_compression_thread");
		
		if( Settings->FindInt32( ARCHIVER_SETTINGS_PRIORITY, &exec_thread_priotity) == B_OK)
			set_thread_priority( exec_thread, exec_thread_priotity);

		// send compression tool's thread_id to ACompressView, so it can have some control
		BMessage msg( ARCHIVER_MSG_COMPRESS_THREAD_ID);
		msg.AddInt32( "thread_id", exec_thread);
		BMessenger( View).SendMessage( &msg);

		// resume exec_thread, and wait until it's finished
		wait_for_thread( exec_thread, &exec_thread_return_value);

		// compression finished (or killed... whatever)
		// free allocated memory
		while ( --arg_c >= 0)
			free( arg_v[arg_c]);

		free( arg_v);
		
		// update file's mime type
		// it doesn't matter if compression finished successfully (file is there)
		// even if not - nothing happens :)
		path.Append( filename);
		update_mime_info( path.Path(), 0, 0, 0);

		// let ACompressView know compression has been finished/killed/etc...
		BMessenger( View).SendMessage( ARCHIVER_MSG_COMPRESS_END);
		
		return( 0);
	}
}
