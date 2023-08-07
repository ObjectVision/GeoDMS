/********************************************************************************
** Form generated from reading UI file 'DmsGuiOptionsWindow.ui'
**
** Created by: Qt User Interface Compiler version 6.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef DMSGUIOPTIONSWINDOW_H
#define DMSGUIOPTIONSWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_DmsGuiOptionsWindow
{
public:
    QLabel *lbl_title;
    QFrame *line;
    QLabel *lbl_treeview;
    QLabel *lbl_show_hidden_items;
    QCheckBox *m_show_hidden_items;
    QCheckBox *m_show_state_colors_in_treeview;
    QPushButton *m_not_calculated_color_ti_button;
    QLabel *lbl_not_yet_calculated;
    QLabel *lbl_failed;
    QLabel *lbl_valid;
    QPushButton *m_valid_color_ti_button;
    QPushButton *m_failed_color_ti_button;
    QLabel *lbl_mapview;
    QFrame *line_2;
    QLabel *lbl_background_color;
    QPushButton *m_background_color_button;
    QLabel *lbl_default_classification_ramp_colors;
    QPushButton *m_end_color_button;
    QPushButton *m_start_color_button;
    QLabel *lbl_end;
    QLabel *lbl_start;
    QFrame *line_3;
    QLabel *lbl_table_statistics;
    QCheckBox *m_show_thousand_separator;
    QPushButton *m_undo;
    QPushButton *m_cancel;
    QPushButton *m_ok;

    void setupUi(QDialog *DmsGuiOptionsWindow)
    {
        if (DmsGuiOptionsWindow->objectName().isEmpty())
            DmsGuiOptionsWindow->setObjectName("DmsGuiOptionsWindow");
        DmsGuiOptionsWindow->resize(699, 444);
        lbl_title = new QLabel(DmsGuiOptionsWindow);
        lbl_title->setObjectName("lbl_title");
        lbl_title->setGeometry(QRect(10, 10, 238, 22));
        QFont font;
        font.setFamilies({QString::fromUtf8("MS Shell Dlg 2")});
        font.setPointSize(10);
        lbl_title->setFont(font);
        line = new QFrame(DmsGuiOptionsWindow);
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
        lbl_treeview = new QLabel(DmsGuiOptionsWindow);
        lbl_treeview->setObjectName("lbl_treeview");
        lbl_treeview->setGeometry(QRect(10, 40, 91, 28));
        QFont font1;
        font1.setPointSize(10);
        lbl_treeview->setFont(font1);
        lbl_treeview->setAutoFillBackground(true);
        lbl_treeview->setFrameShape(QFrame::Box);
        lbl_treeview->setLineWidth(0);
        lbl_show_hidden_items = new QLabel(DmsGuiOptionsWindow);
        lbl_show_hidden_items->setObjectName("lbl_show_hidden_items");
        lbl_show_hidden_items->setGeometry(QRect(10, 80, 171, 16));
        lbl_show_hidden_items->setFont(font1);
        m_show_hidden_items = new QCheckBox(DmsGuiOptionsWindow);
        m_show_hidden_items->setObjectName("m_show_hidden_items");
        m_show_hidden_items->setGeometry(QRect(10, 80, 167, 25));
        m_show_hidden_items->setFont(font1);
        m_show_hidden_items->setChecked(true);
        m_show_state_colors_in_treeview = new QCheckBox(DmsGuiOptionsWindow);
        m_show_state_colors_in_treeview->setObjectName("m_show_state_colors_in_treeview");
        m_show_state_colors_in_treeview->setGeometry(QRect(10, 110, 159, 25));
        m_show_state_colors_in_treeview->setFont(font1);
        m_show_state_colors_in_treeview->setChecked(true);
        m_not_calculated_color_ti_button = new QPushButton(DmsGuiOptionsWindow);
        m_not_calculated_color_ti_button->setObjectName("m_not_calculated_color_ti_button");
        m_not_calculated_color_ti_button->setGeometry(QRect(460, 110, 100, 25));
        m_not_calculated_color_ti_button->setStyleSheet(QString::fromUtf8("background-color:blue;\n"
""));
        lbl_not_yet_calculated = new QLabel(DmsGuiOptionsWindow);
        lbl_not_yet_calculated->setObjectName("lbl_not_yet_calculated");
        lbl_not_yet_calculated->setGeometry(QRect(300, 110, 140, 21));
        lbl_not_yet_calculated->setFont(font);
        lbl_failed = new QLabel(DmsGuiOptionsWindow);
        lbl_failed->setObjectName("lbl_failed");
        lbl_failed->setGeometry(QRect(300, 170, 50, 21));
        lbl_failed->setFont(font);
        lbl_valid = new QLabel(DmsGuiOptionsWindow);
        lbl_valid->setObjectName("lbl_valid");
        lbl_valid->setGeometry(QRect(300, 140, 42, 21));
        lbl_valid->setFont(font);
        m_valid_color_ti_button = new QPushButton(DmsGuiOptionsWindow);
        m_valid_color_ti_button->setObjectName("m_valid_color_ti_button");
        m_valid_color_ti_button->setGeometry(QRect(460, 140, 100, 25));
        m_valid_color_ti_button->setStyleSheet(QString::fromUtf8("background-color:green;\n"
""));
        m_failed_color_ti_button = new QPushButton(DmsGuiOptionsWindow);
        m_failed_color_ti_button->setObjectName("m_failed_color_ti_button");
        m_failed_color_ti_button->setGeometry(QRect(460, 170, 100, 25));
        m_failed_color_ti_button->setStyleSheet(QString::fromUtf8("background-color:red;\n"
""));
        lbl_mapview = new QLabel(DmsGuiOptionsWindow);
        lbl_mapview->setObjectName("lbl_mapview");
        lbl_mapview->setGeometry(QRect(10, 200, 91, 28));
        lbl_mapview->setFont(font1);
        lbl_mapview->setAutoFillBackground(true);
        lbl_mapview->setFrameShape(QFrame::Box);
        lbl_mapview->setLineWidth(0);
        line_2 = new QFrame(DmsGuiOptionsWindow);
        line_2->setObjectName("line_2");
        line_2->setGeometry(QRect(10, 210, 673, 16));
        sizePolicy.setHeightForWidth(line_2->sizePolicy().hasHeightForWidth());
        line_2->setSizePolicy(sizePolicy);
        line_2->setFrameShadow(QFrame::Sunken);
        line_2->setLineWidth(2);
        line_2->setFrameShape(QFrame::HLine);
        lbl_background_color = new QLabel(DmsGuiOptionsWindow);
        lbl_background_color->setObjectName("lbl_background_color");
        lbl_background_color->setGeometry(QRect(10, 240, 133, 21));
        lbl_background_color->setFont(font);
        m_background_color_button = new QPushButton(DmsGuiOptionsWindow);
        m_background_color_button->setObjectName("m_background_color_button");
        m_background_color_button->setGeometry(QRect(460, 240, 100, 25));
        m_background_color_button->setStyleSheet(QString::fromUtf8("background-color:white;\n"
""));
        lbl_default_classification_ramp_colors = new QLabel(DmsGuiOptionsWindow);
        lbl_default_classification_ramp_colors->setObjectName("lbl_default_classification_ramp_colors");
        lbl_default_classification_ramp_colors->setGeometry(QRect(10, 270, 246, 21));
        lbl_default_classification_ramp_colors->setFont(font);
        m_end_color_button = new QPushButton(DmsGuiOptionsWindow);
        m_end_color_button->setObjectName("m_end_color_button");
        m_end_color_button->setGeometry(QRect(460, 300, 100, 25));
        m_end_color_button->setStyleSheet(QString::fromUtf8("background-color:blue;\n"
""));
        m_start_color_button = new QPushButton(DmsGuiOptionsWindow);
        m_start_color_button->setObjectName("m_start_color_button");
        m_start_color_button->setGeometry(QRect(460, 270, 100, 25));
        m_start_color_button->setStyleSheet(QString::fromUtf8("background-color:red;\n"
""));
        lbl_end = new QLabel(DmsGuiOptionsWindow);
        lbl_end->setObjectName("lbl_end");
        lbl_end->setGeometry(QRect(300, 300, 34, 21));
        lbl_end->setFont(font);
        lbl_start = new QLabel(DmsGuiOptionsWindow);
        lbl_start->setObjectName("lbl_start");
        lbl_start->setGeometry(QRect(300, 270, 42, 21));
        lbl_start->setFont(font);
        line_3 = new QFrame(DmsGuiOptionsWindow);
        line_3->setObjectName("line_3");
        line_3->setGeometry(QRect(10, 340, 673, 16));
        sizePolicy.setHeightForWidth(line_3->sizePolicy().hasHeightForWidth());
        line_3->setSizePolicy(sizePolicy);
        line_3->setFrameShadow(QFrame::Sunken);
        line_3->setLineWidth(2);
        line_3->setFrameShape(QFrame::HLine);
        lbl_table_statistics = new QLabel(DmsGuiOptionsWindow);
        lbl_table_statistics->setObjectName("lbl_table_statistics");
        lbl_table_statistics->setGeometry(QRect(10, 330, 191, 28));
        lbl_table_statistics->setFont(font1);
        lbl_table_statistics->setAutoFillBackground(true);
        lbl_table_statistics->setFrameShape(QFrame::Box);
        lbl_table_statistics->setLineWidth(0);
        m_show_thousand_separator = new QCheckBox(DmsGuiOptionsWindow);
        m_show_thousand_separator->setObjectName("m_show_thousand_separator");
        m_show_thousand_separator->setGeometry(QRect(10, 370, 216, 25));
        m_show_thousand_separator->setFont(font1);
        m_undo = new QPushButton(DmsGuiOptionsWindow);
        m_undo->setObjectName("m_undo");
        m_undo->setGeometry(QRect(580, 400, 93, 28));
        m_cancel = new QPushButton(DmsGuiOptionsWindow);
        m_cancel->setObjectName("m_cancel");
        m_cancel->setGeometry(QRect(480, 400, 93, 28));
        m_ok = new QPushButton(DmsGuiOptionsWindow);
        m_ok->setObjectName("m_ok");
        m_ok->setGeometry(QRect(380, 400, 93, 28));
        line_2->raise();
        lbl_title->raise();
        line->raise();
        lbl_treeview->raise();
        lbl_show_hidden_items->raise();
        m_show_hidden_items->raise();
        m_show_state_colors_in_treeview->raise();
        m_not_calculated_color_ti_button->raise();
        lbl_not_yet_calculated->raise();
        lbl_failed->raise();
        lbl_valid->raise();
        m_valid_color_ti_button->raise();
        m_failed_color_ti_button->raise();
        lbl_mapview->raise();
        lbl_background_color->raise();
        m_background_color_button->raise();
        lbl_default_classification_ramp_colors->raise();
        m_end_color_button->raise();
        m_start_color_button->raise();
        lbl_end->raise();
        lbl_start->raise();
        line_3->raise();
        lbl_table_statistics->raise();
        m_show_thousand_separator->raise();
        m_undo->raise();
        m_cancel->raise();
        m_ok->raise();

        retranslateUi(DmsGuiOptionsWindow);

        QMetaObject::connectSlotsByName(DmsGuiOptionsWindow);
    } // setupUi

    void retranslateUi(QDialog *DmsGuiOptionsWindow)
    {
        DmsGuiOptionsWindow->setWindowTitle(QCoreApplication::translate("DmsGuiOptionsWindow", "GUI options", nullptr));
        lbl_title->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Settings for the GeoDMS GUI ", nullptr));
        lbl_treeview->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "<html><head/><body><p><span style=\" font-weight:600;\">TreeView</span></p></body></html>", nullptr));
        lbl_show_hidden_items->setText(QString());
        m_show_hidden_items->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Show hidden items", nullptr));
        m_show_state_colors_in_treeview->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Show state colors", nullptr));
        m_not_calculated_color_ti_button->setText(QString());
        lbl_not_yet_calculated->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Not yet calculated:", nullptr));
        lbl_failed->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Failed:", nullptr));
        lbl_valid->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Valid:", nullptr));
        m_valid_color_ti_button->setText(QString());
        m_failed_color_ti_button->setText(QString());
        lbl_mapview->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "<html><head/><body><p><span style=\" font-weight:600;\">Map View</span></p></body></html>", nullptr));
        lbl_background_color->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Background color:", nullptr));
        m_background_color_button->setText(QString());
        lbl_default_classification_ramp_colors->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Default classification ramp colors", nullptr));
        m_end_color_button->setText(QString());
        m_start_color_button->setText(QString());
        lbl_end->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "End:", nullptr));
        lbl_start->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Start:", nullptr));
        lbl_table_statistics->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "<html><head/><body><p><span style=\" font-weight:600;\">Table/Statistics View</span></p><p><br/></p></body></html>", nullptr));
        m_show_thousand_separator->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Show thousand seperator", nullptr));
        m_undo->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Undo", nullptr));
        m_cancel->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Cancel", nullptr));
        m_ok->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Ok", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DmsGuiOptionsWindow: public Ui_DmsGuiOptionsWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DMSGUIOPTIONSWINDOW_H
