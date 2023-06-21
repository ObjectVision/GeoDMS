#include "RtcBase.h"

#include <QPointer>
#include <QAbstractListModel>
#include <QListView>
#include <QCheckBox>
#include <QLineEdit>

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

class DmsEventLog : public QWidget
{
	Q_OBJECT
public:
	DmsEventLog(QWidget* parent);
	std::unique_ptr<QLineEdit> m_text_filter;
	std::unique_ptr<QCheckBox> m_minor_trace_filter;
	std::unique_ptr<QCheckBox> m_major_trace_filter;
	std::unique_ptr<QCheckBox> m_warning_filter;
	std::unique_ptr<QCheckBox> m_error_filter;
	std::unique_ptr<QListView> m_log;

public slots:
	void scrollToBottomOnTimeout();
	void scrollToBottomThrottled();
	void toggleTextFilter();
	void toggleTypeFilter();

private:
	QPointer<QTimer> m_throttle_timer;
};

void geoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg);
auto createEventLog(MainWindow* dms_main_window) -> std::unique_ptr<DmsEventLog>;