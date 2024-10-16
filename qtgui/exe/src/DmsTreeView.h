#include <QMenu.h>
#include <QPointer.h>
#include <QTreeView>
#include <QCompleter>
#include <QStyledItemDelegate>
#include <qabstractitemmodel.h>

QT_BEGIN_NAMESPACE
class QTreeView;
QT_END_NAMESPACE
struct TreeItem;
class MainWindow;
struct Waiter;

struct InvisibleRootTreeItem
{
	TreeItem* m_dms_root;
};

class TreeModelCompleter : public QCompleter
{
	Q_OBJECT
		Q_PROPERTY(QString separator READ separator WRITE setSeparator)

public:
	explicit TreeModelCompleter(QObject* parent = nullptr);
	explicit TreeModelCompleter(QAbstractItemModel* model, QObject* parent = nullptr);

	QString separator() const;
public slots:
	void setSeparator(const QString& separator);

protected:
	QStringList splitPath(const QString& path) const override;
	QString pathFromIndex(const QModelIndex& index) const override;

private:
	QString sep;
};

class DmsModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	DmsModel() = default;

	void setRoot(const TreeItem* root) { m_root = root; }
	void reset();
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& child) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
	auto flags(const QModelIndex& index) const -> Qt::ItemFlags override;
	QVariant getTreeItemIcon(const QModelIndex& index) const;
	bool updateChachedDisplayFlags();

private:
	QVariant getTreeItemColor(const QModelIndex& index) const;
	const TreeItem* GetTreeItemOrRoot(const QModelIndex& index) const;

public:
	const TreeItem* m_root = nullptr;
	bool show_hidden_items = false;
	bool show_state_colors = false;
};

class TreeItemDelegate : public QStyledItemDelegate
{
public:
	using QStyledItemDelegate::QStyledItemDelegate;
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

class DmsTreeView : public QTreeView
{
	Q_OBJECT
public:
	DmsTreeView(QWidget* parent);
	void showTreeviewContextMenu(const QPoint& pos);
	void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
	auto expandToItem(TreeItem* new_item) -> QModelIndex;
	void setNewCurrentItem(TreeItem* new_current_item);
	bool expandActiveNode(bool doExpand);
	bool expandRecursiveFromCurrentItem();
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;
	void setDmsStyleSheet(bool connecting_lines = true);

	int m_default_size = 0;

public slots:
	void onDoubleClick(const QModelIndex& index);
	void onHeaderSectionClicked(int index);

private:
	std::unique_ptr<QMenu> m_context_menu, m_code_analysis_submenu;
};

auto createTreeview(MainWindow* dms_main_window) -> QPointer<DmsTreeView>;
bool isAncestor(const TreeItem* ancestorTarget, const TreeItem* descendant);
