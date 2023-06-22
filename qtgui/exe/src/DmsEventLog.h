#include "RtcBase.h"

#include <QPointer>
#include <QAbstractListModel>
#include <QListView>
#include <QCheckBox>
#include <QLineEdit>
#include <QModelIndexList>

class MainWindow;

class EventLogModel : public QAbstractListModel
{
	Q_OBJECT

public:
	using item_t = std::pair<SeverityTypeID, QString>;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return m_filtered_indices.empty() ? m_Items.size() : m_filtered_indices.size();
	}

	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	void addText(SeverityTypeID st, CharPtr msg);

public slots:
	void refilter();
	void refilterOnToggle(bool checked);

private:
	auto dataFiltered(int row) const -> std::pair<SeverityTypeID, QString>;
	bool itemPassesTypeFilter(item_t& item);
	bool itemPassesTextFilter(item_t& item);
	bool itemPassesFilter(item_t& item);

	std::vector<size_t> m_filtered_indices;
	std::vector<item_t> m_Items;
	bool m_filter_active = false;
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
	bool m_scroll_to_bottom = true;

public slots:
	void onVerticalScrollbarValueChanged(int value);
	void scrollToBottomOnTimeout();
	void scrollToBottomThrottled();
	void toggleScrollToBottom();
	void toggleScrollToBottomDirectly();
	void toggleTextFilter(bool toggled);
	void toggleTypeFilter(bool toggled);

private:
	bool isScrolledToBottom();
	QPointer<QTimer> m_throttle_timer;
};

void geoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg);
auto createEventLog(MainWindow* dms_main_window) -> std::unique_ptr<DmsEventLog>;