#include <QPointer>
#include <QDialog>
#include "ptr/SharedPtr.h"
#include <vector>

struct TreeItem;
class QCheckBox;
class QPushButton;
class QSlider;
class QLabel;
class QFileDialog;
class QLineEdit;
class QTabWidget;
class QComboBox;
class DmsExportWindow;
class QTextBrowser;

enum class driver_characteristics : UInt32
{
    none = 0,
    is_raster = 0x01,
    native_is_default = 0x02,
    tableset_is_folder = 0x04,
    disable_with_no_geometry = 0x08,
};
inline bool operator &(driver_characteristics lhs, driver_characteristics rhs) { return UInt32(lhs) & UInt32(rhs); }
inline driver_characteristics operator |(driver_characteristics lhs, driver_characteristics rhs) { return driver_characteristics(UInt32(lhs) | UInt32(rhs)); }

struct gdal_driver_id
{
public:
    CharPtr shortname = nullptr;
    CharPtr name = nullptr;
    CharPtr nativeName = nullptr;
    std::vector<CharPtr> exts;
    driver_characteristics driver_characteristics = driver_characteristics::none;

    CharPtr Caption()
    {
        if (name)
            return name;
        return shortname;
    }

    bool HasNativeVersion() { return nativeName; }

    bool IsEmpty()
    {
        return shortname == nullptr;
    }
};

bool currentItemCanBeExportedToVector(const TreeItem* item);
bool currentItemCanBeExportedToRaster(const TreeItem* item);

class ExportTab : public QWidget
{
    Q_OBJECT

public:
    ExportTab(bool is_raster, DmsExportWindow* exportWindow);
    bool m_is_raster = false;
    std::vector<gdal_driver_id> m_available_drivers;
    QPointer<QLineEdit> m_filename_entry;
    QPointer<QCheckBox> m_native_driver_checkbox;
    QPointer<QComboBox> m_driver_selection;
    QPointer<QTextBrowser> m_final_filename;

private slots:
    void setFilenameUsingFileDialog();
    void onComboBoxItemActivate(int index);
    void onFilenameEntryTextChanged(const QString& new_filename);

protected:
    void showEvent(QShowEvent* event) override;

private:
    auto createFinalFileNameText() -> QString;
    void resetFilenameExtension();
    void setNativeDriverCheckbox();
    void repopulateDriverSelection();
};

class DmsExportWindow : public QDialog
{
    Q_OBJECT
public:
    void prepare();
    DmsExportWindow(QWidget* parent = nullptr);

public slots:
    void exportActiveTabInfo();
    void resetExportButton();

private:
    void exportImpl();

    int m_vector_tab_index;
    int m_raster_tab_index;
    QPointer<QTabWidget> m_tabs;
    QPointer<QPushButton> m_export_button;
    bool                  m_export_ready = false;
};