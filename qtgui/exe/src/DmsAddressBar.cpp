#include "DmsAddressBar.h"
#include "DmsMainWindow.h"
#include "DmsTreeView.h"
#include "TicInterface.h"

#include <QApplication>
#include <QValidator>
#include <QRegularExpression>


DmsAddressBar::DmsAddressBar(QWidget* parent)
    : QLineEdit(parent) {
    setFont(QApplication::font());
    QRegularExpression rx("^[^0-9=+\\-|&!?><,.{}();\\]\\[][^=+\\-|&!?><,.{}();\\]\\[]+$");
    auto rx_validator = new QRegularExpressionValidator(rx, this);
    setValidator(rx_validator);
    setDmsCompleter();
}

void DmsAddressBar::setDmsCompleter() {
    auto dms_model = MainWindow::TheOne()->m_dms_model.get();
    TreeModelCompleter* completer = new TreeModelCompleter(this);
    completer->setModel(dms_model);
    completer->setSeparator("/");
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    setCompleter(completer);
}

void DmsAddressBar::setPath(CharPtr itemPath) {
    setText(itemPath);
    onEditingFinished();
}

void DmsAddressBar::findItem(const TreeItem* context, QString path, bool updateHistory) {
    if (!context)
        return;

    auto best_item_ref = TreeItem_GetBestItemAndUnfoundPart(context, path.toUtf8());
    auto found_treeitem = best_item_ref.first.get();
    if (!found_treeitem)
        return;
    MainWindow::TheOne()->setCurrentTreeItem(const_cast<TreeItem*>(found_treeitem), updateHistory);
}

void DmsAddressBar::setPathDirectly(QString path) {
    setText(path);
    findItem(MainWindow::TheOne()->getRootTreeItem(), path, false);
}

void DmsAddressBar::onEditingFinished() {
    findItem(MainWindow::TheOne()->getCurrentTreeItemOrRoot(), text().toUtf8(), true);
}