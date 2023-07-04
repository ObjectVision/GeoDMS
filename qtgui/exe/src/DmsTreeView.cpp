#include <QTreeView>

#include <QObject>
#include <QDockWidget>
#include <QMenubar>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QBoxLayout>
#include <QShortcut>
#include <QHeaderView>
#include <variant>

#include "DmsMainWindow.h"
#include "DmsExport.h"
#include "DmsTreeView.h"
#include "SessionData.h"
#include "TreeItem.h"
#include <QMainWindow>
#include <QApplication>

#include "dbg/SeverityType.h"
#include "ShvDllInterface.h"
#include "TicInterface.h"
#include "StateChangeNotification.h"
#include "dataview.h"
#include "waiter.h"

namespace {
	auto GetTreeItem(const QModelIndex& mi) -> TreeItem* //std::variant<TreeItem*, InvisibleRootTreeItem*>
	{
		return reinterpret_cast<TreeItem*>(mi.internalPointer());
	}

	int GetRow(const TreeItem* ti)
	{
		assert(ti);
		auto p = ti->GetTreeParent();
		if (!p)
			return 0;
		//if (p->m_State.GetProgress() < PS_MetaInfo)
		//	MainWindow::TheOne()->m_treeview->waiter.start();

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

const TreeItem* DmsModel::GetTreeItemOrRoot(const QModelIndex& index) const
{
	auto ti = GetTreeItem(index);
	if (!ti)
		return m_root;
	return ti;
}

void DmsModel::reset()
{
	beginResetModel();
	endResetModel();
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
	assert(ti);

	static auto salmon = QColor(255, 128, 114);
	static auto cool_blue = QColor(82, 136, 219);
	static auto cool_green = QColor(0, 153, 51);

	if (IsDataCurrReady(ti->GetCurrRangeItem()))
		return cool_blue;
	if (ti->m_State.GetProgress() >= PS_Validated)
		return cool_green;

	return salmon;
}

QVariant DmsModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	auto ti = GetTreeItemOrRoot(index);

	if (!ti)
		return QVariant();
	ti->UpdateMetaInfo();

	switch (role)
	{
	case Qt::DecorationRole: 
		return getTreeItemIcon(index);

	case Qt::EditRole: 
		return QString(ti->GetName().c_str());

	case Qt::DisplayRole: 
		return  QString(ti->GetName().c_str());

	case Qt::ForegroundRole:
		return getTreeItemColor(index);

	case Qt::BackgroundRole:
		if (ti->WasFailed())
			return QColor(192, 16, 16);
		switch (TreeItem_GetSupplierLevel(ti))
		{
		case supplier_level::calc: return QColor(158, 201, 226); // clSkyBlue;
		case supplier_level::meta: return QColor(192, 220, 192); // $C0DCC0 clMoneyGreen;
		case supplier_level::calc_source: return QColor(000, 000, 255); // clBlue;
		case supplier_level::meta_source: return QColor(000, 255, 000); // clGreen;
		}
		break; // default background color

	case Qt::SizeHintRole:
	{
		auto font = QApplication::font();
		auto font_metrics = QFontMetrics(font);
		int pixels_wide = font_metrics.horizontalAdvance(ti->GetName().c_str());
		int pixels_high = font_metrics.height();
		return QSize(pixels_wide, pixels_high);
	}

	case Qt::FontRole: 
		return QApplication::font();

	}
	return QVariant();
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
	: QTreeView(parent)//, waiter(false)
{
	setRootIsDecorated(true);
	setUniformRowHeights(true);
	setItemsExpandable(true);
	setDragEnabled(true);
	setDragDropMode(QAbstractItemView::DragOnly);
	setContextMenuPolicy(Qt::CustomContextMenu);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_ForceUpdatesDisabled);
	header()->hide();
	connect(this, &DmsTreeView::doubleClicked, this, &DmsTreeView::onDoubleClick);
}

void DmsTreeView::showTreeviewContextMenu(const QPoint& pos)
{
	QModelIndex index = indexAt(pos);
	if (!index.isValid())
		return;

	if (!m_context_menu)
		m_context_menu = new QMenu(MainWindow::TheOne()); // TODO: does this get properly destroyed if parent gets destroyed?

	m_context_menu->clear();

	auto ti = GetTreeItem(index);
	MainWindow::TheOne()->setCurrentTreeItem(ti); // we assume Popupmenu act on current item, so accomodate now.

	auto viewstyle_flags = SHV_GetViewStyleFlags(ti);

	// export primary data
	auto export_primary_data_action = MainWindow::TheOne()->m_export_primary_data_action.get();
	auto item_can_be_exported = currentItemCanBeExportedToVector(ti) || currentItemCanBeExportedToRaster(ti);
	export_primary_data_action->setEnabled(item_can_be_exported);
	m_context_menu->addAction(export_primary_data_action);

	// step to failreason
	auto step_to_failreason = MainWindow::TheOne()->m_step_to_failreason_action.get();
	step_to_failreason->setEnabled(ti && ti->WasFailed());
	m_context_menu->addAction(step_to_failreason);

	// go to causa prima
	auto go_to_causa_prima = MainWindow::TheOne()->m_go_to_causa_prima_action.get();
	go_to_causa_prima->setEnabled(ti && ti->WasFailed());
	m_context_menu->addAction(go_to_causa_prima);

	// edit config source
	auto edit_config_source = MainWindow::TheOne()->m_edit_config_source_action.get();
	m_context_menu->addAction(edit_config_source);

	// update treeitem
	auto update_treeitem = MainWindow::TheOne()->m_update_treeitem_action.get();
	m_context_menu->addAction(update_treeitem);

	// update subtree
	auto update_subtree = MainWindow::TheOne()->m_update_subtree_action.get();
	m_context_menu->addAction(update_subtree);

	// invalidate 
	auto invalidate = MainWindow::TheOne()->m_invalidate_action.get();
	m_context_menu->addAction(invalidate);

	// default view
	auto default_view_action = MainWindow::TheOne()->m_defaultview_action.get();
	m_context_menu->addAction(default_view_action);

	// table view
	auto table_view_action = MainWindow::TheOne()->m_tableview_action.get();
	m_context_menu->addAction(table_view_action);

	// map view
	auto map_view_action = MainWindow::TheOne()->m_mapview_action.get();
	m_context_menu->addAction(map_view_action);

	// statistics view
	auto statistics_view_action = MainWindow::TheOne()->m_statistics_action.get();
	m_context_menu->addAction(statistics_view_action);
//	m_context_menu->exec(viewport()->mapToGlobal(pos));

	// histogram view
//	auto histogramview = MainWindow::TheOne()->m_histogramview_action.get();
//	histogramview->setDisabled(true);
//	m_context_menu->addAction(histogramview);

	// process scheme
	auto process_scheme = MainWindow::TheOne()->m_process_schemes_action.get();
	m_context_menu->addAction(process_scheme);
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

void DmsTreeView::onDoubleClick(const QModelIndex& index)
{
	if (!index.isValid())
		return;

	auto ti = GetTreeItem(index);
	assert(ti);

	MainWindow::TheOne()->defaultView();
}

auto createTreeview(MainWindow* dms_main_window) -> QPointer<DmsTreeView>
{
    auto dock = new QDockWidget(QObject::tr("TreeView"), dms_main_window);
	dock->setTitleBarWidget(new QWidget(dock));
	QPointer<DmsTreeView> dms_eventlog_widget_pointer = new DmsTreeView(dock);
    dock->setWidget(dms_eventlog_widget_pointer);
	dms_main_window->addDockWidget(Qt::LeftDockWidgetArea, dock);
//	dock->setFeatures(QDockWidget::NoDockWidgetFeatures);

	//auto box_layout = new QBoxLayout(QBoxLayout::Direction::TopToBottom);


    //viewMenu->addAction(dock->toggleViewAction());
    return dms_eventlog_widget_pointer;
}
