#include <QPointer>
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
	CONFIGURATION,
	SOURCEDESCR,
	METADATA,
	STATISTICS,
	VALUE_INFO,
	NONE
};

class DmsDetailPages : public QTextBrowser
{
public:
	DmsDetailPages(QWidget* parent = nullptr);
	QSize sizeHint() const override;
	void connectDetailPagesAnchorClicked();

public slots:
	void show(ActiveDetailPage new_active_detail_page);
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
	void scheduleDrawPage();
	void onTreeItemStateChange();
protected:
	//bool event(QEvent* event) override;
public slots:
	void onAnchorClicked(const QUrl& link);

private:
	void toggleVisualState(ActiveDetailPage new_active_detail_page, bool toggle);
	void toggle(ActiveDetailPage new_active_detail_page);
	void drawPage();
	void scheduleDrawPageImpl(int milliseconds);

	std::atomic<bool> m_DrawPageRequestPending = false;

	ActiveDetailPage m_active_detail_page = ActiveDetailPage::GENERAL;
	SourceDescrMode m_SDM = SourceDescrMode::Configured;
	bool            m_ShowNonDefaultProperties = true;
};

SharedStr FindURL(const TreeItem* ti);
