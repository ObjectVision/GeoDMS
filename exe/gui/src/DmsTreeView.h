#include <QPointer>
QT_BEGIN_NAMESPACE
class QTreeView;
QT_END_NAMESPACE
class MainWindow;

auto createTreeview(MainWindow* dms_main_window) -> QPointer<QTreeView>;