#include <imgui.h>
#include <format>
#include <sstream>

#include "GuiTableview.h"

#include "TicInterface.h"
#include "StateChangeNotification.h"
#include "AbstrUnit.h"
#include "AbstrDataItem.h"
#include "UnitProcessor.h"
#include "DataArray.h"
#include "DataLocks.h"
#include "dbg/DmsCatch.h"

/*
if (test_drag_and_drop && ImGui::BeginDragDropSource())
{
    ImGui::SetDragDropPayload("_TREENODE", NULL, 0);
    ImGui::Text("This is a drag and drop source");
    ImGui::EndDragDropSource();
}

if (ImGui::BeginDragDropTarget())
{
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
        memcpy((float*)&saved_palette[n], payload->Data, sizeof(float) * 3);
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
        memcpy((float*)&saved_palette[n], payload->Data, sizeof(float) * 4);
    ImGui::EndDragDropTarget();
}
*/

void ClearReferences()
{

}

// Make the UI compact because there are so many fields
static void PushStyleCompact()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, (float)(int)(style.FramePadding.y * 0.60f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, (float)(int)(style.ItemSpacing.y * 0.60f)));
}

static void PopStyleCompact()
{
    ImGui::PopStyleVar(2);
}

void GuiTableView::ProcessEvent(GuiEvents event, TreeItem* currentItem)
{
    if (event == GuiEvents::UpdateCurrentItem) // update current item
    {
        m_ActiveItems.clear();
        if (IsDataItem(currentItem))
        {
            auto adi = AsDataItem(currentItem);
            m_ActiveItems.add(adi);
            m_ActiveItems.add(currentItem);
        }

        m_UpdateData = true;
    }

    if (event == GuiEvents::UpdateCurrentAndCompatibleSubItems)
    {
        m_ActiveItems.clear();
        if (IsDataItem(currentItem))
        {
            if (!m_ActiveItems.contains(currentItem))
            {
                auto adi = AsDataItem(currentItem);
                m_ActiveItems.add(adi);
                m_ActiveItems.add(currentItem);
            }
        }

        if (IsUnit(currentItem))
        {
            auto au = AsUnit(currentItem);
            auto subItem = currentItem->_GetFirstSubItem();
            while (subItem)
            {
                if (IsDataItem(subItem))
                {
                    auto adi = AsDataItem(subItem);
                    if (adi->GetAbstrDomainUnit()->UnifyDomain(au, UnifyMode::UM_Throw))
                    {
                        if (!m_ActiveItems.contains(subItem))
                        {
                            m_ActiveItems.add(adi);
                            m_ActiveItems.add(subItem);
                        }
                    }       
                }
                subItem = subItem->GetNextItem();
            }
        }
    }
    m_UpdateData = true;
}

void GuiTableView::ClearReferences()
{
    m_ActiveItems.clear();
}

SizeT GuiTableView::GetNumberOfTableColumns()
{
    return m_ActiveItems.size();
}

void GuiTableView::SetupTableHeadersRow()
{
    for (auto item : m_ActiveItems.get())
        ImGui::TableSetupColumn(item->GetName().c_str());

    ImGui::TableHeadersRow();
}

std::vector<std::string> GetDataItemValueRangeAsString(const AbstrDataItem* adi, SizeT count, tile_id t)
{
    std::vector<std::string> res;
    const AbstrDataObject* ado = adi->GetRefObj();
    auto lock = DMS_DataReadLock_Create(adi, false);
    static std::vector<char> buf(2000);
    for (SizeT i = 0; i < count; i++)
    {
        ado->AsCharArray(i, &buf[0], 2000, *lock, FormattingFlags::None);
        res.push_back(&buf[0]);
    }

    DMS_DataReadLock_Release(lock);

    return res;
}

//tile_id GetTileID(){} // TODO: implement tile based parsing.

void GuiTableView::PopulateDataStringBuffer(SizeT count)
{
    m_DataStringBuffer.clear();
    for (auto item : m_ActiveItems.get())
    {
        auto status = DMS_TreeItem_GetProgressState(item.get_ptr());
        if (status == NotificationCode::NC2_Committed || status == NotificationCode::NC2_DataReady)
        {
            dms_assert(IsDataItem(item.get_ptr()));
            auto adi = AsDataItem(item.get_ptr());
            m_DataStringBuffer.push_back(GetDataItemValueRangeAsString(adi, count, 0));


            //if (AsDataItem(item)->IsFailed(FR_Data))
            //    auto failstr = AsDataItem(item)->GetFailReason().Why();
        }
    }
    int i = 0;
}

SizeT GuiTableView::GetCountFromAnyCalculatedActiveItem()
{
    SizeT count = -1;
    for (auto item : m_ActiveItems.get())
    {
        auto status = DMS_TreeItem_GetProgressState(item.get_ptr());
        if (status == NotificationCode::NC2_Committed || status == NotificationCode::NC2_DataReady)  
        {
            auto adi = AsDataItem(item.get_ptr());
            auto lock = DMS_DataReadLock_Create(adi, false);
            auto adu = adi->GetAbstrDomainUnit();
            count = adu->GetCount();
            DMS_DataReadLock_Release(lock);
            break;
        }
    }
    return count;
}

void GuiTableView::Update(bool* p_open, TreeItem * &currentItem)
{
    ImGui::Begin("Tableview", p_open, NULL);
    if (!m_State.TableViewIsActive)
    {
        ImGui::End();
        return;
    }

    while (m_State.TableViewEvents.HasEvents()) // handle events
    {
        auto event = m_State.TableViewEvents.Pop();
        ProcessEvent(event, currentItem);
    }

    // Using those as a base value to create width/height that are factor of the size of our font
    const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
    static ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;
    static int freeze_cols = 0;
    static int freeze_rows = 1;
    static SizeT item_count = 0;

    auto numColumns = GetNumberOfTableColumns();

    PushStyleCompact();
    if (ImGui::BeginTable("table_scrollx", numColumns, flags))
    {

        if (m_ActiveItems.size())
            item_count = GetCountFromAnyCalculatedActiveItem();

        if (item_count != -1)
        {
            ImGuiListClipper clipper;
            clipper.Begin(item_count); // TODO: make size of domain
            ImGui::TableSetupScrollFreeze(freeze_cols, freeze_rows);

            // setup columns
            SetupTableHeadersRow();

            // draw
            int row = 0;
            while (clipper.Step())
            {
                if (clipper.DisplayStart && m_UpdateData)
                {
                    PopulateDataStringBuffer(item_count);
                    m_UpdateData = false;
                }

                for (row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                {
                    ImGui::TableNextRow();
                    auto ind = ImGui::TableGetRowIndex();
                    for (int column = 0; column < numColumns; column++)
                    {
                        ImGui::TableSetColumnIndex(column);
                        if (m_DataStringBuffer.size() <= column || m_DataStringBuffer.at(column).size() <= row) // buffer not up to date
                            ImGui::Text("");
                        else
                            ImGui::Text(m_DataStringBuffer[column][row].c_str());
                    }

                }
            }
        }
        
        ImGui::EndTable();
    }
    PopStyleCompact();
    ImGui::End();
}
