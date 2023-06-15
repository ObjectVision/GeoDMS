#include "RtcBase.h"

#include <QColor>
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
	using item_t = std::pair<QColor, QString>;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return m_Items.size();
	}

	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	bool insertRows(int position, int rows, const QModelIndex& index = QModelIndex());
	bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);

private:
	std::vector<item_t> m_Items;
};


void geoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg);
auto createEventLog(MainWindow* dms_main_window) -> QPointer<QListView>;
void EventLog_AddText(SeverityTypeID st, CharPtr msg);
