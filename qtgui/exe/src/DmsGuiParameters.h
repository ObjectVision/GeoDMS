#ifndef DMSGUIPARAMETERS_H
#define DMSGUIPARAMETERS_H

#include <qsize.h>

namespace dms_params {
	// windows
	auto const file_changed_window_size = QSize(600, 200);
	auto const error_window_size = QSize(800, 400);
	
	// buttons
	auto const default_push_button_maximum_size = QSize(75, 30);

	// font
	const char* dms_font_resource = ":/res/fonts/dmstext.ttf";
	const char* remix_icon_font_resource = ":/res/fonts/remixicon.ttf";
	const int default_font_size = 10;

}












#endif // DMSGUIPARAMETERS_H