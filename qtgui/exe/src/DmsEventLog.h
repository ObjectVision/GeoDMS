#include "RtcBase.h"
#include "dbg/SeverityType.h"
#include "ptr/SharedStr.h"
#include "ui_DmsEventLogSelection.h"

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

	using item_t = MsgData;

	int rowCount(const QModelIndex & /*parent*/ = QModelIndex()) const override
	{
		return m_filtered_indices.empty() ? m_Items.size() : m_filtered_indices.size();
	}

	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	void addText(MsgData&& msgData);

	QByteArray m_TextFilterAsByteArray;

public slots:
	void clear();
	void refilter();
	void refilterForTextFilter();
	void writeSettingsOnToggle(bool newValue);

private:
	auto dataFiltered(int row) const -> const EventLogModel::item_t&;
	bool itemPassesTypeFilter(item_t& item);
	bool itemPassesCategoryFilter(item_t& item);
	bool itemPassesTextFilter(item_t& item);
	bool itemPassesFilter(item_t& item);

	std::vector<size_t> m_filtered_indices;
	std::vector<item_t> m_Items;
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


public slots:
	void copySelectedEventlogLinesToClipboard();
	void onVerticalScrollbarValueChanged(int value);
	void scrollToBottomOnTimeout();
	void scrollToBottomThrottled();
	void toggleScrollToBottom();
	void toggleScrollToBottomDirectly();
	void toggleFilter(bool toggled);
	void onTextChanged(const QString& text);
	void clearTextFilter();

private:
	bool isScrolledToBottom();
	int default_height = 200;
};


void geoDMSMessage(ClientHandle clientHandle, const MsgData* msgData);
auto createEventLog(MainWindow* dms_main_window) -> std::unique_ptr<DmsEventLog>;