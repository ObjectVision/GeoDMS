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
	DmsModel(const TreeItem* root) : m_root(root) {}
	~DmsModel(); 

	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& child) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
	bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
	

private:
	QVariant getTreeItemIcon(const QModelIndex& index) const;
	QVariant getTreeItemColor(const QModelIndex& index) const;
	const TreeItem* GetTreeItemOrRoot(const QModelIndex& index) const;
	const TreeItem* m_root = nullptr;
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
	auto expandToCurrentItem(TreeItem* new_current_item) -> QModelIndex;
	void setNewCurrentItem(TreeItem* new_current_item);
};

auto createTreeview(MainWindow* dms_main_window) -> QPointer<DmsTreeView>;