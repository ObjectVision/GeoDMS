#include "RtcBase.h"

#include <QPointer>
#include <QAbstractListModel>
#include <QListView>

class MainWindow;

class EventLogModel : public QAbstractListModel
{
	Q_OBJECT

public:
	using item_t = std::pair<SeverityTypeID, QString>;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return m_Items.size();
	}

	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	void addText(SeverityTypeID st, CharPtr msg);

private:
	std::vector<item_t> m_Items;
};

class DmsEventLog : public QListView
{
	Q_OBJECT
public:
	DmsEventLog(QWidget* parent);

public slots:
	void scrollToBottomOnTimeout();
	void scrollToBottomThrottled();

private:
	QPointer<QTimer> m_throttle_timer;
};

void geoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg);
auto createEventLog(MainWindow* dms_main_window) -> std::unique_ptr<DmsEventLog>;