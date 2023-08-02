/********************************************************************************
** Form generated from reading UI file 'DmsEventLogTypeSelection.ui'
**
** Created by: Qt User Interface Compiler version 6.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef DMSEVENTLOGTYPESELECTION_H
#define DMSEVENTLOGTYPESELECTION_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
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

    void setupUi(QWidget *DmsEventLogTypeSelection)
    {
        if (DmsEventLogTypeSelection->objectName().isEmpty())
            DmsEventLogTypeSelection->setObjectName("DmsEventLogTypeSelection");
        DmsEventLogTypeSelection->resize(781, 196);
        groupBox = new QGroupBox(DmsEventLogTypeSelection);
        groupBox->setObjectName("groupBox");
        groupBox->setGeometry(QRect(0, 0, 521, 161));
        groupBox->setAutoFillBackground(false);
        line = new QFrame(groupBox);
        line->setObjectName("line");
        line->setGeometry(QRect(0, 100, 521, 16));
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
        line_2->setGeometry(QRect(180, 1, 20, 105));
        line_2->setFrameShadow(QFrame::Plain);
        line_2->setLineWidth(2);
        line_2->setFrameShape(QFrame::VLine);
        lbl_calculation_progress = new QLabel(groupBox);
        lbl_calculation_progress->setObjectName("lbl_calculation_progress");
        lbl_calculation_progress->setGeometry(QRect(20, 10, 150, 21));
        QFont font;
        font.setFamilies({QString::fromUtf8("MS Shell Dlg 2")});
        font.setPointSize(10);
        lbl_calculation_progress->setFont(font);
        lbl_storage = new QLabel(groupBox);
        lbl_storage->setObjectName("lbl_storage");
        lbl_storage->setGeometry(QRect(210, 10, 57, 21));
        lbl_storage->setFont(font);
        lbl_background_layer = new QLabel(groupBox);
        lbl_background_layer->setObjectName("lbl_background_layer");
        lbl_background_layer->setGeometry(QRect(370, 10, 131, 21));
        lbl_background_layer->setFont(font);
        line_3 = new QFrame(groupBox);
        line_3->setObjectName("line_3");
        line_3->setGeometry(QRect(340, 0, 20, 105));
        line_3->setFrameShadow(QFrame::Plain);
        line_3->setLineWidth(2);
        line_3->setFrameShape(QFrame::VLine);
        m_major_trace_filter = new QCheckBox(groupBox);
        m_major_trace_filter->setObjectName("m_major_trace_filter");
        m_major_trace_filter->setGeometry(QRect(20, 70, 70, 25));
        QFont font1;
        font1.setPointSize(10);
        m_major_trace_filter->setFont(font1);
        m_major_trace_filter->setStyleSheet(QString::fromUtf8("color: darkgreen;"));
        m_major_trace_filter->setChecked(true);
        m_minor_trace_filter = new QCheckBox(groupBox);
        m_minor_trace_filter->setObjectName("m_minor_trace_filter");
        m_minor_trace_filter->setGeometry(QRect(20, 40, 69, 25));
        m_minor_trace_filter->setFont(font1);
        m_minor_trace_filter->setStyleSheet(QString::fromUtf8("color: green;"));
        m_minor_trace_filter->setChecked(true);
        m_warning_filter = new QCheckBox(groupBox);
        m_warning_filter->setObjectName("m_warning_filter");
        m_warning_filter->setGeometry(QRect(140, 120, 86, 25));
        m_warning_filter->setFont(font1);
        m_warning_filter->setStyleSheet(QString::fromUtf8("color: darkorange;"));
        m_warning_filter->setChecked(true);
        m_error_filter = new QCheckBox(groupBox);
        m_error_filter->setObjectName("m_error_filter");
        m_error_filter->setGeometry(QRect(320, 120, 63, 25));
        m_error_filter->setFont(font1);
        m_error_filter->setStyleSheet(QString::fromUtf8("color: red;"));
        m_error_filter->setChecked(true);
        m_read_filter = new QCheckBox(groupBox);
        m_read_filter->setObjectName("m_read_filter");
        m_read_filter->setGeometry(QRect(210, 40, 60, 25));
        m_read_filter->setFont(font1);
        m_read_filter->setStyleSheet(QString::fromUtf8("color: blue;"));
        m_read_filter->setChecked(true);
        m_write_filter = new QCheckBox(groupBox);
        m_write_filter->setObjectName("m_write_filter");
        m_write_filter->setGeometry(QRect(210, 70, 65, 25));
        m_write_filter->setFont(font1);
        m_write_filter->setStyleSheet(QString::fromUtf8("color: navy;"));
        m_write_filter->setChecked(true);
        m_connection_filter = new QCheckBox(groupBox);
        m_connection_filter->setObjectName("m_connection_filter");
        m_connection_filter->setGeometry(QRect(370, 40, 107, 25));
        m_connection_filter->setFont(font1);
        m_connection_filter->setStyleSheet(QString::fromUtf8("color: fuchsia;"));
        m_connection_filter->setChecked(false);
        m_request_filter = new QCheckBox(groupBox);
        m_request_filter->setObjectName("m_request_filter");
        m_request_filter->setGeometry(QRect(370, 70, 83, 25));
        m_request_filter->setFont(font1);
        m_request_filter->setStyleSheet(QString::fromUtf8("color: purple;"));
        m_request_filter->setChecked(false);
        m_category_filter_commands = new QCheckBox(DmsEventLogTypeSelection);
        m_category_filter_commands->setObjectName("m_category_filter_commands");
        m_category_filter_commands->setGeometry(QRect(560, 60, 107, 25));
        m_category_filter_commands->setFont(font1);
        m_category_filter_commands->setStyleSheet(QString::fromUtf8("color: black;"));
        m_category_filter_commands->setChecked(true);
        m_category_filter_other = new QCheckBox(DmsEventLogTypeSelection);
        m_category_filter_other->setObjectName("m_category_filter_other");
        m_category_filter_other->setGeometry(QRect(560, 90, 66, 25));
        m_category_filter_other->setFont(font1);
        m_category_filter_other->setStyleSheet(QString::fromUtf8("color: grey;"));
        m_category_filter_other->setChecked(true);

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
    } // retranslateUi

};

namespace Ui {
    class DmsEventLogTypeSelection: public Ui_DmsEventLogTypeSelection {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DMSEVENTLOGTYPESELECTION_H
