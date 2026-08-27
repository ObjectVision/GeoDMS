#include <QSize>

#ifndef DMSGUIPARAMETERS_H
#define DMSGUIPARAMETERS_H

namespace dms_params {
	// windows
	extern QSize const file_changed_window_size;
	extern QSize const error_window_size;
	
	// buttons
	// The FLOOR for a dialog button, applied through SetDialogButtonSize below.
	extern QSize const default_push_button_minimum_size;
	extern QSize const treeitem_visit_history_fixed_size;

	// Size a dialog button to its own content, but never below default_push_button_minimum_size,
	// so a row of short buttons still looks even (issue #1192).
	//
	// These buttons used to be capped with setMaximumSize(75, 30), which clipped '&Terminate' in
	// the error box: the T and the trailing e were cut off by the frame, and a wider font would
	// have done the same to the others. A plain setMinimumSize is not the answer either -- the
	// layout gives each button a cell of about a third of the dialog, and a button that may grow
	// fills it, so all three would have become ~215px wide. Fixing the size keeps the small,
	// centred buttons these dialogs have and lets the text decide how small.
	void SetDialogButtonSize(class QPushButton* button);
	
	// toolbar
	extern QSize const toolbar_button_spacing;

	// icons
	// The em size the GUI renders its glyph icons at, matching the 16x16 bitmaps they replaced
	// (issues #319, #1220). The glyphs are drawn into a pixmap of this size times the device
	// pixel ratio, so raising this is all it takes to get bigger icons.
	extern const int treeitem_icon_size;

	// coordinates bar
	extern int coordinates_bar_width;

	// font
	extern const char* dms_font_resource;
	extern const char* remix_icon_font_resource;
	extern const int default_font_size;

	// registry keys
	extern const char* reg_key_LastConfigFile;
	extern const char* reg_key_RecentFiles;
	extern const char* reg_key_PinnedFiles;
	extern const char* reg_key_ReopenLastConfigAtStartup;

	// stylesheets
	extern const char* stylesheet_main_window;
	extern const char* stylesheet_treeitem_visit_history;
	extern const char* stylesheet_toolbar;
}

#endif // DMSGUIPARAMETERS_H