#include <QAction>
#include <QString>
#include "ShvUtils.h"
#include "dataview.h"

class QDmsViewArea;

void createDmsToolbar();
void updateDmsToolbar();

struct ToolbarButtonData {
    ToolButtonID id = TB_Undefined;
    std::vector<QString> text;
    std::vector<ToolButtonID> ids;
    std::vector<QString> icons;
    bool is_global = false;
};

class DmsToolbuttonAction : public QAction {
    Q_OBJECT
public:
    DmsToolbuttonAction(const ToolButtonID id, const QIcon& icon, const QString& text, QObject* parent = nullptr, ToolbarButtonData button_data = {}, ViewStyle vs = ViewStyle::tvsUndefined);

public slots:
    void onToolbuttonPressed();

private:
    auto getNumberOfStates() const -> UInt8 { return m_data.ids.size(); };
    void updateState();
    void onGlobalButtonPressed(QDmsViewArea* dms_view_area);
    void onExportButtonPressed(QDmsViewArea* dms_view_area);

    ToolbarButtonData m_data;
    UInt8 m_state = 0;
};