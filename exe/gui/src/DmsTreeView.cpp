#include <QListWidget>

#include <QObject>
#include <QDockWidget>
#include <QMenubar>
#include <QFile>
#include <QTreeView>

#include "DmsTreeView.h"
#include "DmsMainWindow.h"
#include <QMainWindow>
#include "treemodel.h"

auto createTreeview(MainWindow* dms_main_window) -> QPointer<QTreeView>
{
    QDockWidget* dock = new QDockWidget(QObject::tr("TreeView"), dms_main_window);
    QFile file(":res/images/default.txt");

    file.open(QIODevice::ReadOnly);
    auto m_tree_model = new TreeModel(file.readAll());
    file.close();
    
    QPointer<QTreeView> dms_eventlog_widget_pointer = new QTreeView(dock);
    dms_eventlog_widget_pointer->setModel(m_tree_model);
    dms_eventlog_widget_pointer->setHeaderHidden(true);
    dms_eventlog_widget_pointer->setAnimated(false);
    dms_eventlog_widget_pointer->setUniformRowHeights(true);

    dock->setTitleBarWidget(new QWidget(dock));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dock->setWidget(dms_eventlog_widget_pointer);
    //dock->setWidget(m_tree_view);
    dms_main_window->addDockWidget(Qt::LeftDockWidgetArea, dock);

    return dms_eventlog_widget_pointer;
    //viewMenu->addAction(dock->toggleViewAction());
}