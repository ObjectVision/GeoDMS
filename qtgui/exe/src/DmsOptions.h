#pragma once

#include <QPointer>
#include <QDialog>
#include "geo/color.h"
#include "ptr/SharedStr.h"

enum string_option
{
    LocalDataDir,
    SourceDataDir,
    StartEditorCmd,
    Count
};

enum class color_option
{
    tv_valid,
    tv_not_calculated,
    tv_failed,
    tv_exogenic,
    tv_template,

    mapview_background,
    mapview_ramp_start,
    mapview_ramp_end,
    count
};

void LoadColors();
DmsColor GetUserColor(color_option co);

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
    void restoreOptions();

    void changeColor(QPushButton*, color_option co);

private:
    void setChanged(bool isChanged);
    void hasChanged() { return setChanged(true); }

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
    void setChanged(bool isChanged);

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

//======== CONFIG OPTIONS WINDOW ========

struct ConfigOption {
    SharedStr                 name, configured_value;
    QPointer< QCheckBox>      override_cbx;
    QPointer< QLineEdit>      override_value;
};

class DmsConfigOptionsWindow : public QDialog
{
    Q_OBJECT
public:
    DmsConfigOptionsWindow(QWidget* parent = nullptr);

private slots:
    void ok();
    void apply();
    void resetValues();
    void checkbox_toggled();

private:
    void setChanged(bool isChanged);
    void updateAccordingToCheckboxStates();

    bool m_changed = false;
    std::vector<ConfigOption> m_Options;

    QPointer<QPushButton> m_ok;
    QPointer<QPushButton> m_apply;
    QPointer<QPushButton> m_undo;
};
