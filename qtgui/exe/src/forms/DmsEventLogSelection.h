/********************************************************************************
** Form generated from reading UI file 'DmsEventLogSelection.ui'
**
** Created by: Qt User Interface Compiler version 6.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef DMSEVENTLOGSELECTION_H
#define DMSEVENTLOGSELECTION_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_DmsEventLogTypeSelection
{
public:
    QGroupBox *groupBox;
    QFrame *line;
    QFrame *line_2;
    QLabel *lbl_calculation_progress;
    QLabel *lbl_storage;
    QLabel *lbl_background_layer;
    QFrame *line_3;
    QCheckBox *m_major_trace_filter;
    QCheckBox *m_minor_trace_filter;
    QCheckBox *m_warning_filter;
    QCheckBox *m_error_filter;
    QCheckBox *m_read_filter;
    QCheckBox *m_write_filter;
    QCheckBox *m_connection_filter;
    QCheckBox *m_request_filter;
    QCheckBox *m_category_filter_commands;
    QCheckBox *m_category_filter_other;
    QLineEdit *m_text_filter;
    QFrame *line_4;
    QLabel *lbl_background_layer_2;
    QPushButton *m_activate_text_filter;
    QPushButton *m_clear_text_filter;
    QCheckBox *m_category_filter_memory;
    QFrame *line_5;
    QCheckBox *m_thread;
    QCheckBox *m_opening_new_configuration;
    QCheckBox *m_date_time;
    QLabel *lbl_clear_eventlogafter;
    QLabel *lbl_add_to_each_trace;
    QLabel *lbl_background_layer_3;
    QCheckBox *m_reopening_current_configuration;
    QCheckBox *m_thread_2;
    QFrame *line_6;

    void setupUi(QWidget *DmsEventLogTypeSelection)
    {
        if (DmsEventLogTypeSelection->objectName().isEmpty())
            DmsEventLogTypeSelection->setObjectName("DmsEventLogTypeSelection");
        DmsEventLogTypeSelection->resize(1373, 140);
        groupBox = new QGroupBox(DmsEventLogTypeSelection);
        groupBox->setObjectName("groupBox");
        groupBox->setGeometry(QRect(0, 0, 520, 130));
        groupBox->setAutoFillBackground(false);
        line = new QFrame(groupBox);
        line->setObjectName("line");
        line->setGeometry(QRect(0, 70, 521, 16));
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(line->sizePolicy().hasHeightForWidth());
        line->setSizePolicy(sizePolicy);
        line->setFrameShadow(QFrame::Sunken);
        line->setLineWidth(2);
        line->setFrameShape(QFrame::HLine);
        line_2 = new QFrame(groupBox);
        line_2->setObjectName("line_2");
        line_2->setGeometry(QRect(180, 0, 20, 77));
        line_2->setFrameShadow(QFrame::Sunken);
        line_2->setLineWidth(2);
        line_2->setFrameShape(QFrame::VLine);
        lbl_calculation_progress = new QLabel(groupBox);
        lbl_calculation_progress->setObjectName("lbl_calculation_progress");
        lbl_calculation_progress->setGeometry(QRect(20, 7, 150, 21));
        QFont font;
        font.setFamilies({QString::fromUtf8("MS Shell Dlg 2")});
        font.setPointSize(10);
        lbl_calculation_progress->setFont(font);
        lbl_storage = new QLabel(groupBox);
        lbl_storage->setObjectName("lbl_storage");
        lbl_storage->setGeometry(QRect(210, 7, 57, 21));
        lbl_storage->setFont(font);
        lbl_background_layer = new QLabel(groupBox);
        lbl_background_layer->setObjectName("lbl_background_layer");
        lbl_background_layer->setGeometry(QRect(370, 7, 131, 21));
        lbl_background_layer->setFont(font);
        line_3 = new QFrame(groupBox);
        line_3->setObjectName("line_3");
        line_3->setGeometry(QRect(340, 0, 20, 77));
        line_3->setFrameShadow(QFrame::Sunken);
        line_3->setLineWidth(2);
        line_3->setFrameShape(QFrame::VLine);
        m_major_trace_filter = new QCheckBox(groupBox);
        m_major_trace_filter->setObjectName("m_major_trace_filter");
        m_major_trace_filter->setGeometry(QRect(20, 50, 70, 25));
        QFont font1;
        font1.setPointSize(10);
        m_major_trace_filter->setFont(font1);
        m_major_trace_filter->setStyleSheet(QString::fromUtf8("color: darkgreen;"));
        m_major_trace_filter->setChecked(true);
        m_minor_trace_filter = new QCheckBox(groupBox);
        m_minor_trace_filter->setObjectName("m_minor_trace_filter");
        m_minor_trace_filter->setGeometry(QRect(20, 30, 69, 25));
        m_minor_trace_filter->setFont(font1);
        m_minor_trace_filter->setStyleSheet(QString::fromUtf8("color: ForestGreen;"));
        m_minor_trace_filter->setChecked(false);
        m_warning_filter = new QCheckBox(groupBox);
        m_warning_filter->setObjectName("m_warning_filter");
        m_warning_filter->setGeometry(QRect(140, 90, 86, 25));
        m_warning_filter->setFont(font1);
        m_warning_filter->setStyleSheet(QString::fromUtf8("color: darkorange;"));
        m_warning_filter->setChecked(true);
        m_error_filter = new QCheckBox(groupBox);
        m_error_filter->setObjectName("m_error_filter");
        m_error_filter->setGeometry(QRect(320, 90, 63, 25));
        m_error_filter->setFont(font1);
        m_error_filter->setStyleSheet(QString::fromUtf8("color: red;"));
        m_error_filter->setChecked(true);
        m_read_filter = new QCheckBox(groupBox);
        m_read_filter->setObjectName("m_read_filter");
        m_read_filter->setGeometry(QRect(210, 30, 60, 25));
        m_read_filter->setFont(font1);
        m_read_filter->setStyleSheet(QString::fromUtf8("color: blue;"));
        m_read_filter->setChecked(true);
        m_write_filter = new QCheckBox(groupBox);
        m_write_filter->setObjectName("m_write_filter");
        m_write_filter->setGeometry(QRect(210, 50, 65, 25));
        m_write_filter->setFont(font1);
        m_write_filter->setStyleSheet(QString::fromUtf8("color: navy;"));
        m_write_filter->setChecked(true);
        m_connection_filter = new QCheckBox(groupBox);
        m_connection_filter->setObjectName("m_connection_filter");
        m_connection_filter->setGeometry(QRect(370, 30, 107, 25));
        m_connection_filter->setFont(font1);
        m_connection_filter->setStyleSheet(QString::fromUtf8("color: fuchsia;"));
        m_connection_filter->setChecked(false);
        m_request_filter = new QCheckBox(groupBox);
        m_request_filter->setObjectName("m_request_filter");
        m_request_filter->setGeometry(QRect(370, 50, 83, 25));
        m_request_filter->setFont(font1);
        m_request_filter->setStyleSheet(QString::fromUtf8("color: purple;"));
        m_request_filter->setChecked(false);
        m_category_filter_commands = new QCheckBox(DmsEventLogTypeSelection);
        m_category_filter_commands->setObjectName("m_category_filter_commands");
        m_category_filter_commands->setGeometry(QRect(530, 10, 107, 25));
        m_category_filter_commands->setFont(font1);
        m_category_filter_commands->setStyleSheet(QString::fromUtf8("color: black;"));
        m_category_filter_commands->setChecked(true);
        m_category_filter_other = new QCheckBox(DmsEventLogTypeSelection);
        m_category_filter_other->setObjectName("m_category_filter_other");
        m_category_filter_other->setGeometry(QRect(530, 70, 66, 25));
        m_category_filter_other->setFont(font1);
        m_category_filter_other->setStyleSheet(QString::fromUtf8("color: grey;"));
        m_category_filter_other->setChecked(false);
        m_text_filter = new QLineEdit(DmsEventLogTypeSelection);
        m_text_filter->setObjectName("m_text_filter");
        m_text_filter->setGeometry(QRect(670, 40, 200, 25));
        line_4 = new QFrame(DmsEventLogTypeSelection);
        line_4->setObjectName("line_4");
        line_4->setGeometry(QRect(640, 0, 20, 140));
        line_4->setFrameShadow(QFrame::Sunken);
        line_4->setLineWidth(2);
        line_4->setFrameShape(QFrame::VLine);
        lbl_background_layer_2 = new QLabel(DmsEventLogTypeSelection);
        lbl_background_layer_2->setObjectName("lbl_background_layer_2");
        lbl_background_layer_2->setGeometry(QRect(670, 7, 131, 21));
        lbl_background_layer_2->setFont(font);
        m_activate_text_filter = new QPushButton(DmsEventLogTypeSelection);
        m_activate_text_filter->setObjectName("m_activate_text_filter");
        m_activate_text_filter->setGeometry(QRect(670, 80, 93, 28));
        m_activate_text_filter->setFont(font1);
        m_clear_text_filter = new QPushButton(DmsEventLogTypeSelection);
        m_clear_text_filter->setObjectName("m_clear_text_filter");
        m_clear_text_filter->setGeometry(QRect(780, 80, 93, 28));
        m_clear_text_filter->setFont(font1);
        m_category_filter_memory = new QCheckBox(DmsEventLogTypeSelection);
        m_category_filter_memory->setObjectName("m_category_filter_memory");
        m_category_filter_memory->setGeometry(QRect(530, 40, 87, 25));
        m_category_filter_memory->setFont(font1);
        m_category_filter_memory->setStyleSheet(QString::fromUtf8("color: brown;"));
        m_category_filter_memory->setChecked(false);
        line_5 = new QFrame(DmsEventLogTypeSelection);
        line_5->setObjectName("line_5");
        line_5->setGeometry(QRect(890, 0, 20, 140));
        line_5->setFrameShadow(QFrame::Sunken);
        line_5->setLineWidth(2);
        line_5->setFrameShape(QFrame::VLine);
        m_thread = new QCheckBox(DmsEventLogTypeSelection);
        m_thread->setObjectName("m_thread");
        m_thread->setGeometry(QRect(1180, 80, 136, 25));
        m_thread->setFont(font1);
        m_thread->setChecked(false);
        m_opening_new_configuration = new QCheckBox(DmsEventLogTypeSelection);
        m_opening_new_configuration->setObjectName("m_opening_new_configuration");
        m_opening_new_configuration->setGeometry(QRect(910, 60, 222, 25));
        m_opening_new_configuration->setFont(font1);
        m_opening_new_configuration->setChecked(true);
        m_date_time = new QCheckBox(DmsEventLogTypeSelection);
        m_date_time->setObjectName("m_date_time");
        m_date_time->setGeometry(QRect(1180, 60, 100, 25));
        m_date_time->setFont(font1);
        m_date_time->setChecked(true);
        lbl_clear_eventlogafter = new QLabel(DmsEventLogTypeSelection);
        lbl_clear_eventlogafter->setObjectName("lbl_clear_eventlogafter");
        lbl_clear_eventlogafter->setGeometry(QRect(910, 40, 152, 21));
        lbl_clear_eventlogafter->setFont(font);
        lbl_add_to_each_trace = new QLabel(DmsEventLogTypeSelection);
        lbl_add_to_each_trace->setObjectName("lbl_add_to_each_trace");
        lbl_add_to_each_trace->setGeometry(QRect(1180, 40, 166, 21));
        lbl_add_to_each_trace->setFont(font);
        lbl_background_layer_3 = new QLabel(DmsEventLogTypeSelection);
        lbl_background_layer_3->setObjectName("lbl_background_layer_3");
        lbl_background_layer_3->setGeometry(QRect(910, 7, 178, 21));
        lbl_background_layer_3->setFont(font);
        m_reopening_current_configuration = new QCheckBox(DmsEventLogTypeSelection);
        m_reopening_current_configuration->setObjectName("m_reopening_current_configuration");
        m_reopening_current_configuration->setGeometry(QRect(910, 80, 237, 25));
        m_reopening_current_configuration->setFont(font1);
        m_reopening_current_configuration->setChecked(false);
        m_thread_2 = new QCheckBox(DmsEventLogTypeSelection);
        m_thread_2->setObjectName("m_thread_2");
        m_thread_2->setGeometry(QRect(1180, 100, 136, 25));
        m_thread_2->setFont(font1);
        m_thread_2->setChecked(true);
        line_6 = new QFrame(DmsEventLogTypeSelection);
        line_6->setObjectName("line_6");
        line_6->setGeometry(QRect(1150, 40, 20, 100));
        line_6->setFrameShadow(QFrame::Sunken);
        line_6->setLineWidth(2);
        line_6->setFrameShape(QFrame::VLine);

        retranslateUi(DmsEventLogTypeSelection);

        QMetaObject::connectSlotsByName(DmsEventLogTypeSelection);
    } // setupUi

    void retranslateUi(QWidget *DmsEventLogTypeSelection)
    {
        DmsEventLogTypeSelection->setWindowTitle(QCoreApplication::translate("DmsEventLogTypeSelection", "Eventlog type selection", nullptr));
        groupBox->setTitle(QString());
        lbl_calculation_progress->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "Calculation Progress", nullptr));
        lbl_storage->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "Storage", nullptr));
        lbl_background_layer->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "Background Layer", nullptr));
        m_major_trace_filter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "major", nullptr));
        m_minor_trace_filter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "minor", nullptr));
        m_warning_filter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "warning", nullptr));
        m_error_filter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "error", nullptr));
        m_read_filter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "read", nullptr));
        m_write_filter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "write", nullptr));
        m_connection_filter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "connection", nullptr));
        m_request_filter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "request", nullptr));
        m_category_filter_commands->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "commands", nullptr));
        m_category_filter_other->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "other", nullptr));
        lbl_background_layer_2->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "Filter on text:", nullptr));
        m_activate_text_filter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "Filter", nullptr));
        m_clear_text_filter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "Clear ", nullptr));
        m_category_filter_memory->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "memory", nullptr));
        m_thread->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "thread number", nullptr));
        m_opening_new_configuration->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "opening new configuration", nullptr));
        m_date_time->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "date/time", nullptr));
        lbl_clear_eventlogafter->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "Clear eventlog after:", nullptr));
        lbl_add_to_each_trace->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "Add to each message:", nullptr));
        lbl_background_layer_3->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "Filter message contents", nullptr));
        m_reopening_current_configuration->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "reopen current configuration", nullptr));
        m_thread_2->setText(QCoreApplication::translate("DmsEventLogTypeSelection", "category", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DmsEventLogTypeSelection: public Ui_DmsEventLogTypeSelection {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DMSEVENTLOGSELECTION_H
