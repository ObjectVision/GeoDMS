#include <QPointer>
#include <QDialog>;

class QCheckBox;
class QPushButton;
class QSlider;
class QLabel;
class QFileDialog;
class QLineEdit;


class DmsOptionsWindow : public QDialog
{
    Q_OBJECT

private slots:
    void onFlushTresholdValueChange(int value);
    void restoreOptions();
    void ok();
    void apply();
    void undo();
    void onStateChange(int state);
    void onTextChange(const QString& text);
    void setLocalDataDirThroughDialog();
    void setSourceDataDirThroughDialog();

public:
    DmsOptionsWindow(QWidget* parent = nullptr);
private:
    void setInitialLocalDataDirValue();
    void setInitialSourceDatDirValue();
    void setInitialEditorValue();
    void setInitialMemoryFlushTresholdValue();

    bool m_changed = false;

    QPointer<QFileDialog> m_folder_dialog;
    QPointer<QLabel> m_flush_treshold_text;
    QPointer<QCheckBox> m_pp0;
    QPointer<QCheckBox> m_pp1;
    QPointer<QCheckBox> m_pp2;
    QPointer<QCheckBox> m_pp3;
    QPointer<QCheckBox> m_tracelog;
    QPointer<QLineEdit> m_ld_input;
    QPointer<QLineEdit> m_sd_input;
    QPointer<QLineEdit> m_editor_input;
    QPointer<QSlider>   m_flush_treshold;
    QPointer<QPushButton> m_ok;
    QPointer<QPushButton> m_apply;
    QPointer<QPushButton> m_undo;
};