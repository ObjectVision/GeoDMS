#include <QPointer>
QT_BEGIN_NAMESPACE
class QListWidget;
QT_END_NAMESPACE
class MainWindow;

auto createEventLog(MainWindow* dms_main_window) -> QPointer<QListWidget>;