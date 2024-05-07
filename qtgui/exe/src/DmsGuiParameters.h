#ifndef DMSGUIPARAMETERS_H
#define DMSGUIPARAMETERS_H

#include <qsize.h>

namespace dms_params {
	// windows
	auto const file_changed_window_size = QSize(600, 200);
	auto const error_window_size = QSize(800, 400);
	
	// buttons
	auto const default_push_button_maximum_size = QSize(75, 30);
	auto const treeitem_visit_history_fixed_size = QSize(18, 18);
	
	// toolbar
	auto const toolbar_button_spacing = QSize(30, 0);

	// icons
	auto const* default_view_icon = ":/res/images/TV_default_view.bmp";
	auto const* table_view_icon = ":/res/images/TV_table.bmp";
	auto const* map_view_icon = ":/res/images/TV_globe.bmp";
	auto const* statistics_view_icon = ":/res/images/DP_statistics.bmp";

	// font
	const char* dms_font_resource = ":/res/fonts/dmstext.ttf";
	const char* remix_icon_font_resource = ":/res/fonts/remixicon.ttf";
	const int default_font_size = 10;

	// registry keys
	const char* reg_key_LastConfigFile = "LastConfigFile";
	const char* reg_key_RecentFiles = "RecentFiles";

	// stylesheets
	const char* stylesheet_main_window = "QMainWindow::separator{ width: 5px; height: 5px; }";
	const char* stylesheet_treeitem_visit_history = "QComboBox QAbstractItemView {\n"
														"min-width:400px;"
													"}\n"
													"QComboBox::drop-down:button{\n"
														"background-color: transparant;\n"
													"}\n"
													"QComboBox::down-arrow {\n"
														"image: url(:/res/images/arrow_down.png);\n"
													"}\n"

}












#endif // DMSGUIPARAMETERS_H