#include <qsize.h>
#include "DmsGuiParameters.h"

// windows
QSize const dms_params::file_changed_window_size = QSize(600, 200);
QSize const dms_params::error_window_size = QSize(800, 400);

// buttons
QSize const dms_params::default_push_button_maximum_size = QSize(75, 30);
QSize const dms_params::treeitem_visit_history_fixed_size = QSize(18, 18);

// toolbar
QSize const dms_params::toolbar_button_spacing = QSize(30, 0);

// icons
const char* dms_params::default_view_icon = ":/res/images/TV_default_view.bmp";
const char* dms_params::table_view_icon = ":/res/images/TV_table.bmp";
const char* dms_params::map_view_icon = ":/res/images/TV_globe.bmp";
const char* dms_params::statistics_view_icon = ":/res/images/DP_statistics.bmp";

// font
const char* dms_params::dms_font_resource = ":/res/fonts/dmstext.ttf";
const char* dms_params::remix_icon_font_resource = ":/res/fonts/remixicon.ttf";
const int dms_params::default_font_size = 10;

// registry keys
const char* dms_params::reg_key_LastConfigFile = "LastConfigFile";
const char* dms_params::reg_key_RecentFiles = "RecentFiles";

// stylesheets
const char* dms_params::stylesheet_main_window = "QMainWindow::separator{ width: 5px; height: 5px; }";
const char* dms_params::stylesheet_treeitem_visit_history = "QComboBox QAbstractItemView {\n"
"min-width:400px;"
"}\n"
"QComboBox::drop-down:button{\n"
"background-color: transparant;\n"
"}\n"
"QComboBox::down-arrow {\n"
"image: url(:/res/images/arrow_down.png);\n"
"}\n";

const char* dms_params::stylesheet_toolbar =	"QToolBar { background: rgb(117, 117, 138);\n padding : 0px; }\n"
												"QToolButton {padding: 0px;}\n"
												"QToolButton:checked {background-color: rgba(255, 255, 255, 150);}\n"
												"QToolButton:checked {selection-color: rgba(255, 255, 255, 150);}\n"
												"QToolButton:checked {selection-background-color: rgba(255, 255, 255, 150);}\n";