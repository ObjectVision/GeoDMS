#include <QPointer>
#include <qtextbrowser>
#include <QAbstractButton>

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

//auto dp_FromName(CharPtrRange sName) -> ActiveDetailPage;

class DmsDetailPages : public QTextBrowser
{

public:
	DmsDetailPages(QWidget* parent = nullptr);
	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;
	auto activeDetailPageFromName(CharPtrRange sName)->ActiveDetailPage;

	ActiveDetailPage m_active_detail_page = ActiveDetailPage::GENERAL;
	ActiveDetailPage m_last_active_detail_page = ActiveDetailPage::GENERAL;
	int m_default_width = 500;

public slots:
	void show(ActiveDetailPage new_active_detail_page);
	void toggleGeneral();
	void toggleExplorer();
	void toggleProperties();
	void toggleConfiguration();
	void toggleSourceDescr();
	void toggleMetaInfo();
	void propertiesButtonToggled(QAbstractButton* button, bool checked);
	void sourceDescriptionButtonToggled(QAbstractButton* button, bool checked);
	void newCurrentItem();

	//void DoViewAction(TreeItem* tiContext, CharPtrRange sAction);
	void setActiveDetailPage(ActiveDetailPage new_active_detail_page);
	void leaveThisConfig();
	void scheduleDrawPage();
	void onTreeItemStateChange();
	void scheduleDrawPageImpl(int milliseconds);

protected:
	void resizeEvent(QResizeEvent* event) override;

public slots:
	void onAnchorClicked(const QUrl& link);
	void toggle(ActiveDetailPage new_active_detail_page);

private:
	void toggleVisualState(ActiveDetailPage new_active_detail_page, bool toggle);
	void drawPage();

	std::atomic<bool> m_DrawPageRequestPending = false;
	UInt32 m_current_width = 500;

	SourceDescrMode m_SDM = SourceDescrMode::Configured;
	bool            m_AllProperties = true;
};

SharedStr FindURL(const TreeItem* ti);
