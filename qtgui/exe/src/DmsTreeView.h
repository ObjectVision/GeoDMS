#include <QPointer.h>
#include <QTreeView>
#include <QStyledItemDelegate>
#include <qabstractitemmodel.h>

QT_BEGIN_NAMESPACE
class QTreeView;
QT_END_NAMESPACE
struct TreeItem;
class MainWindow;

class DmsModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	DmsModel(TreeItem* root) : m_root(root) {}

	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& child) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

private:
	QVariant getTreeItemIcon(const QModelIndex& index) const;
	TreeItem* GetTreeItemOrRoot(const QModelIndex& index) const;
	TreeItem* m_root = nullptr;
};

class TreeItemDelegate : public QStyledItemDelegate
{
public:
	using QStyledItemDelegate::QStyledItemDelegate;
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

class DmsTreeView : public QTreeView
{
public:
	using QTreeView::QTreeView;
	void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;

};

auto createTreeview(MainWindow* dms_main_window) -> QPointer<DmsTreeView>;