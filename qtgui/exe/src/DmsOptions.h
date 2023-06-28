#include <QPointer>
#include <QDialog>;

class QCheckBox;
class QPushButton;
class QSlider;
class QLabel;
class QFileDialog;
class QLineEdit;

class DmsGuiOptionsWindow : public QDialog
{
    Q_OBJECT
public:
    DmsGuiOptionsWindow(QWidget* parent = nullptr);

private slots:
    void changeValidTreeItemColor();
    void changeNotCalculatedTreeItemColor();
    void changeFailedTreeItemColor();
    void changeMapviewBackgroundColor();
    void changeClassificationStartColor();
    void changeClassificationEndColor();
    void ok();
    void apply();
    void undo();

private:
    bool m_changed = false;
    QPointer<QCheckBox> m_show_hidden_items;
    QPointer<QCheckBox> m_show_thousand_separator;
    QPointer<QCheckBox> m_show_state_colors_in_treeview;

    QPointer<QPushButton> m_valid_color_ti_button;
    QPointer<QPushButton> m_not_calculated_color_ti_button;
    QPointer<QPushButton> m_failed_color_ti_button;
    QPointer<QPushButton> m_background_color_button;
    QPointer<QPushButton> m_start_color_button;
    QPointer<QPushButton> m_end_color_button;
    QPointer<QPushButton> m_ok;
    QPointer<QPushButton> m_apply;
    QPointer<QPushButton> m_undo;
};

class DmsAdvancedOptionsWindow : public QDialog
{
    Q_OBJECT
public:
    DmsAdvancedOptionsWindow(QWidget* parent = nullptr);

private slots:
    void onFlushTresholdValueChange(int value);
    void restoreOptions();
    void ok();
    void apply();
    void undo();
    void onStateChange(int state);
    void onTextChange(const QString& text);
    void setLocalDataDirThroughDialog();
    void setSourceDataDirThroughDialog();

private:
    void setInitialLocalDataDirValue();
    void setInitialSourceDatDirValue();
    void setInitialEditorValue();
    void setInitialMemoryFlushTresholdValue();

    bool m_changed = false;

    QPointer<QFileDialog> m_folder_dialog;
    QPointer<QLabel> m_flush_treshold_text;
    QPointer<QCheckBox> m_pp0;
    QPointer<QCheckBox> m_pp1;
    QPointer<QCheckBox> m_pp2;
    QPointer<QCheckBox> m_pp3;
    QPointer<QCheckBox> m_tracelog;
    QPointer<QLineEdit> m_ld_input;
    QPointer<QLineEdit> m_sd_input;
    QPointer<QLineEdit> m_editor_input;
    QPointer<QSlider>   m_flush_treshold;
    QPointer<QPushButton> m_ok;
    QPointer<QPushButton> m_apply;
    QPointer<QPushButton> m_undo;
};

class DmsConfigOptionsWindow : public QDialog
{
    Q_OBJECT
public:
    DmsConfigOptionsWindow(QWidget* parent = nullptr);
};
