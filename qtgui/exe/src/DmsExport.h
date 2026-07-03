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
    none                     = 0,
    is_raster                = 1,
    native_is_default        = 2,
    tableset_is_folder       = 4,
    disable_with_no_geometry = 8,
    disable_with_geometry    = 16,
    only_native_driver       = 32
};
inline bool operator &(driver_characteristics lhs, driver_characteristics rhs) { return UInt32(lhs) & UInt32(rhs); }
inline driver_characteristics operator |(driver_characteristics lhs, driver_characteristics rhs) { return driver_characteristics(UInt32(lhs) | UInt32(rhs)); }

struct gdal_driver_id
{
    CharPtr shortname = nullptr;
    CharPtr name = nullptr;
    CharPtr nativeName = nullptr;
    std::vector<CharPtr> exts;
    enum driver_characteristics m_driver_characteristics = driver_characteristics::none;

    CharPtr Caption() const
    {
        assert(name);
        return name;
    }

    bool HasNativeVersion() const { return nativeName; }

    bool IsEmpty() const
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
    void setNativeDriverCheckbox();
    bool m_is_raster = false;
    std::vector<gdal_driver_id> m_available_drivers;
    QPointer<QLineEdit> m_foldername_entry;
    QPointer<QLineEdit> m_filename_entry;
    QPointer<QCheckBox> m_native_driver_checkbox;
    QPointer<QComboBox> m_driver_selection;
    QPointer<QTextBrowser> m_final_filename;

private slots:
    void setFoldernameUsingFileDialog();
    void onComboBoxItemActivate(int index);
    void onFilenameEntryTextChanged(const QString& new_filename);

protected:
    void showEvent(QShowEvent* event) override;

private:
    auto createFinalFileNameText() -> QString;
    void repopulateDriverSelection();

    DmsExportWindow* m_export_window = nullptr;
};

class DmsExportWindow : public QDialog
{
    Q_OBJECT
public:
    void prepare(const TreeItem* exportItem);
    DmsExportWindow(QWidget* parent = nullptr);

    // the subject of the export: the current tree item or a constructed
    // Desktops/../ViewData config table of a table view (issue #411)
    const TreeItem* exportItem() const { return m_export_item; }

public slots:
    void exportActiveTabInfoOrCloseAfterExport();
    void resetExportDialog();

private:
    bool exportImpl(); // true: data was exported; false: reported failure in an error box (#872)

    const TreeItem*       m_export_item = nullptr;
    int m_vector_tab_index;
    int m_raster_tab_index;
    QPointer<QTabWidget>  m_tabs;
    QPointer<QPushButton> m_export_button;
    QPointer<QPushButton> m_cancel_button;
    bool                  m_export_ready = false;
};