#include <QDialog>
#include <QPushButton>
#include <QTextBrowser>
#include <QPointer>

class DmsErrorWindow : public QDialog
{
    Q_OBJECT

public:
    DmsErrorWindow(QWidget* parent = nullptr);
    void setErrorMessageHtml(QString message) { m_message->setHtml(message); };

private slots:
    void ignore();
    void terminate();
    void reopen();
    void onAnchorClicked(const QUrl& link);

private:
    QPointer<QPushButton> m_ignore;
    QPointer<QPushButton> m_terminate;
    QPointer<QPushButton> m_reopen;
    QPointer<QTextBrowser> m_message;
};