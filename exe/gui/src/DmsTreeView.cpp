#include <QTreeView>

#include <QObject>
#include <QDockWidget>
#include <QMenubar>
#include <QFile>

#include "DmsTreeView.h"
#include "DmsMainWindow.h"
#include "TreeItem.h"
#include <QMainWindow>

namespace {
	auto GetTreeItem(const QModelIndex& mi) -> const TreeItem*
	{
		return reinterpret_cast<const TreeItem*>(mi.constInternalPointer());
	}

	int GetRow(const TreeItem* ti)
	{
		assert(ti);
		auto p = ti->GetTreeParent();
		if (!p)
			return 0;
		auto si = p->GetFirstSubItem();
		int row = 0;
		while (si != ti)
		{
			assert(si);
			si = si->GetNextItem();
			++row;
		}
		return row;

	}
}

QModelIndex DmsModel::index(int row, int column, const QModelIndex& parent) const
{
	auto ti = GetTreeItem(parent);
	if (!ti)
		return {};

	ti = ti->_GetFirstSubItem();
	assert(ti);
	while (row--)
	{
		ti = ti->GetNextItem();
		assert(ti);
	}
	return createIndex(row, column, ti);
}
QModelIndex DmsModel::parent(const QModelIndex& child) const
{
	auto ti = reinterpret_cast<const TreeItem*>(child.constInternalPointer());
	assert(ti);
	ti = ti->GetTreeParent();
	if (!ti)
		return{};

	return createIndex(GetRow(ti), 0, ti);
}
int DmsModel::rowCount(const QModelIndex& parent) const
{
	auto ti = GetTreeItem(parent);
	if (!ti)
		return 0;

	ti = ti->_GetFirstSubItem();
	int row = 0;
	while (ti)
	{
		ti = ti->GetNextItem();
		++row;
	}
	return row;
}

int DmsModel::columnCount(const QModelIndex& parent) const
{
	return 1;
}

QVariant DmsModel::data(const QModelIndex& index, int role) const
{
	auto ti = GetTreeItem(index);
	auto name = QString(ti->GetName().c_str());
	return name;
}

bool DmsModel::hasChildren(const QModelIndex& parent) const
{
	auto ti = GetTreeItem(parent);
	return ti && ti->HasSubItems();
}


//const int DMS_QT_TREE_NODE_CATEGORY = 1000;

auto createTreeview(MainWindow* dms_main_window) -> QPointer<QTreeView>
{
    auto dock = new QDockWidget(QObject::tr("TreeView"), dms_main_window);
    QPointer<QTreeView> dms_eventlog_widget_pointer = new QTreeView(dock);
    dock->setWidget(dms_eventlog_widget_pointer);
    dock->setTitleBarWidget(new QWidget(dock));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dms_main_window->addDockWidget(Qt::LeftDockWidgetArea, dock);

    //viewMenu->addAction(dock->toggleViewAction());
    return dms_eventlog_widget_pointer;
}
