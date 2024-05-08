#include <QLineEdit>
#include "ptr/SharedPtr.h"

struct TreeItem;

class DmsAddressBar : public QLineEdit {
    Q_OBJECT
public:
    DmsAddressBar(QWidget* parent = nullptr);
    void setDmsCompleter();
    void setPath(CharPtr itemPath);

public slots:
    void setPathDirectly(QString path);
    void onEditingFinished();

private:
    void findItem(const TreeItem* context, QString path, bool updateHistory);
};