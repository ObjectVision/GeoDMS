#include <QPointer>
#include <QDialog>;
#include "ptr/SharedPtr.h"

class QCheckBox;
class QPushButton;
class QSlider;
class QLabel;
class QFileDialog;
class QLineEdit;
class QTabWidget;
class QComboBox;

enum class driver_characteristics : UInt32
{
    none = 0,
    is_raster = 0x01,
    native_is_default = 0x02,
    tableset_is_folder = 0x04,
};
inline bool operator &(driver_characteristics lhs, driver_characteristics rhs) { return UInt32(lhs) & UInt32(rhs); }
inline driver_characteristics operator |(driver_characteristics lhs, driver_characteristics rhs) { return driver_characteristics(UInt32(lhs) | UInt32(rhs)); }

struct gdal_driver_id
{
public:
    CharPtr shortname = nullptr;
    CharPtr name = nullptr;
    CharPtr nativeName = nullptr;
    CharPtr ext = nullptr;
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

class ExportTab : public QWidget
{
    Q_OBJECT

public:
    ExportTab(bool is_raster = false, QWidget* parent = nullptr);

private slots:
    void setFilenameUsingFileDialog();
    void onComboBoxItemActivate(int index);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setNativeDriverCheckbox();
    void repopulateDriverSelection();
    bool m_is_raster = false;
    std::vector<gdal_driver_id> m_available_drivers;
    QPointer<QLineEdit> m_filename_entry;
    QPointer<QCheckBox> m_native_driver_checkbox;
    QPointer<QComboBox> m_driver_selection;
};

class DmsExportWindow : public QDialog
{
    Q_OBJECT
public:
    void prepare();
    DmsExportWindow(QWidget* parent = nullptr);

public slots:
    void exportActiveTabInfo();

private:
    int m_vector_tab_index;
    int m_raster_tab_index;
    QPointer<QTabWidget> m_tabs;
};