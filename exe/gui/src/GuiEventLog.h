#pragma once
#include <imgui.h>
#include "GuiBase.h"

struct OptionsEventLog
{
    bool ShowMessageTypeMinorTrace = true;
    bool ShowMessageTypeMajorTrace = true;
    bool ShowMessageTypeWarning = true;
    bool ShowMessageTypeError = true;
    bool ShowMessageTypeNothing = true;
};

struct EventLogItem
{
    SeverityTypeID m_Severity_type = SeverityTypeID::ST_Nothing;
    std::string    m_Text = "";
    std::string    m_Link = "";
};

auto ItemPassesEventFilter(EventLogItem* item, OptionsEventLog* options) -> bool;
auto ItemPassesTextFilter(EventLogItem* item, std::string_view filter_text) -> bool;
auto ItemPassesFilter(EventLogItem* item, OptionsEventLog* options, std::string_view filter_text) -> bool;

class GuiEventLog : GuiBaseComponent
{
public:
    GuiEventLog();
    ~GuiEventLog();
    auto ShowEventLogOptionsWindow(bool* p_open) -> void;
    auto static GeoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg) -> void;
    auto static GeoDMSExceptionMessage(CharPtr msg) -> void;
    auto Update(bool* p_open, GuiState& state) -> void;

private:
    auto OnItemClick(GuiState& state, EventLogItem* item) -> void;
    auto GetItem(size_t index) -> EventLogItem*;
    auto DrawItem(EventLogItem* item) -> void;
    auto ConvertSeverityTypeIDToColor(SeverityTypeID st) -> ImColor;
    auto ClearLog() -> void;
    auto Refilter() -> void;
    auto static AddLog(SeverityTypeID severity_type, std::string original_message) -> void;

    static std::vector<EventLogItem> m_Items;
    static std::vector<UInt64>       m_FilteredItemIndices;
    static std::string               m_FilterText;
    static OptionsEventLog           m_FilterEvents;

    bool AutoScroll;
    bool ScrollToBottom;
};