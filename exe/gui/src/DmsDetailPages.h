#include <QPointer>
QT_BEGIN_NAMESPACE
class QTreeView;
class QTextBrowser;
QT_END_NAMESPACE
class MainWindow;

auto createDetailPages(MainWindow* dms_main_window) -> QPointer<QTextBrowser>;