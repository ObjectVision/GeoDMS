#pragma once

#include <QPointer>
#include <QDialog>
#include "geo/color.h"
#include "ptr/SharedStr.h"
#include "ui_DmsLocalMachineOptionsWindow.h"
#include "ui_DmsGuiOptionsWindow.h"
#include "ui_DmsConfigOptionsWindow.h"

struct TreeItem;

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
QColor GetUserQColor(color_option co);

class QCheckBox;
class QPushButton;
class QSlider;
class QLabel;
class QFileDialog;
class QLineEdit;



void SetDrawingSizeTresholdValue(Float32 drawing_size);
Float32 GetDrawingSizeInPixels();
class DmsGuiOptionsWindow : public QDialog, Ui::DmsGuiOptionsWindow
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
    void cancel();
    void apply();
    void restoreOptions();
    void changeColor(QPushButton*, color_option co);

private:
    void setChanged(bool isChanged);
    void hasChanged() { return setChanged(true); }

    bool m_changed = false;
};

class DmsLocalMachineOptionsWindow : public QDialog, Ui::DmsLocalMachineOptionsWindow
{
    Q_OBJECT
public:
    DmsLocalMachineOptionsWindow(QWidget* parent = nullptr);

private slots:
    void onFlushTresholdValueChange(int value);
    void restoreOptions();
    void ok();
    void cancel();
    void apply();
    void onStateChange();
    void onTextChange();
    void setLocalDataDirThroughDialog();
    void setSourceDataDirThroughDialog();
    void setEditorProgramThroughDialog();
    void setDefaultEditorParameters();

private:
    void setInitialLocalDataDirValue();
    void setInitialSourceDatDirValue();
    void setInitialEditorValue();
    void setInitialMemoryFlushTresholdValue();
    void setChanged(bool isChanged);

    bool m_changed = false;
    QPointer<QFileDialog> m_folder_dialog;
    QPointer<QFileDialog> m_file_dialog;
};

//======== CONFIG OPTIONS WINDOW ========

struct ConfigOption {
    SharedStr                 name, configured_value;
    QPointer< QCheckBox>      override_cbx;
    QPointer< QLineEdit>      override_value;
};

class DmsConfigOptionsWindow : public QDialog, Ui::DmsConfigOptionsWindow
{
    Q_OBJECT
public:
    DmsConfigOptionsWindow(QWidget* parent = nullptr);

    static bool hasOverridableConfigOptions();

private slots:
    void ok();
    void cancel();
    void apply();
    void resetValues();
    void onTextChange();
    void onCheckboxToggle();

private:
    static auto getFirstOverridableOption() -> const TreeItem*;
    void setChanged(bool isChanged);
    void updateAccordingToCheckboxStates();

    bool m_changed = false;
    std::vector<ConfigOption> m_Options;

    QPointer<QPushButton> m_ok;
    QPointer<QPushButton> m_cancel;
    QPointer<QPushButton> m_undo;
};

inline void setSF(bool value, UInt32& rsf, UInt32 flag)
{
    if (value)
        rsf |= flag;
    else
        rsf &= ~flag;
}

void SetRegStatusFlags(UInt32 newSF);

