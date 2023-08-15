/********************************************************************************
** Form generated from reading UI file 'DmsLocalMachineOptionsWindow.ui'
**
** Created by: Qt User Interface Compiler version 6.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef DMSLOCALMACHINEOPTIONSWINDOW_H
#define DMSLOCALMACHINEOPTIONSWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>

QT_BEGIN_NAMESPACE

class Ui_DmsLocalMachineOptionsWindow
{
public:
    QGridLayout *gridLayout_2;
    QGridLayout *gridLayout;
    QPushButton *m_editor_folder_dialog;
    QCheckBox *m_pp1;
    QCheckBox *m_pp3;
    QLabel *lbl_max;
    QPushButton *m_cancel;
    QFrame *line_3;
    QPushButton *m_ok;
    QSlider *m_flush_treshold;
    QPushButton *m_undo;
    QLabel *lbl_paths;
    QLabel *lbl_title;
    QLabel *lbl_script;
    QFrame *line_2;
    QLabel *lbl_applications_2;
    QLabel *lbl_applications;
    QLabel *label_5;
    QLabel *label_4;
    QLabel *label_3;
    QLabel *lbl_parallel_processing;
    QLabel *lbl_localdata;
    QLineEdit *m_sd_input;
    QLineEdit *m_editor_input;
    QFrame *line;
    QLabel *lbl_sourcedata;
    QPushButton *m_pp_info;
    QLabel *label;
    QLabel *lbl_current_flush_value;
    QLabel *m_flush_treshold_text;
    QLabel *label_2;
    QLabel *lbl_min;
    QFrame *line_4;
    QLabel *lbl_memory_flushing;
    QFrame *line_5;
    QLabel *lbl_logging;
    QCheckBox *m_tracelog;
    QLineEdit *m_ld_input;
    QPushButton *m_sd_folder_dialog;
    QPushButton *m_ld_folder_dialog;
    QCheckBox *m_pp2;
    QCheckBox *m_pp0;
    QLabel *label_6;
    QPushButton *m_set_editor_parameters;
    QLineEdit *m_editor_parameters_input;

    void setupUi(QDialog *DmsLocalMachineOptionsWindow)
    {
        if (DmsLocalMachineOptionsWindow->objectName().isEmpty())
            DmsLocalMachineOptionsWindow->setObjectName("DmsLocalMachineOptionsWindow");
        DmsLocalMachineOptionsWindow->resize(588, 595);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(DmsLocalMachineOptionsWindow->sizePolicy().hasHeightForWidth());
        DmsLocalMachineOptionsWindow->setSizePolicy(sizePolicy);
        DmsLocalMachineOptionsWindow->setMinimumSize(QSize(0, 0));
        gridLayout_2 = new QGridLayout(DmsLocalMachineOptionsWindow);
        gridLayout_2->setObjectName("gridLayout_2");
        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        m_editor_folder_dialog = new QPushButton(DmsLocalMachineOptionsWindow);
        m_editor_folder_dialog->setObjectName("m_editor_folder_dialog");
        QIcon icon;
        icon.addFile(QString::fromUtf8("../tmp/folder.png"), QSize(), QIcon::Normal, QIcon::Off);
        m_editor_folder_dialog->setIcon(icon);

        gridLayout->addWidget(m_editor_folder_dialog, 9, 10, 1, 1);

        m_pp1 = new QCheckBox(DmsLocalMachineOptionsWindow);
        m_pp1->setObjectName("m_pp1");
        QFont font;
        font.setPointSize(10);
        m_pp1->setFont(font);
        m_pp1->setChecked(true);

        gridLayout->addWidget(m_pp1, 14, 6, 1, 5);

        m_pp3 = new QCheckBox(DmsLocalMachineOptionsWindow);
        m_pp3->setObjectName("m_pp3");
        m_pp3->setFont(font);
        m_pp3->setChecked(true);

        gridLayout->addWidget(m_pp3, 15, 6, 1, 5);

        lbl_max = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_max->setObjectName("lbl_max");
        QFont font1;
        font1.setFamilies({QString::fromUtf8("MS Shell Dlg 2")});
        font1.setPointSize(10);
        lbl_max->setFont(font1);

        gridLayout->addWidget(lbl_max, 19, 10, 1, 1);

        m_cancel = new QPushButton(DmsLocalMachineOptionsWindow);
        m_cancel->setObjectName("m_cancel");
        m_cancel->setFont(font);

        gridLayout->addWidget(m_cancel, 26, 9, 1, 1);

        line_3 = new QFrame(DmsLocalMachineOptionsWindow);
        line_3->setObjectName("line_3");
        QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(line_3->sizePolicy().hasHeightForWidth());
        line_3->setSizePolicy(sizePolicy1);
        line_3->setFrameShadow(QFrame::Sunken);
        line_3->setLineWidth(2);
        line_3->setFrameShape(QFrame::HLine);

        gridLayout->addWidget(line_3, 13, 0, 1, 11);

        m_ok = new QPushButton(DmsLocalMachineOptionsWindow);
        m_ok->setObjectName("m_ok");
        m_ok->setFont(font);

        gridLayout->addWidget(m_ok, 26, 8, 1, 1);

        m_flush_treshold = new QSlider(DmsLocalMachineOptionsWindow);
        m_flush_treshold->setObjectName("m_flush_treshold");
        m_flush_treshold->setMinimum(50);
        m_flush_treshold->setMaximum(100);
        m_flush_treshold->setValue(80);
        m_flush_treshold->setOrientation(Qt::Horizontal);
        m_flush_treshold->setTickPosition(QSlider::TicksBelow);
        m_flush_treshold->setTickInterval(10);

        gridLayout->addWidget(m_flush_treshold, 19, 1, 1, 9);

        m_undo = new QPushButton(DmsLocalMachineOptionsWindow);
        m_undo->setObjectName("m_undo");
        m_undo->setFont(font);

        gridLayout->addWidget(m_undo, 26, 10, 1, 1);

        lbl_paths = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_paths->setObjectName("lbl_paths");
        lbl_paths->setFont(font);
        lbl_paths->setAutoFillBackground(true);
        lbl_paths->setFrameShape(QFrame::Box);
        lbl_paths->setLineWidth(0);

        gridLayout->addWidget(lbl_paths, 2, 0, 1, 1);

        lbl_title = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_title->setObjectName("lbl_title");
        lbl_title->setFont(font1);

        gridLayout->addWidget(lbl_title, 0, 0, 1, 6);

        lbl_script = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_script->setObjectName("lbl_script");
        lbl_script->setFont(font);
        lbl_script->setAutoFillBackground(true);
        lbl_script->setFrameShape(QFrame::Box);
        lbl_script->setLineWidth(0);

        gridLayout->addWidget(lbl_script, 7, 0, 1, 3);

        line_2 = new QFrame(DmsLocalMachineOptionsWindow);
        line_2->setObjectName("line_2");
        sizePolicy1.setHeightForWidth(line_2->sizePolicy().hasHeightForWidth());
        line_2->setSizePolicy(sizePolicy1);
        line_2->setFrameShadow(QFrame::Sunken);
        line_2->setLineWidth(2);
        line_2->setFrameShape(QFrame::HLine);

        gridLayout->addWidget(line_2, 8, 0, 1, 11);

        lbl_applications_2 = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_applications_2->setObjectName("lbl_applications_2");
        lbl_applications_2->setFont(font1);

        gridLayout->addWidget(lbl_applications_2, 10, 0, 1, 2);

        lbl_applications = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_applications->setObjectName("lbl_applications");
        lbl_applications->setFont(font1);

        gridLayout->addWidget(lbl_applications, 9, 0, 1, 2);

        label_5 = new QLabel(DmsLocalMachineOptionsWindow);
        label_5->setObjectName("label_5");

        gridLayout->addWidget(label_5, 21, 0, 1, 1);

        label_4 = new QLabel(DmsLocalMachineOptionsWindow);
        label_4->setObjectName("label_4");

        gridLayout->addWidget(label_4, 16, 0, 1, 1);

        label_3 = new QLabel(DmsLocalMachineOptionsWindow);
        label_3->setObjectName("label_3");

        gridLayout->addWidget(label_3, 11, 0, 1, 1);

        lbl_parallel_processing = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_parallel_processing->setObjectName("lbl_parallel_processing");
        lbl_parallel_processing->setFont(font);
        lbl_parallel_processing->setAutoFillBackground(true);
        lbl_parallel_processing->setFrameShape(QFrame::Box);
        lbl_parallel_processing->setLineWidth(0);

        gridLayout->addWidget(lbl_parallel_processing, 12, 0, 1, 3);

        lbl_localdata = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_localdata->setObjectName("lbl_localdata");
        lbl_localdata->setFont(font1);

        gridLayout->addWidget(lbl_localdata, 4, 0, 1, 2);

        m_sd_input = new QLineEdit(DmsLocalMachineOptionsWindow);
        m_sd_input->setObjectName("m_sd_input");

        gridLayout->addWidget(m_sd_input, 5, 2, 1, 8);

        m_editor_input = new QLineEdit(DmsLocalMachineOptionsWindow);
        m_editor_input->setObjectName("m_editor_input");

        gridLayout->addWidget(m_editor_input, 9, 2, 1, 8);

        line = new QFrame(DmsLocalMachineOptionsWindow);
        line->setObjectName("line");
        sizePolicy1.setHeightForWidth(line->sizePolicy().hasHeightForWidth());
        line->setSizePolicy(sizePolicy1);
        line->setFrameShadow(QFrame::Sunken);
        line->setLineWidth(2);
        line->setFrameShape(QFrame::HLine);

        gridLayout->addWidget(line, 3, 0, 1, 11);

        lbl_sourcedata = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_sourcedata->setObjectName("lbl_sourcedata");
        lbl_sourcedata->setFont(font1);

        gridLayout->addWidget(lbl_sourcedata, 5, 0, 1, 2);

        m_pp_info = new QPushButton(DmsLocalMachineOptionsWindow);
        m_pp_info->setObjectName("m_pp_info");
        QSizePolicy sizePolicy2(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(m_pp_info->sizePolicy().hasHeightForWidth());
        m_pp_info->setSizePolicy(sizePolicy2);
        m_pp_info->setCursor(QCursor(Qt::WhatsThisCursor));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8("../images/DP_ValueInfo.bmp"), QSize(), QIcon::Normal, QIcon::Off);
        m_pp_info->setIcon(icon1);
        m_pp_info->setFlat(true);

        gridLayout->addWidget(m_pp_info, 12, 3, 1, 1);

        label = new QLabel(DmsLocalMachineOptionsWindow);
        label->setObjectName("label");

        gridLayout->addWidget(label, 1, 0, 1, 1);

        lbl_current_flush_value = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_current_flush_value->setObjectName("lbl_current_flush_value");
        lbl_current_flush_value->setFont(font1);

        gridLayout->addWidget(lbl_current_flush_value, 20, 0, 1, 2);

        m_flush_treshold_text = new QLabel(DmsLocalMachineOptionsWindow);
        m_flush_treshold_text->setObjectName("m_flush_treshold_text");
        m_flush_treshold_text->setFont(font1);

        gridLayout->addWidget(m_flush_treshold_text, 20, 2, 1, 1);

        label_2 = new QLabel(DmsLocalMachineOptionsWindow);
        label_2->setObjectName("label_2");

        gridLayout->addWidget(label_2, 6, 0, 1, 1);

        lbl_min = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_min->setObjectName("lbl_min");
        lbl_min->setFont(font1);

        gridLayout->addWidget(lbl_min, 19, 0, 1, 1);

        line_4 = new QFrame(DmsLocalMachineOptionsWindow);
        line_4->setObjectName("line_4");
        sizePolicy1.setHeightForWidth(line_4->sizePolicy().hasHeightForWidth());
        line_4->setSizePolicy(sizePolicy1);
        line_4->setFrameShadow(QFrame::Sunken);
        line_4->setLineWidth(2);
        line_4->setFrameShape(QFrame::HLine);

        gridLayout->addWidget(line_4, 18, 0, 1, 11);

        lbl_memory_flushing = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_memory_flushing->setObjectName("lbl_memory_flushing");
        QFont font2;
        font2.setPointSize(10);
        font2.setBold(true);
        lbl_memory_flushing->setFont(font2);
        lbl_memory_flushing->setAutoFillBackground(true);
        lbl_memory_flushing->setFrameShape(QFrame::Box);
        lbl_memory_flushing->setLineWidth(0);

        gridLayout->addWidget(lbl_memory_flushing, 17, 0, 1, 7);

        line_5 = new QFrame(DmsLocalMachineOptionsWindow);
        line_5->setObjectName("line_5");
        sizePolicy1.setHeightForWidth(line_5->sizePolicy().hasHeightForWidth());
        line_5->setSizePolicy(sizePolicy1);
        line_5->setFrameShadow(QFrame::Sunken);
        line_5->setLineWidth(2);
        line_5->setFrameShape(QFrame::HLine);

        gridLayout->addWidget(line_5, 23, 0, 1, 11);

        lbl_logging = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_logging->setObjectName("lbl_logging");
        lbl_logging->setFont(font2);
        lbl_logging->setAutoFillBackground(true);
        lbl_logging->setFrameShape(QFrame::Box);
        lbl_logging->setLineWidth(0);

        gridLayout->addWidget(lbl_logging, 22, 0, 1, 2);

        m_tracelog = new QCheckBox(DmsLocalMachineOptionsWindow);
        m_tracelog->setObjectName("m_tracelog");
        m_tracelog->setFont(font);
        m_tracelog->setChecked(true);

        gridLayout->addWidget(m_tracelog, 24, 0, 1, 5);

        m_ld_input = new QLineEdit(DmsLocalMachineOptionsWindow);
        m_ld_input->setObjectName("m_ld_input");

        gridLayout->addWidget(m_ld_input, 4, 2, 1, 8);

        m_sd_folder_dialog = new QPushButton(DmsLocalMachineOptionsWindow);
        m_sd_folder_dialog->setObjectName("m_sd_folder_dialog");
        m_sd_folder_dialog->setIcon(icon);

        gridLayout->addWidget(m_sd_folder_dialog, 5, 10, 1, 1);

        m_ld_folder_dialog = new QPushButton(DmsLocalMachineOptionsWindow);
        m_ld_folder_dialog->setObjectName("m_ld_folder_dialog");
        m_ld_folder_dialog->setIcon(icon);

        gridLayout->addWidget(m_ld_folder_dialog, 4, 10, 1, 1);

        m_pp2 = new QCheckBox(DmsLocalMachineOptionsWindow);
        m_pp2->setObjectName("m_pp2");
        m_pp2->setFont(font);
        m_pp2->setChecked(true);

        gridLayout->addWidget(m_pp2, 15, 0, 1, 6);

        m_pp0 = new QCheckBox(DmsLocalMachineOptionsWindow);
        m_pp0->setObjectName("m_pp0");
        m_pp0->setFont(font);
        m_pp0->setChecked(true);

        gridLayout->addWidget(m_pp0, 14, 0, 1, 6);

        label_6 = new QLabel(DmsLocalMachineOptionsWindow);
        label_6->setObjectName("label_6");

        gridLayout->addWidget(label_6, 25, 0, 1, 1);

        m_set_editor_parameters = new QPushButton(DmsLocalMachineOptionsWindow);
        m_set_editor_parameters->setObjectName("m_set_editor_parameters");

        gridLayout->addWidget(m_set_editor_parameters, 10, 7, 1, 4);

        m_editor_parameters_input = new QLineEdit(DmsLocalMachineOptionsWindow);
        m_editor_parameters_input->setObjectName("m_editor_parameters_input");

        gridLayout->addWidget(m_editor_parameters_input, 10, 2, 1, 5);


        gridLayout_2->addLayout(gridLayout, 0, 0, 1, 1);


        retranslateUi(DmsLocalMachineOptionsWindow);

        QMetaObject::connectSlotsByName(DmsLocalMachineOptionsWindow);
    } // setupUi

    void retranslateUi(QDialog *DmsLocalMachineOptionsWindow)
    {
        DmsLocalMachineOptionsWindow->setWindowTitle(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Local machine options", nullptr));
        m_editor_folder_dialog->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "...", nullptr));
        m_pp1->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "1: Tile/segment tasks", nullptr));
        m_pp3->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "3: Pipelined tile operations", nullptr));
        lbl_max->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "100 %", nullptr));
        m_cancel->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Cancel", nullptr));
        m_ok->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Ok", nullptr));
        m_undo->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Undo", nullptr));
        lbl_paths->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p><span style=\" font-weight:600;\">Paths </span></p></body></html>", nullptr));
        lbl_title->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Settings for your local machine", nullptr));
        lbl_script->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p><span style=\" font-weight:600;\">Script Editor </span></p></body></html>", nullptr));
        lbl_applications_2->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Parameters:", nullptr));
        lbl_applications->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Application:", nullptr));
        label_5->setText(QString());
        label_4->setText(QString());
        label_3->setText(QString());
        lbl_parallel_processing->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p><span style=\" font-weight:600;\">Parallel Processing</span></p></body></html>", nullptr));
        lbl_localdata->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Local Data: ", nullptr));
        lbl_sourcedata->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Source Data: ", nullptr));
#if QT_CONFIG(tooltip)
        m_pp_info->setToolTip(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p><span style=\" font-weight:600;\">Parallel processing </span>is a type of computation in which many calculations are carried out simultaneously.</p><p>For more information see <a href=\"https://en.wikipedia.org/wiki/Parallel_computing\"><span style=\" text-decoration: underline; color:#0000ff;\">parrallel computing</span></a>.</p><p><br/>The current options are:</p><p>0: keeps the GUI responsive while the GeoDMS is calcyulating</p><p>1: split up tasks in tiles (for 2 dimensional data) or segments (for 1 dimensional data)</p><p>2: performs multiple operations simultaneously using multiple cores</p><p>3: use multiple cores to pipeline operations on tiles/segments</p><p><br/></p><p>It is advised to use the default settings in most cases. For specific configurations/machines it might be useful to deviate from these setting, usually after an advice to do so.</p><p><br/></p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        m_pp_info->setText(QString());
        label->setText(QString());
        lbl_current_flush_value->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Current Value:", nullptr));
        m_flush_treshold_text->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "80%", nullptr));
        label_2->setText(QString());
        lbl_min->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "50 %", nullptr));
        lbl_memory_flushing->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p>Treshold for memory flushing wait procedure </p></body></html>", nullptr));
        lbl_logging->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p>Logging </p></body></html>", nullptr));
        m_tracelog->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", " Write TraceLog file", nullptr));
        m_sd_folder_dialog->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "...", nullptr));
        m_ld_folder_dialog->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "...", nullptr));
        m_pp2->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "2: Multiple operations simultaneoulsy          ", nullptr));
        m_pp0->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "0: Suspend view update to favor gui          ", nullptr));
        label_6->setText(QString());
        m_set_editor_parameters->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Set default parameters for application", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DmsLocalMachineOptionsWindow: public Ui_DmsLocalMachineOptionsWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DMSLOCALMACHINEOPTIONSWINDOW_H
