#include <QTreeView>

#include <QObject>
#include <QDockWidget>
#include <QMenubar>
#include <QFile>
#include <QPainter>
#include <QPixmap>

#include "DmsMainWindow.h"
#include "DmsTreeView.h"
#include "TreeItem.h"
#include <QMainWindow>

namespace {
	auto GetTreeItem(const QModelIndex& mi) -> TreeItem*
	{
		return reinterpret_cast<TreeItem*>(mi.internalPointer());
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

TreeItem* DmsModel::GetTreeItemOrRoot(const QModelIndex& index) const
{
	auto ti = GetTreeItem(index);
	if (!ti)
		return m_root;
	return ti;
}

QVariant DmsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return data({}, role);

	return QVariant();
}

QModelIndex DmsModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	auto ti = GetTreeItemOrRoot(parent);
	assert(ti);

	ti = ti->_GetFirstSubItem();
	assert(ti);

	int items_to_be_stepped = row;
	while (items_to_be_stepped--)
	{
		ti = ti->GetNextItem();
		assert(ti);
	}
	return createIndex(row, column, ti);
}

QModelIndex DmsModel::parent(const QModelIndex& child) const
{
	if (!child.isValid())
		return QModelIndex();

	auto ti = GetTreeItem(child);
	assert(ti);
	auto parent = ti->GetTreeParent();
	if (!parent)
		return{};

	return createIndex(GetRow(parent), 0, parent);
}

int DmsModel::rowCount(const QModelIndex& parent) const
{
	auto ti = GetTreeItemOrRoot(parent);

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
	if (!index.isValid())
		return QVariant();

	if (role == Qt::DecorationRole)
	{
		return QVariant::fromValue(QPixmap(":/res/images/TV_globe.bmp"));
		//return QVariant();
		//Qt::Variant.fromValue(Qt::Pixmap.new(':/path/to/resource'))
	}

	if (role != Qt::DisplayRole)
		return QVariant();

	auto ti = GetTreeItemOrRoot(index);
	auto name = QString(ti->GetName().c_str());
	return name;
}

bool DmsModel::hasChildren(const QModelIndex& parent) const
{
	auto ti = GetTreeItemOrRoot(parent);
	return ti && ti->HasSubItems();
}

void TreeItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
	QStyledItemDelegate::paint(painter, option, index);
	return;
	//QStyleOptionViewItem opt = option;
	//initStyleOption(&opt, index);

	

	painter->save();
	auto ti = GetTreeItem(index);
	painter->setRenderHint(QPainter::Antialiasing, true);
	if (option.state & QStyle::State_Selected)
		painter->fillRect(option.rect, option.palette.highlightedText());

	//if (option.state & QStyle::State_MouseOver)
	//	painter->fillRect(option.rect, option.palette.highlightedText());

	//painter->translate(option.rect.x(), option.rect.y());
	
	painter->drawText(option.rect.topLeft().x(), option.rect.topLeft().y(), option.rect.topRight().x()-option.rect.topLeft().x(), option.rect.topLeft().y()-option.rect.bottomLeft().y(), Qt::AlignCenter, ti->GetName().c_str());
	painter->restore();
}

void DmsTreeView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
	auto ti = GetTreeItem(current);
	auto* main_window = static_cast<MainWindow*>(parent()->parent());
	main_window->setCurrentTreeItem(const_cast<TreeItem*>(ti));
	int i = 0;
}

auto createTreeview(MainWindow* dms_main_window) -> QPointer<DmsTreeView>
{
    auto dock = new QDockWidget(QObject::tr("TreeView"), dms_main_window);
    QPointer<DmsTreeView> dms_eventlog_widget_pointer = new DmsTreeView(dock);
    dock->setWidget(dms_eventlog_widget_pointer);
    dock->setTitleBarWidget(new QWidget(dock));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dms_main_window->addDockWidget(Qt::LeftDockWidgetArea, dock);

    //viewMenu->addAction(dock->toggleViewAction());
    return dms_eventlog_widget_pointer;
}
