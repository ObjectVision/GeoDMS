#include <QApplication>
#include <QColor>
#include <QObject>
#include <QDockWidget>
#include <QMenubar>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollBar>
#include <QClipBoard>

#include "dbg/Timer.h"

#include "DmsEventLog.h"
#include "DmsMainWindow.h"
#include <QMainWindow>
#include "dbg/SeverityType.h"

static QColor html_ForestGreen = QColor(34, 139, 34); // EventLog: minor in Calculation Progress
static QColor html_DarkGreen = QColor(0, 100, 0); // EventLog: major in Calculation Progress
static QColor html_blue = QColor(0, 0, 255); // EventLog: read in Storage
static QColor html_navy = QColor(0, 0, 128); // EventLog: write in Storage
static QColor html_fuchsia = QColor(255, 0, 255); // EventLog: connection in Background layer
static QColor html_purple = QColor(128, 0, 128); // EventLog: request in Background layer
static QColor html_black = QColor(0, 0, 0); // EventLog: commands
static QColor html_gray = QColor(128, 128, 128); // EventLog: other
static QColor html_darkorange = QColor(255, 140, 0); // EventLog: warning
static QColor html_red = QColor(255, 0, 0); // EventLog: error
static QColor html_brown = QColor(165, 42, 42); // EventLog: memory

auto EventLogModel::dataFiltered(int row) const -> const EventLogModel::item_t&
{
	return m_Items.at(m_filtered_indices.at(row));
}

void EventLogModel::clear()
{
	m_filtered_indices.clear();
	m_Items.clear();
	refilter();
	MainWindow::TheOne()->m_eventlog->m_clear->setDisabled(true);
}

QVariant EventLogModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (m_filter_active && m_filtered_indices.empty()) // filter active, but no item passed filter
		return QVariant();

	auto row = index.row();
	const item_t& item_data = m_filter_active ? dataFiltered(row) : m_Items[row];

	switch (role)
	{
	case Qt::DisplayRole:
		return item_data.m_Msg;

	case Qt::ForegroundRole:
	{
		auto return_color = QColor();
		switch (item_data.GetSeverityType()) 
		{
		case SeverityTypeID::ST_DispError:// error
		case SeverityTypeID::ST_FatalError:
		case SeverityTypeID::ST_Error: {return html_red; }
		case SeverityTypeID::ST_Warning: {return html_darkorange; }// warning
		case SeverityTypeID::ST_MinorTrace: {return_color = html_ForestGreen; break; } // minor trace							  
		case SeverityTypeID::ST_MajorTrace: { return_color = html_DarkGreen; break; }// major trace
		}

		switch (item_data.GetMsgCategory())
		{
		case MsgCategory::storage_read: {return html_blue;}
		case MsgCategory::storage_write: {return html_navy; }
		case MsgCategory::background_layer_connection: {return html_fuchsia; }
		case MsgCategory::background_layer_request: {return html_purple; }
		case MsgCategory::commands: { return html_black; }
		case MsgCategory::memory: {return html_brown; }
		case MsgCategory::other: {return html_gray; }
		}
		return return_color;
	}
	case Qt::SizeHintRole:
	{
		static int pixels_high = QFontMetrics(QApplication::font()).height();
		return QSize(10, pixels_high);
	}
	}
	return QVariant();
}

bool EventLogModel::itemPassesTypeFilter(item_t& item)
{
	auto eventlog = MainWindow::TheOne()->m_eventlog.get();
	switch (item.GetSeverityType())
	{
	case SeverityTypeID::ST_MinorTrace: {return eventlog->m_eventlog_filter->m_minor_trace_filter->isChecked(); };
	case SeverityTypeID::ST_MajorTrace: {return eventlog->m_eventlog_filter->m_major_trace_filter->isChecked(); };
	case SeverityTypeID::ST_Warning: {return eventlog->m_eventlog_filter->m_warning_filter->isChecked(); };
	case SeverityTypeID::ST_Error: {return eventlog->m_eventlog_filter->m_error_filter->isChecked(); };
	default: return true;
	}
}

bool EventLogModel::itemPassesCategoryFilter(item_t& item)
{
	auto eventlog = MainWindow::TheOne()->m_eventlog.get();
	switch (item.GetMsgCategory())
	{
	case MsgCategory::storage_read: {return eventlog->m_eventlog_filter->m_read_filter->isChecked(); }
	case MsgCategory::storage_write: {return eventlog->m_eventlog_filter->m_write_filter->isChecked(); }
	case MsgCategory::background_layer_connection: {return eventlog->m_eventlog_filter->m_connection_filter->isChecked(); }
	case MsgCategory::background_layer_request: {return eventlog->m_eventlog_filter->m_request_filter->isChecked(); }
	case MsgCategory::memory: {return eventlog->m_eventlog_filter->m_category_filter_memory->isChecked(); }
	case MsgCategory::other: {return eventlog->m_eventlog_filter->m_category_filter_other->isChecked(); }
	case MsgCategory::commands: {return eventlog->m_eventlog_filter->m_category_filter_commands->isChecked(); }
	}
	return false;
}

bool EventLogModel::itemPassesTextFilter(item_t& item)
{
	auto eventlog = MainWindow::TheOne()->m_eventlog.get();
	auto text_filter_string = eventlog->m_eventlog_filter->m_text_filter->text();
	if (text_filter_string.isEmpty())
		return true;
	return item.m_Msg.contains(text_filter_string, Qt::CaseSensitivity::CaseInsensitive);
}

bool EventLogModel::itemPassesFilter(item_t& item)
{
	auto item_passes_type_filter = itemPassesTypeFilter(item);
	auto item_passes_category_filter = itemPassesCategoryFilter(item);
	if (item.GetMsgCategory() == MsgCategory::progress)
		item_passes_category_filter = item_passes_type_filter;
	auto item_passes_text_filter = itemPassesTextFilter(item);

	return (item_passes_type_filter || item_passes_category_filter) && item_passes_text_filter;
}

void EventLogModel::refilter()
{
	beginResetModel();
	m_filter_active = true;
	m_filtered_indices.clear();

	UInt64 index = 0;
	for (auto& item : m_Items)
	{
		if (itemPassesFilter(item))
			m_filtered_indices.push_back(index);

		index++;
	}
	endResetModel();

	MainWindow::TheOne()->m_eventlog->toggleScrollToBottomDirectly();
}

void EventLogModel::refilterOnToggle(bool checked)
{
	refilter();
}

void EventLogModel::addText(SeverityTypeID st, MsgCategory msgCat, CharPtr msg)
{
	auto rowCount_ = rowCount();
	auto new_eventlog_item = item_t{ BYTE(st), BYTE(msgCat), msg };
	//if (m_Items.empty() || m_Items.back().m_Msg.compare(new_eventlog_item.m_Msg)) // exact duplicate log message, skip
	//	return;

	auto eventlog = MainWindow::TheOne()->m_eventlog.get();

	if (st == SeverityTypeID::ST_Error)
		int i = 0;

	eventlog->m_clear->setEnabled(true);
	m_Items.insert(m_Items.end(), std::move(new_eventlog_item));
	bool new_item_passes_filter = itemPassesFilter(m_Items.back());
	if (!new_item_passes_filter)
		return;

	beginInsertRows(QModelIndex(), rowCount_, rowCount_);

	m_filtered_indices.push_back(m_Items.size()-1);

	endInsertRows();
	QModelIndex index;
	index = this->index(rowCount_, 0, QModelIndex());

	emit dataChanged(index, index);
	//eventlog->repaint(); // TODO: also repaints treeview.
}

DmsTypeFilter::DmsTypeFilter(QWidget* parent)
	: QWidget(parent)
{
	setupUi(this);
}

QSize DmsTypeFilter::sizeHint() const
{
	auto type_filter_size_hint = groupBox->size();
	return type_filter_size_hint;
}

DmsEventLog::DmsEventLog(QWidget* parent)
	: QWidget(parent)
{
	const QIcon event_filter_icon = QIcon(":/res/images/EL_selection.bmp");
	m_event_filter_toggle = std::make_unique<QPushButton>(event_filter_icon, "");
	m_event_filter_toggle->setToolTip(tr("Filters"));
	m_event_filter_toggle->setStatusTip("Turn eventlog filter dialog on or off");
	m_event_filter_toggle->setCheckable(true);
	m_event_filter_toggle->setStyleSheet("QPushButton { icon-size: 32px; padding: 0px}\n");
	connect(m_event_filter_toggle.get(), &QPushButton::toggled, this, &DmsEventLog::toggleTextFilter);

	/*const QIcon eventlog_type_filter_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/EL_selection_type.bmp"));
	m_event_type_filter_toggle = std::make_unique<QPushButton>(eventlog_type_filter_icon, "");
	m_event_type_filter_toggle->setToolTip(tr("Type filter"));
	m_event_type_filter_toggle->setStatusTip("Turn eventlog type-filter on or off");
	m_event_type_filter_toggle->setCheckable(true);
	m_event_type_filter_toggle->setStyleSheet("QPushButton { icon-size: 32px; padding: 0px}\n");
	connect(m_event_type_filter_toggle.get(), &QPushButton::toggled, this, &DmsEventLog::toggleTypeFilter);*/

	const QIcon eventlog_type_clear_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/EL_clear.bmp"));
	m_clear = std::make_unique<QPushButton>(eventlog_type_clear_icon, "");
	m_clear->setToolTip(tr("Clear"));
	m_clear->setStatusTip("Clear all eventlog messages");
	m_clear->setDisabled(true);
	m_clear->setStyleSheet("QPushButton { icon-size: 32px; padding: 0px}\n");
	connect(m_clear.get(), &QPushButton::pressed, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::clear);

	const QIcon eventlog_scroll_to_bottom_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/EL_scroll_down.bmp"));
	m_scroll_to_bottom_toggle = std::make_unique<QPushButton>(eventlog_scroll_to_bottom_icon, "");
	m_scroll_to_bottom_toggle->setToolTip(tr("Scroll to bottom"));
	m_scroll_to_bottom_toggle->setStatusTip("Scroll content of eventlog to bottom and keep up with new log messages");
	m_scroll_to_bottom_toggle->setDisabled(true);
	m_scroll_to_bottom_toggle->setStyleSheet("QPushButton { icon-size: 32px; padding: 0px}\n");
	connect(m_scroll_to_bottom_toggle.get(), &QPushButton::pressed, this, &DmsEventLog::toggleScrollToBottomDirectly);

	// filters
	//m_text_filter = std::make_unique<QLineEdit>();
	m_eventlog_filter = std::make_unique<DmsTypeFilter>();
	connect(m_eventlog_filter.get()->m_minor_trace_filter, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
	connect(m_eventlog_filter.get()->m_major_trace_filter, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
	connect(m_eventlog_filter.get()->m_warning_filter, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
	connect(m_eventlog_filter.get()->m_error_filter, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);

	//connect(m_dms_type_filter.get()->m_category_filter_system, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
	//connect(m_dms_type_filter.get()->m_category_filter_progress, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
	connect(m_eventlog_filter.get()->m_category_filter_commands, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
	connect(m_eventlog_filter.get()->m_category_filter_memory, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
	connect(m_eventlog_filter.get()->m_connection_filter, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
	connect(m_eventlog_filter.get()->m_request_filter, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
	connect(m_eventlog_filter.get()->m_category_filter_other, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);
	//connect(m_dms_type_filter.get()->m_category_filter_memory, &QCheckBox::toggled, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilterOnToggle);

	m_eventlog_filter->m_clear_text_filter->setDisabled(true);
	m_eventlog_filter->m_activate_text_filter->setDisabled(true);

	connect(m_eventlog_filter->m_text_filter, &QLineEdit::returnPressed, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilter);
	connect(m_eventlog_filter->m_activate_text_filter, &QPushButton::released, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::refilter);
	connect(m_eventlog_filter->m_text_filter, &QLineEdit::textChanged, this, &DmsEventLog::onTextChanged);
	connect(m_eventlog_filter->m_clear_text_filter, &QPushButton::released, this, &DmsEventLog::clearTextFilter);

	// eventlog
	m_log = std::make_unique<QListView>();
	m_log->setSelectionRectVisible(true);
	m_log->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_log->setUniformItemSizes(true);
	connect(m_log->verticalScrollBar(), &QScrollBar::valueChanged, this, &DmsEventLog::onVerticalScrollbarValueChanged);

	auto vertical_layout = new QVBoxLayout();
	auto grid_layout = new QGridLayout();

	auto eventlog_toolbar = new QVBoxLayout();
	eventlog_toolbar->addWidget(m_event_filter_toggle.get());
	//eventlog_toolbar->addWidget(m_event_type_filter_toggle.get());
	eventlog_toolbar->addWidget(m_clear.get());
	eventlog_toolbar->addWidget(m_scroll_to_bottom_toggle.get());
	QWidget* spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	eventlog_toolbar->addWidget(spacer);
	vertical_layout->addWidget(m_eventlog_filter.get(), 0);

	grid_layout->addWidget(m_log.get(), 0, 0);
	grid_layout->addLayout(eventlog_toolbar, 0, 1);
	vertical_layout->addLayout(grid_layout);
	setLayout(vertical_layout);
	toggleTextFilter(false);
	toggleTypeFilter(false);
}

void DmsEventLog::copySelectedEventlogLinesToClipboard()
{
	auto eventlog_model = MainWindow::TheOne()->m_eventlog_model.get();
	auto selected_indexes = m_log->selectionModel()->selectedIndexes();

	if (selected_indexes.isEmpty())
		return;

	std::sort(selected_indexes.begin(), selected_indexes.end());

	QString new_cliboard_text = "";

	for (auto& index : selected_indexes)
	{
		auto eventlog_item = eventlog_model->data(index, Qt::DisplayRole);
		if (!eventlog_item.isValid())
			continue;

		auto item_text = eventlog_item.toString();
		if (item_text.isEmpty())
			continue;

		new_cliboard_text = new_cliboard_text + item_text + "\n";
	}
	if (new_cliboard_text.isEmpty())
		return;

	QGuiApplication::clipboard()->clear();
	QGuiApplication::clipboard()->setText(new_cliboard_text, QClipboard::Clipboard);
}

void DmsEventLog::keyPressEvent(QKeyEvent* event)
{
	if (event == QKeySequence::Copy)
	{
		//return QWidget::keyPressEvent(event);
		copySelectedEventlogLinesToClipboard();
		event->accept();
		return;
	}
	return QWidget::keyPressEvent(event);
}

void DmsEventLog::onVerticalScrollbarValueChanged(int value)
{
	auto vertical_scroll_bar = m_log->verticalScrollBar();
	auto maximum = vertical_scroll_bar->maximum();
	if ((m_scroll_to_bottom && value < maximum))
		toggleScrollToBottom();

	if (value == maximum)
		toggleScrollToBottomDirectly();
}

bool DmsEventLog::isScrolledToBottom()
{
	auto vertical_scroll_bar = m_log->verticalScrollBar();
	auto current = vertical_scroll_bar->sliderPosition();
	auto maximum = vertical_scroll_bar->maximum();
	if (current == maximum || maximum == 0)
		return true;
	return false;
}

void DmsEventLog::scrollToBottomOnTimeout()
{
	auto new_current_index = m_log->model()->index(m_log->model()->rowCount()-1, 0);
	m_log->setCurrentIndex(new_current_index);
	m_log->scrollToBottom();
}

void DmsEventLog::toggleScrollToBottom()
{
	m_scroll_to_bottom = !m_scroll_to_bottom;
	m_scroll_to_bottom ? m_scroll_to_bottom_toggle->setDisabled(true) : m_scroll_to_bottom_toggle->setEnabled(true);
	if (m_scroll_to_bottom)
		scrollToBottomThrottled();
}

void DmsEventLog::toggleScrollToBottomDirectly()
{
	m_scroll_to_bottom = true;
	m_scroll_to_bottom_toggle->setDisabled(true);
	auto new_current_index = m_log->model()->index(m_log->model()->rowCount() - 1, 0);
	m_log->setCurrentIndex(new_current_index);
	m_log->scrollToBottom();
}

void DmsEventLog::scrollToBottomThrottled()
{
	if (!m_scroll_to_bottom)
		return;

	scrollToBottomOnTimeout();
}

void DmsEventLog::toggleTextFilter(bool toggled)
{
	m_eventlog_filter->setVisible(toggled);
}

void DmsEventLog::toggleTypeFilter(bool toggled)
{
	m_eventlog_filter->setVisible(toggled);
}

void DmsEventLog::onTextChanged(const QString& text)
{
	if (m_eventlog_filter->m_text_filter->text().isEmpty())
	{
		m_eventlog_filter->m_activate_text_filter->setDisabled(true);
		m_eventlog_filter->m_clear_text_filter->setDisabled(true);
	}
	else
	{
		m_eventlog_filter->m_activate_text_filter->setEnabled(true); 
		m_eventlog_filter->m_clear_text_filter->setEnabled(true);
	}
}

void DmsEventLog::clearTextFilter()
{
	m_eventlog_filter->m_text_filter->clear();
	MainWindow::TheOne()->m_eventlog_model->refilter();
}

void geoDMSMessage(ClientHandle /*clientHandle*/, SeverityTypeID st, MsgCategory msgCat, CharPtr msg)
{
//	assert(IsMainThread());
	if (st == SeverityTypeID::ST_Nothing)
	{
		// assume async call to notify desire to call ProcessMainThreadOpers() in a near future
//		QTimer::singleShot(0, [] { ProcessMainThreadOpers();  });
		PostMessage(nullptr, WM_APP + 3, 0, 0);
		return;
	}
	assert(IsMainThread());
	auto* eventlog_model = MainWindow::TheOne()->m_eventlog_model.get(); assert(eventlog_model);
	auto* eventlog_view = MainWindow::TheOne()->m_eventlog.get(); assert(eventlog_view);
	eventlog_model->addText(st, msgCat, msg);
	eventlog_view->scrollToBottomThrottled();
}

auto createEventLog(MainWindow* dms_main_window) -> std::unique_ptr<DmsEventLog>
{
    MainWindow::TheOne()->m_eventlog_dock = new QDockWidget(QObject::tr("EventLog"), dms_main_window);
	dms_main_window->m_eventlog_model = std::make_unique<EventLogModel>();
	auto dms_eventlog_pointer = std::make_unique<DmsEventLog>(MainWindow::TheOne()->m_eventlog_dock);
	
	MainWindow::TheOne()->m_eventlog_dock->setWidget(dms_eventlog_pointer.get());
	MainWindow::TheOne()->m_eventlog_dock->setTitleBarWidget(new QWidget(MainWindow::TheOne()->m_eventlog_dock));
//    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dms_main_window->addDockWidget(Qt::BottomDockWidgetArea, MainWindow::TheOne()->m_eventlog_dock);

    //viewMenu->addAction(dock->toggleViewAction());

	dms_eventlog_pointer->m_log->setModel(dms_main_window->m_eventlog_model.get());
	//dock->show();
	return dms_eventlog_pointer;
}