#include <QApplication>
#include <QColor>
#include <QObject>
#include <QDockWidget>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenubar>
#include <QEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollBar>
#include <QClipBoard>

#include "dbg/Timer.h"

#include "DmsEventLog.h"
#include "DmsTreeView.h"
#include "DmsOptions.h"
#include "DmsMainWindow.h"
#include <QMainWindow>
#include "dbg/SeverityType.h"

static QColor html_ForestGreen = QColor(34, 139, 34); // EventLog: minor in Calculation Progress
static QColor html_DarkGreen = QColor(0, 100, 0); // EventLog: major in Calculation Progress
static QColor html_blue = QColor(0, 0, 255); // EventLog: read in Storage
static QColor html_navy = QColor(0, 0, 128); // EventLog: write in Storage
static QColor html_fuchsia = QColor(255, 0, 255); // EventLog: connection in Background layer
static QColor html_purple = QColor(128, 0, 128); // EventLog: request in Background layer
static QColor html_black = QColor(0, 0, 0); // EventLog: commands
static QColor html_gray = QColor(128, 128, 128); // EventLog: other
static QColor html_darkorange = QColor(255, 140, 0); // EventLog: warning
static QColor html_red = QColor(255, 0, 0); // EventLog: error
static QColor html_brown = QColor(165, 42, 42); // EventLog: memory

auto EventLogModel::dataFiltered(int row) const -> const EventLogModel::item_t&
{
	return m_Items.at(m_filtered_indices.at(row));
}

void EventLogModel::clear()
{
	m_filtered_indices.clear();
	m_Items.clear();
	refilter();
	last_updated_message_index = 0;
	MainWindow::TheOne()->m_eventlog->m_clear->setDisabled(true);
}

QVariant EventLogModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (m_filtered_indices.empty()) // no item passed filter
		return QVariant();

	auto row = index.row();
	const item_t& item_data = dataFiltered(row);

	switch (role)
	{
	case Qt::DisplayRole:
	{
		SharedStr msgTxt = item_data.m_Txt;
		//auto dms_reg_status_flags = GetRegStatusFlags();
		
		if (((cached_reg_flags & RSF_EventLog_ShowAnyExtra) == 0) && !item_data.m_IsFollowup)
			return QString(msgTxt.c_str());

		bool showDateTime = (cached_reg_flags & RSF_EventLog_ShowDateTime);
		bool showThreadID = (cached_reg_flags & RSF_EventLog_ShowThreadID);
		bool showCategory = (cached_reg_flags & RSF_EventLog_ShowCategory);

		VectorOutStreamBuff buff;
		FormattedOutStream fout(&buff, {});
		if (!item_data.m_IsFollowup)
		{
			if (showDateTime)
				fout << item_data.m_DateTime << " ";
			if (showThreadID)
				fout << "[" << item_data.m_ThreadID << "] ";
			if (showCategory)
				fout << AsString(item_data.m_MsgCategory) << " ";
		}
		else
			fout << "        ";

		fout << msgTxt;
		fout << char(0);
		return QString(buff.GetData());
	}

	case Qt::ForegroundRole:
	{
		auto return_color = QColor();
		switch (item_data.m_SeverityType) 
		{
		case SeverityTypeID::ST_DispError:// error
		case SeverityTypeID::ST_FatalError:
		case SeverityTypeID::ST_Error: {return html_red; }
		case SeverityTypeID::ST_Warning: {return html_darkorange; }// warning
		case SeverityTypeID::ST_MinorTrace: {return_color = html_ForestGreen; break; } // minor trace							  
		case SeverityTypeID::ST_MajorTrace: { return_color = html_DarkGreen; break; }// major trace
		}

		switch (item_data.m_MsgCategory)
		{
		case MsgCategory::storage_read: {return html_blue;}
		case MsgCategory::storage_write: {return html_navy; }
		case MsgCategory::background_layer_connection: {return html_fuchsia; }
		case MsgCategory::background_layer_request: {return html_purple; }
		case MsgCategory::commands: { return html_black; }
		case MsgCategory::memory: {return html_brown; }
		case MsgCategory::other: {return html_gray; }
		}
		return return_color;
	}
	case Qt::SizeHintRole:
	{
		static int pixels_high = QFontMetrics(QApplication::font()).height();
		return QSize(10, pixels_high);
	}
	}
	return QVariant();
}

bool itemIsError(EventLogModel::item_t& item)
{
	switch (item.m_SeverityType)
	{
	case SeverityTypeID::ST_Warning:
	case SeverityTypeID::ST_Error:
		return true;
	}
	return false;
}

bool EventLogModel::itemPassesTypeFilter(item_t& item)
{
	auto eventlog = MainWindow::TheOne()->m_eventlog.get();
	switch (item.m_SeverityType)
	{
	case SeverityTypeID::ST_MinorTrace: {return eventlog->m_eventlog_filter->m_minor_trace_filter->isChecked(); };
	case SeverityTypeID::ST_MajorTrace: {return eventlog->m_eventlog_filter->m_major_trace_filter->isChecked(); };
	case SeverityTypeID::ST_Warning: {return eventlog->m_eventlog_filter->m_warning_filter->isChecked(); };
	case SeverityTypeID::ST_Error: {return eventlog->m_eventlog_filter->m_error_filter->isChecked(); };
	default: return true;
	}
}

bool EventLogModel::itemPassesCategoryFilter(item_t& item)
{
	auto eventlog = MainWindow::TheOne()->m_eventlog.get();
	switch (item.m_MsgCategory)
	{
	case MsgCategory::storage_read: {return eventlog->m_eventlog_filter->m_read_filter->isChecked(); }
	case MsgCategory::storage_write: {return eventlog->m_eventlog_filter->m_write_filter->isChecked(); }
	case MsgCategory::background_layer_connection: {return eventlog->m_eventlog_filter->m_connection_filter->isChecked(); }
	case MsgCategory::background_layer_request: {return eventlog->m_eventlog_filter->m_request_filter->isChecked(); }
	case MsgCategory::memory: {return eventlog->m_eventlog_filter->m_category_filter_memory->isChecked(); }
	case MsgCategory::other: {return eventlog->m_eventlog_filter->m_category_filter_other->isChecked(); }
	case MsgCategory::commands: {return eventlog->m_eventlog_filter->m_category_filter_commands->isChecked(); }
	}
	return false;
}

bool EventLogModel::itemPassesTextFilter(item_t& item)
{
	if (m_TextFilterAsByteArray.isEmpty())
		return true;

	CharPtr textBegin = m_TextFilterAsByteArray.constData();
	auto searchText = CharPtrRange(textBegin, m_TextFilterAsByteArray.size());
	return item.m_Txt.contains_case_insensitive(searchText);
}

bool EventLogModel::itemPassesFilter(item_t& item)
{
	auto item_passes_type_filter = itemPassesTypeFilter(item);
	auto item_passes_category_filter = itemPassesCategoryFilter(item);
	auto item_passes_text_filter = itemPassesTextFilter(item);
	auto item_is_warning_or_error = (item.m_SeverityType == SeverityTypeID::ST_Warning || item.m_SeverityType == SeverityTypeID::ST_Error);
	if (item.m_MsgCategory == MsgCategory::progress || item_is_warning_or_error)
		return item_passes_type_filter && item_passes_text_filter;

	return item_passes_category_filter && item_passes_text_filter;
}

void EventLogModel::refilter()
{
	beginResetModel();
	m_filtered_indices.clear();

	auto eventlog = MainWindow::TheOne()->m_eventlog.get();

	UInt64 index = 0;

	auto text_filter_string = eventlog->m_eventlog_filter->m_text_filter->text();
	m_TextFilterAsByteArray = text_filter_string.toUtf8();
	
	for (auto& item : m_Items)
	{
		if (itemPassesFilter(item))
			m_filtered_indices.push_back(index);

		index++;
	}
	endResetModel();

	MainWindow::TheOne()->m_eventlog->toggleScrollToBottomDirectly();
}

void EventLogModel::refilterForTextFilter()
{
	MainWindow::TheOne()->m_eventlog->m_text_filter_active = true;
	MainWindow::TheOne()->m_eventlog->m_eventlog_filter->m_clear_text_filter->setDisabled(false);
	MainWindow::TheOne()->m_eventlog->m_eventlog_filter->m_activate_text_filter->setDisabled(true);
	refilter();
}

void EventLogModel::writeSettingsOnToggle(bool newValue)
{
	auto eventlog = MainWindow::TheOne()->m_eventlog.get();
	auto eventlog_filter_ptr = eventlog->m_eventlog_filter.get();
	bool clearOnOpen = eventlog_filter_ptr->m_opening_new_configuration->isChecked();
	bool clearOnReopen = eventlog_filter_ptr->m_reopening_current_configuration->isChecked();
	if (clearOnReopen && !clearOnOpen)
	{
		clearOnReopen = newValue;
		clearOnOpen = newValue;
		eventlog_filter_ptr->m_opening_new_configuration->setChecked(newValue);
		eventlog_filter_ptr->m_reopening_current_configuration->setChecked(newValue);
	}

	auto dms_reg_status_flags = GetRegStatusFlags();
	setSF(clearOnOpen, dms_reg_status_flags, RSF_EventLog_ClearOnLoad);
	setSF(clearOnReopen, dms_reg_status_flags, RSF_EventLog_ClearOnReLoad);
	SetRegStatusFlags(dms_reg_status_flags);
}

RTC_CALL bool IsProcessingMainThreadOpers();

void EventLogModel::updateOnNewMessages()
{
	has_queued_update = false;
	time_since_last_direct_update = QDateTime::currentDateTime();
	
	auto number_of_new_messages = m_Items.size() - last_updated_message_index;
	if (!number_of_new_messages) // no new messages
		return;

	size_t number_of_added_filtered_indices = 0;
	for (int i = last_updated_message_index; i < m_Items.size(); i++)
	{
		auto& item = m_Items.at(i);
		bool new_item_passes_filter = itemPassesFilter(item);
		if (!new_item_passes_filter)
			continue;

		m_filtered_indices.push_back(i);
		number_of_added_filtered_indices++;
	}
	if (!number_of_added_filtered_indices)
		return;

	auto rowCount_ = rowCount();

	beginInsertRows(QModelIndex(), rowCount_, rowCount_+number_of_added_filtered_indices-1);
	endInsertRows();

	last_updated_message_index = m_Items.size();

	// update view
	auto main_window = MainWindow::TheOne();
	if (IsProcessingMainThreadOpers())
		main_window->m_treeview->setUpdatesEnabled(false);
	main_window->m_eventlog->scrollToBottomThrottled();
	if (IsProcessingMainThreadOpers())
	{
		main_window->m_eventlog->m_log->repaint();
		main_window->m_treeview->setUpdatesEnabled(true);
	}
}

void EventLogModel::addText(MsgData&& msgData)
{
	auto mainWindow = MainWindow::TheOne();
	if (!mainWindow)
		return;

	auto eventlog = mainWindow->m_eventlog.get();

	eventlog->m_clear->setEnabled(true);
	m_Items.emplace_back(std::move(msgData));

	// direct update
	auto current_time = QDateTime::currentDateTime();
	auto do_direct_update = time_since_last_direct_update.secsTo(current_time) > direct_update_interval;
	if (do_direct_update)
	{
		updateOnNewMessages();
		return;
	}

	if (has_queued_update)
		return;

	// idle time update
	has_queued_update = true;
	QTimer::singleShot(5000, this, [=]()
		{
			updateOnNewMessages();
		});
}

DmsTypeFilter::DmsTypeFilter(QWidget* parent)
	: QWidget(parent)
{
	setupUi(this);
}

QSize DmsTypeFilter::sizeHint() const
{
	auto type_filter_size_hint = groupBox->size();
	return type_filter_size_hint;
}

DmsEventLog::DmsEventLog(QWidget* parent)
	: QWidget(parent)
{
	const QIcon copy_icon = QIcon(":/res/images/TB_copy.bmp");
	m_copy_selected_to_clipboard = std::make_unique<QPushButton>(copy_icon, "");
	m_copy_selected_to_clipboard->setToolTip(tr("Copy selected eventlog lines to clipboard"));
	m_copy_selected_to_clipboard->setStatusTip(tr("Copy selected eventlog lines to clipboard"));
	m_copy_selected_to_clipboard->setStyleSheet("QPushButton { icon-size: 32px; padding: 0px}\n");
	connect(m_copy_selected_to_clipboard.get(), &QPushButton::pressed, this, &DmsEventLog::copySelectedEventlogLinesToClipboard);

	const QIcon event_filter_icon = QIcon(":/res/images/EL_selection.bmp");
	m_event_filter_toggle = std::make_unique<QPushButton>(event_filter_icon, "");
	m_event_filter_toggle->setToolTip(tr("Filters"));
	m_event_filter_toggle->setStatusTip("Turn eventlog filter dialog on or off");
	m_event_filter_toggle->setCheckable(true);
	m_event_filter_toggle->setStyleSheet("QPushButton { icon-size: 32px; padding: 0px}\n");
	connect(m_event_filter_toggle.get(), &QPushButton::toggled, this, &DmsEventLog::toggleFilter);

	auto eventlog_model_ptr = MainWindow::TheOne()->m_eventlog_model.get();

	const QIcon eventlog_type_clear_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/EL_clear.bmp"));
	m_clear = std::make_unique<QPushButton>(eventlog_type_clear_icon, "");
	m_clear->setToolTip(tr("Clear"));
	m_clear->setStatusTip("Clear all eventlog messages");
	m_clear->setDisabled(true);
	m_clear->setStyleSheet("QPushButton { icon-size: 32px; padding: 0px}\n");
	connect(m_clear.get(), &QPushButton::pressed, eventlog_model_ptr, &EventLogModel::clear);

	const QIcon eventlog_scroll_to_bottom_icon = QIcon::fromTheme("detailpages-metainfo", QIcon(":/res/images/EL_scroll_down.bmp"));
	m_scroll_to_bottom_toggle = std::make_unique<QPushButton>(eventlog_scroll_to_bottom_icon, "");
	m_scroll_to_bottom_toggle->setToolTip(tr("Scroll to bottom"));
	m_scroll_to_bottom_toggle->setStatusTip("Scroll content of eventlog to bottom and keep up with new log messages");
	m_scroll_to_bottom_toggle->setDisabled(true);
	m_scroll_to_bottom_toggle->setStyleSheet("QPushButton { icon-size: 32px; padding: 0px}\n");
	connect(m_scroll_to_bottom_toggle.get(), &QPushButton::pressed, this, &DmsEventLog::toggleScrollToBottomDirectly);

	// filters
	//m_text_filter = std::make_unique<QLineEdit>();
	m_eventlog_filter = std::make_unique<DmsTypeFilter>();
	auto dms_reg_status_flags = GetRegStatusFlags();

	m_eventlog_filter->m_date_time->setChecked(dms_reg_status_flags & RSF_EventLog_ShowDateTime);
	m_eventlog_filter->m_thread   ->setChecked(dms_reg_status_flags & RSF_EventLog_ShowThreadID);
	m_eventlog_filter->m_category ->setChecked(dms_reg_status_flags & RSF_EventLog_ShowCategory);

	connect(m_eventlog_filter.get()->m_minor_trace_filter, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);
	connect(m_eventlog_filter.get()->m_major_trace_filter, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);
	connect(m_eventlog_filter.get()->m_warning_filter, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);
	connect(m_eventlog_filter.get()->m_error_filter, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);
	connect(m_eventlog_filter.get()->m_read_filter, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);
	connect(m_eventlog_filter.get()->m_write_filter, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);

	connect(m_eventlog_filter.get()->m_category_filter_commands, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);
	connect(m_eventlog_filter.get()->m_category_filter_memory, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);
	connect(m_eventlog_filter.get()->m_connection_filter, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);
	connect(m_eventlog_filter.get()->m_request_filter, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);
	connect(m_eventlog_filter.get()->m_category_filter_other, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::refilter);

	m_eventlog_filter->m_clear_text_filter->setDisabled(true);
	m_eventlog_filter->m_activate_text_filter->setDisabled(true);

	connect(m_eventlog_filter->m_text_filter, &QLineEdit::returnPressed, eventlog_model_ptr, &EventLogModel::refilterForTextFilter);
	connect(m_eventlog_filter->m_activate_text_filter, &QPushButton::released, eventlog_model_ptr, &EventLogModel::refilterForTextFilter);
	connect(m_eventlog_filter->m_text_filter, &QLineEdit::textChanged, this, &DmsEventLog::onTextChanged);
	connect(m_eventlog_filter->m_clear_text_filter, &QPushButton::released, this, &DmsEventLog::clearTextFilter);

	connect(m_eventlog_filter->m_date_time, &QCheckBox::toggled, this, &DmsEventLog::invalidateOnVisualChange);
	connect(m_eventlog_filter->m_thread   , &QCheckBox::toggled, this, &DmsEventLog::invalidateOnVisualChange);
	connect(m_eventlog_filter->m_category , &QCheckBox::toggled, this, &DmsEventLog::invalidateOnVisualChange);

	m_eventlog_filter->m_opening_new_configuration      ->setChecked(dms_reg_status_flags & RSF_EventLog_ClearOnLoad);
	m_eventlog_filter->m_reopening_current_configuration->setChecked(dms_reg_status_flags & RSF_EventLog_ClearOnReLoad);

	connect(m_eventlog_filter->m_opening_new_configuration, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::writeSettingsOnToggle);
	connect(m_eventlog_filter->m_reopening_current_configuration, &QCheckBox::toggled, eventlog_model_ptr, &EventLogModel::writeSettingsOnToggle);

	// eventlog
	m_log = std::make_unique<QListView>();
	m_log->setSelectionRectVisible(true);
	m_log->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_log->setUniformItemSizes(true);
	connect(m_log->verticalScrollBar(), &QScrollBar::valueChanged, this, &DmsEventLog::onVerticalScrollbarValueChanged);

	auto vertical_layout = new QVBoxLayout(this);
	auto grid_layout = new QGridLayout(this);
	auto eventlog_toolbar = new QVBoxLayout(this);
	eventlog_toolbar->addWidget(m_copy_selected_to_clipboard.get());
	eventlog_toolbar->addWidget(m_event_filter_toggle.get());
	eventlog_toolbar->addWidget(m_clear.get());
	eventlog_toolbar->addWidget(m_scroll_to_bottom_toggle.get());
	QWidget* spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	eventlog_toolbar->addWidget(spacer);
	vertical_layout->addWidget(m_eventlog_filter.get(), 0);

	grid_layout->addWidget(m_log.get(), 0, 0);
	grid_layout->addLayout(eventlog_toolbar, 0, 1);
	vertical_layout->addLayout(grid_layout);
	vertical_layout->setContentsMargins(0, 0, 0, 0);
	setLayout(vertical_layout);
	toggleFilter(false);

	// click events
	connect(m_log.get(), &QListView::clicked, this, &DmsEventLog::onItemClicked);
	//clicked(const QModelIndex& index)
}

void DmsEventLog::copySelectedEventlogLinesToClipboard()
{
	auto eventlog_model = MainWindow::TheOne()->m_eventlog_model.get();
	auto selected_indexes = m_log->selectionModel()->selectedIndexes();

	if (selected_indexes.isEmpty())
		return;

	std::sort(selected_indexes.begin(), selected_indexes.end());

	QString new_cliboard_text = "";

	for (auto& index : selected_indexes)
	{
		auto eventlog_item = eventlog_model->data(index, Qt::DisplayRole);
		if (!eventlog_item.isValid())
			continue;

		auto item_text = eventlog_item.toString();
		if (item_text.isEmpty())
			continue;

		new_cliboard_text = new_cliboard_text + item_text + "\n";
	}
	if (new_cliboard_text.isEmpty())
		return;

	QGuiApplication::clipboard()->clear();
	QGuiApplication::clipboard()->setText(new_cliboard_text, QClipboard::Clipboard);
}

void DmsEventLog::keyPressEvent(QKeyEvent* event)
{
	if (event->matches(QKeySequence::Copy))
	{
		//return QWidget::keyPressEvent(event);
		copySelectedEventlogLinesToClipboard();
		event->accept();
		return;
	}
	return QWidget::keyPressEvent(event);
}

void DmsEventLog::onVerticalScrollbarValueChanged(int value)
{
	auto vertical_scroll_bar = m_log->verticalScrollBar();
	auto maximum = vertical_scroll_bar->maximum();
	if ((m_scroll_to_bottom && value < maximum))
		toggleScrollToBottom();

	if (value == maximum)
		toggleScrollToBottomDirectly();
}

bool DmsEventLog::isScrolledToBottom()
{
	auto vertical_scroll_bar = m_log->verticalScrollBar();
	auto current = vertical_scroll_bar->sliderPosition();
	auto maximum = vertical_scroll_bar->maximum();
	if (current == maximum || maximum == 0)
		return true;
	return false;
}

void DmsEventLog::scrollToBottomOnTimeout()
{
	auto new_current_index = m_log->model()->index(m_log->model()->rowCount()-1, 0);
	m_log->setCurrentIndex(new_current_index);
	m_log->scrollToBottom();
}

void DmsEventLog::toggleScrollToBottom()
{
	m_scroll_to_bottom = !m_scroll_to_bottom;
	m_scroll_to_bottom ? m_scroll_to_bottom_toggle->setDisabled(true) : m_scroll_to_bottom_toggle->setEnabled(true);
	if (m_scroll_to_bottom)
		scrollToBottomThrottled();
}

void DmsEventLog::toggleScrollToBottomDirectly()
{
	m_scroll_to_bottom = true;
	m_scroll_to_bottom_toggle->setDisabled(true);
	auto new_current_index = m_log->model()->index(m_log->model()->rowCount() - 1, 0);
	m_log->setCurrentIndex(new_current_index);
	m_log->scrollToBottom();
}

void DmsEventLog::scrollToBottomThrottled()
{
	if (!m_scroll_to_bottom)
		return;

	scrollToBottomOnTimeout();
}

void DmsEventLog::toggleFilter(bool toggled)
{
	auto main_window = MainWindow::TheOne();

	auto current_height = height();
	if (toggled)
		default_height = current_height + 150;
	else
		default_height = current_height - 150;

	main_window->resizeDocks({ main_window->m_eventlog_dock }, { default_height }, Qt::Vertical);

	m_eventlog_filter->setVisible(toggled);
}

void DmsEventLog::invalidateOnVisualChange()
{
	auto dms_reg_status_flags = GetRegStatusFlags();
	setSF(m_eventlog_filter->m_date_time->isChecked(), dms_reg_status_flags, RSF_EventLog_ShowDateTime);
	setSF(m_eventlog_filter->m_thread->isChecked(), dms_reg_status_flags, RSF_EventLog_ShowThreadID);
	setSF(m_eventlog_filter->m_category->isChecked(), dms_reg_status_flags, RSF_EventLog_ShowCategory);
	SetRegStatusFlags(dms_reg_status_flags);

	auto* eventlog_model = MainWindow::TheOne()->m_eventlog_model.get();
	eventlog_model->cached_reg_flags = GetRegStatusFlags();

	auto first_index = eventlog_model->index(0);
	auto last_index = eventlog_model->index(eventlog_model->m_filtered_indices.size()-1);
	m_log->model()->dataChanged(first_index, last_index);
	//m_log->update();
}

void DmsEventLog::onTextChanged(const QString& text)
{
	bool filter_button_is_disabled = false;
	bool clear_button_is_disabled = !m_text_filter_active;
	if (MainWindow::TheOne()->m_eventlog_model->m_TextFilterAsByteArray == text)
		filter_button_is_disabled = true;

	m_eventlog_filter->m_activate_text_filter->setDisabled(filter_button_is_disabled);
	m_eventlog_filter->m_clear_text_filter->setDisabled(clear_button_is_disabled);
}

void DmsEventLog::clearTextFilter()
{
	m_eventlog_filter->m_text_filter->clear();
	m_text_filter_active = false;
	m_eventlog_filter->m_clear_text_filter->setDisabled(true);
	m_eventlog_filter->m_activate_text_filter->setDisabled(true);
	MainWindow::TheOne()->m_eventlog_model->refilter();
}

void DmsEventLog::onItemClicked(const QModelIndex& index)
{
	auto row = index.row();
	const MsgData& item_data = MainWindow::TheOne()->m_eventlog_model->dataFiltered(row);

	auto log_item_message_view = std::string_view(item_data.m_Txt);
	auto link_start_index = log_item_message_view.find_first_of('[[', 0);
	if (link_start_index == std::string::npos)
		return;
	
	auto link_end_index = log_item_message_view.find_first_of(']]', link_start_index);
	if (link_end_index == std::string::npos)
		return;

	auto link = log_item_message_view.substr(link_start_index+2, link_end_index-(link_start_index+2));
	MainWindow::TheOne()->m_current_item_bar->setPath(link.data());
}

QSize DmsEventLog::sizeHint() const
{
	return QSize(0, default_height);
}

void geoDMSMessage(ClientHandle /*clientHandle*/, const MsgData* msgData)
{
	assert(msgData);
	SeverityTypeID st = msgData->m_SeverityType;

	if (st == SeverityTypeID::ST_Nothing)
	{
		PostMessage(nullptr, WM_APP + 3, 0, 0);
		return;
	}

	assert(IsMainThread());
	if (g_IsTerminating)
		return;

	auto mainWindow = MainWindow::TheOne();
	if (!mainWindow)
		return;

	auto* eventlog_model = mainWindow->m_eventlog_model.get(); assert(eventlog_model);
	eventlog_model->addText(MsgData(*msgData));
}

auto createEventLog(MainWindow* dms_main_window) -> std::unique_ptr<DmsEventLog>
{
    MainWindow::TheOne()->m_eventlog_dock = new QDockWidget(QObject::tr("EventLog"), dms_main_window);
	dms_main_window->m_eventlog_model = std::make_unique<EventLogModel>();
	auto dms_eventlog_pointer = std::make_unique<DmsEventLog>(MainWindow::TheOne()->m_eventlog_dock);
	
	dms_eventlog_pointer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	MainWindow::TheOne()->m_eventlog_dock->setWidget(dms_eventlog_pointer.get());
	MainWindow::TheOne()->m_eventlog_dock->setTitleBarWidget(new QWidget(MainWindow::TheOne()->m_eventlog_dock));
	
    dms_main_window->addDockWidget(Qt::BottomDockWidgetArea, MainWindow::TheOne()->m_eventlog_dock);

	dms_eventlog_pointer->m_log->setModel(dms_main_window->m_eventlog_model.get());
	return dms_eventlog_pointer;
}