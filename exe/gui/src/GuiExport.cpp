// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3

#include <imgui.h>
#include "GuiExport.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"

#include "dbg/DmsCatch.h"
#include "mci/ValueClass.h"
#include "utl/Environment.h"
#include "utl/mySPrintF.h"
#include "utl/splitPath.h"

#include "ShvUtils.h"

#include "ItemUpdate.h"
#include "Unit.h"

const AbstrUnit* CommonDomain(const TreeItem* item)
{
    if (IsDataItem(item))
        return AsDataItem(item)->GetAbstrDomainUnit();

    const AbstrUnit* domainCandidate = nullptr;
    bool foundSomeAttr = false;
    if (IsUnit(item))
    {
        domainCandidate = AsUnit(item);
        if (!domainCandidate->CanBeDomain())
            return nullptr;
        domainCandidate->UpdateMetaInfo();
    }

    for (auto subItem = item->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
        if (IsDataItem(subItem))
        {
            auto adu = AsDataItem(subItem)->GetAbstrDomainUnit();
            adu->UpdateMetaInfo();
            if (domainCandidate && domainCandidate->GetUnitClass() != Unit<Void>::GetStaticClass())
            {
                if (!domainCandidate->UnifyDomain(adu, "", "", UnifyMode::UM_AllowVoidRight))
                    return nullptr;
            }
            else
                domainCandidate = adu;
            foundSomeAttr = true;
        }

    return foundSomeAttr ? domainCandidate : nullptr;
}

bool CurrentItemCanBeExportedAsTable(const TreeItem* item)
{
    // category Attr: a single data item
    return CommonDomain(item) != nullptr;
}

bool CurrentItemCanBeExportedAsDatabase(const TreeItem* item)
{
    // category Table-b: container with data-items as direct sub-items that all have a compatible domain
    for (auto subItem = item->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
        if (CommonDomain(subItem))
            return true;
    return false;
}

bool CurrentItemCanBeExportedAsTableOrDatabase(const TreeItem* item)
{
    if (CurrentItemCanBeExportedAsTable(item))
        return true;
    return CurrentItemCanBeExportedAsDatabase(item);
}

bool CurrentItemCanBeExportedToVector(const TreeItem* item)
{
    if (!item)
        return false;

    // category Table
    if (CurrentItemCanBeExportedAsTableOrDatabase(item))
        return true;

    // category Database: container with tables.
    for (auto subItem = item->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
        if (CurrentItemCanBeExportedAsTable(subItem))
            return true;

    return false;
}

bool CanBeRasterDomain(const AbstrUnit* domainCandidate)
{
    assert(domainCandidate);
    assert(domainCandidate->CanBeDomain()); // precondition that it was domain of something.
    if (domainCandidate->GetNrDimensions() == 2)
        return true;
    if (RefersToMappable(domainCandidate))
    { 
        auto baseGridCandidate = GetMappedData(domainCandidate);
        if (baseGridCandidate->GetAbstrDomainUnit()->GetNrDimensions() != 2) 
            return false; // some other sort of reference ?
        auto compactedDomain = baseGridCandidate->GetAbstrValuesUnit();
        if (compactedDomain->UnifyDomain(domainCandidate))
            return true;
    }
    return false;
}

bool CurrentItemCanBeExportedToRaster(const TreeItem* item)
{
    if (!item)
        return false;

    if (!IsDataItem(item))
    {
        if (IsUnit(item))
        {
            auto domainCandidate = AsUnit(item);
            if (!domainCandidate->CanBeDomain())
                return false;
            for (auto subItem = item; subItem; subItem = item->WalkConstSubTree(subItem))
                if (IsDataItem(subItem) && domainCandidate->UnifyDomain(AsDataItem(subItem)->GetAbstrDomainUnit()))
                    if (CurrentItemCanBeExportedToRaster(subItem))
                        return true;
        }
        return false;
    }

    auto adi = AsDataItem(item); assert(adi);
    if (adi->GetValueComposition() != ValueComposition::Single)
        return false;
    auto avu = adi->GetAbstrValuesUnit(); assert(avu);
    if (!avu->GetValueType()->IsNumericOrBool())
        return false;

    auto adu = adi->GetAbstrDomainUnit(); assert(adu);

    return CanBeRasterDomain(adu);
}

void GuiExport::SetDefaultNativeDriverUsage()
{
    // TODO: expand logic, simplified implementation
    if (m_selected_driver.HasNativeVersion() && m_selected_driver.native_is_default)
        m_use_native_driver = true;
    else
        m_use_native_driver = false;
}

void GuiExport::SelectDriver(bool is_raster)
{
    if (m_selected_driver.is_raster != is_raster)
        m_selected_driver = {};

    ImGui::Text("Format:               "); ImGui::SameLine();
    


    if (ImGui::BeginCombo("##driver_selector", m_selected_driver.Caption(), ImGuiComboFlags_None))
    {
        for (auto& available_driver : m_available_drivers)
        {
            if (is_raster != available_driver.is_raster)
                continue;

            bool is_selected = m_selected_driver.shortname == available_driver.shortname;
            if (ImGui::Selectable(available_driver.Caption(), is_selected))
            {
                m_selected_driver = available_driver;
                SetDefaultNativeDriverUsage();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // native driver selection
    ImGui::BeginDisabled(m_selected_driver.IsEmpty() || !m_selected_driver.HasNativeVersion());
    ImGui::Checkbox("##native_driver", &m_use_native_driver);
    ImGui::SameLine();
    ImGui::Text("Use native driver");
    ImGui::EndDisabled();
    
}

void GuiExport::SetStorageLocation()
{            
    // foldername
    ImGui::Text("Output folder:   "); ImGui::SameLine();

    if (ImGui::InputText("##LocalDataDir", &m_folder_name, ImGuiInputTextFlags_None, InputTextCallback, nullptr))
    {
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.f, 0.f, 0.f, 0.f));

    if (ImGui::Button(ICON_RI_FOLDER_OPEN))
    {
        auto tmp_folder_name = BrowseFolder(GetLocalDataDir().c_str());//StartWindowsFileDialog(ConvertDmsFileNameAlways(GetGeoDmsRegKey("LocalDataDir")).c_str(), L"test", L"*.*");
        if (!tmp_folder_name.empty())
        {
            m_folder_name = tmp_folder_name;
        }
    }
    ImGui::PopStyleColor(3);

    // filename
    ImGui::TextUnformatted("Filename:           ");
    ImGui::SameLine();
    if (ImGui::InputText("##export_filename", &m_file_name, ImGuiInputTextFlags_EnterReturnsTrue, InputTextCallback, nullptr))
    {
    }
}

GuiExport::GuiExport()
{
    m_folder_name = GetLocalDataDir().c_str();

    m_available_drivers.emplace_back("ESRI Shapefile", "ESRI Shapefile / DBF", "shp", false, false);
    m_available_drivers.emplace_back("GPKG", "GeoPackage vector (*.gpkg)", nullptr, false, false);
    m_available_drivers.emplace_back("CSV", "Comma Separated Value (*.csv)", "csv", false, true);
    m_available_drivers.emplace_back("GML", "Geography Markup Language (*.GML)", nullptr, false, false);
    m_available_drivers.emplace_back("GeoJSON", "GeoJSON", nullptr, false, false);
    m_available_drivers.emplace_back("GTiff", "GeoTIFF File Format", "tif", true, false);
    m_available_drivers.emplace_back("netCDF", "NetCDF: Network Common Data Form", nullptr, true, false);
    m_available_drivers.emplace_back("PNG", "Portable Network Graphics (*.png)", nullptr, true, false);
    m_available_drivers.emplace_back("JPEG", "JPEG JFIF File Format (*.jpg)", nullptr, true, false);
}

void GuiExport::Update(bool* p_open, GuiState &state)
{
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_Once);
    if (!m_ExportRootItem)
    {
        // Open Dialog on current Item.
        m_ExportRootItem = state.GetCurrentItem();
        if (!m_ExportRootItem)
        {
            CloseDialog(state);
            return;
        }
        m_Caption = std::string("Export ") + m_ExportRootItem->GetFullName().c_str();
        m_EnableVector = CurrentItemCanBeExportedToVector(m_ExportRootItem);
        m_EnableRaster = CurrentItemCanBeExportedToRaster(m_ExportRootItem);
    }

    if (!ImGui::Begin(m_Caption.c_str(), p_open, ImGuiWindowFlags_None | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar))
    {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        SetKeyboardFocusToThisHwnd();

    bool enable_export = m_EnableVector || m_EnableRaster;

    if (ImGui::BeginTabBar("ExportTypes", ImGuiTabBarFlags_None))
    {
        ImGui::BeginDisabled(!m_EnableVector);
        if (ImGui::BeginTabItem("Vector"))
        {
            SelectDriver(false);
            SetStorageLocation();
            ImGui::EndTabItem();
        }
        ImGui::EndDisabled();
        
        ImGui::BeginDisabled(!m_EnableRaster);
        if (ImGui::BeginTabItem("Raster"))
        {
            SelectDriver(true);
            SetStorageLocation();
            ImGui::EndTabItem();
        }

        ImGui::EndDisabled();
        ImGui::EndTabBar();
    }

    auto options_window_pos = ImGui::GetWindowPos();
    auto options_window_content_region = ImGui::GetWindowContentRegionMax();
    auto current_cursor_pos_X = ImGui::GetCursorPosX();
    ImGui::SetCursorPos(ImVec2(current_cursor_pos_X, options_window_content_region.y - 1.5 * ImGui::GetTextLineHeight()));

    ImGui::BeginDisabled(!enable_export);
    if (ImGui::Button("Export", ImVec2(50, 1.5 * ImGui::GetTextLineHeight())))
    {
        if (DoExport())
            CloseDialog(state);
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(50, 1.5 * ImGui::GetTextLineHeight())))
        CloseDialog(state);

    ImGui::End();
}

// ========================== actual export procedures, possibly to move to a separate code unit

#include "ser/FileStreamBuff.h"
#include "stg/AbstrStoragemanager.h"
#include "TicInterface.h"

void DoExportTableToCSV(const TreeItem* tableItem, SharedStr fullFileName)
{
    std::vector<TableColumnSpec> columnSpecs;
    auto domain = CommonDomain(tableItem);
    assert(domain);

    if (IsDataItem(tableItem))
        columnSpecs.emplace_back().m_DataItem = AsDataItem(tableItem);
    else for (auto attrItem = tableItem->GetFirstSubItem(); attrItem; attrItem->GetNextItem())
        if (IsDataItem(attrItem))
        {
            auto adi = AsDataItem(attrItem);
            if (adi->GetAbstrDomainUnit()->UnifyDomain(domain)) // adi could also be a skippable parameter
                columnSpecs.emplace_back().m_DataItem = adi;
        }

    auto fout = std::make_unique<FileOutStreamBuff>(ConvertDosFileName(fullFileName), nullptr, true);

    Table_Dump(fout.get(), begin_ptr(columnSpecs), end_ptr(columnSpecs), nullptr, nullptr);
}

void DoExportTableorDatabaseToCSV(const TreeItem* tableOrDatabaseItem, SharedStr fullFileName)
{
    if (CurrentItemCanBeExportedAsTable(tableOrDatabaseItem))
    {
        DoExportTableToCSV(tableOrDatabaseItem, fullFileName);
        return;
    }
    if (!CurrentItemCanBeExportedAsDatabase(tableOrDatabaseItem))
        return;

    for (auto tableItem=tableOrDatabaseItem->GetFirstSubItem(); tableItem; tableItem->GetNextItem())
        if (CurrentItemCanBeExportedAsTable(tableItem))
        {
            auto subFileName = DelimitedConcat(fullFileName.c_str(), tableItem->GetName().c_str());
            DoExportTableToCSV(tableItem, subFileName);
        }
}

#include <filesystem>

void DoExportTable(const TreeItem* ti, SharedStr fn, TreeItem* vdc)
{
    // common domain for applicable TreeItems
    auto auCommon = CommonDomain(ti); if (!auCommon) return;

    const AbstrDataItem* adiGeometry = nullptr;
    // find geometry, if any.
    if (RefersToMappable(auCommon)) {
        adiGeometry = GetMappedData(auCommon);
        if (adiGeometry)
            if (adiGeometry->GetAbstrValuesUnit()->GetValueType()->GetNrDims() != 2)
                adiGeometry = nullptr;
    }

    if (!adiGeometry)
        while (adiGeometry = DataContainer_NextItem(ti, adiGeometry, auCommon, false))
            if (adiGeometry->GetAbstrValuesUnit()->GetValueType()->GetNrDims() == 2)
                break;
    if (!adiGeometry && auCommon != ti)
        while (adiGeometry = DataContainer_NextItem(auCommon, adiGeometry, auCommon, false))
            if (adiGeometry->GetAbstrValuesUnit()->GetValueType()->GetNrDims() == 2)
                break;

    // install the storage manager(s) and run the export
    AbstrDataItem* vdGeometry = nullptr;
    if (adiGeometry) {
        vdGeometry = DMS_CreateDataItem(vdc, "Geometry", auCommon, adiGeometry->GetAbstrValuesUnit(), adiGeometry->GetValueComposition());
        vdGeometry->SetExpr(adiGeometry->GetFullName());
    }

    const AbstrDataItem* adi = nullptr;
    while (adi = DataContainer_NextItem(ti, adi, auCommon, false))
        if (adi != adiGeometry)
        {
            // TODO: reproduce multi-level structure of DataContainer
            auto vda = CreateDataItem(vdc, UniqueName(vdc, adi->GetID()), auCommon, adi->GetAbstrValuesUnit(), adi->GetValueComposition());
            vda->SetExpr(adi->GetFullName());
        }

    if (fn.empty())
        return;

    auto filePath = std::filesystem::path(fn.c_str());
    if (vdGeometry)
    {
        auto shpPath = filePath; shpPath.replace_extension("shp");
        vdGeometry->SetStorageManager(shpPath.generic_string().c_str(), "shp", false);
    }
    filePath.replace_extension("dbf");
    vdc->SetStorageManager(filePath.generic_string().c_str(), "dbf", false);
}

static TokenID exportTableID = GetTokenID("ExportTable");
static TokenID exportDbID = GetTokenID("ExportDatabase");
static TokenID dbTableID = GetTokenID("DbTable");
static TokenID rasterID = GetTokenID("Raster");

auto DoExportTableOrDatabase(const TreeItem* tableOrDatabaseItem, bool nativeFlagged, SharedStr fn, CharPtr storageTypeName, CharPtr driverName, CharPtr options) -> const TreeItem*
{
    bool nativeShapeFile = nativeFlagged && !stricmp(storageTypeName, "ESRI Shapefile");
    auto avd = GetExportsContainer(GetDefaultDesktopContainer(tableOrDatabaseItem));
    TreeItem* vdc = nullptr;

    if (CurrentItemCanBeExportedAsTable(tableOrDatabaseItem))
    {
        vdc = avd->CreateItem(UniqueName(avd, exportTableID));
        DoExportTable(tableOrDatabaseItem, nativeShapeFile ? fn : SharedStr(), vdc);
    }
    else
    {
        if (!CurrentItemCanBeExportedAsDatabase(tableOrDatabaseItem))
            return nullptr;

        vdc = avd->CreateItem(UniqueName(avd, exportDbID));
        for (auto tableItem = tableOrDatabaseItem->GetFirstSubItem(); tableItem; tableItem = tableItem->GetNextItem())
            if (CurrentItemCanBeExportedAsTable(tableItem))
            {
                SharedStr subFileName;
                if (nativeShapeFile)
                    subFileName = DelimitedConcat(fn.c_str(), tableItem->GetName().c_str());
                auto subContainer = vdc->CreateItem(UniqueName(vdc, dbTableID));

                DoExportTable(tableItem, subFileName, subContainer);
            }
    }

    if (!nativeShapeFile)
        vdc->SetStorageManager(fn.c_str(), storageTypeName, false, driverName, options);

    return vdc;
}

auto DoExportRasterOrMatrixData(const TreeItem* rasterItemOrDomain, bool nativeFlagged, SharedStr fn, CharPtr storageTypeName, CharPtr driverName, CharPtr options) -> const TreeItem*
{
    if (!CurrentItemCanBeExportedToRaster(rasterItemOrDomain))
        return nullptr;

    if (nativeFlagged && !stricmp(storageTypeName, "GTiff"))
        storageTypeName = "tif";

    auto avd = GetExportsContainer(GetDefaultDesktopContainer(rasterItemOrDomain));
    auto subContainer = avd->CreateItem(UniqueName(avd, rasterID));

    auto adu = IsUnit(rasterItemOrDomain) ? AsUnit(rasterItemOrDomain) : AsDataItem(rasterItemOrDomain)->GetAbstrDomainUnit();
    assert(CanBeRasterDomain(adu));
    const AbstrDataItem* baseGrid = nullptr;
    if (adu->GetNrDimensions() != 2)
    {
        assert(RefersToMappable(adu));
        baseGrid = GetMappedData(adu);
    }
    auto rasterDomain = baseGrid ? baseGrid->GetAbstrDomainUnit() : adu;

    auto storeData = [=](const AbstrDataItem* adi)
    {
        assert(adi->GetValueComposition() == ValueComposition::Single);
        auto vda = CreateDataItem(subContainer, UniqueName(subContainer, adi->GetID()), rasterDomain, adi->GetAbstrValuesUnit(), adi->GetValueComposition());
        auto expr = adi->GetFullName();
        if (baseGrid)
            expr = mySSPrintF("%s[%s]", expr.c_str(), baseGrid->GetFullName().c_str());
        vda->SetExpr(expr);
    };

    if (IsDataItem(rasterItemOrDomain))
        storeData(AsDataItem(rasterItemOrDomain));
    else if (IsUnit(rasterItemOrDomain))
    {
        for (auto subItem = rasterItemOrDomain; subItem; subItem = rasterItemOrDomain->WalkConstSubTree(subItem))
            if (IsDataItem(subItem))
            {
                auto adi = AsDataItem(subItem);
                if (!adu->UnifyDomain(adi->GetAbstrDomainUnit()))
                    continue;
                if (adi->GetValueComposition() != ValueComposition::Single)
                    continue;
                if (!adi->GetAbstrValuesUnit()->GetValueType()->IsNumericOrBool())
                    continue;
                storeData(adi);
            }
    }

    subContainer->SetStorageManager(fn.c_str(), storageTypeName, false, driverName, options);

    return subContainer;
}

bool GuiExport::DoExport()
{
    DMS_CALL_BEGIN

    auto item = m_ExportRootItem;
    auto selectedDriver = m_selected_driver;
    auto folderName = m_folder_name;
    auto fileName = m_file_name;

    SharedStr ffName = DelimitedConcat(folderName.c_str(), fileName.c_str());
    CharPtr driverName = nullptr;
    CharPtr storageTypeName = nullptr;

    if (m_use_native_driver && selectedDriver.HasNativeVersion())
    {
        storageTypeName = selectedDriver.nativeName;
        if (!stricmp(storageTypeName, "CSV"))
        {
            DoExportTableToCSV(item, ffName);
            return true;
        }
    }
    else
    {
        driverName = selectedDriver.shortname;
        if (selectedDriver.is_raster)
            storageTypeName = "gdalwrite.grid";
        else
            storageTypeName = "gdalwrite.vect";
    }

    const TreeItem* exportConfig = nullptr;
    if (selectedDriver.is_raster)
        exportConfig = DoExportRasterOrMatrixData(item, m_use_native_driver, ffName, storageTypeName, driverName, "");
    else
        exportConfig = DoExportTableOrDatabase(item, m_use_native_driver, ffName, storageTypeName, driverName, "");

    if (exportConfig)
    {
        Tree_Update(exportConfig, "Export");
        return true;
    }

    DMS_CALL_END

    return false;
}

