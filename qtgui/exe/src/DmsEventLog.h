#include "RtcBase.h"
#include "dbg/SeverityType.h"
#include "ptr/SharedStr.h"
#include "ui_DmsEventLogSelection.h"
#include "utl/Environment.h"

#include <QPointer>
#include <QAbstractListModel>
#include <QListView>
#include <QCheckBox>
#include <QLineEdit>
#include <QModelIndexList>
#include <QPushButton>
#include <QDateTime>

class MainWindow;
using BYTE = unsigned char;

class EventLogModel : public QAbstractListModel
{
	Q_OBJECT

	using msg_line_index_t = int;

public:

	int rowCount(const QModelIndex & /*parent*/ = QModelIndex()) const override
	{
		return m_filtered_indices.size();
	}

	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	void addText(MsgData&& msgData, bool moreToCome);
	auto dataFiltered(int row) const -> const MsgData&;

	QByteArray m_TextFilterAsByteArray;
	UInt32 cached_reg_flags = GetRegStatusFlags();

public slots:
	void clear();
	void refilter();
	void refilterForTextFilter();
	void writeSettingsOnToggle(bool newValue);
	void updateOnNewMessages();

private:
	bool itemPassesTypeFilter(const MsgData& item) const;
	bool itemPassesCategoryFilter(const MsgData& item) const;
	bool itemPassesTextFilter(const MsgData& item) const;
	bool itemPassesFilter(const MsgData& item) const;
	void scanFilter(msg_line_index_t index);

public:
	std::vector<msg_line_index_t> m_filtered_indices;
	std::vector<MsgData> m_MsgLines;

#if defined(MG_DEBUG)
	bool md_AddTextCompleted = true;
#endif

private:

	bool has_queued_update = false;
	size_t m_last_updated_message_count = 0;
	QDateTime time_since_last_direct_update = QDateTime::currentDateTime();
	size_t direct_update_interval = 5;
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
	bool m_text_filter_active = false;
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;


public slots:
	void copySelectedEventlogLinesToClipboard();
	void onVerticalScrollbarValueChanged(int value);
	void scrollToBottomOnTimeout();
	void scrollToBottomThrottled();
	void toggleScrollToBottom();
	void toggleScrollToBottomDirectly();
	void toggleFilter(bool toggled);
	void invalidateOnVisualChange();
	void onTextChanged(const QString& text);
	void clearTextFilter();
	void onItemClicked(const QModelIndex& index);

private:
	bool isScrolledToBottom();
	int default_height = 0;
};

void geoDMSMessage(ClientHandle clientHandle, const MsgData* msgData, bool moreToCome);
auto createEventLog(MainWindow* dms_main_window) -> std::unique_ptr<DmsEventLog>;