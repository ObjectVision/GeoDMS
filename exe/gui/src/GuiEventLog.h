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

bool ItemPassesEventFilter(EventLogItem* item, OptionsEventLog* options);
bool ItemPassesTextFilter(EventLogItem* item, std::string_view filter_text);
bool ItemPassesFilter(EventLogItem* item, OptionsEventLog* options, std::string_view filter_text);

/*struct StatusMessageViewport
{
    ImGuiViewport* vp;
    ImVec2 cursor_pos;
    ImVec2 display_pos;
    ImVec2 display_size;
    ImVec2 frame_buffer_scale = { 1.0f, 1.0f };
};*/

struct ContentRegion
{
    ImVec2 cursor;
    ImVec2 pos;
    ImVec2 size;
    size_t index;
};

struct EventlogDirectUpdateInformation
{
    ImGuiViewport* viewport;
    std::chrono::system_clock::time_point time_since_last_update;
    ContentRegion log;
    ContentRegion status;
};

class GuiEventLog
{
public:
    GuiEventLog();
    ~GuiEventLog();
    void ShowEventLogOptionsWindow(bool* p_open);
    void static GeoDMSMessage(ClientHandle clientHandle, SeverityTypeID st, CharPtr msg);
    void static GeoDMSExceptionMessage(CharPtr msg);
    void Update(bool* p_open, GuiState& state);
    void DirectUpdate(GuiState& state);
    static void GeoDMSContextMessage(ClientHandle clientHandle, CharPtr msg);
    //static StatusMessageViewport m_smvp;

private:

    void OnItemClick(GuiState& state, EventLogItem* item);
    auto GetItem(size_t index) -> EventLogItem*;
    void DrawItem(EventLogItem* item);
    auto ConvertSeverityTypeIDToColor(SeverityTypeID st) -> ImColor;
    void ClearLog();
    void Refilter();
    void AddLog(SeverityTypeID severity_type, std::string_view original_message);

    static std::vector<EventLogItem> m_Items;
    static std::vector<UInt64>       m_FilteredItemIndices;
    static std::string               m_FilterText;
    static OptionsEventLog           m_FilterEvents;
    EventlogDirectUpdateInformation m_direct_update_information;

    bool is_docking_initialized = false;
    bool AutoScroll;
    bool ScrollToBottom;
};