#include "DmsOptions.h"
#include "utl/Environment.h"
#include "Parallel.h"
#include "ptr/SharedStr.h"
#include "StgBase.h"
#include "AbstrDataItem.h"

#include "Unit.h"
#include "UnitClass.h"
#include "utl/Registry.h"
#include "DmsMainWindow.h"

#include <QCheckBox>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QFileDialog>
#include <QLineEdit>
#include <QGridLayout>
#include <QColorDialog>
#include <QProcess>

struct colorOptionAttr {

    CharPtr regKey;
    CharPtr descr;
    DmsColor color;
    UInt32  palette_index = 0;
    QColor AsQColor() 
    {
        return QColor(GetRed(color), GetGreen(color),GetBlue(color));
    }

    void apply(DmsColor clr)
    {
        clr &= 0xFFFFFF;
        CheckColor(clr);
        this->color = clr;
        if (this->palette_index)
            STG_Bmp_SetDefaultColor(this->palette_index, clr);
    }
};

static DmsColor salmon = CombineRGB(255, 128, 114);
static DmsColor darkGrey = CombineRGB(100, 100, 100);
static DmsColor cool_blue = CombineRGB(82, 136, 219);
static DmsColor cool_green = CombineRGB(0, 153, 51);
static DmsColor white = CombineRGB(255, 255, 255);

colorOptionAttr sColorOptionData[(int)color_option::count] =
{
    { "Valid", "Pick the TreeItem valid status color", cool_blue},
    { "Invalidated", "Pick the color for not calcualate, status", salmon},
    { "Failed", "Pick the TreeItem failed status color", DmsRed},
    { "Exogenic", "Pick the color for exogenic items", cool_green},
    { "Operator", "Pick the color for template items", darkGrey},
    { "Background", "Pick the Mapview background color", white, 256},
    { "RampStart", "Pick the classification ramp start color", DmsRed, 257},
    { "RampEnd", "Pick the classification ramp end color", DmsBlue, 258},


};

auto backgroundColor2StyleSheet(QColor clr) -> QString
{
    return QString("* { background-color: rgb(")
        + QString::number(qRed(clr.rgba())) + ","
        + QString::number(qGreen(clr.rgba())) + ","
        + QString::number(qBlue(clr.rgba()))
     + ") }";
}

void setBackgroundQColor(QPushButton* btn, QColor clr)
{
    btn->setStyleSheet(backgroundColor2StyleSheet(clr));
}

void setBackgroundColor(QPushButton* btn, color_option co)
{
    setBackgroundQColor(btn, sColorOptionData[(int)co].AsQColor());
}

auto getBackgroundColor(QPushButton* btn) -> QColor
{
    return btn->palette().color(QPalette::ColorRole::Button);
}

void saveBackgroundColor(QPushButton* btn, color_option co)
{
    auto qClr = getBackgroundColor(btn).rgb();
    auto clr = CombineRGB(qRed(qClr), qGreen(qClr), qBlue(qClr));

    auto& colorOptionData = sColorOptionData[(int)co];
    SetGeoDmsRegKeyDWord(colorOptionData.regKey, clr, "Colors");
    colorOptionData.apply(clr);
}

void LoadColors()
{
    for (color_option co = color_option(0); co != color_option::count; reinterpret_cast<int&>(co)++)
    {
        auto& colorOptionData = sColorOptionData[(int)co];
        auto clr = GetGeoDmsRegKeyDWord(colorOptionData.regKey, colorOptionData.color, "Colors");
        colorOptionData.apply(clr);
    }
}

DmsColor GetUserColor(color_option co)
{
    assert(co < color_option::count);
    return sColorOptionData[(int)co].color;
}

QColor GetUserQColor(color_option co)
{
    auto clr = GetUserColor(co);
    return QColor(GetRed(clr), GetGreen(clr), GetBlue(clr));
}

void DmsGuiOptionsWindow::changeColor(QPushButton* btn, color_option co)
{
    auto old_color = getBackgroundColor(btn);
    auto new_color = QColorDialog::getColor(old_color, this, sColorOptionData[(int)co].descr);
    if (not new_color.isValid())
        return;
    if (new_color == old_color)
        return;

    setBackgroundQColor(btn, new_color);
    setChanged(true);
}


//======== BEGIN GUI OPTIONS WINDOW ========

void DmsGuiOptionsWindow::changeValidTreeItemColor()
{
    changeColor(m_valid_color_ti_button, color_option::tv_valid);
}

void DmsGuiOptionsWindow::changeNotCalculatedTreeItemColor()
{
    changeColor(m_not_calculated_color_ti_button, color_option::tv_not_calculated);
 }

void DmsGuiOptionsWindow::changeFailedTreeItemColor()
{
    changeColor(m_failed_color_ti_button, color_option::tv_failed);
}

void DmsGuiOptionsWindow::changeMapviewBackgroundColor()
{
    changeColor(m_background_color_button, color_option::mapview_background);
}

void DmsGuiOptionsWindow::changeClassificationStartColor()
{
    changeColor(m_start_color_button, color_option::mapview_ramp_start);
}

void DmsGuiOptionsWindow::changeClassificationEndColor()
{
    changeColor(m_end_color_button, color_option::mapview_ramp_end);
}

DmsGuiOptionsWindow::DmsGuiOptionsWindow(QWidget* parent)
    : QDialog(parent)
{
    setupUi(this);
    connect(m_show_hidden_items, &QCheckBox::stateChanged, this, &DmsGuiOptionsWindow::hasChanged);
    connect(m_show_thousand_separator, &QCheckBox::stateChanged, this, &DmsGuiOptionsWindow::hasChanged);
    connect(m_show_state_colors_in_treeview, &QCheckBox::stateChanged, this, &DmsGuiOptionsWindow::hasChanged);
    connect(m_valid_color_ti_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeValidTreeItemColor);
    connect(m_not_calculated_color_ti_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeNotCalculatedTreeItemColor);
    connect(m_failed_color_ti_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeFailedTreeItemColor);
    connect(m_background_color_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeMapviewBackgroundColor);
    connect(m_start_color_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeClassificationStartColor);
    connect(m_end_color_button, &QPushButton::released, this, &DmsGuiOptionsWindow::changeClassificationEndColor);

    connect(m_ok, &QPushButton::released, this, &DmsGuiOptionsWindow::ok);
    connect(m_cancel, &QPushButton::released, this, &DmsGuiOptionsWindow::apply);
    connect(m_undo, &QPushButton::released, this, &DmsGuiOptionsWindow::restoreOptions);

    restoreOptions();
    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_DeleteOnClose);
}

void DmsGuiOptionsWindow::setChanged(bool isChanged)
{
    m_changed = isChanged;
    m_cancel->setEnabled(isChanged);
    m_undo->setEnabled(isChanged);
}

void DmsGuiOptionsWindow::apply()
{
    auto dms_reg_status_flags = GetRegStatusFlags();
    setSF(m_show_hidden_items->isChecked(), dms_reg_status_flags, RSF_AdminMode);
    setSF(m_show_thousand_separator->isChecked(), dms_reg_status_flags, RSF_ShowThousandSeparator);
    setSF(m_show_state_colors_in_treeview->isChecked(), dms_reg_status_flags, RSF_ShowStateColors);
    SetRegStatusFlags(dms_reg_status_flags);

    saveBackgroundColor(m_valid_color_ti_button, color_option::tv_valid);
    saveBackgroundColor(m_not_calculated_color_ti_button, color_option::tv_not_calculated);
    saveBackgroundColor(m_failed_color_ti_button, color_option::tv_failed);

    saveBackgroundColor(m_background_color_button, color_option::mapview_background);
    saveBackgroundColor(m_start_color_button, color_option::mapview_ramp_start);
    saveBackgroundColor(m_end_color_button, color_option::mapview_ramp_end);

    setChanged(false);
}

void DmsGuiOptionsWindow::restoreOptions()
{   
    auto dms_reg_status_flags = GetRegStatusFlags();
    m_show_hidden_items->setChecked(dms_reg_status_flags & RSF_AdminMode);
    m_show_thousand_separator->setChecked(dms_reg_status_flags & RSF_ShowThousandSeparator);
    m_show_state_colors_in_treeview->setChecked(dms_reg_status_flags & RSF_ShowStateColors);

    setBackgroundColor(m_valid_color_ti_button, color_option::tv_valid);
    setBackgroundColor(m_not_calculated_color_ti_button, color_option::tv_not_calculated);
    setBackgroundColor(m_failed_color_ti_button, color_option::tv_failed);
    setBackgroundColor(m_background_color_button, color_option::mapview_background);
    setBackgroundColor(m_start_color_button, color_option::mapview_ramp_start);
    setBackgroundColor(m_end_color_button, color_option::mapview_ramp_end);

    setChanged(false);
}

void DmsGuiOptionsWindow::ok()
{
    if (m_changed)
        apply();
    done(QDialog::Accepted);
}

void DmsGuiOptionsWindow::cancel()
{
    restoreOptions();
    done(QDialog::Rejected);
}

//======== BEGIN GUI OPTIONS WINDOW ========

//======== BEGIN ADVANCED OPTIONS WINDOW ========
DmsAdvancedOptionsWindow::DmsAdvancedOptionsWindow(QWidget* parent)
    : QDialog(parent)
{
    setupUi(this);

    m_folder_dialog = new QFileDialog(this);
    m_folder_dialog->setFileMode(QFileDialog::FileMode::Directory);

    m_file_dialog = new QFileDialog(this);
    m_file_dialog->setFileMode(QFileDialog::FileMode::ExistingFile);

    connect(m_ld_input, &QLineEdit::textChanged, this, &DmsAdvancedOptionsWindow::onTextChange);
    connect(m_sd_input, &QLineEdit::textChanged, this, &DmsAdvancedOptionsWindow::onTextChange);
    connect(m_editor_input, &QLineEdit::textChanged, this, &DmsAdvancedOptionsWindow::onTextChange);
    connect(m_editor_parameters_input, &QLineEdit::textChanged, this, &DmsAdvancedOptionsWindow::onTextChange);

    connect(m_ld_folder_dialog, &QPushButton::clicked, this, &DmsAdvancedOptionsWindow::setLocalDataDirThroughDialog);
    connect(m_sd_folder_dialog, &QPushButton::clicked, this, &DmsAdvancedOptionsWindow::setSourceDataDirThroughDialog);
    connect(m_editor_folder_dialog, &QPushButton::clicked, this, &DmsAdvancedOptionsWindow::setEditorProgramThroughDialog);
    connect(m_set_editor_parameters, &QPushButton::clicked, this, &DmsAdvancedOptionsWindow::setDefaultEditorParameters);
    m_ld_folder_dialog->setIcon(QIcon(":/res/images/DP_explore.bmp"));
    m_ld_folder_dialog->setText("");
    m_sd_folder_dialog->setIcon(QIcon(":/res/images/DP_explore.bmp"));
    m_sd_folder_dialog->setText("");
    m_editor_folder_dialog->setIcon(QIcon(":/res/images/DP_explore.bmp"));
    m_editor_folder_dialog->setText("");
    m_pp_info->setIcon(QIcon(":/res/images/DP_ValueInfo.bmp"));

    // parallel processing
    m_pp0->setChecked(IsMultiThreaded0());
    m_pp1->setChecked(IsMultiThreaded1());
    m_pp2->setChecked(IsMultiThreaded2());
    m_pp3->setChecked(IsMultiThreaded3());
    connect(m_pp0, &QCheckBox::stateChanged, this, &DmsAdvancedOptionsWindow::onStateChange);
    connect(m_pp1, &QCheckBox::stateChanged, this, &DmsAdvancedOptionsWindow::onStateChange);
    connect(m_pp2, &QCheckBox::stateChanged, this, &DmsAdvancedOptionsWindow::onStateChange);
    connect(m_pp3, &QCheckBox::stateChanged, this, &DmsAdvancedOptionsWindow::onStateChange);

    // flush treshold
    m_flush_treshold->setTickPosition(QSlider::TickPosition::TicksBelow);
    connect(m_flush_treshold, &QSlider::valueChanged, this, &DmsAdvancedOptionsWindow::onFlushTresholdValueChange);
    connect(m_tracelog, &QCheckBox::stateChanged, this, &DmsAdvancedOptionsWindow::onStateChange);

    // ok/apply/cancel buttons
    m_ok->setAutoDefault(true);
    m_ok->setDefault(true);
    m_cancel->setDisabled(true);
    m_undo->setDisabled(true);
    connect(m_ok, &QPushButton::released, this, &DmsAdvancedOptionsWindow::ok);
    connect(m_cancel, &QPushButton::released, this, &DmsAdvancedOptionsWindow::cancel);
    connect(m_undo, &QPushButton::released, this, &DmsAdvancedOptionsWindow::restoreOptions);

    restoreOptions();
    onFlushTresholdValueChange(m_flush_treshold->value());
    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_DeleteOnClose);
    setChanged(false);
}

struct string_option_attr {
    CharPtr reg_key, default_value;
};

const string_option_attr sStringOptionsData[string_option::Count] = {
    {"LocalDataDir", "C:\\LocalData"},
    {"SourceDataDir", "C:\\SourceData"},
    {"DmsEditor", """%env:ProgramFiles%\\Notepad++\\Notepad++.exe"" ""%F"" -n%L"},
};

void setInitialStringValue(string_option so, QLineEdit* widget)
{
    const auto& stringOptionsData = sStringOptionsData[so];
    auto regKeyName = stringOptionsData.reg_key;
    auto regKey = GetGeoDmsRegKey(regKeyName);
    if (regKey.empty())
    {
        regKey = stringOptionsData.default_value;
        SetGeoDmsRegKeyString(regKeyName, regKey.c_str());
    }
    widget->setText(regKey.c_str());
}

void DmsAdvancedOptionsWindow::setInitialLocalDataDirValue()
{
    setInitialStringValue(string_option::LocalDataDir, m_ld_input);
}

void DmsAdvancedOptionsWindow::setInitialSourceDatDirValue()
{
    setInitialStringValue(string_option::SourceDataDir, m_sd_input);
}

void DmsAdvancedOptionsWindow::setInitialEditorValue()
{
    const auto& editorOptionsData = sStringOptionsData[string_option::StartEditorCmd];
    auto regKeyName = editorOptionsData.reg_key;
    auto regKey = GetGeoDmsRegKey(regKeyName);
    if (regKey.empty())
    {
        regKey = editorOptionsData.default_value;
        SetGeoDmsRegKeyString(regKeyName, regKey.c_str());
    }

    auto cmd_qstring = QString(regKey.c_str());
    cmd_qstring.replace(QString("\""), QString("\"\"\"")); // only triple quotes will be interpreted as single quote by QProcess::splitCommand
    // replace first two occurences of 
    auto first_index_of_triple_quotes = cmd_qstring.indexOf("\"\"\"");
    if (first_index_of_triple_quotes == 0) // command line program is quoted
        cmd_qstring.replace(first_index_of_triple_quotes, 3, "\"");
        cmd_qstring.replace(cmd_qstring.indexOf("\"\"\""), 3, "\"");

    QStringList args = QProcess::splitCommand(cmd_qstring);
    if (args.isEmpty())
        return;

    auto editor_program = args.takeFirst();
    m_editor_input->setText("\"" + editor_program + "\"");

    QString editor_arguments = args.join(" ");
    if (editor_arguments.isEmpty())
        return;

    m_editor_parameters_input->setText(editor_arguments);
}

void DmsAdvancedOptionsWindow::setInitialMemoryFlushTresholdValue()
{
    auto flush_treshold = RTC_GetRegDWord(RegDWordEnum::MemoryFlushThreshold);
    m_flush_treshold->setValue(flush_treshold);
}

void DmsAdvancedOptionsWindow::restoreOptions()
{
    {
        const QSignalBlocker blocker1(m_ld_input);
        const QSignalBlocker blocker2(m_sd_input);
        const QSignalBlocker blocker3(m_editor_input);
        const QSignalBlocker blocker4(m_flush_treshold);
        const QSignalBlocker blocker5(m_pp0);
        const QSignalBlocker blocker6(m_pp1);
        const QSignalBlocker blocker7(m_pp2);
        const QSignalBlocker blocker8(m_pp3);
        const QSignalBlocker blocker9(m_tracelog);

        setInitialLocalDataDirValue();
        setInitialSourceDatDirValue();
        setInitialEditorValue();
        setInitialMemoryFlushTresholdValue();
        m_pp0->setChecked(IsMultiThreaded0());
        m_pp1->setChecked(IsMultiThreaded1());
        m_pp2->setChecked(IsMultiThreaded2());
        m_pp3->setChecked(IsMultiThreaded3());
        m_tracelog->setChecked(GetRegStatusFlags() & RSF_TraceLogFile);
    }
    setChanged(false);
}

void DmsAdvancedOptionsWindow::cancel()
{
    restoreOptions();
    done(QDialog::Accepted);
}

void DmsAdvancedOptionsWindow::apply()
{
    SetGeoDmsRegKeyString("LocalDataDir", m_ld_input->text().toStdString());
    SetGeoDmsRegKeyString("SourceDataDir", m_sd_input->text().toStdString());
    SetGeoDmsRegKeyString("DmsEditor", (m_editor_input->text() + " " + m_editor_parameters_input->text()).toStdString());

    auto dms_reg_status_flags = GetRegStatusFlags();
    setSF(m_pp0->isChecked(), dms_reg_status_flags, RSF_SuspendForGUI);
    setSF(m_pp1->isChecked(), dms_reg_status_flags, RSF_MultiThreading1);
    setSF(m_pp2->isChecked(), dms_reg_status_flags, RSF_MultiThreading2);
    setSF(m_pp3->isChecked(), dms_reg_status_flags, RSF_MultiThreading3);
    setSF(m_tracelog->isChecked(), dms_reg_status_flags, RSF_TraceLogFile);
    SetRegStatusFlags(dms_reg_status_flags);

    auto flushThreshold = m_flush_treshold->value();
    SetGeoDmsRegKeyDWord("MemoryFlushThreshold", flushThreshold);
    RTC_SetCachedDWord(RegDWordEnum::MemoryFlushThreshold, flushThreshold);

    setChanged(false);
}

void DmsAdvancedOptionsWindow::setChanged(bool isChanged)
{
    m_changed = isChanged;
    m_cancel->setEnabled(isChanged);
    m_undo->setEnabled(isChanged);
}

void DmsAdvancedOptionsWindow::ok()
{
    if (m_changed)
        apply();
    done(QDialog::Accepted);
}

void DmsAdvancedOptionsWindow::onStateChange()
{
    setChanged(true);
}

void DmsAdvancedOptionsWindow::onTextChange()
{
    setChanged(true);
}

void DmsAdvancedOptionsWindow::setLocalDataDirThroughDialog()
{
    auto new_local_data_dir_folder = m_folder_dialog->QFileDialog::getExistingDirectory(this, tr("Open LocalData Directory"), m_ld_input->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!new_local_data_dir_folder.isEmpty())
        m_ld_input->setText(new_local_data_dir_folder);
}

void DmsAdvancedOptionsWindow::setSourceDataDirThroughDialog()
{
    auto new_source_data_dir_folder = m_folder_dialog->QFileDialog::getExistingDirectory(this, tr("Open SourceData Directory"), m_sd_input->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!new_source_data_dir_folder.isEmpty())
        m_sd_input->setText(new_source_data_dir_folder);
}

void DmsAdvancedOptionsWindow::setEditorProgramThroughDialog()
{
    auto new_editor_program = m_folder_dialog->QFileDialog::getOpenFileName(this, tr("Select Editor Program"), "C:/Program Files", tr("Exe files (*.exe)"));;
    if (!new_editor_program.isEmpty())
        m_editor_input->setText("\"" + new_editor_program + "\""); // set quoted .exe editor program
}

void DmsAdvancedOptionsWindow::setDefaultEditorParameters()
{
    auto editor_program = m_editor_input->text();
    if (editor_program.contains("notepad++", Qt::CaseInsensitive))
        m_editor_parameters_input->setText("\"%F\" -n%L");
    if (editor_program.contains("crimson", Qt::CaseInsensitive))
        m_editor_parameters_input->setText("/L:%L \"%F\"");
}

void DmsAdvancedOptionsWindow::onFlushTresholdValueChange(int value)
{
    m_flush_treshold_text->setText(QString::number(value).rightJustified(3, ' ') + "%");
    setChanged(true);
}
//======== END ADVANCED OPTIONS WINDOW ========

//======== BEGIN CONFIG OPTIONS WINDOW ========

bool IsOverridableConfigSetting(const TreeItem* tiCursor)
{
    if (!IsDataItem(tiCursor))
        return false;
    auto adi = AsDataItem(tiCursor);
    auto avu = adi->GetAbstrValuesUnit();
    return avu->GetUnitClass() == Unit<SharedStr>::GetStaticClass();
}


DmsConfigOptionsWindow::DmsConfigOptionsWindow(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString("Config options"));
    setMinimumSize(800, 400);

    auto grid_layout = new QGridLayout(this);
    grid_layout->setVerticalSpacing(0);

    grid_layout->addWidget(new QLabel("Option", this), 0, 0);
    grid_layout->addWidget(new QLabel("Override", this), 0, 1);
    grid_layout->addWidget(new QLabel("Configured value or User and LocalMachine specific overridden value", this), 0, 2);

    unsigned int nrRows = 1;

    auto tiCursor = getFirstOverridableOption();
    if (tiCursor)
    {
        GuiReadLock lockHolder;
        for (; tiCursor; tiCursor = tiCursor->GetNextItem())
        {
            if (!IsOverridableConfigSetting(tiCursor))
                continue;
            auto adi = AsDataItem(tiCursor);
            
//            auto box_layout = new QHBoxLayout(this);
            auto tiName = SharedStr(tiCursor->GetName());
            auto label= new QLabel(tiName.c_str(), this);

            auto option_cbx = new QCheckBox(this);
            auto option_input = new QLineEdit(this);
            connect(option_cbx, &QCheckBox::toggled, this, &DmsConfigOptionsWindow::onCheckboxToggle);
            connect(option_input, &QLineEdit::textChanged, this, &DmsConfigOptionsWindow::onTextChange);

            grid_layout->addWidget(label, nrRows, 0);
            grid_layout->addWidget(option_cbx, nrRows, 1);
            grid_layout->addWidget(option_input, nrRows, 2);

            SharedDataItemInterestPtr data_item = adi;

            PreparedDataReadLock drl(adi); // interestCount held by m_Options;
            auto configuredValue = data_item->GetRefObj()->AsString(0, lockHolder);

            m_Options.emplace_back(ConfigOption{ std::move(tiName), std::move(configuredValue), option_cbx, option_input });

            nrRows++;
        }
    }

    // ok/apply/cancel buttons
    auto box_layout = new QHBoxLayout(this);
    m_ok = new QPushButton("Ok", this);
    m_ok->setMaximumSize(75, 30);
    m_ok->setAutoDefault(true);
    m_ok->setDefault(true);
    m_apply = new QPushButton("Apply", this);
    m_apply->setMaximumSize(75, 30);
    m_undo = new QPushButton("Undo", this);
    m_undo->setMaximumSize(75, 30);

    connect(m_ok, &QPushButton::released, this, &DmsConfigOptionsWindow::ok);
    connect(m_apply, &QPushButton::released, this, &DmsConfigOptionsWindow::apply);
    connect(m_undo, &QPushButton::released, this, &DmsConfigOptionsWindow::resetValues);

    QWidget* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    grid_layout->addWidget(spacer, nrRows+1, 0, 1, 3);

    QWidget* button_spacer = new QWidget(this);
    button_spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    box_layout->addWidget(button_spacer);
    box_layout->addWidget(m_ok);
    box_layout->addWidget(m_apply);
    box_layout->addWidget(m_undo);
    grid_layout->addLayout(box_layout, nrRows+2, 2, 1, 1);

    setWindowModality(Qt::ApplicationModal);
    setAttribute(Qt::WA_DeleteOnClose);
    resetValues();
}

auto DmsConfigOptionsWindow::getFirstOverridableOption() -> const TreeItem*
{
    static TokenID tConfigSettings = GetTokenID_mt("ConfigSettings");
    static TokenID tOverridable = GetTokenID_mt("Overridable");

    auto tiRoot = MainWindow::TheOne()->getRootTreeItem();
    if (!tiRoot) return nullptr;

    auto tiConfigSettings = tiRoot->GetSubTreeItemByID(tConfigSettings);
    if (!tiConfigSettings) return nullptr;

    auto tiOverridableSettings = tiConfigSettings->GetSubTreeItemByID(tOverridable);
    if (!tiOverridableSettings) return nullptr;

    auto tiCursor = tiOverridableSettings->_GetFirstSubItem();
    for (; tiCursor; tiCursor = tiCursor->GetNextItem())
    {
        if (IsOverridableConfigSetting(tiCursor))
            return tiCursor;
    }
    return nullptr;
}

bool DmsConfigOptionsWindow::hasOverridableConfigOptions()
{
    auto firstOption = getFirstOverridableOption();
    return firstOption != nullptr;
}

void DmsConfigOptionsWindow::setChanged(bool isChanged)
{
    m_changed = isChanged;
    m_apply->setEnabled(isChanged);
    m_undo->setEnabled(isChanged);
}

void DmsConfigOptionsWindow::resetValues()
{
    if (m_Options.size())
    {
        RegistryHandleLocalMachineRO regLM;
        for (auto& option : m_Options)
        {
            bool valueExists = regLM.ValueExists(option.name.c_str());
            option.override_cbx->setChecked(valueExists);
        }
    }
    updateAccordingToCheckboxStates();

    setChanged(false);
}

void DmsConfigOptionsWindow::updateAccordingToCheckboxStates()
{
    if (m_Options.size())
    {
        RegistryHandleLocalMachineRO regLM;
        for (auto& option : m_Options)
        {
            bool valueExists = regLM.ValueExists(option.name.c_str());
            bool valueOverridden = option.override_cbx->isChecked();
            if (valueOverridden && valueExists)
                option.override_value->setText(regLM.ReadString(option.name.c_str()).c_str());
            else
                option.override_value->setText(option.configured_value.c_str());

            option.override_value->setEnabled(valueOverridden);
        }
    }
}

void DmsConfigOptionsWindow::onTextChange()
{
    setChanged(true);
}

void DmsConfigOptionsWindow::onCheckboxToggle()
{
    updateAccordingToCheckboxStates();
    setChanged(true);
}

void DmsConfigOptionsWindow::apply()
{
    if (m_Options.size())
    {
        RegistryHandleLocalMachineRW regLM;
        for (auto& option : m_Options)
        {
            bool valueExists = regLM.ValueExists(option.name.c_str());
            bool valueOverridden = option.override_cbx->isChecked();
            if (valueOverridden)
            {
                QByteArray overrideValue = option.override_value->text().toUtf8();
                regLM.WriteString(option.name.c_str(), CharPtrRange(overrideValue.cbegin(), overrideValue.cend()));
            }
            else if (valueExists)
                regLM.DeleteValue(option.name.c_str());
        }
    }

    setChanged(false);
}


void DmsConfigOptionsWindow::ok()
{
    if (m_changed)
        apply();
    done(QDialog::Accepted);
}


void SetRegStatusFlags(UInt32 newSF)
{
    SetGeoDmsRegKeyDWord("StatusFlags", newSF);
    DMS_Appl_SetRegStatusFlags(newSF);
}

//======== END CONFIG OPTIONS WINDOW ========