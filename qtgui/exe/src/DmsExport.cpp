// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////

// The export dialog of the GeoDMS GUI: raster and vector export of tree
// items through the storage managers.

#include "DmsExport.h"
#include "DmsMainWindow.h"
#include "DmsTreeView.h"
#include "ExportInfo.h"
#include "utl/Environment.h"
#include "utl/FileSystem.h"
#include "Parallel.h"
#include "ptr/SharedStr.h"
#include "TicInterface.h"
#include "ShvDllInterface.h"

#include <filesystem>

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "Unit.h"
#include "UnitClass.h"


#include "ItemUpdate.h"
#include "dbg/DmsCatch.h"
#include "ptr/InterestHolders.h"

#include "mci/ValueClass.h"

#include <QCheckBox>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QFileDialog>
#include <QLineEdit>
#include <QGridLayout>
#include <QTabWidget>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>

#include "ser/FileStreamBuff.h"
#include "stg/AbstrStoragemanager.h"
#include "GridStorageManager.h"
#include "utl/splitPath.h"

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
    {
        if (subItem->WasFailed())
            continue;

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
    {
        if (subItem->WasFailed())
            continue;

        if (CommonDomain(subItem))
            return true;
    }
    return false;
}

bool CurrentItemCanBeExportedAsTableOrDatabase(const TreeItem* item)
{
    if (item && item->InTemplate()) // template-internal items have unresolved (null) units: not exportable
        return false;
    if (CurrentItemCanBeExportedAsTable(item))
        return true;
    return CurrentItemCanBeExportedAsDatabase(item);
}

bool isGeometryItem(const TreeItem* item)
{
    if (!IsDataItem(item))
        return false;
    auto adi = AsDataItem(item);
    auto avu = adi->GetAbstrValuesUnit();
    if (!avu) // unresolved-generic (in-template) values unit: not a geometry item
        return false;
    auto vc = adi->GetValueComposition();
    auto vci = avu->GetValueType()->GetValueClassID();
    return vc <= ValueComposition::Sequence && (vci >= ValueClassID::VT_SPoint && vci < ValueClassID::VT_FirstAfterPolygon);
}

bool isItemMappable(const TreeItem* item)
{
    auto vsflags = SHV_GetViewStyleFlags(item);
    return vsflags & ViewStyleFlags::vsfMapView;
}

bool isItemOrItsSubItemsMappable(const TreeItem* item)
{
    if (isItemMappable(item))
        return true;

    auto next_sub_item = item->GetFirstSubItem();
    while (next_sub_item)
    {
        ViewStyleFlags vsflags = SHV_GetViewStyleFlags(next_sub_item); //TODO: may throw
        if (vsflags & ViewStyleFlags::vsfMapView)
            return true;

        next_sub_item = next_sub_item->GetNextItem();
    }

    return false;
}

void DoExportTableToCSV(const TreeItem* tableItem, SharedStr fullFileName)
{
    std::vector<TableColumnSpec> columnSpecs;
    auto domain = CommonDomain(tableItem);
    assert(domain);

    if (IsDataItem(tableItem))
        columnSpecs.emplace_back().m_DataItem = AsDataItem(tableItem);
    else
    {
        for (auto attrItem = tableItem->GetFirstSubItem(); attrItem; attrItem = attrItem->GetNextItem())
        {
            if (IsDataItem(attrItem))
            {
                auto adi = AsDataItem(attrItem);
                if (adi->GetAbstrDomainUnit()->UnifyDomain(domain)) // adi could also be a skippable parameter
                    columnSpecs.emplace_back().m_DataItem = adi;
            }
        }
    }

    auto fout = std::make_unique<FileOutStreamBuff>(ConvertDosFileName(fullFileName), true);

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

    for (auto tableItem = tableOrDatabaseItem->GetFirstSubItem(); tableItem; tableItem->GetNextItem())
        if (CurrentItemCanBeExportedAsTable(tableItem))
        {
            auto subFileName = DelimitedConcat(fullFileName.c_str(), tableItem->GetName().c_str());
            DoExportTableToCSV(tableItem, subFileName);
        }
}

void DoExportTable(const TreeItem* ti, SharedStr fn, TreeItem* vdc)
{
    // common domain for applicable TreeItems
    auto auCommon = CommonDomain(ti); if (!auCommon) return;

    const AbstrDataItem* adiGeometry = nullptr;
    // find geometry, if any.
    if (isGeometryItem(ti))
        adiGeometry = AsDataItem(ti);

    if (!adiGeometry && RefersToMappable(auCommon)) {
        adiGeometry = GetMappedData(auCommon);
        if (adiGeometry)
            if (adiGeometry->GetAbstrValuesUnit()->GetValueType()->GetNrDims() != 2)
                adiGeometry = nullptr;
    }

    if (!adiGeometry)
        while ((adiGeometry = DataContainer_NextItem(ti, adiGeometry, auCommon, false)))
            if (adiGeometry->GetAbstrValuesUnit()->GetValueType()->GetNrDims() == 2)
                break;
    if (!adiGeometry && auCommon != ti)
        while ((adiGeometry = DataContainer_NextItem(auCommon, adiGeometry, auCommon, false)))
            if (adiGeometry->GetAbstrValuesUnit()->GetValueType()->GetNrDims() == 2)
                break;

    // install the storage manager(s) and run the export
    AbstrDataItem* vdGeometry = nullptr;
    if (adiGeometry) {
        vdGeometry = DMS_CreateDataItem(vdc, "Geometry", auCommon, adiGeometry->GetAbstrValuesUnit(), adiGeometry->GetValueComposition());
        vdGeometry->SetExpr(adiGeometry->GetFullName());
    }

    const AbstrDataItem* adi = nullptr;
    while ((adi = DataContainer_NextItem(ti, adi, auCommon, false)))
        if (adi != adiGeometry)
        {
            // TODO: reproduce multi-level structure of DataContainer
            // TODO: cleanup
            auto value_composition = adi->GetValueComposition();
            auto values_unit = adi->GetAbstrValuesUnit();
            auto vci = values_unit->GetValueType()->GetValueClassID();
            auto vc = adi->GetValueComposition();
            bool is_geometry = vc <= ValueComposition::Sequence && (vci >= ValueClassID::VT_SPoint && vci < ValueClassID::VT_FirstAfterPolygon);

            if (is_geometry)
                continue;
            auto vda = CreateDataItem(vdc, UniqueName(vdc, adi->GetID()), auCommon, values_unit, value_composition);
            vda->SetExpr(adi->GetFullName());
        }

    if (fn.empty())
        return;

    // fn is UTF-8; std::filesystem::path(const char*) on MSVC interprets it
    // as the active code page (CP1252), garbling non-ASCII paths. Build the
    // path from a wchar_t* via Utf8_2_wchar so the bytes are decoded as UTF-8.
    // See #1101 for the original symptom of this antipattern.
#if defined(_MSC_VER)
    auto filePath = std::filesystem::path(Utf8_2_wchar(fn.c_str()).get());
#else
    auto filePath = std::filesystem::path(fn.c_str());
#endif
    if (vdGeometry)
    {
        auto shpPath = filePath; shpPath.replace_extension("shp");
        vdGeometry->SetStorageManager(shpPath.generic_string().c_str(), "shp", StorageReadOnlySetting::Default);
    }
    filePath.replace_extension("dbf");
    vdc->SetStorageManager(filePath.generic_string().c_str(), "dbf", StorageReadOnlySetting::Default);
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
        vdc = avd->CreateItem(UniqueName(avd, exportTableID)).get(); // owned by avd (parent)
        DoExportTable(tableOrDatabaseItem, nativeShapeFile ? fn : SharedStr(), vdc);
    }
    else
    {
        if (!CurrentItemCanBeExportedAsDatabase(tableOrDatabaseItem))
            return nullptr;

        vdc = avd->CreateItem(UniqueName(avd, exportDbID)).get(); // owned by avd (parent)
        for (auto tableItem = tableOrDatabaseItem->GetFirstSubItem(); tableItem; tableItem = tableItem->GetNextItem())
            if (CurrentItemCanBeExportedAsTable(tableItem))
            {
                SharedStr subFileName;
                if (nativeShapeFile)
                    subFileName = DelimitedConcat(fn.c_str(), tableItem->GetName().c_str());
                auto subContainer = vdc->CreateItem(UniqueName(vdc, dbTableID)).get(); // owned by vdc (parent)

                DoExportTable(tableItem, subFileName, subContainer);
            }
    }

    if (!nativeShapeFile)
        vdc->SetStorageManager(fn.c_str(), storageTypeName, StorageReadOnlySetting::Default, driverName, options);

    return vdc;
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

auto DoExportRasterOrMatrixData(const TreeItem* rasterItemOrDomain, bool nativeFlagged, SharedStr fn, CharPtr storageTypeName, CharPtr driverName, CharPtr options) -> const TreeItem*
{
    if (!currentItemCanBeExportedToRaster(rasterItemOrDomain))
        return nullptr;

    auto avd = GetExportsContainer(GetDefaultDesktopContainer(rasterItemOrDomain));
    TokenID t_gdal_grid_driver_options = GetTokenID_mt("GDAL_Options");
    auto gdal_driver_options = CreateDataItem(avd, UniqueName(avd, t_gdal_grid_driver_options), Unit<Void>::GetStaticClass()->CreateDefault(), Unit<SharedStr>::GetStaticClass()->CreateDefault(), ValueComposition::Void);
    SharedStr gdal_driver_options_expr("'TFW=YES'");// mySSPrintF("{}[{}]", expr.c_str(), baseGrid->GetFullName().c_str());
    gdal_driver_options->SetExpr(gdal_driver_options_expr);

    auto adu = IsUnit(rasterItemOrDomain) ? AsUnit(rasterItemOrDomain) : AsDataItem(rasterItemOrDomain)->GetAbstrDomainUnit();
    assert(CanBeRasterDomain(adu));
    const AbstrDataItem* baseGrid = nullptr;
    if (adu->GetNrDimensions() != 2)
    {
        assert(RefersToMappable(adu));
        baseGrid = GetMappedData(adu);
    }
    auto rasterDomain = baseGrid ? baseGrid->GetAbstrDomainUnit() : adu;

    auto storeData = [=](const AbstrDataItem* adi) -> AbstrDataItem*
    {

        assert(adi->GetValueComposition() == ValueComposition::Single);
        auto vda = CreateDataItem(avd, UniqueName(avd, adi->GetID()), rasterDomain, adi->GetAbstrValuesUnit(), adi->GetValueComposition());
        auto expr = adi->GetFullName();
        if (baseGrid)
            expr = mySSPrintF("{}[{}]", expr.c_str(), baseGrid->GetFullName().c_str());
        vda->SetExpr(expr);
        return vda.get(); // owned by avd (parent)
    };

    AbstrDataItem* export_raster = nullptr;
    if (IsDataItem(rasterItemOrDomain))
        export_raster = storeData(AsDataItem(rasterItemOrDomain));
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
                export_raster = storeData(adi);
            }
    }

    if (export_raster)
        export_raster->SetStorageManager(fn.c_str(), storageTypeName, StorageReadOnlySetting::Default, driverName, options);

    return export_raster;
}

bool currentItemCanBeExportedToVector(const TreeItem* item)
{
    if (!item)
        return false;

    if (item->IsFailed())
        return false;

    if (item->InTemplate()) // template-internal item: unresolved (null) units, not exportable
        return false;

    if (IsUnit(item)) // exclude unit that is grid domain
    {
        if (IsGridDomain(AsUnit(item)))
            return false;
    }

    if (IsDataItem(item)) // exclude dataitem with grid domain
    {
        if (HasGridDomain(AsDataItem(item)))
            return false;
    }

    // category Table
    if (CurrentItemCanBeExportedAsTableOrDatabase(item))
        return true;

    // category Database: container with tables.
    for (auto subItem = item->GetFirstSubItem(); subItem; subItem = subItem->GetNextItem())
    {
        if (subItem->WasFailed())
            continue;

        if (CurrentItemCanBeExportedAsTable(subItem))
            return true;
    }
    return false;
}

// GDAL raster drivers (e.g. GTiff) cannot write sub-byte value types (Bool,
// UInt2, UInt4): the write attempt fails with "driver ... does not support
// writing of values type ...", producing no output file. Only the native
// GeoDMS tiff driver writes these. So for such items the "Use native driver"
// option must be forced on. See issue #1141.
static bool rasterItemRequiresNativeDriver(const TreeItem* item)
{
    if (!item || !IsDataItem(item))
        return false;
    auto avu = AsDataItem(item)->GetAbstrValuesUnit();
    if (!avu)
        return false;
    auto vt = avu->GetValueType();
    return vt && vt->IsSubByteElem();
}

bool currentItemCanBeExportedToRaster(const TreeItem* item)
{
    if (!item)
        return false;

    if (item->IsFailed())
        return false;

    if (item->InTemplate()) // template-internal item: unresolved (null) units, not exportable
        return false;

    if (!IsDataItem(item))
    {
        /*if (IsUnit(item))
        {
            auto domainCandidate = AsUnit(item);
            if (!domainCandidate->CanBeDomain())
                return false;
            for (auto subItem = item; subItem; subItem = item->WalkConstSubTree(subItem))
                if (IsDataItem(subItem) && domainCandidate->UnifyDomain(AsDataItem(subItem)->GetAbstrDomainUnit()))
                    if (currentItemCanBeExportedToRaster(subItem))
                        return true;
        }*/
        return false;
    }

    auto adi = AsDataItem(item); assert(adi);
    if (adi->GetValueComposition() != ValueComposition::Single)
        return false;
    // values/domain unit can be null for an in-template generic item (unresolved type variable);
    // a raw deref here is an SEH access violation that the caller's catch(...) cannot catch under
    // /EHsc, so null-check rather than rely on the (Release-noop) asserts.
    auto avu = adi->GetAbstrValuesUnit();
    if (!avu || !avu->GetValueType()->IsNumericOrBool())
        return false;

    auto adu = adi->GetAbstrDomainUnit();
    if (!adu)
        return false;

    return CanBeRasterDomain(adu);
}

auto getAvailableDrivers() -> std::vector<gdal_driver_id>
{
    std::vector<gdal_driver_id> available_drivers;
    available_drivers.emplace_back("CSV", "Comma Separated Value (*.csv)", "csv", std::vector<CharPtr>{".csv"}, driver_characteristics::native_is_default | driver_characteristics::tableset_is_folder);
    available_drivers.emplace_back("ESRI Shapefile", "ESRI Shapefile", "shp", std::vector<CharPtr>{".shp", ".shx", ".dbf", ".prj"}, driver_characteristics::disable_with_no_geometry | driver_characteristics::tableset_is_folder);
    available_drivers.emplace_back("GPKG", "GeoPackage vector (*.gpkg)", nullptr, std::vector<CharPtr>{".gpkg"}, driver_characteristics::disable_with_no_geometry);
    available_drivers.emplace_back("GML", "Geography Markup Language (*.GML)", nullptr, std::vector<CharPtr>{".gml"}, driver_characteristics::disable_with_no_geometry);
    available_drivers.emplace_back("GML", "XML (*.xml)", nullptr, std::vector<CharPtr>{".xml", ".xsd"}, driver_characteristics::none);
    available_drivers.emplace_back("GeoJSON", "GeoJSON", nullptr, std::vector<CharPtr>{".json"}, driver_characteristics::disable_with_no_geometry);
    available_drivers.emplace_back("ESRI Shapefile", "DBF", "DBF", std::vector<CharPtr>{".dbf" }, driver_characteristics::tableset_is_folder | driver_characteristics::disable_with_geometry);
    available_drivers.emplace_back("GTiff", "GeoTIFF File Format", "tif", std::vector<CharPtr>{".tif", ".tfw"}, driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
    //available_drivers.emplace_back("BMP", "Microsoft Windows Device Independent Bitmap", nullptr, std::vector<CharPtr>{".bmp"}, driver_characteristics::is_raster);
    //available_drivers.emplace_back("netCDF", "NetCDF: Network Common Data Form", nullptr, std::vector<CharPtr>{".cdf"}, driver_characteristics::is_raster);
    //available_drivers.emplace_back("PNG", "Portable Network Graphics (*.png)", nullptr, ".png", driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
    //available_drivers.emplace_back("JPEG", "JPEG JFIF File Format (*.jpg)", nullptr, ".jpg", driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
    return available_drivers;
}

void ExportTab::setNativeDriverCheckbox()
{
    auto driver = m_available_drivers.at(m_driver_selection->currentIndex());
    auto driver_has_native_version = driver.HasNativeVersion();
    m_native_driver_checkbox->setChecked(driver_has_native_version);
    // #1141: GDAL cannot write sub-byte raster types (Bool/UInt2/UInt4); force the native driver.
    bool require_native = m_is_raster && driver_has_native_version
        && rasterItemRequiresNativeDriver(m_export_window->exportItem());
    m_native_driver_checkbox->setEnabled(driver_has_native_version && !require_native);
}

void ExportTab::repopulateDriverSelection()
{
    m_available_drivers.clear();
    QStringList driver_namesnames;
    auto available_drivers = getAvailableDrivers();
    for (auto& driver : available_drivers)
    {
        if (m_is_raster && (driver.m_driver_characteristics & driver_characteristics::is_raster)) // raster driver
        {
            m_available_drivers.push_back(driver);
            m_driver_selection->addItem(driver.Caption());
        }
        if (!m_is_raster && !(driver.m_driver_characteristics & driver_characteristics::is_raster)) // vector driver
        {
            m_available_drivers.push_back(driver);
            m_driver_selection->addItem(driver.Caption());
        }
    }
}

void ExportTab::onComboBoxItemActivate(int index)
{
    auto driver = m_available_drivers.at(index);
    setNativeDriverCheckbox();
    onFilenameEntryTextChanged(QString());
}

void ExportTab::setFoldernameUsingFileDialog()
{
    auto new_export_folder = QFileDialog::getExistingDirectory(this, tr("Open LocalData Directory"), m_foldername_entry->text(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (new_export_folder.isEmpty())
        return;
    
    m_foldername_entry->setText(new_export_folder);
}

auto ExportTab::createFinalFileNameText() -> QString
{
    auto driver = m_available_drivers.at(m_driver_selection->currentIndex());
    QString final_filename_text = "";
    auto new_filename = m_foldername_entry->text() + "/" + m_filename_entry->text();
    for (auto ext : driver.exts)
    {
        final_filename_text = final_filename_text + new_filename + ext + "\n\n";
    }
    return final_filename_text;
}

void ExportTab::onFilenameEntryTextChanged(const QString& new_filename)
{
    auto driver = m_available_drivers.at(m_driver_selection->currentIndex());
    auto markdown_text = createFinalFileNameText();
    m_final_filename->setMarkdown(markdown_text);
}

ExportTab::ExportTab(bool is_raster, DmsExportWindow* exportWindow)
	: QWidget(nullptr)
{
	m_is_raster = is_raster;
	m_export_window = exportWindow;

    // tab form
    auto grid_layout_box = new QGridLayout(this);
    auto format_label = new QLabel("Format", this);


    auto filename = new QLabel("Filename (no extension):", this);
    auto foldername = new QLabel("Foldername:", this);

    m_foldername_entry = new QLineEdit(this);
    m_filename_entry = new QLineEdit(this);

    connect(m_foldername_entry, &QLineEdit::textChanged, this, &ExportTab::onFilenameEntryTextChanged);
    connect(m_foldername_entry, &QLineEdit::textChanged, exportWindow, &DmsExportWindow::resetExportDialog);

    connect(m_filename_entry, &QLineEdit::textChanged, this, &ExportTab::onFilenameEntryTextChanged);
    connect(m_filename_entry, &QLineEdit::textChanged, exportWindow, &DmsExportWindow::resetExportDialog);

    auto folder_browser_button = new QPushButton(QIcon(":/res/images/DP_explore.bmp"), "", this);
    connect(folder_browser_button, &QPushButton::clicked, this, &ExportTab::setFoldernameUsingFileDialog);
    grid_layout_box->addWidget(foldername, 0, 0);
    grid_layout_box->addWidget(m_foldername_entry, 0, 1);
    grid_layout_box->addWidget(folder_browser_button, 0, 2);

    grid_layout_box->addWidget(filename, 1, 0);
    grid_layout_box->addWidget(m_filename_entry, 1, 1);


    m_driver_selection = new QComboBox(this);
    m_driver_selection->setStyleSheet("QComboBox { background-color: rgb(255, 255, 255); }");
    repopulateDriverSelection();
    connect(m_driver_selection, &QComboBox::currentIndexChanged, this, &ExportTab::onComboBoxItemActivate);
    connect(m_driver_selection, &QComboBox::currentIndexChanged, exportWindow, &DmsExportWindow::resetExportDialog);

    m_native_driver_checkbox = new QCheckBox("Use native driver", this);
    connect(m_native_driver_checkbox, &QCheckBox::stateChanged, exportWindow, &DmsExportWindow::resetExportDialog);
    grid_layout_box->addWidget(format_label, 2, 0);
    grid_layout_box->addWidget(m_driver_selection, 2, 1);
    grid_layout_box->addWidget(m_native_driver_checkbox, 2, 2);
    //setNativeDriverCheckbox();

    auto line_editor = new QFrame(this);
    line_editor->setFrameShape(QFrame::HLine);
    line_editor->setFrameShadow(QFrame::Plain);
    line_editor->setLineWidth(1);
    line_editor->setMidLineWidth(1);
    grid_layout_box->addWidget(line_editor, 3, 0, 1, 3);

    auto final_filename = new QLabel("Resulting filename(s):", this);
    m_final_filename = new QTextBrowser(this);
    m_final_filename->setReadOnly(true);
    m_final_filename->setStyleSheet("QTextBrowser { background-color: rgb(240, 240, 240); }");

    grid_layout_box->addWidget(final_filename, 4, 0);
    grid_layout_box->addWidget(m_final_filename, 4, 1);


    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid_layout_box->addWidget(spacer, 5, 0, 1, 3);
}

#include <QStandardItemModel>

auto convertFullNameToFoldernameExtension(const TreeItem* current_item) -> QString
{
    auto parent_item = current_item->GetParent();
    return QString(parent_item->GetFullName().c_str());
}

void ExportTab::showEvent(QShowEvent* event)
{
    // Qt calls this when the tab becomes visible; the TreeItem queries below
    // (GetFullFolderNameBase, GetName, isItemOrItsSubItemsMappable) can throw.
    try {
        const auto& currDriver = m_available_drivers.at(m_driver_selection->currentIndex());
        auto driver_has_native_version = currDriver.HasNativeVersion();

        auto current_item = m_export_window->exportItem();
        if (!current_item)
            return;
        bool require_native = m_is_raster && driver_has_native_version && rasterItemRequiresNativeDriver(current_item); // #1141

        m_native_driver_checkbox->setEnabled(driver_has_native_version);
        if (driver_has_native_version && ((currDriver.m_driver_characteristics & driver_characteristics::only_native_driver) || require_native))
        {
            m_native_driver_checkbox->setChecked(true);
            m_native_driver_checkbox->setEnabled(false);
        }
        else
        {
            m_native_driver_checkbox->setChecked(false);
        }
        auto full_foldername_base = GetFullFolderNameBase(current_item);
        auto current_item_folder_name_extension = convertFullNameToFoldernameExtension(current_item);
        m_foldername_entry->setText((QString(full_foldername_base.c_str())+current_item_folder_name_extension));// +current_item_folder_name_extention));
        auto filename = current_item->GetName();
        m_filename_entry->setText(filename.c_str());

        // set disabled drivers in combobox
        QStandardItemModel* model = qobject_cast<QStandardItemModel*>(m_driver_selection->model());

        if (!model)
            return;

    //    auto vsflags = SHV_GetViewStyleFlags(current_item);
        auto is_mappable = isItemOrItsSubItemsMappable(current_item);

        if (!m_is_raster && is_mappable)
            m_driver_selection->setCurrentIndex(1); // ESRI Shapefile
        else
            m_driver_selection->setCurrentIndex(0); // CSV

        for (int i=0; i<model->rowCount(); i++)
        {
            auto* item = model->item(i);
            const auto& otherDriver = m_available_drivers.at(i);
            if (!is_mappable && (otherDriver.m_driver_characteristics & driver_characteristics::disable_with_no_geometry))
                item->setEnabled(false);
            else if (is_mappable && (otherDriver.m_driver_characteristics & driver_characteristics::disable_with_geometry))
                item->setEnabled(false);
            else
                item->setEnabled(true);
        }
    }
    catch (...) {
        catchAndReportException();
    }
}

void DmsExportWindow::prepare(const TreeItem* exportItem)
{
    try {
        SuspendTrigger::Resume();

        m_export_item = exportItem;
        auto current_item = m_export_item;
        setWindowTitle(QString("Export ") + current_item->GetFullName().c_str());

        bool can_be_exported_to_vector = currentItemCanBeExportedToVector(current_item);
        bool can_be_exported_to_raster = currentItemCanBeExportedToRaster(current_item);
        m_tabs->setTabEnabled(m_vector_tab_index, can_be_exported_to_vector);
        m_tabs->setTabEnabled(m_raster_tab_index, can_be_exported_to_raster);
        if (can_be_exported_to_raster)
            m_tabs->setCurrentIndex(m_raster_tab_index);
        m_tabs->widget(m_vector_tab_index);
        resetExportDialog();
    }
    catch (...) {
        catchAndReportException();
    }
}

void DmsExportWindow::resetExportDialog()
{
    m_export_button->setText("Export");
    m_cancel_button->setEnabled(true);
    m_export_button->setStatusTip("");
    m_export_ready = false;
}

// #872: bring export failures to the user's attention immediately, instead of leaving
// them waiting for an export that already failed with only an event-log trace.
static void reportExportError(QWidget* parent, CharPtr why)
{
    reportF(SeverityTypeID::ST_Error, "{}", why);
    QMessageBox::critical(parent, QObject::tr("Export failed"), QString::fromUtf8(why));
}

bool DmsExportWindow::exportImpl()
{
    auto current_export_item = m_export_item;
    ObjectMsgGenerator thisMsgGenerator(current_export_item, "Export");
    Waiter logWaitingtime(&thisMsgGenerator);

    auto active_tab = static_cast<ExportTab*>(m_tabs->currentWidget());
    auto& driver = active_tab->m_available_drivers.at(active_tab->m_driver_selection->currentIndex());
    bool use_native_driver = active_tab->m_native_driver_checkbox->isChecked();
    auto filename = SharedStr((active_tab->m_foldername_entry->text() + "/" + active_tab->m_filename_entry->text() + driver.exts.at(0)).toStdString().c_str());

    CharPtr driverName = nullptr;
    CharPtr storageTypeName = nullptr;

    if (use_native_driver && driver.HasNativeVersion())
    {
        storageTypeName = driver.nativeName;
        if (!stricmp(storageTypeName, "CSV"))
        {
            DoExportTableorDatabaseToCSV(current_export_item, filename);
            return true;
        }
    }
    else
    {
        driverName = driver.shortname;
        if (driver.m_driver_characteristics & driver_characteristics::is_raster)
            storageTypeName = "gdalwrite.grid";
        else
            storageTypeName = "gdalwrite.vect";
    }

    const TreeItem* exportConfig = nullptr;
    if (driver.m_driver_characteristics & driver_characteristics::is_raster)
        exportConfig = DoExportRasterOrMatrixData(current_export_item, use_native_driver, filename, storageTypeName, driverName, "");
    else
        exportConfig = DoExportTableOrDatabase(current_export_item, use_native_driver, filename, storageTypeName, driverName, "");

    if (!exportConfig)
    {
        auto msg = mySSPrintF("no exportable data found in {} for driver {}"
            , current_export_item->GetFullName().c_str()
            , driver.Caption());
        reportExportError(this, msg.c_str());
        return false;
    }

    SuspendTrigger::Resume();

    // Hold interest in all storable data items of the export config during the whole
    // update: Tree_Update_Or_Return_Failer updates and commits the items one by one,
    // and gdalwrite.vect only writes a layer once all fields-of-interest have their
    // data. Without this collective interest each field is written in a separate
    // session, which loses the previous fields for recreate-only drivers (GML/XML).
    std::vector<SharedTreeItemInterestPtr> exportInterests;
    for (auto si = exportConfig->WalkConstSubTree(nullptr); si; si = exportConfig->WalkConstSubTree(si))
        if (IsDataItem(si) && si->IsStorable())
        {
            si->UpdateMetaInfo();
            exportInterests.emplace_back(si);
        }

    auto failedItem = Tree_Update_Or_Return_Failer(exportConfig, "Export");
    if (!failedItem)
        return true;

    MG_CHECK(!SuspendTrigger::DidSuspend());
    auto errMsgPtr = failedItem->GetFailReason();
    auto msg = errMsgPtr
        ? errMsgPtr->m_Why
        : mySSPrintF("export of {} failed without a fail reason", failedItem->GetFullName().c_str());
    reportExportError(this, msg.c_str());
    return false;
}

void DmsExportWindow::exportActiveTabInfoOrCloseAfterExport()
{
    if (m_export_ready)
    {
        reject();
        return;
    }
    SuspendTrigger::Resume();
    m_export_button->setText("Exporting...");
    //m_export_button->setDisabled(true);
    m_cancel_button->setDisabled(true);


    {
        MainWindow::TheOne()->m_treeview->setUpdatesEnabled(false);
        MainThreadBlocker blockThis;
        m_export_button->repaint();
        MainWindow::TheOne()->m_treeview->setUpdatesEnabled(true);
    }

    try {
        if (exportImpl())
        {
            m_export_button->setText("Export ready, close dialog");
            m_export_button->setStatusTip("");
            m_export_ready = true;
        }
        else // failure was already shown in an error box (#872); allow a retry with adjusted settings
            resetExportDialog();
    }
    catch (...)
    {
        auto errMsgPtr = catchAndReportException();
        QMessageBox::critical(this, tr("Export failed"), QString::fromUtf8(errMsgPtr->m_Why.c_str()));
        resetExportDialog();
    }
}

DmsExportWindow::DmsExportWindow(QWidget* parent)
	: QDialog(parent)
{
    auto tab_layout = new QVBoxLayout(this);
	m_tabs = new QTabWidget(this);
	m_vector_tab_index = m_tabs->addTab(new ExportTab(false, this), tr("Vector"));
	m_raster_tab_index = m_tabs->addTab(new ExportTab(true, this), tr("Raster"));


    m_tabs->setCurrentIndex(m_vector_tab_index);
    tab_layout->addWidget(m_tabs);

    QWidget* export_cancel_widgets = new QWidget(this);
    setMinimumSize(800, 400);
    auto h_layout = new QHBoxLayout(this);
    m_export_button = new QPushButton("Export", this);
    connect(m_export_button, &QPushButton::released, this, &DmsExportWindow::exportActiveTabInfoOrCloseAfterExport); //TODO: refactor, needs to be created before SetNativeDriverCheckbox is called
    h_layout->addWidget(m_export_button);

    m_cancel_button = new QPushButton("Cancel", this);
    connect(m_cancel_button, &QPushButton::released, this, &DmsExportWindow::reject);
    h_layout->addWidget(m_cancel_button);
    export_cancel_widgets->setLayout(h_layout);

    tab_layout->addWidget(export_cancel_widgets);

    static_cast<ExportTab*>(m_tabs->widget(m_vector_tab_index))->setNativeDriverCheckbox();
    static_cast<ExportTab*>(m_tabs->widget(m_raster_tab_index))->setNativeDriverCheckbox();
    setWindowModality(Qt::ApplicationModal);
}