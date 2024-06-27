// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "DmsMainWindow.h"
#include "DmsDetailPages.h"
#include "DmsViewArea.h"

#include "DmsValueInfo.h"
#include "ser/MoreStreamBuff.h"
#include "xml/XmlOut.h"

#include <QDockWidget>

StudyObjectHistory::StudyObjectHistory() {}
StudyObjectHistory::~StudyObjectHistory() {}

auto StudyObjectHistory::currentContext() -> Explain::CalcExplImpl* const
{
    return study_objects.at(current_index).explain_context.get();
}

auto StudyObjectHistory::currentStudyObject() -> SharedDataItemInterestPtr const
{
    return study_objects.at(current_index).study_object;
}

auto StudyObjectHistory::currentIndex() -> Int64 const
{
    return study_objects.at(current_index).index;
}

auto StudyObjectHistory::currentExtraInfo() -> SharedStr const
{
    return study_objects.at(current_index).extra_info;
}

auto StudyObjectHistory::nrPreviousStudyObjects() -> SizeT const
{
    return current_index;
}

auto StudyObjectHistory::nrNextStudyObjects() -> SizeT const
{
    SizeT number_of_studyobjects = study_objects.size();
    return number_of_studyobjects - (current_index+1);
}

bool StudyObjectHistory::previous()
{
    if (!nrPreviousStudyObjects())
        return false;

    current_index--;
    return true;
}

bool StudyObjectHistory::next()
{
    if (!nrNextStudyObjects())
        return false;

    current_index++;
    return true;
}

void StudyObjectHistory::deleteAfterCurrentIndex()
{
    assert(current_index < study_objects.size() || current_index == -1);
    while (study_objects.size() > current_index+1)
    {
        garbage.emplace_back(std::move(study_objects.back().explain_context));
        study_objects.pop_back();
    }
    assert(current_index + 1 == study_objects.size());
}

void StudyObjectHistory::ClearGarbage()
{
    garbage.clear();
}

void StudyObjectHistory::insert(SharedDataItemInterestPtr studyObject, SizeT index, SharedStr extraInfo)
{
    auto newContext = Explain::CreateContext();
    deleteAfterCurrentIndex();

    study_objects.emplace_back(studyObject, index, extraInfo, std::move(newContext));
    current_index++;
    assert(current_index + 1 == study_objects.size());
}

ValueInfoWindow::ValueInfoWindow(SharedDataItemInterestPtr studyObject, SizeT index, SharedStr extraInfo, QWidget* parent)
    : QWidget(parent)
{
    // window flags
    setWindowFlag(Qt::Window, true);
    setAttribute(Qt::WA_DeleteOnClose, true);

    // icon
    setWindowIcon(QIcon(":/res/images/DP_ValueInfo.bmp"));

    // layout
    QVBoxLayout* v_layout = new QVBoxLayout(this);
    QHBoxLayout* h_layout = new QHBoxLayout(this);

    // close value info window shortcuts
    QShortcut* shortcut_CTRL_W = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_W), this);
    QShortcut* shortcut_CTRL_F4 = new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_F4), this);
    QObject::connect(shortcut_CTRL_W, &QShortcut::activated, this, &QWidget::close);
    QObject::connect(shortcut_CTRL_F4, &QShortcut::activated, this, &QWidget::close);

    m_browser = new ValueInfoBrowser(this, studyObject, index, extraInfo);
    h_layout->addWidget(m_browser->back_button.get());
    h_layout->addWidget(m_browser->forward_button.get());
    v_layout->addLayout(h_layout);
    v_layout->addWidget(m_browser);



    setLayout(v_layout);
    resize(800, 500);

    show();

    m_browser->restart_updating();
}

ValueInfoBrowser::ValueInfoBrowser(QWidget* parent, SharedDataItemInterestPtr studyObject, SizeT index, SharedStr extraInfo)//, QWidget* window)
    : QUpdatableTextBrowser(parent)
{
    m_history.insert(studyObject, index, extraInfo);
    setProperty("DmsHelperWindowType", DmsHelperWindowType::HW_VALUEINFO);

    back_button = std::make_unique<QPushButton>(QIcon(":/res/images/DP_back.bmp"), "");
    forward_button = std::make_unique<QPushButton>(QIcon(":/res/images/DP_forward.bmp"), "");
    back_button->setDisabled(true);
    forward_button->setDisabled(true);

    connect(back_button.get(), &QPushButton::pressed, this, &ValueInfoBrowser::previousStudyObject);
    connect(forward_button.get(), &QPushButton::pressed, this, &ValueInfoBrowser::nextStudyObject);

    connect(this, &ValueInfoBrowser::anchorClicked, this, &ValueInfoBrowser::onAnchorClicked);

    updateWindowTitle();
}

bool ValueInfoBrowser::update()
{
    vos_buffer_type buffer;

    ExternalVectorOutStreamBuff outStreamBuff(buffer);

    auto xmlOut = OutStream_HTM(&outStreamBuff, "html", nullptr);
    SuspendTrigger::Resume();

    bool done = Explain::AttrValueToXML(m_history.currentContext(), m_history.currentStudyObject(), &xmlOut, m_history.currentIndex(), m_history.currentExtraInfo().c_str(), true); // m_Context.get(), m_StudyObject, & xmlOut, m_Index, "", true);
    outStreamBuff.WriteByte(char(0));

    setHtml(outStreamBuff.GetData());

    // clean-up;
    m_history.ClearGarbage();
    return done;
}

void ValueInfoBrowser::updateNavigationButtons()
{
    back_button->setEnabled(m_history.nrPreviousStudyObjects());
    forward_button->setEnabled(m_history.nrNextStudyObjects());
}

void ValueInfoBrowser::updateWindowTitle()
{
    auto title = mySSPrintF("%s row %d", m_history.currentStudyObject()->GetFullName(), m_history.currentIndex());
    if (!parent())
        return;
    
    auto* value_info_window = dynamic_cast<ValueInfoWindow*>(parent());
    if (!value_info_window)
        return;

    value_info_window->setWindowTitle(title.c_str());
}

void ValueInfoBrowser::nextStudyObject()
{
    m_history.next();
    restart_updating();
    updateNavigationButtons();
    updateWindowTitle();
}

void ValueInfoBrowser::previousStudyObject()
{
    m_history.previous();
    restart_updating();
    updateNavigationButtons();
    updateWindowTitle();
}

void ValueInfoBrowser::addStudyObject(SharedDataItemInterestPtr studyObject, SizeT index, SharedStr extraInfo)
{
    m_history.insert(studyObject, index, extraInfo);
    restart_updating();
    updateNavigationButtons();
    updateWindowTitle();
}

void ValueInfoBrowser::onAnchorClicked(const QUrl& link)
{
    MainWindow::TheOne()->onInternalLinkClick(link, static_cast<QWidget*>(this));
}