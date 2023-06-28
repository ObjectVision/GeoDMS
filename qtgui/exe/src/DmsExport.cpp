#include "DmsExport.h"
#include "DmsMainWindow.h"
#include "ExportInfo.h"
#include "utl/Environment.h"
#include "Parallel.h"
#include "ptr/SharedStr.h"

#include "AbstrDataItem.h"
#include "AbstrDataObject.h"
#include "AbstrUnit.h"
#include "Unit.h"

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
    available_drivers.emplace_back("PNG", "Portable Network Graphics (*.png)", nullptr, ".png", driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
    available_drivers.emplace_back("JPEG", "JPEG JFIF File Format (*.jpg)", nullptr, ".jpg", driver_characteristics::is_raster | driver_characteristics::tableset_is_folder);
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

ExportTab::ExportTab(bool is_raster, QWidget * parent)
	: QWidget(parent)
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

    m_native_driver_checkbox = new QCheckBox("Use native driver", this);
    grid_layout_box->addWidget(format_label, 1, 0);
    grid_layout_box->addWidget(m_driver_selection, 1, 1);
    grid_layout_box->addWidget(m_native_driver_checkbox, 1, 2);
    setNativeDriverCheckbox();

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid_layout_box->addWidget(spacer, 2, 0, 1, 3);

    auto export_button = new QPushButton("Export");
    connect(export_button, &QPushButton::clicked, MainWindow::TheOne()->m_export_window, &DmsExportWindow::exportActiveTabInfo);
    grid_layout_box->addWidget(export_button, 3, 0);

    auto cancel_button = new QPushButton("Cancel");
    connect(cancel_button, &QPushButton::clicked, MainWindow::TheOne()->m_export_window, &DmsExportWindow::reject);
    grid_layout_box->addWidget(cancel_button, 3, 1);
}

void ExportTab::showEvent(QShowEvent* event)
{
    auto driver = m_available_drivers.at(m_driver_selection->currentIndex());
    auto current_item = MainWindow::TheOne()->getCurrentTreeItem();
    auto full_filename_base = GetFullFileNameBase(current_item);
    m_filename_entry->setText(QString(full_filename_base.c_str()) + driver.ext);
}

void DmsExportWindow::prepare()
{
    auto current_item = MainWindow::TheOne()->getCurrentTreeItem();
    setWindowTitle(QString("Export ") + current_item->GetFullName().c_str());
    m_tabs->setTabEnabled(m_vector_tab_index, CurrentItemCanBeExportedToVector(current_item));
    m_tabs->setTabEnabled(m_raster_tab_index, CurrentItemCanBeExportedToRaster(current_item));
    m_tabs->widget(m_vector_tab_index);
}

void DmsExportWindow::exportActiveTabInfo()
{
    // TODO: implement
}

DmsExportWindow::DmsExportWindow(QWidget* parent)
	: QDialog(parent)
{
    setMinimumSize(600, 400);
    auto tab_layout = new QVBoxLayout(this);
	m_tabs = new QTabWidget(this);
	m_vector_tab_index = m_tabs->addTab(new ExportTab(false), tr("Vector"));
	m_raster_tab_index = m_tabs->addTab(new ExportTab(true), tr("Raster"));
    m_tabs->setCurrentIndex(m_vector_tab_index);
    tab_layout->addWidget(m_tabs);
    setWindowModality(Qt::ApplicationModal);
}