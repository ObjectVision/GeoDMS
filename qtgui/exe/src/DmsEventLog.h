#include "RtcBase.h"
#include "dbg/SeverityType.h"
#include "ptr/SharedStr.h"
#include "DmsEventLogSelection.h"

#include <QPointer>
#include <QAbstractListModel>
#include <QListView>
#include <QCheckBox>
#include <QLineEdit>
#include <QModelIndexList>
#include <QPushButton>

class MainWindow;
using BYTE = unsigned char;

class EventLogModel : public QAbstractListModel
{
	Q_OBJECT

public:

	struct item_t {
		SeverityTypeID m_SeverityCode;
		MsgCategory m_MsgCode;
		dms_thread_id m_ThreadID = 1;
		StreamableDateTime m_DateTime;
		QString m_Msg;

		SeverityTypeID GetSeverityType() const { return m_SeverityCode; }
		MsgCategory GetMsgCategory() const { return m_MsgCode; }
	};

	int rowCount(const QModelIndex& /*parent*/ = QModelIndex()) const override
	{
		return m_filtered_indices.empty() ? m_Items.size() : m_filtered_indices.size();
	}

	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	void addText(SeverityTypeID st, MsgCategory msgCat, StreamableDateTime when, dms_thread_id threadID, CharPtr msg);

public slots:
	void clear();
	void refilter();
	void refilterOnToggle(bool checked);

private:
	auto dataFiltered(int row) const -> const EventLogModel::item_t&;
	bool itemPassesTypeFilter(item_t& item);
	bool itemPassesCategoryFilter(item_t& item);
	bool itemPassesTextFilter(item_t& item);
	bool itemPassesFilter(item_t& item);

	std::vector<size_t> m_filtered_indices;
	std::vector<item_t> m_Items;
	bool m_filter_active = true;
};

class DmsTypeFilter : public QWidget, public Ui::DmsEventLogTypeSelection
{
public:
	DmsTypeFilter(QWidget* parent = nullptr);
	QSize sizeHint() const override;
};

class DmsEventLog : public QWidget
{
	Q_OBJECT
public:
	DmsEventLog(QWidget* parent);
	void keyPressEvent(QKeyEvent* event) override;
	std::unique_ptr<DmsTypeFilter> m_eventlog_filter;
	std::unique_ptr<QListView> m_log;
	std::unique_ptr<QPushButton> m_copy_selected_to_clipboard, m_scroll_to_bottom_toggle, m_event_filter_toggle, m_clear;
	bool m_scroll_to_bottom = true;


public slots:
	void copySelectedEventlogLinesToClipboard();
	void onVerticalScrollbarValueChanged(int value);
	void scrollToBottomOnTimeout();
	void scrollToBottomThrottled();
	void toggleScrollToBottom();
	void toggleScrollToBottomDirectly();
	void toggleTextFilter(bool toggled);
	void toggleTypeFilter(bool toggled);
	void onTextChanged(const QString& text);
	void clearTextFilter();

private:

	bool isScrolledToBottom();
	//QPointer<QTimer> m_throttle_timer;
	
};


void geoDMSMessage(ClientHandle clientHandle, MsgData* msgData);
auto createEventLog(MainWindow* dms_main_window) -> std::unique_ptr<DmsEventLog>;