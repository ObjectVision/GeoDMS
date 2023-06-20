#include <QListWidget>

#include <QApplication>
#include <QColor>
#include <QObject>
#include <QDockWidget>
#include <QMenubar>

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

void EventLogModel::AddText(SeverityTypeID st, CharPtr msg)
{
	auto rowCount_ = rowCount();
	beginInsertRows(QModelIndex(), rowCount_, rowCount_);

	m_Items.insert(m_Items.end(), item_t(st, msg));

	endInsertRows();

	auto index = this->index(rowCount_, 0, QModelIndex());
	emit dataChanged(index, index);
}

void EventLog_AddText(SeverityTypeID st, CharPtr msg)
{
	if (!MainWindow::IsExisting())
		return;

	auto* theModel = MainWindow::TheOne()->m_eventlog_model.get(); assert(theModel);
	theModel->AddText(st, msg);

	static Timer t;
	if (t.PassedSecs(5))
		MainWindow::TheOne()->getEventLogViewPtr()->scrollToBottom();
}

void geoDMSMessage(ClientHandle /*clientHandle*/, SeverityTypeID st, CharPtr msg)
{
	EventLog_AddText(st, msg);
}

auto createEventLog(MainWindow* dms_main_window) -> QPointer<QListView>
{
    auto dock = new QDockWidget(QObject::tr("EventLog"), dms_main_window);
    QPointer<QListView> dms_eventlog_pointer = new QListView(dock);
    dock->setWidget(dms_eventlog_pointer);
    dock->setTitleBarWidget(new QWidget(dock));
//    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    dms_main_window->addDockWidget(Qt::BottomDockWidgetArea, dock);

    dms_eventlog_pointer->setUniformItemSizes(true);

    //viewMenu->addAction(dock->toggleViewAction());
	dms_main_window->m_eventlog_model = std::make_unique< EventLogModel>();
	dms_eventlog_pointer->setModel(dms_main_window->m_eventlog_model.get());

	return dms_eventlog_pointer;
}