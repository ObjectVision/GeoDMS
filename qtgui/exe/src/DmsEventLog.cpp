#include <QListWidget>

#include <QApplication>
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
		return m_Items[row].first;

	case Qt::SizeHintRole:
	{
		static int pixels_high = QFontMetrics(QApplication::font()).height();
		return QSize(10, pixels_high);
	}
	}
	return QVariant();
}

bool EventLogModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if (index.isValid())
		switch (role) 
		{
			case Qt::EditRole:
			{
				m_Items[index.row()].second = value.toString();
				emit dataChanged(index, index);
				return true;
			}

			case Qt::ForegroundRole:
			{
				m_Items[index.row()].first = value.value<QColor>();
				emit dataChanged(index, index);
				return true;
			}
		}

	return false;
}

bool EventLogModel::insertRows(int row, int count, const QModelIndex& parent)
{
	beginInsertRows(QModelIndex(), row, row + count - 1);

	for (int i= 0; i< count; ++i)
		m_Items.insert(m_Items.begin() + row + i, item_t());

	endInsertRows();
	return true;

}

void EventLog_AddText(SeverityTypeID st, CharPtr msg)
{
	if (!MainWindow::IsExisting())
		return;

	auto* theModel = MainWindow::TheOne()->m_eventlog_model.get(); assert(theModel);
	auto rowCount = theModel->rowCount();
	theModel->insertRow(rowCount);
	auto index = theModel->index(rowCount, 0, QModelIndex());
	theModel->setData(index, QString(msg), Qt::EditRole);

	switch (st) {
	case SeverityTypeID::ST_FatalError:
	case SeverityTypeID::ST_Error:
		theModel->setData(index, QColor(Qt::red), Qt::ForegroundRole);
		break;
	case SeverityTypeID::ST_Warning:
		theModel->setData(index, QColor(Qt::darkYellow), Qt::ForegroundRole);
		break;
	case SeverityTypeID::ST_MajorTrace:
		theModel->setData(index, QColor(Qt::darkBlue), Qt::ForegroundRole);
		break;
	}

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