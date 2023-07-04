#include "DmsExport.h"
#include "DmsMainWindow.h"
#include "ExportInfo.h"
#include "utl/Environment.h"
#include "Parallel.h"
#include "ptr/SharedStr.h"
#include "TicInterface.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "Unit.h"

#include "ItemUpdate.h"
#include "dbg/DmsCatch.h"

#include "mci/ValueClass.h"

#include <QCheckBox>
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

#include "ser/FileStreamBuff.h"
#include "stg/AbstrStoragemanager.h"
#include "TicInterface.h"
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

    for (auto tableItem = tableOrDatabaseItem->GetFirstSubItem(); tableItem; tableItem->GetNextItem())
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

bool currentItemCanBeExportedToVector(const TreeItem* item)
{
    if (!item)
        return false;

    if (item->IsFailed())
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

bool currentItemCanBeExportedToRaster(const TreeItem* item)
{
    if (!item)
        return false;

    if (item->IsFailed())
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
                    if (currentItemCanBeExportedToRaster(subItem))
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

auto getAvailableDrivers() -> std::vector<gdal_driver_id>
{
    std::vector<gdal_driver_id> available_drivers;
    available_drivers.emplace_back("ESRI Shapefile", "ESRI Shapefile / DBF", "shp", ".shp", driver_characteristics::tableset_is_folder);
    available_drivers.emplace_back("GPKG", "GeoPackage vector (*.gpkg)", nullptr, ".gpkg");
    available_drivers.emplace_back("CSV", "Comma Separated Value (*.csv)", "csv", ".csv", driver_characteristics::native_is_default | driver_characteristics::tableset_is_folder);
    available_drivers.emplace_back("GML", "Geography Markup Language (*.GML)", nullptr, ".gml");
    available_drivers.emplace_back("GeoJSON", "GeoJSON", nullptr, ".json");
    available_drivers.emplace_back("GTiff", "GeoTIFF File Format", "tif", ".tif", driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
    available_drivers.emplace_back("netCDF", "NetCDF: Network Common Data Form", nullptr, ".cdf", driver_characteristics::is_raster);
    //available_drivers.emplace_back("PNG", "Portable Network Graphics (*.png)", nullptr, ".png", driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
    //available_drivers.emplace_back("JPEG", "JPEG JFIF File Format (*.jpg)", nullptr, ".jpg", driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
    return available_drivers;
}

void ExportTab::setNativeDriverCheckbox()
{
    auto driver = m_available_drivers.at(m_driver_selection->currentIndex());
    m_native_driver_checkbox->setChecked(driver.HasNativeVersion());
}

void ExportTab::repopulateDriverSelection()
{
    m_available_drivers.clear();
    QStringList driver_namesnames;
    auto available_drivers = getAvailableDrivers();
    for (auto& driver : available_drivers)
    {
        if (m_is_raster && (driver.driver_characteristics & driver_characteristics::is_raster)) // raster driver
        {
            m_available_drivers.push_back(driver);
            m_driver_selection->addItem(driver.Caption());
        }
        if (!m_is_raster && !(driver.driver_characteristics & driver_characteristics::is_raster)) // vector driver
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
}

void ExportTab::setFilenameUsingFileDialog()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Set export filename"), m_filename_entry->text());
    m_filename_entry->setText(filename);
}

ExportTab::ExportTab(bool is_raster, DmsExportWindow* exportWindow)
	: QWidget(nullptr)
{
	m_is_raster = is_raster;

    // tab form
    auto grid_layout_box = new QGridLayout(this);
    auto format_label = new QLabel("Format", this);


    auto filename = new QLabel("Filename:", this);
    m_filename_entry = new QLineEdit(this);
    auto filename_button = new QPushButton(QIcon(":/res/images/DP_explore.bmp"), "", this);
    connect(filename_button, &QPushButton::clicked, this, &ExportTab::setFilenameUsingFileDialog);
    grid_layout_box->addWidget(filename, 0, 0);
    grid_layout_box->addWidget(m_filename_entry, 0, 1);
    grid_layout_box->addWidget(filename_button, 0, 2);

    m_driver_selection = new QComboBox(this);
    repopulateDriverSelection();
    connect(m_driver_selection, &QComboBox::activated, this, &ExportTab::onComboBoxItemActivate);
    connect(m_driver_selection, &QComboBox::activated, exportWindow, &DmsExportWindow::resetExportButton);

    m_native_driver_checkbox = new QCheckBox("Use native driver", this);
    grid_layout_box->addWidget(format_label, 1, 0);
    grid_layout_box->addWidget(m_driver_selection, 1, 1);
    grid_layout_box->addWidget(m_native_driver_checkbox, 1, 2);
    setNativeDriverCheckbox();

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid_layout_box->addWidget(spacer, 2, 0, 1, 3);
}

void ExportTab::showEvent(QShowEvent* event)
{
    auto driver = m_available_drivers.at(m_driver_selection->currentIndex());
    if (driver.HasNativeVersion())
    {
        m_native_driver_checkbox->setEnabled(true);
        m_native_driver_checkbox->setChecked(true);
    }
    else
    {
        m_native_driver_checkbox->setEnabled(false);
        m_native_driver_checkbox->setChecked(false);
    }

    auto current_item = MainWindow::TheOne()->getCurrentTreeItem();
    auto full_filename_base = GetFullFileNameBase(current_item);

    m_filename_entry->setText(QString(full_filename_base.c_str()) + driver.ext);
}

void DmsExportWindow::prepare()
{
    SuspendTrigger::Resume();

    auto current_item = MainWindow::TheOne()->getCurrentTreeItem();
    setWindowTitle(QString("Export ") + current_item->GetFullName().c_str());
    
    m_tabs->setTabEnabled(m_vector_tab_index, currentItemCanBeExportedToVector(current_item));
    m_tabs->setTabEnabled(m_raster_tab_index, currentItemCanBeExportedToRaster(current_item));
    m_tabs->widget(m_vector_tab_index);
    resetExportButton();
}

void DmsExportWindow::resetExportButton()
{
    m_export_button->setText("Export");
    m_export_button->setStatusTip("");
    m_export_ready = false;
}

void DmsExportWindow::exportImpl()
{
    auto current_export_item = MainWindow::TheOne()->getCurrentTreeItem();

    ObjectMsgGenerator thisMsgGenerator(current_export_item, "Export");
    Waiter logWaitingtime(&thisMsgGenerator);

    auto active_tab = static_cast<ExportTab*>(m_tabs->currentWidget());
    auto& driver = active_tab->m_available_drivers.at(active_tab->m_driver_selection->currentIndex());
    bool use_native_driver = active_tab->m_native_driver_checkbox->isChecked();
    auto filename = SharedStr(active_tab->m_filename_entry->text().toStdString().c_str());

    CharPtr driverName = nullptr;
    CharPtr storageTypeName = nullptr;

    if (use_native_driver && driver.HasNativeVersion())
    {
        storageTypeName = driver.nativeName;
        if (!stricmp(storageTypeName, "CSV"))
        {
            DoExportTableorDatabaseToCSV(current_export_item, filename);
            return;
        }
    }
    else
    {
        driverName = driver.shortname;
        if (driver.driver_characteristics & driver_characteristics::is_raster)
            storageTypeName = "gdalwrite.grid";
        else
            storageTypeName = "gdalwrite.vect";
    }

    const TreeItem* exportConfig = nullptr;
    if (driver.driver_characteristics & driver_characteristics::is_raster)
        exportConfig = DoExportRasterOrMatrixData(current_export_item, use_native_driver, filename, storageTypeName, driverName, "");
    else
        exportConfig = DoExportTableOrDatabase(current_export_item, use_native_driver, filename, storageTypeName, driverName, "");

    if (exportConfig)
    {
        Tree_Update(exportConfig, "Export");
        return;
    }
}

void DmsExportWindow::exportActiveTabInfo()
{
    if (m_export_ready)
    {
        reject();
        return;
    }
    SuspendTrigger::Resume();
    m_export_button->setText("Exporting...");
    m_export_button->repaint();

    try {
        exportImpl();
        m_export_button->setText("Ready");
        m_export_button->setStatusTip("");
        m_export_ready = true;
    }
    catch (...)
    {
        auto errMsgPtr = catchAndReportException();
        m_export_button->setText(errMsgPtr->m_Why.c_str());
        m_export_button->setStatusTip(errMsgPtr->m_Why.c_str());
    }
}

DmsExportWindow::DmsExportWindow(QWidget* parent)
	: QDialog(parent)
{
    setMinimumSize(600, 400);
    auto tab_layout = new QVBoxLayout(this);
	m_tabs = new QTabWidget(this);
	m_vector_tab_index = m_tabs->addTab(new ExportTab(false, this), tr("Vector"));
	m_raster_tab_index = m_tabs->addTab(new ExportTab(true, this), tr("Raster"));
    m_tabs->setCurrentIndex(m_vector_tab_index);
    tab_layout->addWidget(m_tabs);

    QWidget* export_cancel_widgets = new QWidget(this);

    auto h_layout = new QHBoxLayout(this);
    m_export_button = new QPushButton("Export", this);
    connect(m_export_button, &QPushButton::released, this, &DmsExportWindow::exportActiveTabInfo);
    h_layout->addWidget(m_export_button);

    auto cancel_button = new QPushButton("Cancel", this);
    connect(cancel_button, &QPushButton::released, this, &DmsExportWindow::reject);
    h_layout->addWidget(cancel_button);
    export_cancel_widgets->setLayout(h_layout);

    tab_layout->addWidget(export_cancel_widgets);

    setWindowModality(Qt::ApplicationModal);
}