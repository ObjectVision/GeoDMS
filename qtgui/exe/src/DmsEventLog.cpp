#include <QApplication>
#include <QColor>
#include <QObject>
#include <QDockWidget>
#include <QMenubar>
#include <QTimer>
#include <QGridLayout>
#include <QScrollBar>

#include "dbg/Timer.h"

#include "DmsEventLog.h"
#include "DmsMainWindow.h"
#include <QMainWindow>
#include "dbg/SeverityType.h"

QVariant EventLogModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	auto row = index.row();
	assert(row < m_Items.size());

	switch (role)
	{
	case Qt::DisplayRole:
		return m_Items[row].second;

	case Qt::ForegroundRole:
	{
		switch (m_Items[row].first) {
		case SeverityTypeID::ST_FatalError:
		case SeverityTypeID::ST_Error:
			return QColor(Qt::red);
		case SeverityTypeID::ST_Warning:
			return QColor(Qt::darkYellow);
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

void EventLogModel::addText(SeverityTypeID st, CharPtr msg)
{
	auto rowCount_ = rowCount();
	beginInsertRows(QModelIndex(), rowCount_, rowCount_);

	m_Items.insert(m_Items.end(), item_t(st, msg));

	endInsertRows();

	auto index = this->index(rowCount_, 0, QModelIndex());
	emit dataChanged(index, index);
}

DmsEventLog::DmsEventLog(QWidget* parent)
	: QWidget(parent)
{
	// throttle
	m_throttle_timer = new QTimer(this);
	m_throttle_timer->setSingleShot(true);
	connect(m_throttle_timer, &QTimer::timeout, this, &DmsEventLog::scrollToBottomOnTimeout);

	// filters
	m_text_filter = std::make_unique<QLineEdit>();
	m_minor_trace_filter = std::make_unique<QCheckBox>("Minor trace");
	m_major_trace_filter = std::make_unique<QCheckBox>("Major trace");
	m_warning_filter = std::make_unique<QCheckBox>("Warning");
	m_error_filter = std::make_unique<QCheckBox>("Error");

	// eventlog
	m_log = std::make_unique<QListView>();
	m_log->setUniformItemSizes(true);
	connect(m_log->verticalScrollBar(), &QScrollBar::valueChanged, this, &DmsEventLog::onVerticalScrollbarValueChanged);

	auto grid_layout = new QGridLayout(MainWindow::TheOne());
	grid_layout->addWidget(m_text_filter.get(), 0, 0, 1, 4);
	grid_layout->addWidget(m_minor_trace_filter.get(), 1, 0);
	grid_layout->addWidget(m_major_trace_filter.get(), 1, 1);
	grid_layout->addWidget(m_warning_filter.get(), 1, 2);
	grid_layout->addWidget(m_error_filter.get(), 1, 3);
	grid_layout->addWidget(m_log.get(), 2, 0, 1, 4);
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
	m_scroll_to_bottom ? MainWindow::TheOne()->m_eventlog_scroll_to_bottom_toggle->setDisabled(true) : MainWindow::TheOne()->m_eventlog_scroll_to_bottom_toggle->setEnabled(true);
	if (m_scroll_to_bottom)
		scrollToBottomThrottled();
}

void DmsEventLog::toggleScrollToBottomDirectly()
{
	m_scroll_to_bottom = true;
	MainWindow::TheOne()->m_eventlog_scroll_to_bottom_toggle->setDisabled(true);
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

void geoDMSMessage(ClientHandle /*clientHandle*/, SeverityTypeID st, CharPtr msg)
{
	auto* eventlog_model = MainWindow::TheOne()->m_eventlog_model.get(); assert(eventlog_model);
	auto* eventlog_view = MainWindow::TheOne()->m_eventlog.get(); assert(eventlog_view);
	eventlog_model->addText(st, msg);
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