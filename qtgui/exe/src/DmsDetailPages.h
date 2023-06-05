#include <QPointer>
#include <QTimer>
#include <qtextbrowser>

#include "ptr/SharedStr.h"
#include "ptr/InterestHolders.h"
#include "Ticbase.h"

enum class ActiveDetailPage
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
	using QTextBrowser::QTextBrowser;

	void connectDetailPagesAnchorClicked();

public slots:
	void toggleGeneral();
	void toggleExplorer();
	void toggleProperties();
	void toggleConfiguration();

	void newCurrentItem();

	void DoViewAction(TreeItem* tiContext, CharPtrRange sAction);
	void setActiveDetailPage(ActiveDetailPage new_active_detail_page);

private slots:
	void onAnchorClicked(const QUrl& link);

private:
	void toggle(ActiveDetailPage new_active_detail_page);
	void drawPage();

	QTimer m_Repeater;

	ActiveDetailPage m_active_detail_page = ActiveDetailPage::GENERAL;
	SharedTreeItemInterestPtr m_tiValueInfoContext;
	SizeT m_RecNo;

};

