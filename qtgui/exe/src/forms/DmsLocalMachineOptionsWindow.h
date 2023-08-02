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
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>

QT_BEGIN_NAMESPACE

class Ui_DmsLocalMachineOptionsWindow
{
public:
    QLabel *lbl_title;
    QFrame *line;
    QLabel *lbl_paths;
    QLabel *lbl_localdata;
    QPushButton *m_ld_folder_dialog;
    QLabel *lbl_sourcedata;
    QPushButton *m_sd_folder_dialog;
    QFrame *line_2;
    QLabel *lbl_script;
    QPushButton *m_editor_folder_dialog;
    QFrame *line_3;
    QLabel *lbl_parallel_processing;
    QCheckBox *m_pp0;
    QCheckBox *m_pp2;
    QCheckBox *m_pp1;
    QCheckBox *m_pp3;
    QFrame *line_4;
    QSlider *m_flush_treshold;
    QLabel *lbl_min;
    QLabel *lbl_max;
    QLabel *m_flush_treshold_text;
    QLabel *lbl_memory_flushing;
    QFrame *line_5;
    QLabel *lbl_logging;
    QCheckBox *m_tracelog;
    QPushButton *m_ok;
    QPushButton *m_cancel;
    QPushButton *m_undo;
    QLineEdit *m_ld_input;
    QLineEdit *m_sd_input;
    QLineEdit *m_editor_input;
    QLabel *lbl_current_flush_value;

    void setupUi(QDialog *DmsLocalMachineOptionsWindow)
    {
        if (DmsLocalMachineOptionsWindow->objectName().isEmpty())
            DmsLocalMachineOptionsWindow->setObjectName("DmsLocalMachineOptionsWindow");
        DmsLocalMachineOptionsWindow->resize(700, 600);
        lbl_title = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_title->setObjectName("lbl_title");
        lbl_title->setGeometry(QRect(10, 10, 228, 21));
        QFont font;
        font.setFamilies({QString::fromUtf8("MS Shell Dlg 2")});
        font.setPointSize(10);
        lbl_title->setFont(font);
        line = new QFrame(DmsLocalMachineOptionsWindow);
        line->setObjectName("line");
        line->setGeometry(QRect(10, 50, 673, 16));
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(line->sizePolicy().hasHeightForWidth());
        line->setSizePolicy(sizePolicy);
        line->setFrameShadow(QFrame::Sunken);
        line->setLineWidth(2);
        line->setFrameShape(QFrame::HLine);
        lbl_paths = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_paths->setObjectName("lbl_paths");
        lbl_paths->setGeometry(QRect(10, 40, 61, 28));
        QFont font1;
        font1.setPointSize(10);
        lbl_paths->setFont(font1);
        lbl_paths->setAutoFillBackground(true);
        lbl_paths->setFrameShape(QFrame::Box);
        lbl_paths->setLineWidth(0);
        lbl_localdata = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_localdata->setObjectName("lbl_localdata");
        lbl_localdata->setGeometry(QRect(10, 74, 90, 21));
        lbl_localdata->setFont(font);
        m_ld_folder_dialog = new QPushButton(DmsLocalMachineOptionsWindow);
        m_ld_folder_dialog->setObjectName("m_ld_folder_dialog");
        m_ld_folder_dialog->setGeometry(QRect(640, 70, 30, 25));
        QIcon icon;
        icon.addFile(QString::fromUtf8("../tmp/folder.png"), QSize(), QIcon::Normal, QIcon::Off);
        m_ld_folder_dialog->setIcon(icon);
        m_ld_folder_dialog->setIconSize(QSize(24, 24));
        lbl_sourcedata = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_sourcedata->setObjectName("lbl_sourcedata");
        lbl_sourcedata->setGeometry(QRect(10, 104, 102, 21));
        lbl_sourcedata->setFont(font);
        m_sd_folder_dialog = new QPushButton(DmsLocalMachineOptionsWindow);
        m_sd_folder_dialog->setObjectName("m_sd_folder_dialog");
        m_sd_folder_dialog->setGeometry(QRect(640, 100, 30, 25));
        m_sd_folder_dialog->setIcon(icon);
        m_sd_folder_dialog->setIconSize(QSize(24, 24));
        line_2 = new QFrame(DmsLocalMachineOptionsWindow);
        line_2->setObjectName("line_2");
        line_2->setGeometry(QRect(10, 160, 673, 16));
        sizePolicy.setHeightForWidth(line_2->sizePolicy().hasHeightForWidth());
        line_2->setSizePolicy(sizePolicy);
        line_2->setFrameShadow(QFrame::Sunken);
        line_2->setLineWidth(2);
        line_2->setFrameShape(QFrame::HLine);
        lbl_script = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_script->setObjectName("lbl_script");
        lbl_script->setGeometry(QRect(10, 150, 121, 21));
        lbl_script->setFont(font1);
        lbl_script->setAutoFillBackground(true);
        lbl_script->setFrameShape(QFrame::Box);
        lbl_script->setLineWidth(0);
        m_editor_folder_dialog = new QPushButton(DmsLocalMachineOptionsWindow);
        m_editor_folder_dialog->setObjectName("m_editor_folder_dialog");
        m_editor_folder_dialog->setGeometry(QRect(640, 180, 30, 25));
        m_editor_folder_dialog->setIcon(icon);
        m_editor_folder_dialog->setIconSize(QSize(24, 24));
        line_3 = new QFrame(DmsLocalMachineOptionsWindow);
        line_3->setObjectName("line_3");
        line_3->setGeometry(QRect(10, 240, 673, 16));
        sizePolicy.setHeightForWidth(line_3->sizePolicy().hasHeightForWidth());
        line_3->setSizePolicy(sizePolicy);
        line_3->setFrameShadow(QFrame::Sunken);
        line_3->setLineWidth(2);
        line_3->setFrameShape(QFrame::HLine);
        lbl_parallel_processing = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_parallel_processing->setObjectName("lbl_parallel_processing");
        lbl_parallel_processing->setGeometry(QRect(10, 230, 181, 21));
        lbl_parallel_processing->setFont(font1);
        lbl_parallel_processing->setAutoFillBackground(true);
        lbl_parallel_processing->setFrameShape(QFrame::Box);
        lbl_parallel_processing->setLineWidth(0);
        m_pp0 = new QCheckBox(DmsLocalMachineOptionsWindow);
        m_pp0->setObjectName("m_pp0");
        m_pp0->setGeometry(QRect(10, 260, 293, 25));
        m_pp0->setFont(font1);
        m_pp0->setChecked(true);
        m_pp2 = new QCheckBox(DmsLocalMachineOptionsWindow);
        m_pp2->setObjectName("m_pp2");
        m_pp2->setGeometry(QRect(10, 290, 303, 25));
        m_pp2->setFont(font1);
        m_pp2->setChecked(true);
        m_pp1 = new QCheckBox(DmsLocalMachineOptionsWindow);
        m_pp1->setObjectName("m_pp1");
        m_pp1->setGeometry(QRect(350, 260, 189, 25));
        m_pp1->setFont(font1);
        m_pp1->setChecked(true);
        m_pp3 = new QCheckBox(DmsLocalMachineOptionsWindow);
        m_pp3->setObjectName("m_pp3");
        m_pp3->setGeometry(QRect(350, 290, 224, 25));
        m_pp3->setFont(font1);
        m_pp3->setChecked(true);
        line_4 = new QFrame(DmsLocalMachineOptionsWindow);
        line_4->setObjectName("line_4");
        line_4->setGeometry(QRect(10, 350, 673, 16));
        sizePolicy.setHeightForWidth(line_4->sizePolicy().hasHeightForWidth());
        line_4->setSizePolicy(sizePolicy);
        line_4->setFrameShadow(QFrame::Sunken);
        line_4->setLineWidth(2);
        line_4->setFrameShape(QFrame::HLine);
        m_flush_treshold = new QSlider(DmsLocalMachineOptionsWindow);
        m_flush_treshold->setObjectName("m_flush_treshold");
        m_flush_treshold->setGeometry(QRect(64, 380, 551, 22));
        m_flush_treshold->setMinimum(50);
        m_flush_treshold->setMaximum(100);
        m_flush_treshold->setValue(80);
        m_flush_treshold->setOrientation(Qt::Horizontal);
        m_flush_treshold->setTickPosition(QSlider::TicksBelow);
        m_flush_treshold->setTickInterval(10);
        lbl_min = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_min->setObjectName("lbl_min");
        lbl_min->setGeometry(QRect(10, 380, 40, 21));
        lbl_min->setFont(font);
        lbl_max = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_max->setObjectName("lbl_max");
        lbl_max->setGeometry(QRect(630, 380, 49, 21));
        lbl_max->setFont(font);
        m_flush_treshold_text = new QLabel(DmsLocalMachineOptionsWindow);
        m_flush_treshold_text->setObjectName("m_flush_treshold_text");
        m_flush_treshold_text->setGeometry(QRect(140, 420, 35, 21));
        m_flush_treshold_text->setFont(font);
        lbl_memory_flushing = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_memory_flushing->setObjectName("lbl_memory_flushing");
        lbl_memory_flushing->setGeometry(QRect(10, 340, 401, 21));
        QFont font2;
        font2.setPointSize(10);
        font2.setBold(true);
        lbl_memory_flushing->setFont(font2);
        lbl_memory_flushing->setAutoFillBackground(true);
        lbl_memory_flushing->setFrameShape(QFrame::Box);
        lbl_memory_flushing->setLineWidth(0);
        line_5 = new QFrame(DmsLocalMachineOptionsWindow);
        line_5->setObjectName("line_5");
        line_5->setGeometry(QRect(10, 470, 673, 16));
        sizePolicy.setHeightForWidth(line_5->sizePolicy().hasHeightForWidth());
        line_5->setSizePolicy(sizePolicy);
        line_5->setFrameShadow(QFrame::Sunken);
        line_5->setLineWidth(2);
        line_5->setFrameShape(QFrame::HLine);
        lbl_logging = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_logging->setObjectName("lbl_logging");
        lbl_logging->setGeometry(QRect(10, 460, 81, 21));
        lbl_logging->setFont(font2);
        lbl_logging->setAutoFillBackground(true);
        lbl_logging->setFrameShape(QFrame::Box);
        lbl_logging->setLineWidth(0);
        m_tracelog = new QCheckBox(DmsLocalMachineOptionsWindow);
        m_tracelog->setObjectName("m_tracelog");
        m_tracelog->setGeometry(QRect(10, 490, 172, 25));
        m_tracelog->setFont(font1);
        m_tracelog->setChecked(true);
        m_ok = new QPushButton(DmsLocalMachineOptionsWindow);
        m_ok->setObjectName("m_ok");
        m_ok->setGeometry(QRect(380, 560, 93, 28));
        m_cancel = new QPushButton(DmsLocalMachineOptionsWindow);
        m_cancel->setObjectName("m_cancel");
        m_cancel->setGeometry(QRect(480, 560, 93, 28));
        m_undo = new QPushButton(DmsLocalMachineOptionsWindow);
        m_undo->setObjectName("m_undo");
        m_undo->setGeometry(QRect(580, 560, 93, 28));
        m_ld_input = new QLineEdit(DmsLocalMachineOptionsWindow);
        m_ld_input->setObjectName("m_ld_input");
        m_ld_input->setGeometry(QRect(110, 70, 521, 25));
        m_sd_input = new QLineEdit(DmsLocalMachineOptionsWindow);
        m_sd_input->setObjectName("m_sd_input");
        m_sd_input->setGeometry(QRect(110, 100, 521, 25));
        m_editor_input = new QLineEdit(DmsLocalMachineOptionsWindow);
        m_editor_input->setObjectName("m_editor_input");
        m_editor_input->setGeometry(QRect(10, 180, 621, 25));
        lbl_current_flush_value = new QLabel(DmsLocalMachineOptionsWindow);
        lbl_current_flush_value->setObjectName("lbl_current_flush_value");
        lbl_current_flush_value->setGeometry(QRect(10, 420, 107, 21));
        lbl_current_flush_value->setFont(font);
        line->raise();
        lbl_title->raise();
        lbl_paths->raise();
        lbl_localdata->raise();
        m_ld_folder_dialog->raise();
        lbl_sourcedata->raise();
        m_sd_folder_dialog->raise();
        line_2->raise();
        lbl_script->raise();
        m_editor_folder_dialog->raise();
        line_3->raise();
        lbl_parallel_processing->raise();
        m_pp0->raise();
        m_pp2->raise();
        m_pp1->raise();
        m_pp3->raise();
        line_4->raise();
        m_flush_treshold->raise();
        lbl_min->raise();
        lbl_max->raise();
        m_flush_treshold_text->raise();
        lbl_memory_flushing->raise();
        line_5->raise();
        lbl_logging->raise();
        m_tracelog->raise();
        m_ok->raise();
        m_cancel->raise();
        m_undo->raise();
        m_ld_input->raise();
        m_sd_input->raise();
        m_editor_input->raise();
        lbl_current_flush_value->raise();

        retranslateUi(DmsLocalMachineOptionsWindow);

        QMetaObject::connectSlotsByName(DmsLocalMachineOptionsWindow);
    } // setupUi

    void retranslateUi(QDialog *DmsLocalMachineOptionsWindow)
    {
        DmsLocalMachineOptionsWindow->setWindowTitle(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "LocalMachine options", nullptr));
        lbl_title->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Settings for your local machine", nullptr));
        lbl_paths->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p><span style=\" font-weight:600;\">Paths</span></p></body></html>", nullptr));
        lbl_localdata->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Local Data: ", nullptr));
        m_ld_folder_dialog->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "...", nullptr));
        lbl_sourcedata->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Source Data: ", nullptr));
        m_sd_folder_dialog->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "...", nullptr));
        lbl_script->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p><span style=\" font-weight:600;\">Script Editor</span></p></body></html>", nullptr));
        m_editor_folder_dialog->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "...", nullptr));
        lbl_parallel_processing->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p><span style=\" font-weight:600;\">Parallel Processing</span></p></body></html>", nullptr));
        m_pp0->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "0: Suspend view update to favor gui", nullptr));
        m_pp2->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "2: Multiple operations simultaneoulsy", nullptr));
        m_pp1->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "1: Tile/segment tasks", nullptr));
        m_pp3->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "3: Pipelined tile operations", nullptr));
        lbl_min->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "50 %", nullptr));
        lbl_max->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "100 %", nullptr));
        m_flush_treshold_text->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "80%", nullptr));
        lbl_memory_flushing->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p>Treshold for memory flushing wait procedure</p></body></html>", nullptr));
        lbl_logging->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "<html><head/><body><p>Logging</p></body></html>", nullptr));
        m_tracelog->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", " Write TraceLog file", nullptr));
        m_ok->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Ok", nullptr));
        m_cancel->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Cancel", nullptr));
        m_undo->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Undo", nullptr));
        lbl_current_flush_value->setText(QCoreApplication::translate("DmsLocalMachineOptionsWindow", "Current Value:", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DmsLocalMachineOptionsWindow: public Ui_DmsLocalMachineOptionsWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DMSLOCALMACHINEOPTIONSWINDOW_H
