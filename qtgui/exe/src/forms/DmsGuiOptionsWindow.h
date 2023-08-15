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
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_DmsGuiOptionsWindow
{
public:
    QGridLayout *gridLayout_2;
    QGridLayout *gridLayout;
    QFrame *line_3;
    QPushButton *m_start_color_button;
    QLabel *lbl_start;
    QLabel *lbl_default_classification_ramp_colors;
    QPushButton *m_end_color_button;
    QLabel *lbl_table_statistics;
    QLabel *lbl_empty;
    QPushButton *m_ok;
    QCheckBox *m_show_thousand_separator;
    QPushButton *m_background_color_button;
    QLabel *lbl_end;
    QPushButton *m_cancel;
    QPushButton *m_undo;
    QLabel *lbl_title;
    QLabel *lbl_treeview;
    QCheckBox *m_show_state_colors_in_treeview;
    QFrame *line;
    QLabel *lbl_not_yet_calculated;
    QCheckBox *m_show_hidden_items;
    QPushButton *m_not_calculated_color_ti_button;
    QLabel *lbl_background_color;
    QFrame *line_2;
    QPushButton *m_valid_color_ti_button;
    QLabel *lbl_mapview;
    QLabel *lbl_valid;
    QLabel *lbl_failed;
    QPushButton *m_failed_color_ti_button;

    void setupUi(QDialog *DmsGuiOptionsWindow)
    {
        if (DmsGuiOptionsWindow->objectName().isEmpty())
            DmsGuiOptionsWindow->setObjectName("DmsGuiOptionsWindow");
        DmsGuiOptionsWindow->resize(679, 529);
        gridLayout_2 = new QGridLayout(DmsGuiOptionsWindow);
        gridLayout_2->setObjectName("gridLayout_2");
        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        line_3 = new QFrame(DmsGuiOptionsWindow);
        line_3->setObjectName("line_3");
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(line_3->sizePolicy().hasHeightForWidth());
        line_3->setSizePolicy(sizePolicy);
        line_3->setFrameShape(QFrame::HLine);
        line_3->setFrameShadow(QFrame::Sunken);

        gridLayout->addWidget(line_3, 16, 1, 1, 3);

        m_start_color_button = new QPushButton(DmsGuiOptionsWindow);
        m_start_color_button->setObjectName("m_start_color_button");
        m_start_color_button->setStyleSheet(QString::fromUtf8("background-color:red;\n"
""));

        gridLayout->addWidget(m_start_color_button, 13, 3, 1, 1);

        lbl_start = new QLabel(DmsGuiOptionsWindow);
        lbl_start->setObjectName("lbl_start");
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(lbl_start->sizePolicy().hasHeightForWidth());
        lbl_start->setSizePolicy(sizePolicy1);
        QFont font;
        font.setPointSize(10);
        lbl_start->setFont(font);

        gridLayout->addWidget(lbl_start, 13, 2, 1, 1);

        lbl_default_classification_ramp_colors = new QLabel(DmsGuiOptionsWindow);
        lbl_default_classification_ramp_colors->setObjectName("lbl_default_classification_ramp_colors");
        sizePolicy1.setHeightForWidth(lbl_default_classification_ramp_colors->sizePolicy().hasHeightForWidth());
        lbl_default_classification_ramp_colors->setSizePolicy(sizePolicy1);
        lbl_default_classification_ramp_colors->setFont(font);

        gridLayout->addWidget(lbl_default_classification_ramp_colors, 13, 1, 1, 1);

        m_end_color_button = new QPushButton(DmsGuiOptionsWindow);
        m_end_color_button->setObjectName("m_end_color_button");
        m_end_color_button->setStyleSheet(QString::fromUtf8("background-color:blue;\n"
""));

        gridLayout->addWidget(m_end_color_button, 14, 3, 1, 1);

        lbl_table_statistics = new QLabel(DmsGuiOptionsWindow);
        lbl_table_statistics->setObjectName("lbl_table_statistics");
        QFont font1;
        font1.setPointSize(10);
        font1.setBold(true);
        lbl_table_statistics->setFont(font1);

        gridLayout->addWidget(lbl_table_statistics, 15, 1, 1, 3);

        lbl_empty = new QLabel(DmsGuiOptionsWindow);
        lbl_empty->setObjectName("lbl_empty");

        gridLayout->addWidget(lbl_empty, 19, 1, 1, 3);

        m_ok = new QPushButton(DmsGuiOptionsWindow);
        m_ok->setObjectName("m_ok");
        m_ok->setFont(font);

        gridLayout->addWidget(m_ok, 21, 1, 1, 1);

        m_show_thousand_separator = new QCheckBox(DmsGuiOptionsWindow);
        m_show_thousand_separator->setObjectName("m_show_thousand_separator");
        m_show_thousand_separator->setFont(font);

        gridLayout->addWidget(m_show_thousand_separator, 17, 1, 1, 1);

        m_background_color_button = new QPushButton(DmsGuiOptionsWindow);
        m_background_color_button->setObjectName("m_background_color_button");
        m_background_color_button->setStyleSheet(QString::fromUtf8("background-color:white;\n"
""));

        gridLayout->addWidget(m_background_color_button, 12, 3, 1, 1);

        lbl_end = new QLabel(DmsGuiOptionsWindow);
        lbl_end->setObjectName("lbl_end");
        lbl_end->setFont(font);

        gridLayout->addWidget(lbl_end, 14, 2, 1, 1);

        m_cancel = new QPushButton(DmsGuiOptionsWindow);
        m_cancel->setObjectName("m_cancel");
        m_cancel->setFont(font);

        gridLayout->addWidget(m_cancel, 21, 2, 1, 1);

        m_undo = new QPushButton(DmsGuiOptionsWindow);
        m_undo->setObjectName("m_undo");
        m_undo->setFont(font);

        gridLayout->addWidget(m_undo, 21, 3, 1, 1);

        lbl_title = new QLabel(DmsGuiOptionsWindow);
        lbl_title->setObjectName("lbl_title");
        lbl_title->setFont(font);

        gridLayout->addWidget(lbl_title, 0, 1, 1, 3);

        lbl_treeview = new QLabel(DmsGuiOptionsWindow);
        lbl_treeview->setObjectName("lbl_treeview");
        lbl_treeview->setFont(font1);

        gridLayout->addWidget(lbl_treeview, 1, 1, 1, 3);

        m_show_state_colors_in_treeview = new QCheckBox(DmsGuiOptionsWindow);
        m_show_state_colors_in_treeview->setObjectName("m_show_state_colors_in_treeview");
        m_show_state_colors_in_treeview->setFont(font);
        m_show_state_colors_in_treeview->setChecked(true);

        gridLayout->addWidget(m_show_state_colors_in_treeview, 7, 1, 1, 1);

        line = new QFrame(DmsGuiOptionsWindow);
        line->setObjectName("line");
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);

        gridLayout->addWidget(line, 4, 1, 1, 3);

        lbl_not_yet_calculated = new QLabel(DmsGuiOptionsWindow);
        lbl_not_yet_calculated->setObjectName("lbl_not_yet_calculated");
        lbl_not_yet_calculated->setFont(font);

        gridLayout->addWidget(lbl_not_yet_calculated, 7, 2, 1, 1);

        m_show_hidden_items = new QCheckBox(DmsGuiOptionsWindow);
        m_show_hidden_items->setObjectName("m_show_hidden_items");
        m_show_hidden_items->setFont(font);
        m_show_hidden_items->setChecked(true);

        gridLayout->addWidget(m_show_hidden_items, 6, 1, 1, 1);

        m_not_calculated_color_ti_button = new QPushButton(DmsGuiOptionsWindow);
        m_not_calculated_color_ti_button->setObjectName("m_not_calculated_color_ti_button");
        m_not_calculated_color_ti_button->setStyleSheet(QString::fromUtf8("background-color:blue;"));

        gridLayout->addWidget(m_not_calculated_color_ti_button, 7, 3, 1, 1);

        lbl_background_color = new QLabel(DmsGuiOptionsWindow);
        lbl_background_color->setObjectName("lbl_background_color");
        lbl_background_color->setFont(font);

        gridLayout->addWidget(lbl_background_color, 12, 1, 1, 2);

        line_2 = new QFrame(DmsGuiOptionsWindow);
        line_2->setObjectName("line_2");
        line_2->setFrameShape(QFrame::HLine);
        line_2->setFrameShadow(QFrame::Sunken);

        gridLayout->addWidget(line_2, 11, 1, 1, 3);

        m_valid_color_ti_button = new QPushButton(DmsGuiOptionsWindow);
        m_valid_color_ti_button->setObjectName("m_valid_color_ti_button");
        m_valid_color_ti_button->setStyleSheet(QString::fromUtf8("background-color:green;\n"
""));

        gridLayout->addWidget(m_valid_color_ti_button, 8, 3, 1, 1);

        lbl_mapview = new QLabel(DmsGuiOptionsWindow);
        lbl_mapview->setObjectName("lbl_mapview");
        lbl_mapview->setFont(font1);

        gridLayout->addWidget(lbl_mapview, 10, 1, 1, 3);

        lbl_valid = new QLabel(DmsGuiOptionsWindow);
        lbl_valid->setObjectName("lbl_valid");
        lbl_valid->setFont(font);

        gridLayout->addWidget(lbl_valid, 8, 2, 1, 1);

        lbl_failed = new QLabel(DmsGuiOptionsWindow);
        lbl_failed->setObjectName("lbl_failed");
        lbl_failed->setFont(font);

        gridLayout->addWidget(lbl_failed, 9, 2, 1, 1);

        m_failed_color_ti_button = new QPushButton(DmsGuiOptionsWindow);
        m_failed_color_ti_button->setObjectName("m_failed_color_ti_button");
        m_failed_color_ti_button->setStyleSheet(QString::fromUtf8("background-color:red;\n"
""));

        gridLayout->addWidget(m_failed_color_ti_button, 9, 3, 1, 1);


        gridLayout_2->addLayout(gridLayout, 1, 0, 1, 1);


        retranslateUi(DmsGuiOptionsWindow);

        QMetaObject::connectSlotsByName(DmsGuiOptionsWindow);
    } // setupUi

    void retranslateUi(QDialog *DmsGuiOptionsWindow)
    {
        DmsGuiOptionsWindow->setWindowTitle(QCoreApplication::translate("DmsGuiOptionsWindow", "GUI options", nullptr));
        m_start_color_button->setText(QString());
        lbl_start->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Start:", nullptr));
        lbl_default_classification_ramp_colors->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Default classification ramp colors          ", nullptr));
        m_end_color_button->setText(QString());
        lbl_table_statistics->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Table/Statistics View", nullptr));
        lbl_empty->setText(QString());
        m_ok->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Ok", nullptr));
        m_show_thousand_separator->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Show thousand seperator", nullptr));
        m_background_color_button->setText(QString());
        lbl_end->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "End:", nullptr));
        m_cancel->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Cancel", nullptr));
        m_undo->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Undo", nullptr));
        lbl_title->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Settings for the GeoDMS GUI", nullptr));
        lbl_treeview->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "TreeView", nullptr));
        m_show_state_colors_in_treeview->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Show state colors", nullptr));
        lbl_not_yet_calculated->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Not yet calculated:", nullptr));
        m_show_hidden_items->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Show hidden items", nullptr));
        m_not_calculated_color_ti_button->setText(QString());
        lbl_background_color->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Background color:", nullptr));
        m_valid_color_ti_button->setText(QString());
        lbl_mapview->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Map View", nullptr));
        lbl_valid->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Valid:", nullptr));
        lbl_failed->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Failed:", nullptr));
        m_failed_color_ti_button->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class DmsGuiOptionsWindow: public Ui_DmsGuiOptionsWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DMSGUIOPTIONSWINDOW_H
