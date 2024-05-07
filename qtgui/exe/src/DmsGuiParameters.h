#include <qsize.h>

#ifndef DMSGUIPARAMETERS_H
#define DMSGUIPARAMETERS_H

namespace dms_params {
	// windows
	extern QSize const file_changed_window_size;
	extern QSize const error_window_size;
	
	// buttons
	extern QSize const default_push_button_maximum_size;
	extern QSize const treeitem_visit_history_fixed_size;
	
	// toolbar
	extern QSize const toolbar_button_spacing;

	// icons
	extern const char* default_view_icon;
	extern const char* table_view_icon;
	extern const char* map_view_icon;
	extern const char* statistics_view_icon;

	// font
	extern const char* dms_font_resource;
	extern const char* remix_icon_font_resource;
	extern const int default_font_size;

	// registry keys
	extern const char* reg_key_LastConfigFile;
	extern const char* reg_key_RecentFiles;

	// stylesheets
	extern const char* stylesheet_main_window;
	extern const char* stylesheet_treeitem_visit_history;
}

#endif // DMSGUIPARAMETERS_H