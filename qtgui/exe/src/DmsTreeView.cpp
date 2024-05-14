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
#include <QScrollbar>

#include <variant>

#include "DmsMainWindow.h"
#include "DmsOptions.h"
#include "DmsExport.h"
#include "DmsOptions.h"
#include "DmsTreeView.h"
#include "SessionData.h"
#include "TreeItem.h"
#include <QMainWindow>
#include <QApplication>

#include "utl/scoped_exit.h"

#include "dbg/Check.h"
#include "dbg/DmsCatch.h"
#include "dbg/SeverityType.h"
#include "StateChangeNotification.h"

#include "TicInterface.h"

#include "ShvDllInterface.h"
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

		bool isWaiting = Waiter::IsWaiting();

		SuspendTrigger::Resume();
		ObjectMsgGenerator thisMsgGenerator(ti, "CountSubItems");
		Waiter showWaitingStatus;
		if (!isWaiting && !p->Was(PS_MetaInfo) && !p->WasFailed())
			showWaitingStatus.start(&thisMsgGenerator);

		auto si = isWaiting ? p->_GetFirstSubItem() : p->GetFirstSubItem(); // update metainfo
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

QVariant DmsModel::headerData(int /*section*/, Qt::Orientation orientation, int role) const
{
	//if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	//	return data({}, role);
	if (role == Qt::DisplayRole)
		return QString("test");

	return QVariant();
}

QModelIndex DmsModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	auto ti = GetTreeItemOrRoot(parent);
	assert(ti);

	int currRow = 0;
	for (ti = ti->_GetFirstSubItem(); ti; ti = ti->GetNextItem())
	{
		if (show_hidden_items || !ti->GetTSF(TSF_IsHidden))
		{
			if (currRow == row)
				return createIndex(row, column, ti);
			++currRow;
		}
	}

	reportF(MsgCategory::other, SeverityTypeID::ST_FatalError, "Invalid row at DmsModel::index");
	return QModelIndex();
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
	int number_of_rows = 0;
	try {
		auto ti = GetTreeItemOrRoot(parent);
		if (ti)
			for (auto si = ti->GetFirstSubItem(); si; si = si->GetNextItem())
			{
				if (show_hidden_items || !si->GetTSF(TSF_IsHidden))
					number_of_rows++;
			}
	}
	catch (...)
	{}

	return number_of_rows;
}

int DmsModel::columnCount(const QModelIndex& /*parent*/) const
{
	return 1;
}

bool DmsModel::updateChachedDisplayFlags()
{
	bool was_updated = false;
	auto dms_reg_status_flags = GetRegStatusFlags();

	bool reg_show_hidden_items = (dms_reg_status_flags & RSF_AdminMode);
	if (!show_hidden_items == reg_show_hidden_items)
	{
		was_updated = true;
		show_hidden_items = reg_show_hidden_items;
	}

	bool reg_show_state_colors = (dms_reg_status_flags & RSF_ShowStateColors);
	if (!show_state_colors == reg_show_state_colors)
	{
		was_updated = true;
		show_state_colors = reg_show_state_colors;
	}

	return was_updated;
}

QVariant DmsModel::getTreeItemIcon(const QModelIndex& index) const
{
	auto ti = GetTreeItemOrRoot(index);
	if (!ti)
		return QVariant();

	bool isTemplate = ti->IsTemplate();

	// TODO, CODE CLEAN-UP: All followwing code return a QVariant::fromValue(QPixmap(CharPtr)
	// so we can factor the postprocessing after resource determination out and/or use a map to store the pixmaps and return the right one.

	if (isTemplate)
		return QVariant::fromValue(QPixmap(":/res/images/TV_template.bmp")); 

	bool isInTemplate = ti->InTemplate();
	auto vsflags = SHV_GetViewStyleFlags(ti);

	if (vsflags & ViewStyleFlags::vsfMapView) 
		return isInTemplate 
		? QVariant::fromValue(QPixmap(":/res/images/TV_globe_bw.bmp")) 
		: QVariant::fromValue(QPixmap(":/res/images/TV_globe.bmp"));

	if (vsflags & ViewStyleFlags::vsfTableContainer)
		return isInTemplate 
		? QVariant::fromValue(QPixmap(":/res/images/TV_container_table_bw.bmp")) 
		: QVariant::fromValue(QPixmap(":/res/images/TV_container_table.bmp"));

	if (vsflags & ViewStyleFlags::vsfTableView)
		return isInTemplate 
		? QVariant::fromValue(QPixmap(":/res/images/TV_table_bw.bmp")) 
		: QVariant::fromValue(QPixmap(":/res/images/TV_table.bmp"));

	if (vsflags & ViewStyleFlags::vsfPaletteEdit)
		return isInTemplate 
		? QVariant::fromValue(QPixmap(":/res/images/TV_palette_bw.bmp")) 
		: QVariant::fromValue(QPixmap(":/res/images/TV_palette.bmp"));

	if (vsflags & ViewStyleFlags::vsfContainer) 
		return isInTemplate 
		? QVariant::fromValue(QPixmap(":/res/images/TV_container_bw.bmp")) 
		: QVariant::fromValue(QPixmap(":/res/images/TV_container.bmp"));

	bool isDataItem = IsDataItem(ti);
	if (isDataItem)
		return isInTemplate
		? QVariant::fromValue(QPixmap(":/res/images/TV_table_bw.bmp"))
		: QVariant::fromValue(QPixmap(":/res/images/TV_table.bmp"));

	return QVariant::fromValue(QPixmap(":/res/images/TV_unit_transparant.bmp"));
}

color_option getColorOption(const TreeItem* ti)
{
	assert(ti);
	bool isInTemplate = ti->InTemplate();

	if (isInTemplate)
		return color_option::tv_template;

	if (ti->Was(PS_MetaInfo))
	{
		if (IsDataCurrReady(ti->GetCurrRangeItem()))
			return color_option::tv_valid;
		if (ti->m_State.GetProgress() >= PS_Validated)
			return color_option::tv_exogenic;
	}

	return color_option::tv_not_calculated;
}

QVariant DmsModel::getTreeItemColor(const QModelIndex& index) const
{
	auto ti = GetTreeItemOrRoot(index);
	assert(ti);
	
	if (!show_state_colors)
		return QColor(0,0,0); // black

	if (ti->WasFailed())
		return QColor(255, 255, 255); // white

	auto co = getColorOption(ti);
	return GetUserQColor(co);
}

QVariant DmsModel::data(const QModelIndex& index, int role) const
{
	try {
		if (!index.isValid())
			return QVariant();

		auto ti = GetTreeItemOrRoot(index);

		if (!ti)
			return QVariant();

		SuspendTrigger::Resume();
		if (!ti->Was(PS_MetaInfo) && !ti->WasFailed(FR_MetaInfo))
		{
			ObjectMsgGenerator thisMsgGenerator(ti, "UpdateMetaInfo");
			Waiter showWaitingStatus(&thisMsgGenerator);

			try {
				ti->UpdateMetaInfo();
			}
			catch (...)
			{
				ti->CatchFail(FR_MetaInfo);
			}
		}
		switch (role)
		{
		case Qt::DecorationRole:
			return getTreeItemIcon(index);

		case Qt::EditRole:
		case Qt::DisplayRole:
			return QString(ti->GetName().c_str());

		case Qt::ForegroundRole:
			return getTreeItemColor(index);

		case Qt::BackgroundRole:
			if (ti->WasFailed() && !MainWindow::TheOne()->m_treeview->selectionModel()->selectedIndexes().empty()
				&& MainWindow::TheOne()->m_treeview->selectionModel()->selectedIndexes().at(0) == index)
			{
				return QColor(150, 0, 0);
			}

			if (ti->WasFailed())
				return QColor(255, 0, 0);

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
			int pixels_wide = font_metrics.horizontalAdvance(ti->GetName().c_str()) + 50;
			int pixels_high = font_metrics.height();
			return QSize(pixels_wide, pixels_high);
		}

		case Qt::FontRole:
			return QApplication::font();

		}
	}
	catch (...)
	{
		catchException(false);
	}
	return QVariant();
}

bool DmsModel::hasChildren(const QModelIndex& parent) const
{
	try {
		auto ti = GetTreeItemOrRoot(parent);
		if (!ti)
			return false;

		if (ti->Was(PS_MetaInfo))
			return ti->_GetFirstSubItem() != nullptr;

		ObjectMsgGenerator context(ti, "HasSubItems");
		Waiter monitor(&context);
		return ti->HasSubItems();
	}
	catch (...)
	{
		catchException(false);
	}
	return false;
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
	painter->save();

	auto painter_exit_guard = make_scoped_exit([painter] { painter->restore(); });

	// last job: draw storage icon if needed or return if not needed
	TreeItem* ti = nullptr;
	try
	{
		ti = GetTreeItem(index);
		if (!ti)
			return;

		const TreeItem* storageHolder = ti->GetStorageParent(true);
		if (!storageHolder)
			return;
		assert(!ti->IsDisabledStorage());

		bool is_read_only = storageHolder->GetStorageManager()->IsReadOnly();
		if (is_read_only && ti->HasCalculator())
			return;

		QFontMetrics fm(QApplication::font());
		int offset_item_text = fm.horizontalAdvance(index.data(Qt::DisplayRole).toString());

		auto item_icon = MainWindow::TheOne()->m_dms_model->getTreeItemIcon(index).value<QImage>();
		int offset_icon = item_icon.width();
		auto rect = option.rect;
		auto cur_brush = painter->brush();
		auto offset = rect.topLeft().x() + offset_icon + offset_item_text + 15;

		QFont font;
		font.setFamily("remixicon");
		painter->setFont(font);

		// set transparancy if not committed yet
		NotificationCode ti_state = static_cast<NotificationCode>(TreeItem_GetProgressState(ti));
		if (ti_state < NotificationCode::NC2_DataReady)
			painter->setOpacity(0.5);

	
		if (ti->IsDataFailed())
			painter->setPen(QColor(255,0,0,255));
		painter->drawText(QPoint(offset, rect.center().y() + 5), is_read_only ? "\uEC15":"\uF0B0");
	}
	catch (...)
	{
//		catchException(false);	// doesn't do anything and return values isn't used; reporting is not desired as this is a paint method
	}

	return;
}

void DmsTreeView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
	if (current == previous)
		return;

	auto ti = GetTreeItem(current);
	auto* main_window = MainWindow::TheOne();
	main_window->setCurrentTreeItem(ti);
}

bool isAncestor(const TreeItem* ancestorTarget, const TreeItem* descendant)
{
	for (auto ancestorCandidate = descendant; ancestorCandidate; ancestorCandidate = ancestorCandidate->GetTreeParent())
		if (ancestorCandidate == ancestorTarget)
			return true;

	return false;
}

auto DmsTreeView::expandToItem(TreeItem* target_item) -> QModelIndex
{
	auto root_node_index = rootIndex();
	auto currItem = MainWindow::TheOne()->getRootTreeItem();
	if (target_item == currItem)
		return {};
	if (target_item->GetRoot() != currItem)
		return {};

	MG_CHECK(isAncestor(currItem, target_item));

	ObjectMsgGenerator thisMsgGenerator(target_item, "TreeView::expandToItem");
	Waiter showWaitingStatus(&thisMsgGenerator);

	auto parent_index = root_node_index;

search_at_parent_index:
	auto child_count = model()->rowCount(parent_index);
	for (int i = 0; i < child_count; i++)
	{
		auto child_index = model()->index(i, 0, parent_index);
		auto childItem = GetTreeItem(child_index);
		if (childItem == target_item)
		{
			setCurrentIndex(child_index);
			return child_index;
		}

		if (isAncestor(childItem, target_item))
		{
			if (!isExpanded(child_index))
				expand(child_index);
			currItem = childItem;
			parent_index = child_index;
			goto search_at_parent_index; // break with current child_count and continue this quest with the new parent_index  
		}
	}
	// maybe descendant was hidden
	MG_CHECK(isAncestor(currItem, target_item));
	MG_CHECK(!MainWindow::TheOne()->m_dms_model->show_hidden_items);
	MG_CHECK(target_item->GetTSF(TSF_InHidden));
	reportF(MsgCategory::other, SeverityTypeID::ST_FatalError, "cannnot activate '%1%' in TreeView as it seems to be a hidden sub-item of '%2%'"
		"\nIllegal Target for DmsTreeView::expandToItem"
		, target_item->GetFullName().c_str()
		, currItem->GetFullName().c_str()
	);
	return parent_index;
}

bool DmsTreeView::expandActiveNode(bool doExpand)
{
	auto index = currentIndex();
	if (!index.isValid())
		return false;

	if (doExpand)
		expand(index);
	else
		collapse(index);
	return true;
}

bool DmsTreeView::expandRecursiveFromCurrentItem()
{
	auto index = currentIndex();
	if (!index.isValid())
		return false;

	expandRecursively(index);
	return true;
}

QSize DmsTreeView::sizeHint() const
{
	return QSize(m_default_size, 0);
}

QSize DmsTreeView::minimumSizeHint() const
{
	return QSize(m_default_size, 0);
}

void DmsTreeView::setDmsStyleSheet(bool connecting_lines)
{
	if (GetRegStatusFlags() & RSF_TreeView_FollowOSLayout)
	{
		setStyleSheet(
			"QTreeView {"
			"           padding-top: 5px;"
			"           background-color: transparent;"
			"}"
		);
		return;
	}

	setStyleSheet(
		"QTreeView::branch:has-siblings:!adjoins-item {\n"
		"    border-image: url(:/res/images/TV_vline.png) 0;\n"
		"}\n"
		"QTreeView::branch:!has-children:!has-siblings:adjoins-item {\n"
		"    border-image: url(:/res/images/TV_branch_end.png) 0;\n"
		"}\n"
		"QTreeView::branch:has-siblings:adjoins-item {\n"
		"    border-image: url(:/res/images/TV_branch_more.png) 0;\n"
		"}\n"
		"QTreeView::branch:has-children:!has-siblings:closed,"
		"QTreeView::branch:closed:has-children:has-siblings {"
		"        border-image: none;"
		"        image: url(:/res/images/right_arrow.png);"
		"}"
		"QTreeView::branch:closed:hover:has-children {"
		"        border-image: none;"
		"        image: url(:/res/images/right_arrow_hover.png);"
		"}"
		"QTreeView::branch:open:has-children:!has-siblings,"
		"QTreeView::branch:open:has-children:has-siblings {"
		"           border-image: none;"
		"           image: url(:/res/images/down_arrow.png);"
		"}"
		"QTreeView::branch:open:hover:has-children {"
		"           border-image: none;"
		"           image: url(:/res/images/down_arrow_hover.png);"
		"}"
		"QTreeView {"
		"           padding-top: 5px;"
		"           background-color: transparent;"
		"}");
}

DmsTreeView::DmsTreeView(QWidget* parent)
	: QTreeView(parent)
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
	connect(this, &DmsTreeView::customContextMenuRequested, this, &DmsTreeView::showTreeviewContextMenu);
	
	horizontalScrollBar()->setEnabled(true);
	setHorizontalScrollBarPolicy(Qt::ScrollBarPolicy::ScrollBarAsNeeded);
	header()->setStretchLastSection(false);
	connect(header(), &QHeaderView::sectionClicked, this, &DmsTreeView::onHeaderSectionClicked);

	header()->setSectionResizeMode(QHeaderView::ResizeToContents);

	setDmsStyleSheet();

	setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding);
}

void DmsTreeView::showTreeviewContextMenu(const QPoint& pos)
{
	QModelIndex index = indexAt(pos);
	if (!index.isValid())
		return;

	auto export_primary_data_action = MainWindow::TheOne()->m_export_primary_data_action.get();
	auto step_to_failreason = MainWindow::TheOne()->m_step_to_failreason_action.get();
	auto go_to_causa_prima = MainWindow::TheOne()->m_go_to_causa_prima_action.get();
	auto edit_config_source = MainWindow::TheOne()->m_edit_config_source_action.get();
	auto update_treeitem = MainWindow::TheOne()->m_update_treeitem_action.get();
	auto update_subtree = MainWindow::TheOne()->m_update_subtree_action.get();
	auto default_view_action = MainWindow::TheOne()->m_defaultview_action.get();
	auto table_view_action = MainWindow::TheOne()->m_tableview_action.get();
	auto map_view_action = MainWindow::TheOne()->m_mapview_action.get();
	auto statistics_view_action = MainWindow::TheOne()->m_statistics_action.get();

	if (!m_context_menu)
	{
		m_context_menu = std::make_unique<QMenu>(MainWindow::TheOne());

		m_context_menu->addAction(export_primary_data_action);
		m_context_menu->addSeparator();

		m_context_menu->addAction(step_to_failreason);
		m_context_menu->addAction(go_to_causa_prima);
		m_context_menu->addSeparator();

		// edit config source
		m_context_menu->addAction(edit_config_source);
		m_context_menu->addSeparator();

		m_context_menu->addAction(update_treeitem);
		m_context_menu->addAction(update_subtree);
		m_code_analysis_submenu = MainWindow::TheOne()->CreateCodeAnalysisSubMenu(m_context_menu.get());
		m_context_menu->addSeparator();

		m_context_menu->addAction(default_view_action);
		m_context_menu->addAction(table_view_action);
		m_context_menu->addAction(map_view_action);
		m_context_menu->addAction(statistics_view_action);
		//	m_context_menu->exec(viewport()->mapToGlobal(pos));

		// histogram view
		//	auto histogramview = MainWindow::TheOne()->m_histogramview_action.get();
		//	histogramview->setDisabled(true);
		//	m_context_menu->addAction(histogramview);

		// process scheme
		//	auto process_scheme = MainWindow::TheOne()->m_process_schemes_action.get(); //TODO: to be implemented or not..
		//	m_context_menu->addAction(process_scheme);
	}
	auto ti = GetTreeItem(index);
	MainWindow::TheOne()->setCurrentTreeItem(ti); // we assume Popupmenu act on current item, so accomodate now.
	auto ti_is_or_is_in_template = ti->InTemplate() && ti->IsTemplate();
	auto ti_is_dataitem = IsDataItem(ti);

	auto item_can_be_exported = !ti->WasFailed() && !ti_is_or_is_in_template && (currentItemCanBeExportedToVector(ti) || currentItemCanBeExportedToRaster(ti));
	export_primary_data_action->setEnabled(item_can_be_exported);
	step_to_failreason->setEnabled(ti && WasInFailed(ti));
	go_to_causa_prima->setEnabled(ti && WasInFailed(ti));
	update_treeitem->setDisabled(ti_is_or_is_in_template);
	update_subtree->setDisabled(ti_is_or_is_in_template);
	default_view_action->setDisabled(ti_is_or_is_in_template);
	table_view_action->setDisabled(ti_is_or_is_in_template);
	map_view_action->setDisabled(ti_is_or_is_in_template);
	statistics_view_action->setDisabled(!ti_is_dataitem || ti_is_or_is_in_template);

	m_context_menu->popup(viewport()->mapToGlobal(pos));
	MainWindow::TheOne()->updateToolsMenu();
}

void DmsTreeView::setNewCurrentItem(TreeItem* target_item)
{
	auto current_node_index = currentIndex();
	auto root_node_index = rootIndex();
	auto root_ti = GetTreeItem(root_node_index);
	if (root_ti == target_item)
		return;

	if (current_node_index.isValid())
	{
		auto ti = GetTreeItem(current_node_index);
		if (target_item == ti) // treeview already has current item
			return;
	}

	MG_CHECK(!root_ti || isAncestor(root_ti, target_item));
	if (!MainWindow::TheOne()->m_dms_model->show_hidden_items)
	{
		if (target_item->GetTSF(TSF_InHidden) )
		{
			const TreeItem* visible_parent = target_item;
			while (visible_parent && visible_parent->GetTSF(TSF_InHidden))
				visible_parent = visible_parent->GetTreeParent();
			reportF(MsgCategory::other, SeverityTypeID::ST_Warning, "cannnot activate '%1%' in TreeView as it seems to be a hidden sub-item of '%2%'"
				"\nHint: you can make hidden items visible in the Settings->GUI Options Dialog"
				, target_item->GetFullName().c_str()
				, visible_parent ? visible_parent->GetFullName().c_str() : "a hidden root"
			);
		}
	}

	try
	{
		expandToItem(target_item);
	}
	catch (...)
	{
		catchException(false);
	}
	
}

void DmsTreeView::onDoubleClick(const QModelIndex& index)
{
	if (!index.isValid())
		return;

	MainWindow::TheOne()->defaultViewOrAddItemToCurrentView();
}

void  DmsTreeView::onHeaderSectionClicked(int index)
{
}

auto createTreeview(MainWindow* dms_main_window) -> QPointer<DmsTreeView>
{
	auto main_window = MainWindow::TheOne();

	main_window->m_treeview_dock = new QDockWidget(QObject::tr("TreeView"), dms_main_window);
	main_window->m_treeview_dock->setTitleBarWidget(new QWidget(MainWindow::TheOne()->m_treeview_dock));
	QPointer<DmsTreeView> dms_treeview_widget_pointer = new DmsTreeView(MainWindow::TheOne()->m_treeview_dock);
	main_window->m_treeview_dock->setWidget(dms_treeview_widget_pointer);
	dms_main_window->addDockWidget(Qt::LeftDockWidgetArea, MainWindow::TheOne()->m_treeview_dock);

    return dms_treeview_widget_pointer;
}
