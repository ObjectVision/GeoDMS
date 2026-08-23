// Copyright (C) 1998-2026 Object Vision B.V.
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////
//
// Issue #471: editor navigation for items produced by template/function
// instantiation.  Keep the existing shared QAction, but make its target the
// definition item and add the instantiation roots as alternate context-menu
// targets.

#include "DmsMainWindow.h"
#include "TreeItem.h"
#include "TreeItemUtils.h"
#include "utl/FileSystem.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QTimer>

#include <set>
#include <string>
#include <vector>

namespace {

const TreeItem* DefinitionTarget(const TreeItem* item)
{
    // A copied item keeps the item from which it was instantiated in mc_OrgItem.
    // Follow the chain as well: an instantiation can itself occur in a template
    // or function that is instantiated again.
    std::set<const TreeItem*> seen;
    while (item && seen.insert(item).second)
    {
        auto org = item->mc_OrgItem.lock();
        if (!org)
            break;
        item = org.get();
    }
    return item;
}

const TreeItem* DefinitionRoot(const TreeItem* item)
{
    for (auto p = item; p; p = p->GetTreeParent().get())
        if (p->IsTemplate())
            return p;
    return nullptr;
}

QString ItemName(const TreeItem* item)
{
    return item ? QString::fromUtf8(item->GetName().c_str()) : QString();
}

QString DefinitionActionText(const TreeItem* selected)
{
    auto target = DefinitionTarget(selected);
    if (!target)
        return QObject::tr("Open in Editor");

    auto defRoot = DefinitionRoot(target);
    if (!defRoot)
        return QObject::tr("Open %1 in Editor").arg(ItemName(target));

    auto kind = GetItemIconKind(defRoot, false, false);
    auto definitionKind = kind == item_icon_kind::function_def
        ? QObject::tr("function")
        : QObject::tr("template");

    if (target == defRoot)
        return QObject::tr("Open %1 %2 in Editor")
            .arg(definitionKind, ItemName(defRoot));

    return QObject::tr("Open %1 in %2 %3 in Editor")
        .arg(ItemName(target), definitionKind, ItemName(defRoot));
}

void OpenItem(MainWindow* mainWindow, const TreeItem* item)
{
    if (!mainWindow || !item)
        return;

    auto filename = ConvertDmsFileNameAlways(item->GetConfigFileName());
    auto line = std::to_string(item->GetConfigFileLineNr());
    mainWindow->openConfigSourceDirectly(filename.c_str(), line);
}

struct SourcePosition
{
    QString name;
    std::string filename;
    std::string line;
};

std::vector<SourcePosition> InstantiationRoots(const TreeItem* selected)
{
    std::vector<SourcePosition> result;
    std::set<const TreeItem*> seenTargets;

    // For call/x the copied x points at f/x, while its ancestor call points at
    // template root f.  The latter is the source expression the user expects as
    // the alternate "Open call in Editor" target.  Repeating this over the
    // ancestor chain naturally exposes outer instantiations as further choices.
    for (auto p = selected; p; p = p->GetTreeParent().get())
    {
        auto org = p->mc_OrgItem.lock();
        if (!org || !org->IsTemplate() || !seenTargets.insert(p).second)
            continue;

        auto filename = ConvertDmsFileNameAlways(p->GetConfigFileName());
        result.push_back({ ItemName(p), filename.c_str(), std::to_string(p->GetConfigFileLineNr()) });
    }
    return result;
}

class EditorNavigationMenuFilter final : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() != QEvent::Show)
            return QObject::eventFilter(watched, event);

        auto menu = qobject_cast<QMenu*>(watched);
        auto mainWindow = MainWindow::TheOne();
        if (!menu || !mainWindow || !mainWindow->m_edit_config_source_action)
            return QObject::eventFilter(watched, event);

        auto primary = mainWindow->m_edit_config_source_action.get();
        auto actions = menu->actions();
        if (!actions.contains(primary))
            return QObject::eventFilter(watched, event);

        // The primary action is useful in both the Edit menu and the tree popup.
        // Its label describes the actual Ctrl+E destination.
        auto selected = mainWindow->getCurrentTreeItemOrRoot();
        primary->setText(DefinitionActionText(selected));

        // Alternate instantiation targets belong only in the tree popup.  Its
        // export action distinguishes it from the main Edit menu, which also
        // contains the shared primary action.
        if (!actions.contains(mainWindow->m_export_primary_data_action.get()))
            return QObject::eventFilter(watched, event);

        // Remove alternates inserted on the previous popup invocation.
        for (auto action : actions)
        {
            if (!action->property("instantiationEditorAction").toBool())
                continue;
            menu->removeAction(action);
            action->deleteLater();
        }

        actions = menu->actions();
        auto primaryIndex = actions.indexOf(primary);
        QAction* insertBefore = primaryIndex >= 0 && primaryIndex + 1 < actions.size()
            ? actions[primaryIndex + 1]
            : nullptr;

        for (const auto& source : InstantiationRoots(selected))
        {
            auto action = new QAction(QObject::tr("Open %1 in Editor").arg(source.name), menu);
            action->setProperty("instantiationEditorAction", true);
            QObject::connect(action, &QAction::triggered, mainWindow,
                [mainWindow, filename = source.filename, line = source.line]() {
                    mainWindow->openConfigSourceDirectly(filename, line);
                });
            menu->insertAction(insertBefore, action);
        }

        return QObject::eventFilter(watched, event);
    }
};

void EditorNavigation()
{
    // Q_COREAPP_STARTUP_FUNCTION runs after QApplication construction, but before
    // MainWindow is necessarily built.  Run installation on the first event-loop
    // turn; retrying is harmless for atypical startup sequences.
    if (!MainWindow::IsExisting() || !MainWindow::TheOne()->m_edit_config_source_action)
    {
        QTimer::singleShot(0, EditorNavigation);
        return;
    }

    static bool installed = false;
    if (installed)
        return;
    installed = true;

    auto mainWindow = MainWindow::TheOne();
    auto primary = mainWindow->m_edit_config_source_action.get();

    // Replace the old selected-item target while preserving the QAction and its
    // Ctrl+E shortcut everywhere it is used.
    QObject::disconnect(primary, nullptr, mainWindow, nullptr);
    QObject::connect(primary, &QAction::triggered, mainWindow, [mainWindow]() {
        OpenItem(mainWindow, DefinitionTarget(mainWindow->getCurrentTreeItemOrRoot()));
    });

    qApp->installEventFilter(new EditorNavigationMenuFilter(qApp));
}

void ScheduleEditorNavigation()
{
    QTimer::singleShot(0, EditorNavigation);
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(ScheduleEditorNavigation)
