#include "RtcBase.h"

#include <QPointer>
#include <QAbstractListModel>

QT_BEGIN_NAMESPACE
class QListView;
QT_END_NAMESPACE
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

	void AddText(SeverityTypeID st, CharPtr msg);

private:
	std::vector<item_t> m_Items;
};


void geoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg);
auto createEventLog(MainWindow* dms_main_window) -> QPointer<QListView>;
void EventLog_AddText(SeverityTypeID st, CharPtr msg);
