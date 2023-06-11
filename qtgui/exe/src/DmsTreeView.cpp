#include <QTreeView>

#include <QObject>
#include <QDockWidget>
#include <QMenubar>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QBoxLayout>
#include <QShortcut>

#include "DmsMainWindow.h"
#include "DmsTreeView.h"
#include "TreeItem.h"
#include <QMainWindow>
#include <QApplication>

#include "dbg/SeverityType.h"
#include "ShvDllInterface.h"
#include "TicInterface.h"
#include "StateChangeNotification.h"
#include "dataview.h"

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

		auto si = p->GetFirstSubItem(); // update metainfo
		int row = 1;
		while (si != ti)
		{
			assert(si);
			si = si->GetNextItem();
			++row;
		}
		return row;

	}
}

TreeModelCompleter::TreeModelCompleter(QObject* parent)
	: QCompleter(parent)
{
}

TreeModelCompleter::TreeModelCompleter(QAbstractItemModel* model, QObject* parent)
	: QCompleter(model, parent)
{
}

void TreeModelCompleter::setSeparator(const QString& separator)
{
	sep = separator;
}

QString TreeModelCompleter::separator() const
{
	return sep;
}

QStringList TreeModelCompleter::splitPath(const QString& path) const
{
	QStringList split_path = (sep.isNull() ? QCompleter::splitPath(path) : path.split(sep));
	split_path.remove(0);
	return split_path;
}

QString TreeModelCompleter::pathFromIndex(const QModelIndex& index) const
{
	if (sep.isNull())
		return QCompleter::pathFromIndex(index);

	QStringList data_list;
	for (QModelIndex i = index; i.isValid(); i = i.parent())
		data_list.prepend(model()->data(i, completionRole()).toString());
	data_list.remove(0);
	if (!data_list.isEmpty())
		data_list[0] = "/" + data_list[0];
	auto rval = data_list.join(sep);

	return rval;
}

DmsModel::~DmsModel()
{
	MainWindow::EventLog(SeverityTypeID::ST_MajorTrace, "bye bye DmsModel");
}

const TreeItem* DmsModel::GetTreeItemOrRoot(const QModelIndex& index) const
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
	//if (ti == m_root)
	//	return createIndex(row, column, ti);

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

QVariant DmsModel::getTreeItemIcon(const QModelIndex& index) const
{
	auto ti = GetTreeItemOrRoot(index);
	if (!ti)
		return QVariant();

	auto vsflags = SHV_GetViewStyleFlags(ti);
	if (vsflags & ViewStyleFlags::vsfMapView) { return QVariant::fromValue(QPixmap(":/res/images/TV_globe.bmp"));}
	else if (vsflags & ViewStyleFlags::vsfTableContainer) { return QVariant::fromValue(QPixmap(":/res/images/TV_container_table.bmp")); }
	else if (vsflags & ViewStyleFlags::vsfTableView) { return QVariant::fromValue(QPixmap(":/res/images/TV_table.bmp")); }
	else if (vsflags & ViewStyleFlags::vsfPaletteEdit) { return QVariant::fromValue(QPixmap(":/res/images/TV_palette.bmp")); }
	else if (vsflags & ViewStyleFlags::vsfContainer) { return QVariant::fromValue(QPixmap(":/res/images/TV_container.bmp")); }
	else { return QVariant::fromValue(QPixmap(":/res/images/TV_unit_transparant.bmp")); } 
}

QVariant DmsModel::getTreeItemColor(const QModelIndex& index) const
{
	auto ti = GetTreeItemOrRoot(index);
	if (!ti)
		return QVariant();

	if (ti->IsFailed())
		return QColor(0, 0, 0, 255);

	auto salmon = QColor(255, 0, 102, 255);
	auto cool_blue = QColor(82, 136, 219, 255);
	auto cool_green = QColor(0, 153, 51, 255);
	auto ti_state = static_cast<NotificationCode>(DMS_TreeItem_GetProgressState(ti));
	switch (ti_state)
	{
	case NotificationCode::NC2_FailedFlag: return QColor(0, 0, 0, 255);
	case NotificationCode::NC2_DataReady: return cool_blue;
	case NotificationCode::NC2_Validated: return cool_green;
	case NotificationCode::NC2_Committed: return cool_green;
	case NotificationCode::NC2_Invalidated:
	case NotificationCode::NC2_MetaReady: return salmon;
	default: return salmon;
	}

	return {};
}

QVariant DmsModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	auto ti = GetTreeItemOrRoot(index);

	switch (role)
	{
	case Qt::DecorationRole: return getTreeItemIcon(index);
	case Qt::EditRole: return QString(ti->GetName().c_str());
	case Qt::DisplayRole: return  QString(ti->GetName().c_str());
	case Qt::ForegroundRole:
	{
		return QColor(Qt::red);
	}
	case Qt::SizeHintRole:
	{
		auto font = QApplication::font();
		auto font_metrics = QFontMetrics(font);
		int pixels_wide = font_metrics.horizontalAdvance(ti->GetName().c_str());
		int pixels_high = font_metrics.height();
		return QSize(pixels_wide, pixels_high);
	}
	case Qt::FontRole: {return QApplication::font(); };
	default: return QVariant();
	}
}

bool DmsModel::hasChildren(const QModelIndex& parent) const
{
	auto ti = GetTreeItemOrRoot(parent);
	return ti && ti->HasSubItems();
}

auto DmsModel::flags(const QModelIndex& index) const -> Qt::ItemFlags
{
	if (!index.isValid())
		return Qt::NoItemFlags;

	return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled |  QAbstractItemModel::flags(index);
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
	if (current == previous)
		return;

	auto ti = GetTreeItem(current);
	auto* main_window = MainWindow::TheOne();
	main_window->setCurrentTreeItem(ti);
}

bool isAncestor(TreeItem* ancestorTarget, TreeItem* descendant)
{
	if (!descendant)
		return false;
	auto ancestorCandidate = descendant->GetParent();
	while (ancestorCandidate)
	{
		if (ancestorCandidate == ancestorTarget)
			return true;
		ancestorCandidate = ancestorCandidate->GetParent();
	}
	return false;
}

auto DmsTreeView::expandToCurrentItem(TreeItem* new_current_item) -> QModelIndex
{
	auto root_node_index = rootIndex();
	if (new_current_item == MainWindow::TheOne()->getRootTreeItem())
		return {};

	auto parent_index = root_node_index;
	while (true)
	{
		auto child_count = model()->rowCount(parent_index);
		for (int i = 0; i < child_count; i++)
		{
			auto child_index = model()->index(i, 0, parent_index);
			auto ti = GetTreeItem(child_index);
			if (ti == new_current_item)
			{
				setCurrentIndex(child_index);
				return child_index;
			}

			if (isAncestor(ti, new_current_item))
			{
				if (!isExpanded(child_index))
					expand(child_index);
				parent_index = child_index;
			}
		}
	}
}

DmsTreeView::DmsTreeView(QWidget* parent)
	: QTreeView(parent)
{}

void DmsTreeView::showTreeviewContextMenu(const QPoint& pos)
{
	QModelIndex index = indexAt(pos);
	if (!index.isValid())
		return;

	if (!m_context_menu)
		m_context_menu = new QMenu(MainWindow::TheOne()); // TODO: does this get properly destroyed if parent gets destroyed?

	m_context_menu->clear();

	auto ti = GetTreeItem(index);
	auto viewstyle_flags = SHV_GetViewStyleFlags(ti);

	// default view
	auto default_view_action = MainWindow::TheOne()->getDefaultviewAction();
	default_view_action->setDisabled((viewstyle_flags & (ViewStyleFlags::vsfDefault|ViewStyleFlags::vsfTableView|ViewStyleFlags::vsfTableContainer| ViewStyleFlags::vsfMapView)) ? false : true); // TODO: vsfDefault appears to never be set
	m_context_menu->addAction(default_view_action);

	// table view
	auto table_view_action = MainWindow::TheOne()->getTableviewAction();
	table_view_action->setDisabled((viewstyle_flags & (ViewStyleFlags::vsfTableView|ViewStyleFlags::vsfTableContainer)) ? false : true);
	m_context_menu->addAction(table_view_action);

	// map view
	auto map_view_action = MainWindow::TheOne()->getMapviewAction();
	map_view_action->setDisabled((viewstyle_flags & ViewStyleFlags::vsfMapView) ? false : true);
	m_context_menu->addAction(map_view_action);
	m_context_menu->exec(viewport()->mapToGlobal(pos));
}

void DmsTreeView::setNewCurrentItem(TreeItem* new_current_item)
{
	auto current_node_index = currentIndex();
	auto root_node_index = rootIndex();
	auto root_ti = GetTreeItem(root_node_index);
	if (root_ti == new_current_item)
		return;

	if (current_node_index.isValid())
	{
		auto ti = GetTreeItem(current_node_index);
		if (new_current_item == ti) // treeview already has current item
			return;
	}

	expandToCurrentItem(new_current_item);
}

auto createTreeview(MainWindow* dms_main_window) -> QPointer<DmsTreeView>
{
    auto dock = new QDockWidget(QObject::tr("TreeView"), dms_main_window);
    QPointer<DmsTreeView> dms_eventlog_widget_pointer = new DmsTreeView(dock);
    dock->setWidget(dms_eventlog_widget_pointer);
    dock->setTitleBarWidget(new QWidget(dock));
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);

	//auto box_layout = new QBoxLayout(QBoxLayout::Direction::TopToBottom);

    dms_main_window->addDockWidget(Qt::LeftDockWidgetArea, dock);

    //viewMenu->addAction(dock->toggleViewAction());
    return dms_eventlog_widget_pointer;
}
