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
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_DmsGuiOptionsWindow
{
public:
    QGridLayout *gridLayout_2;
    QGridLayout *gridLayout;
    QLabel *lbl_mapview;
    QLabel *lbl_valid;
    QLabel *lbl_default_classification_ramp_colors;
    QPushButton *m_failed_color_ti_button;
    QLabel *lbl_failed;
    QFrame *line_3;
    QLabel *lbl_background_color;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *m_ok;
    QPushButton *m_cancel;
    QPushButton *m_undo;
    QCheckBox *m_show_thousand_separator;
    QPushButton *m_end_color_button;
    QPushButton *m_start_color_button;
    QPushButton *m_background_color_button;
    QLabel *lbl_treeview;
    QLabel *lbl_start;
    QLabel *lbl_table_statistics;
    QLabel *lbl_empty;
    QLabel *lbl_title;
    QPushButton *m_not_calculated_color_ti_button;
    QCheckBox *m_show_state_colors_in_treeview;
    QLabel *lbl_end;
    QLabel *lbl_not_yet_calculated;
    QFrame *line;
    QFrame *line_2;
    QPushButton *m_valid_color_ti_button;
    QCheckBox *m_show_hidden_items;
    QLabel *label;

    void setupUi(QDialog *DmsGuiOptionsWindow)
    {
        if (DmsGuiOptionsWindow->objectName().isEmpty())
            DmsGuiOptionsWindow->setObjectName("DmsGuiOptionsWindow");
        DmsGuiOptionsWindow->resize(573, 434);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(DmsGuiOptionsWindow->sizePolicy().hasHeightForWidth());
        DmsGuiOptionsWindow->setSizePolicy(sizePolicy);
        DmsGuiOptionsWindow->setMinimumSize(QSize(0, 0));
        gridLayout_2 = new QGridLayout(DmsGuiOptionsWindow);
        gridLayout_2->setObjectName("gridLayout_2");
        gridLayout = new QGridLayout();
        gridLayout->setObjectName("gridLayout");
        lbl_mapview = new QLabel(DmsGuiOptionsWindow);
        lbl_mapview->setObjectName("lbl_mapview");
        QFont font;
        font.setPointSize(10);
        font.setBold(true);
        lbl_mapview->setFont(font);

        gridLayout->addWidget(lbl_mapview, 11, 1, 1, 3);

        lbl_valid = new QLabel(DmsGuiOptionsWindow);
        lbl_valid->setObjectName("lbl_valid");
        QFont font1;
        font1.setPointSize(10);
        lbl_valid->setFont(font1);

        gridLayout->addWidget(lbl_valid, 9, 2, 1, 1);

        lbl_default_classification_ramp_colors = new QLabel(DmsGuiOptionsWindow);
        lbl_default_classification_ramp_colors->setObjectName("lbl_default_classification_ramp_colors");
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(lbl_default_classification_ramp_colors->sizePolicy().hasHeightForWidth());
        lbl_default_classification_ramp_colors->setSizePolicy(sizePolicy1);
        lbl_default_classification_ramp_colors->setFont(font1);

        gridLayout->addWidget(lbl_default_classification_ramp_colors, 14, 1, 1, 1);

        m_failed_color_ti_button = new QPushButton(DmsGuiOptionsWindow);
        m_failed_color_ti_button->setObjectName("m_failed_color_ti_button");
        m_failed_color_ti_button->setFont(font1);
        m_failed_color_ti_button->setStyleSheet(QString::fromUtf8("background-color:red;\n"
""));

        gridLayout->addWidget(m_failed_color_ti_button, 10, 3, 1, 1);

        lbl_failed = new QLabel(DmsGuiOptionsWindow);
        lbl_failed->setObjectName("lbl_failed");
        lbl_failed->setFont(font1);

        gridLayout->addWidget(lbl_failed, 10, 2, 1, 1);

        line_3 = new QFrame(DmsGuiOptionsWindow);
        line_3->setObjectName("line_3");
        QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(line_3->sizePolicy().hasHeightForWidth());
        line_3->setSizePolicy(sizePolicy2);
        line_3->setFont(font1);
        line_3->setFrameShape(QFrame::HLine);
        line_3->setFrameShadow(QFrame::Sunken);

        gridLayout->addWidget(line_3, 17, 1, 1, 3);

        lbl_background_color = new QLabel(DmsGuiOptionsWindow);
        lbl_background_color->setObjectName("lbl_background_color");
        lbl_background_color->setFont(font1);

        gridLayout->addWidget(lbl_background_color, 13, 1, 1, 2);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        m_ok = new QPushButton(DmsGuiOptionsWindow);
        m_ok->setObjectName("m_ok");
        m_ok->setFont(font1);

        horizontalLayout_2->addWidget(m_ok);

        m_cancel = new QPushButton(DmsGuiOptionsWindow);
        m_cancel->setObjectName("m_cancel");
        m_cancel->setFont(font1);

        horizontalLayout_2->addWidget(m_cancel);

        m_undo = new QPushButton(DmsGuiOptionsWindow);
        m_undo->setObjectName("m_undo");
        m_undo->setFont(font1);

        horizontalLayout_2->addWidget(m_undo);


        gridLayout->addLayout(horizontalLayout_2, 21, 3, 1, 1);

        m_show_thousand_separator = new QCheckBox(DmsGuiOptionsWindow);
        m_show_thousand_separator->setObjectName("m_show_thousand_separator");
        m_show_thousand_separator->setFont(font1);

        gridLayout->addWidget(m_show_thousand_separator, 18, 1, 1, 2);

        m_end_color_button = new QPushButton(DmsGuiOptionsWindow);
        m_end_color_button->setObjectName("m_end_color_button");
        m_end_color_button->setFont(font1);
        m_end_color_button->setStyleSheet(QString::fromUtf8("background-color:blue;\n"
""));

        gridLayout->addWidget(m_end_color_button, 15, 3, 1, 1);

        m_start_color_button = new QPushButton(DmsGuiOptionsWindow);
        m_start_color_button->setObjectName("m_start_color_button");
        m_start_color_button->setFont(font1);
        m_start_color_button->setStyleSheet(QString::fromUtf8("background-color:red;\n"
""));

        gridLayout->addWidget(m_start_color_button, 14, 3, 1, 1);

        m_background_color_button = new QPushButton(DmsGuiOptionsWindow);
        m_background_color_button->setObjectName("m_background_color_button");
        m_background_color_button->setFont(font1);
        m_background_color_button->setStyleSheet(QString::fromUtf8("background-color:white;\n"
""));

        gridLayout->addWidget(m_background_color_button, 13, 3, 1, 1);

        lbl_treeview = new QLabel(DmsGuiOptionsWindow);
        lbl_treeview->setObjectName("lbl_treeview");
        lbl_treeview->setFont(font);

        gridLayout->addWidget(lbl_treeview, 2, 1, 1, 3);

        lbl_start = new QLabel(DmsGuiOptionsWindow);
        lbl_start->setObjectName("lbl_start");
        sizePolicy1.setHeightForWidth(lbl_start->sizePolicy().hasHeightForWidth());
        lbl_start->setSizePolicy(sizePolicy1);
        lbl_start->setFont(font1);

        gridLayout->addWidget(lbl_start, 14, 2, 1, 1);

        lbl_table_statistics = new QLabel(DmsGuiOptionsWindow);
        lbl_table_statistics->setObjectName("lbl_table_statistics");
        lbl_table_statistics->setFont(font);

        gridLayout->addWidget(lbl_table_statistics, 16, 1, 1, 3);

        lbl_empty = new QLabel(DmsGuiOptionsWindow);
        lbl_empty->setObjectName("lbl_empty");
        lbl_empty->setFont(font1);

        gridLayout->addWidget(lbl_empty, 20, 1, 1, 3);

        lbl_title = new QLabel(DmsGuiOptionsWindow);
        lbl_title->setObjectName("lbl_title");
        lbl_title->setFont(font1);

        gridLayout->addWidget(lbl_title, 0, 1, 1, 3);

        m_not_calculated_color_ti_button = new QPushButton(DmsGuiOptionsWindow);
        m_not_calculated_color_ti_button->setObjectName("m_not_calculated_color_ti_button");
        m_not_calculated_color_ti_button->setFont(font1);
        m_not_calculated_color_ti_button->setStyleSheet(QString::fromUtf8("background-color:blue;"));

        gridLayout->addWidget(m_not_calculated_color_ti_button, 8, 3, 1, 1);

        m_show_state_colors_in_treeview = new QCheckBox(DmsGuiOptionsWindow);
        m_show_state_colors_in_treeview->setObjectName("m_show_state_colors_in_treeview");
        m_show_state_colors_in_treeview->setFont(font1);
        m_show_state_colors_in_treeview->setChecked(true);

        gridLayout->addWidget(m_show_state_colors_in_treeview, 8, 1, 1, 1);

        lbl_end = new QLabel(DmsGuiOptionsWindow);
        lbl_end->setObjectName("lbl_end");
        lbl_end->setFont(font1);

        gridLayout->addWidget(lbl_end, 15, 2, 1, 1);

        lbl_not_yet_calculated = new QLabel(DmsGuiOptionsWindow);
        lbl_not_yet_calculated->setObjectName("lbl_not_yet_calculated");
        lbl_not_yet_calculated->setFont(font1);

        gridLayout->addWidget(lbl_not_yet_calculated, 8, 2, 1, 1);

        line = new QFrame(DmsGuiOptionsWindow);
        line->setObjectName("line");
        line->setFont(font1);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);

        gridLayout->addWidget(line, 5, 1, 1, 3);

        line_2 = new QFrame(DmsGuiOptionsWindow);
        line_2->setObjectName("line_2");
        line_2->setFont(font1);
        line_2->setFrameShape(QFrame::HLine);
        line_2->setFrameShadow(QFrame::Sunken);

        gridLayout->addWidget(line_2, 12, 1, 1, 3);

        m_valid_color_ti_button = new QPushButton(DmsGuiOptionsWindow);
        m_valid_color_ti_button->setObjectName("m_valid_color_ti_button");
        m_valid_color_ti_button->setFont(font1);
        m_valid_color_ti_button->setStyleSheet(QString::fromUtf8("background-color:green;\n"
""));

        gridLayout->addWidget(m_valid_color_ti_button, 9, 3, 1, 1);

        m_show_hidden_items = new QCheckBox(DmsGuiOptionsWindow);
        m_show_hidden_items->setObjectName("m_show_hidden_items");
        m_show_hidden_items->setFont(font1);
        m_show_hidden_items->setChecked(true);

        gridLayout->addWidget(m_show_hidden_items, 7, 1, 1, 1);

        label = new QLabel(DmsGuiOptionsWindow);
        label->setObjectName("label");

        gridLayout->addWidget(label, 1, 1, 1, 1);


        gridLayout_2->addLayout(gridLayout, 1, 0, 1, 1);


        retranslateUi(DmsGuiOptionsWindow);

        QMetaObject::connectSlotsByName(DmsGuiOptionsWindow);
    } // setupUi

    void retranslateUi(QDialog *DmsGuiOptionsWindow)
    {
        DmsGuiOptionsWindow->setWindowTitle(QCoreApplication::translate("DmsGuiOptionsWindow", "GUI options", nullptr));
        lbl_mapview->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Map View", nullptr));
        lbl_valid->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Valid:", nullptr));
        lbl_default_classification_ramp_colors->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Default classification ramp colors  ", nullptr));
        m_failed_color_ti_button->setText(QString());
        lbl_failed->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Failed:", nullptr));
        lbl_background_color->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Background color:", nullptr));
        m_ok->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Ok", nullptr));
        m_cancel->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Cancel", nullptr));
        m_undo->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Undo", nullptr));
        m_show_thousand_separator->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Show thousand seperator", nullptr));
        m_end_color_button->setText(QString());
        m_start_color_button->setText(QString());
        m_background_color_button->setText(QString());
        lbl_treeview->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "TreeView", nullptr));
        lbl_start->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Start:", nullptr));
        lbl_table_statistics->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Table/Statistics View", nullptr));
        lbl_empty->setText(QString());
        lbl_title->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Settings for the GeoDMS GUI", nullptr));
        m_not_calculated_color_ti_button->setText(QString());
        m_show_state_colors_in_treeview->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Show state colors", nullptr));
        lbl_end->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "End:", nullptr));
        lbl_not_yet_calculated->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Not yet calculated:", nullptr));
        m_valid_color_ti_button->setText(QString());
        m_show_hidden_items->setText(QCoreApplication::translate("DmsGuiOptionsWindow", "Show hidden items", nullptr));
        label->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class DmsGuiOptionsWindow: public Ui_DmsGuiOptionsWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DMSGUIOPTIONSWINDOW_H
