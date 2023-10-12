// Copyright (C) 2023 Object Vision b.v. 
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

auto StudyObjectHistory::currentContext() -> Explain::CalcExplImpl*
{
    return explain_contexts.at(current_index).get();
}

auto StudyObjectHistory::currentStudyObject() -> SharedDataItemInterestPtr
{
    return study_objects.at(current_index);
}

auto StudyObjectHistory::currentIndex() -> Int64
{
    return indices.at(current_index);
}

auto StudyObjectHistory::countPrevious() -> SizeT
{
    return current_index;
}

auto StudyObjectHistory::countNext() -> SizeT
{
    SizeT number_of_studyobjects = study_objects.size();
    return number_of_studyobjects - (current_index+1);
}

bool StudyObjectHistory::previous()
{
    if (!countPrevious())
        return false;

    current_index--;
    return true;
}

bool StudyObjectHistory::next()
{
    if (!countNext())
        return false;

    current_index++;
    return true;
}

void StudyObjectHistory::deleteFromCurrentIndexUpToEnd()
{
    if (!countNext())
        return;

    study_objects.erase(study_objects.begin()+ current_index +1, study_objects.end());
    explain_contexts.erase(explain_contexts.begin() + current_index + 1, explain_contexts.end());
    indices.erase(indices.begin() + current_index + 1, indices.end());
}

void StudyObjectHistory::insert(SharedDataItemInterestPtr studyObject, SizeT index)
{
    deleteFromCurrentIndexUpToEnd();
    study_objects.push_back(studyObject);
    explain_contexts.push_back(std::move(Explain::CreateContext()));
    indices.push_back(index);
    current_index++;
}

ValueInfoBrowser::ValueInfoBrowser(QWidget* parent, SharedDataItemInterestPtr studyObject, SizeT index, QWidget* window)
    : QUpdatableTextBrowser(parent)
{
    m_history.insert(studyObject, index);

    setOpenLinks(false);
    setOpenExternalLinks(false);
    setWordWrapMode(QTextOption::NoWrap);
    connect(this, &QTextBrowser::anchorClicked, this, &ValueInfoBrowser::onAnchorClicked);

    back_button = std::make_unique<QPushButton>(QIcon(":/res/images/DP_back.bmp"), "");
    forward_button = std::make_unique<QPushButton>(QIcon(":/res/images/DP_forward.bmp"), "");
    value_info_window = window;
    back_button->setDisabled(true);
    forward_button->setDisabled(true);

    connect(back_button.get(), &QPushButton::pressed, this, &ValueInfoBrowser::previousStudyObject);
    connect(forward_button.get(), &QPushButton::pressed, this, &ValueInfoBrowser::nextStudyObject);

    updateWindowTitle();
}

bool ValueInfoBrowser::update()
{
    vos_buffer_type buffer;

    ExternalVectorOutStreamBuff outStreamBuff(buffer);

    auto xmlOut = OutStream_HTM(&outStreamBuff, "html", nullptr);
    SuspendTrigger::Resume();

    bool done = Explain::AttrValueToXML(m_history.currentContext(), m_history.currentStudyObject(), &xmlOut, m_history.currentIndex(), "", true); // m_Context.get(), m_StudyObject, & xmlOut, m_Index, "", true);
    outStreamBuff.WriteByte(char(0));

    setText(outStreamBuff.GetData());

    // clean-up;
    return done;
}

void ValueInfoBrowser::updateNavigationButtons()
{
    back_button->setEnabled(m_history.countPrevious());
    forward_button->setEnabled(m_history.countNext());
}

void ValueInfoBrowser::updateWindowTitle()
{
    auto title = mySSPrintF("%s row %d", m_history.currentStudyObject()->GetFullName(), m_history.currentIndex());
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

void ValueInfoBrowser::addStudyObject(SharedDataItemInterestPtr studyObject, SizeT index)
{
    m_history.insert(studyObject, index);
    restart_updating();
    updateNavigationButtons();
    updateWindowTitle();
}

void ValueInfoBrowser::onAnchorClicked(const QUrl& link)
{
    MainWindow::TheOne()->onInternalLinkClick(link, static_cast<QWidget*>(this));
}