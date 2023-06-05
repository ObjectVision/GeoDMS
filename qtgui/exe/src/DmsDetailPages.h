#include <QPointer>
#include <qtextbrowser>

#include "ptr/SharedStr.h"
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
//	void setDummyText();
	void connectDetailPagesAnchorClicked();

public slots:
	void toggleGeneral();
	void toggleExplorer();
	void toggleProperties();
	void toggleConfiguration();

	void newCurrentItem();

	void DoViewAction(const TreeItem* tiContext, CharPtrRange sAction);

private slots:
	void onAnchorClicked(const QUrl& link);

private:
	void toggle(ActiveDetailPage new_active_detail_page);
	void setActiveDetailPage(ActiveDetailPage new_active_detail_page);
	void drawPage();

	ActiveDetailPage m_active_detail_page = ActiveDetailPage::GENERAL;
	SharedTreeItem m_tiContext;
	SizeT m_RecNo;
};

