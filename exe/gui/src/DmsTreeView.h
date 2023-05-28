#include <QPointer>
QT_BEGIN_NAMESPACE
class QListWidget;
class QTreeView;
QT_END_NAMESPACE
class MainWindow;
class TreeItem;


class TreeNode
{
	explicit TreeNode(TreeItem* tree_item);
	~TreeNode();
	auto child(int number) -> TreeNode*;
	auto childCount() -> int;
	QVariant data(int column) const;
	// TODO: continue here

private:
	TreeItem* m_tree_item = nullptr;
	TreeNode* m_parent_node = nullptr;
};

auto createTreeview(MainWindow* dms_main_window) -> QPointer<QListWidget>;