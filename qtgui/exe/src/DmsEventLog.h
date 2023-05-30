#include "RtcBase.h"

#include <QPointer>
QT_BEGIN_NAMESPACE
class QListWidget;
QT_END_NAMESPACE
class MainWindow;


void geoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg);
auto createEventLog(MainWindow* dms_main_window) -> QPointer<QListWidget>;