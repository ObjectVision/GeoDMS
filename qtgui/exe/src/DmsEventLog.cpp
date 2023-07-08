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

#include "dbg/Timer.h"

#include "DmsEventLog.h"
#include "DmsMainWindow.h"
#include <QMainWindow>
#include "dbg/SeverityType.h"

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
		switch (item_data.GetSeverityType()) {
		case SeverityTypeID::ST_FatalError:
		case SeverityTypeID::ST_Error:
			return QColor(Qt::red);
		case SeverityTypeID::ST_Warning:
			return QColor(255, 127, 80);
		case SeverityTypeID::ST_MajorTrace:
			return QColor(Qt::black);
		}
		break;
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
	case SeverityTypeID::ST_MinorTrace: {return eventlog->m_minor_trace_filter->isChecked(); };
	case SeverityTypeID::ST_MajorTrace: {return eventlog->m_major_trace_filter->isChecked(); };
	case SeverityTypeID::ST_Warning: {return eventlog->m_warning_filter->isChecked(); };
	case SeverityTypeID::ST_Error: {return eventlog->m_error_filter->isChecked(); };
	default: return true;
	}
}

bool EventLogModel::itemPassesTextFilter(item_t& item)
{
	auto text_filter_string = MainWindow::TheOne()->m_eventlog.get()->m_text_filter->text();
	if (text_filter_string.isEmpty())
		return true;

	return item.m_Msg.contains(text_filter_string, Qt::CaseSensitivity::CaseInsensitive);
}

bool EventLogModel::itemPassesFilter(item_t& item)
{
	return itemPassesTypeFilter(item) && itemPassesTextFilter(item);
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
	MainWindow::TheOne()->m_eventlog->m_clear->setEnabled(true);
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
}

DmsEventLog::DmsEventLog(QWidget* parent)
	: QWidget(parent)
{
	// actions
	const QIcon event_text_filter_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/EL_selection_text.bmp"));
	m_event_text_filter_toggle = std::make_unique<QPushButton>(event_text_filter_icon, "");
	m_event_text_filter_toggle->setToolTip(tr("&Eventlog: text filter"));
	m_event_text_filter_toggle->setCheckable(true);
	m_event_text_filter_toggle->setFixedSize(32,32);
	connect(m_event_text_filter_toggle.get(), &QPushButton::toggled, this, &DmsEventLog::toggleTextFilter);

	const QIcon eventlog_type_filter_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/EL_selection_type.bmp"));
	m_event_type_filter_toggle = std::make_unique<QPushButton>(eventlog_type_filter_icon, "");
	m_event_type_filter_toggle->setToolTip(tr("&Eventlog: type filter"));
	m_event_type_filter_toggle->setCheckable(true);
	m_event_type_filter_toggle->setFixedSize(32, 32);
	connect(m_event_type_filter_toggle.get(), &QPushButton::toggled, this, &DmsEventLog::toggleTypeFilter);

	const QIcon eventlog_type_clear_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/EL_clear.bmp"));
	m_clear = std::make_unique<QPushButton>(eventlog_type_clear_icon, "");
	m_clear->setToolTip(tr("&Eventlog: clear"));
	m_clear->setDisabled(true);
	m_clear->setFixedSize(32, 32);
	connect(m_clear.get(), &QPushButton::pressed, MainWindow::TheOne()->m_eventlog_model.get(), &EventLogModel::clear);

	const QIcon eventlog_scroll_to_bottom_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/EL_scroll_down.bmp"));
	m_scroll_to_bottom_toggle = std::make_unique<QPushButton>(eventlog_scroll_to_bottom_icon, "");
	m_scroll_to_bottom_toggle->setToolTip(tr("&Eventlog: scroll to bottom"));
	m_scroll_to_bottom_toggle->setDisabled(true);
	m_scroll_to_bottom_toggle->setFixedSize(32, 32);
	connect(m_scroll_to_bottom_toggle.get(), &QPushButton::pressed, this, &DmsEventLog::toggleScrollToBottomDirectly);

	// throttle
	m_throttle_timer = new QTimer(this);
	m_throttle_timer->setSingleShot(true);
	connect(m_throttle_timer, &QTimer::timeout, this, &DmsEventLog::scrollToBottomOnTimeout);

	// filters
	m_text_filter = std::make_unique<QLineEdit>();
	m_minor_trace_filter = std::make_unique<QCheckBox>("Minor trace");
	m_minor_trace_filter->setCheckable(true);
	m_minor_trace_filter->setChecked(false);
	//m_filter
	m_major_trace_filter = std::make_unique<QCheckBox>("Major trace");
	m_major_trace_filter->setCheckable(true);
	m_major_trace_filter->setChecked(true);
	m_warning_filter = std::make_unique<QCheckBox>("Warning");
	m_warning_filter->setCheckable(true);
	m_warning_filter->setChecked(true);
	m_error_filter = std::make_unique<QCheckBox>("Error");
	m_error_filter->setCheckable(true);
	m_error_filter->setChecked(true);

	// eventlog
	m_log = std::make_unique<QListView>();
	m_log->setSelectionRectVisible(true);
	m_log->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_log->setUniformItemSizes(true);
	connect(m_log->verticalScrollBar(), &QScrollBar::valueChanged, this, &DmsEventLog::onVerticalScrollbarValueChanged);

	auto grid_layout = new QGridLayout();
	auto eventlog_toolbar = new QVBoxLayout();

	auto type_filter_layout = new QHBoxLayout();

	//eventlog_toolbar->setMovable(false);
	//eventlog_toolbar->setOrientation(Qt::Vertical);
	eventlog_toolbar->addWidget(m_event_text_filter_toggle.get());
	eventlog_toolbar->addWidget(m_event_type_filter_toggle.get());
	eventlog_toolbar->addWidget(m_clear.get());
	eventlog_toolbar->addWidget(m_scroll_to_bottom_toggle.get());
	
	QWidget* spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	eventlog_toolbar->addWidget(spacer);

	grid_layout->addWidget(m_text_filter.get(), 0, 0, 1, 5);
	type_filter_layout->addWidget(m_minor_trace_filter.get());
	type_filter_layout->addWidget(m_major_trace_filter.get());
	type_filter_layout->addWidget(m_warning_filter.get());
	type_filter_layout->addWidget(m_error_filter.get());
	grid_layout->addLayout(type_filter_layout, 1, 0, 1, 5);

	//grid_layout->addWidget(m_minor_trace_filter.get(), 1, 0);
	//grid_layout->addWidget(m_major_trace_filter.get(), 1, 1);
	//grid_layout->addWidget(m_warning_filter.get(), 1, 2);
	//grid_layout->addWidget(m_error_filter.get(), 1, 3);
	grid_layout->addWidget(m_log.get(), 2, 0);
	m_log->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	grid_layout->addLayout(eventlog_toolbar, 2, 1); // , Qt::AlignmentFlag::AlignRight
	setLayout(grid_layout);

	toggleTextFilter(false);
	toggleTypeFilter(false);
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

	if (m_throttle_timer->isActive())
		return;

	m_throttle_timer->start(1000);
}

void DmsEventLog::toggleTextFilter(bool toggled)
{
	toggled ? m_text_filter->show() : m_text_filter->hide();
}

void DmsEventLog::toggleTypeFilter(bool toggled)
{
	toggled ? m_minor_trace_filter->show() : m_minor_trace_filter->hide();
	toggled ? m_major_trace_filter->show() : m_major_trace_filter->hide();
	toggled ? m_warning_filter->show() : m_warning_filter->hide();
	toggled ? m_error_filter->show() : m_error_filter->hide();
}

void geoDMSMessage(ClientHandle /*clientHandle*/, SeverityTypeID st, MsgCategory msgCat, CharPtr msg)
{
	auto* eventlog_model = MainWindow::TheOne()->m_eventlog_model.get(); assert(eventlog_model);
	auto* eventlog_view = MainWindow::TheOne()->m_eventlog.get(); assert(eventlog_view);
	eventlog_model->addText(st, msgCat, msg);
	eventlog_view->scrollToBottomThrottled();
}

auto createEventLog(MainWindow* dms_main_window) -> std::unique_ptr<DmsEventLog>
{
    auto dock = new QDockWidget(QObject::tr("EventLog"), dms_main_window);
	auto dms_eventlog_pointer = std::make_unique<DmsEventLog>(dock);
	
    dock->setWidget(dms_eventlog_pointer.get());
    dock->setTitleBarWidget(new QWidget(dock));
//    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dms_main_window->addDockWidget(Qt::BottomDockWidgetArea, dock);

    //viewMenu->addAction(dock->toggleViewAction());
	dms_main_window->m_eventlog_model = std::make_unique<EventLogModel>();
	dms_eventlog_pointer->m_log->setModel(dms_main_window->m_eventlog_model.get());
	//dock->show();
	return dms_eventlog_pointer;
}