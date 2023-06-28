#include <QPointer>
#include <QTimer>
#include <qtextbrowser>

#include "ptr/SharedStr.h"
#include "ptr/InterestHolders.h"
#include "Ticbase.h"
#include "TreeItemProps.h"

enum ActiveDetailPage
{
	GENERAL,
	EXPLORE,
	PROPERTIES,
	METADATA,
	VALUE_INFO,
	CONFIGURATION,
	SOURCEDESCR,
	STATISTICS,
	NONE
};

class DmsDetailPages : public QTextBrowser
{
public:
	DmsDetailPages(QWidget* parent = nullptr);

	void connectDetailPagesAnchorClicked();

public slots:
	void toggleGeneral();
	void toggleExplorer();
	void toggleProperties();
	void toggleConfiguration();
	void toggleSourceDescr();
	void toggleMetaInfo();

	void newCurrentItem();

	void DoViewAction(TreeItem* tiContext, CharPtrRange sAction);
	void setActiveDetailPage(ActiveDetailPage new_active_detail_page);
	void leaveThisConfig();

private slots:
	void onAnchorClicked(const QUrl& link);

private:
	void toggleVisualState(ActiveDetailPage new_active_detail_page, bool toggle);
	void toggle(ActiveDetailPage new_active_detail_page);
	void drawPage();

	QTimer m_Repeater;

	ActiveDetailPage m_active_detail_page = ActiveDetailPage::GENERAL;
	SharedTreeItemInterestPtr m_tiValueInfoContext;
	SizeT m_RecNo;
	SourceDescrMode m_SDM = SourceDescrMode::Configured;
};

