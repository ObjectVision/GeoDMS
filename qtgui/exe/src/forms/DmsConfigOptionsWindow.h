/********************************************************************************
** Form generated from reading UI file 'DmsConfigOptionsWindow.ui'
**
** Created by: Qt User Interface Compiler version 6.5.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef DMSCONFIGOPTIONSWINDOW_H
#define DMSCONFIGOPTIONSWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_DmsConfigOptionsWindow
{
public:
    QLabel *lbl_title;
    QFrame *line;
    QLabel *lbl_option;
    QLabel *lbl_override;
    QLabel *lbl_configured_or_overridden_value;
    QPushButton *m_ok;
    QPushButton *m_cancel;
    QPushButton *m_undo;

    void setupUi(QDialog *DmsConfigOptionsWindow)
    {
        if (DmsConfigOptionsWindow->objectName().isEmpty())
            DmsConfigOptionsWindow->setObjectName("DmsConfigOptionsWindow");
        DmsConfigOptionsWindow->resize(700, 600);
        lbl_title = new QLabel(DmsConfigOptionsWindow);
        lbl_title->setObjectName("lbl_title");
        lbl_title->setGeometry(QRect(10, 10, 446, 42));
        QFont font;
        font.setFamilies({QString::fromUtf8("MS Shell Dlg 2")});
        font.setPointSize(10);
        lbl_title->setFont(font);
        line = new QFrame(DmsConfigOptionsWindow);
        line->setObjectName("line");
        line->setGeometry(QRect(10, 40, 631, 16));
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(line->sizePolicy().hasHeightForWidth());
        line->setSizePolicy(sizePolicy);
        line->setFrameShadow(QFrame::Sunken);
        line->setLineWidth(2);
        line->setFrameShape(QFrame::HLine);
        lbl_option = new QLabel(DmsConfigOptionsWindow);
        lbl_option->setObjectName("lbl_option");
        lbl_option->setGeometry(QRect(10, 70, 49, 21));
        lbl_option->setFont(font);
        lbl_override = new QLabel(DmsConfigOptionsWindow);
        lbl_override->setObjectName("lbl_override");
        lbl_override->setGeometry(QRect(120, 70, 63, 21));
        lbl_override->setFont(font);
        lbl_configured_or_overridden_value = new QLabel(DmsConfigOptionsWindow);
        lbl_configured_or_overridden_value->setObjectName("lbl_configured_or_overridden_value");
        lbl_configured_or_overridden_value->setGeometry(QRect(220, 70, 217, 21));
        lbl_configured_or_overridden_value->setFont(font);
        m_ok = new QPushButton(DmsConfigOptionsWindow);
        m_ok->setObjectName("m_ok");
        m_ok->setGeometry(QRect(350, 550, 93, 28));
        m_cancel = new QPushButton(DmsConfigOptionsWindow);
        m_cancel->setObjectName("m_cancel");
        m_cancel->setGeometry(QRect(450, 550, 93, 28));
        m_undo = new QPushButton(DmsConfigOptionsWindow);
        m_undo->setObjectName("m_undo");
        m_undo->setGeometry(QRect(550, 550, 93, 28));

        retranslateUi(DmsConfigOptionsWindow);

        QMetaObject::connectSlotsByName(DmsConfigOptionsWindow);
    } // setupUi

    void retranslateUi(QDialog *DmsConfigOptionsWindow)
    {
        DmsConfigOptionsWindow->setWindowTitle(QCoreApplication::translate("DmsConfigOptionsWindow", "Dialog", nullptr));
        lbl_title->setText(QCoreApplication::translate("DmsConfigOptionsWindow", "Settings for user and local machine specific overriden values\n"
"", nullptr));
        lbl_option->setText(QCoreApplication::translate("DmsConfigOptionsWindow", "Option", nullptr));
        lbl_override->setText(QCoreApplication::translate("DmsConfigOptionsWindow", "Override", nullptr));
        lbl_configured_or_overridden_value->setText(QCoreApplication::translate("DmsConfigOptionsWindow", "Configured or overriden value", nullptr));
        m_ok->setText(QCoreApplication::translate("DmsConfigOptionsWindow", "Ok", nullptr));
        m_cancel->setText(QCoreApplication::translate("DmsConfigOptionsWindow", "Cancel", nullptr));
        m_undo->setText(QCoreApplication::translate("DmsConfigOptionsWindow", "Undo", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DmsConfigOptionsWindow: public Ui_DmsConfigOptionsWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // DMSCONFIGOPTIONSWINDOW_H
