#include <QApplication>
#include <QColor>
#include <QObject>
#include <QDockWidget>
#include <QMenubar>
#include <QTimer>

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
	: QListView(parent)
{
	m_throttle_timer = new QTimer(this);
	m_throttle_timer->setSingleShot(true);
	connect(m_throttle_timer, &QTimer::timeout, this, &DmsEventLog::scrollToBottomOnTimeout);
}

void DmsEventLog::scrollToBottomOnTimeout()
{
	scrollToBottom();
}

void DmsEventLog::scrollToBottomThrottled()
{
	if (m_throttle_timer->isActive())
		return;

	m_throttle_timer->start(1000);
}

void EventLog_AddText(SeverityTypeID st, CharPtr msg)
{
	if (!MainWindow::IsExisting())
		return;

	auto* theModel = MainWindow::TheOne()->m_eventlog_model.get(); assert(theModel);
	theModel->addText(st, msg);

	static Timer t;
	if (t.PassedSecs(5))
		MainWindow::TheOne()->getEventLogViewPtr()->scrollToBottom();
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

    dms_eventlog_pointer->setUniformItemSizes(true);

    //viewMenu->addAction(dock->toggleViewAction());
	dms_main_window->m_eventlog_model = std::make_unique<EventLogModel>();
	dms_eventlog_pointer->setModel(dms_main_window->m_eventlog_model.get());

	return dms_eventlog_pointer;
}