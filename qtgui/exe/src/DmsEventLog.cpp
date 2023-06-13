#include <QListWidget>

#include <QObject>
#include <QDockWidget>
#include <QMenubar>

#include "DmsEventLog.h"
#include "DmsMainWindow.h"
#include <QMainWindow>
#include "dbg/SeverityType.h"

void geoDMSMessage(ClientHandle /*clientHandle*/, SeverityTypeID st, CharPtr msg)
{
//    auto eventlog_widget_pointer = reinterpret_cast<QListWidget*>(clientHandle); 
//    assert(eventlog_widget_pointer);                                             
    MainWindow::EventLog(st, msg);
}

auto createEventLog(MainWindow* dms_main_window) -> QPointer<QListWidget>
{
    auto dock = new QDockWidget(QObject::tr("EventLog"), dms_main_window);
    QPointer<QListWidget> dms_eventlog_widget_pointer = new QListWidget(dock);
    dock->setWidget(dms_eventlog_widget_pointer);
    dock->setTitleBarWidget(new QWidget(dock));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dms_main_window->addDockWidget(Qt::BottomDockWidgetArea, dock);

    dms_eventlog_widget_pointer->setUniformItemSizes(true);

    //viewMenu->addAction(dock->toggleViewAction());
	return dms_eventlog_widget_pointer;
}