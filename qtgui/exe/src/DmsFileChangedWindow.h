#include <QDialog>
#include <QPushButton>
#include <QTextBrowser>
#include <QPointer>

class DmsFileChangedWindow : public QDialog
{
    Q_OBJECT

public:
    DmsFileChangedWindow(QWidget* parent = nullptr);
    void setFileChangedMessage(std::string_view changed_files);

private slots:
    void ignore();
    void reopen();
    void onAnchorClicked(const QUrl& link);

private:
    QPointer<QPushButton> m_ignore;
    QPointer<QPushButton> m_reopen;
    QPointer<QTextBrowser> m_message;
};