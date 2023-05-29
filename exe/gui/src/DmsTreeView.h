#include <QPointer.h>
#include <qabstractitemmodel.h>

QT_BEGIN_NAMESPACE
class QTreeView;
class QTreeView;
QT_END_NAMESPACE
struct TreeItem;

namespace {
	auto GetTreeItem(const QModelIndex& mi) -> const TreeItem*;
	int GetRow(const TreeItem* ti);
}

class DmsModel : public QAbstractItemModel
{
public:
	DmsModel(TreeItem* root) : m_root(root) {}

	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& child) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

private:
	TreeItem* m_root = nullptr;
};
};

auto createTreeview(MainWindow* dms_main_window) -> QPointer<QTreeView>;
