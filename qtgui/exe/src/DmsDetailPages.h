#include <QPointer>
QT_BEGIN_NAMESPACE
class QTreeView;
class QTextBrowser;
QT_END_NAMESPACE
class MainWindow;

enum class ActiveDetailPage
{
	GENERAL,
	EXPLORE,
	PROPERTIES,
	METADATA,
	VALUE_INFO,
	CONFIGURATION,
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

private slots:
	void onAnchorClicked(const QUrl& link);

private:
	void toggle(ActiveDetailPage new_active_detail_page);
	void setActiveDetailPage(ActiveDetailPage new_active_detail_page);
	void drawPage();

	ActiveDetailPage m_active_detail_page = ActiveDetailPage::GENERAL;
};
