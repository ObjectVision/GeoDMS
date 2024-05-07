#include "DmsActions.h"
#include "DmsMainWindow.h"

#include <memory>
#include <QObject>
#include <QMenu>
#include <QTranslator>
#include <QAction>
#include <QMenuBar>
#include <QShortCut>

#include "DmsEventLog.h"
#include "DmsViewArea.h"

void createDmsActions() {
    auto main_window = MainWindow::TheOne();

    auto file_open_act = new QAction(QObject::tr("&Open Configuration"), main_window);
    file_open_act->setShortcuts(QKeySequence::Open);
    file_open_act->setStatusTip(QObject::tr("Open an existing configuration file"));
    
    main_window->connect(file_open_act, &QAction::triggered, main_window, &MainWindow::fileOpen);
    main_window->m_file_menu->addAction(file_open_act);

    auto re_open_act = new QAction(QObject::tr("&Reopen current Configuration"), main_window);
    re_open_act->setShortcut(QKeySequence(QObject::tr("Alt+R")));
    re_open_act->setStatusTip(QObject::tr("Reopen the current configuration and reactivate the current active item"));
    main_window->connect(re_open_act, &QAction::triggered, main_window, &MainWindow::reopen);
    main_window->m_file_menu->addAction(re_open_act);

    main_window->m_quit_action = std::make_unique<QAction>(QObject::tr("&Quit"));
    main_window->connect(main_window->m_quit_action.get(), &QAction::triggered, qApp, &QCoreApplication::quit);
    main_window->m_file_menu->addAction(main_window->m_quit_action.get());
    main_window->m_quit_action->setShortcuts(QKeySequence::Quit);
    main_window->m_quit_action->setStatusTip(QObject::tr("Quit the application"));

    main_window->m_file_menu->addSeparator();

    main_window->connect(main_window->m_file_menu.get(), &QMenu::aboutToShow, main_window, &MainWindow::updateFileMenu);
    main_window->updateFileMenu(); //this will be done again when the it's about showtime, but we already need the list of recent_file_actions fn case we insert or refresh one

    main_window->m_edit_menu = std::make_unique<QMenu>(QObject::tr("&Edit"));
    main_window->menuBar()->addMenu(main_window->m_edit_menu.get());

    // focus address bar
    auto focus_address_bar_shortcut = new QShortcut(QKeySequence(QObject::tr("Alt+D")), main_window);
    main_window->connect(focus_address_bar_shortcut, &QShortcut::activated, main_window, &MainWindow::focusAddressBar);

    // export primary data
    main_window->m_export_primary_data_action = std::make_unique<QAction>(QObject::tr("&Export Primary Data"));
    main_window->connect(main_window->m_export_primary_data_action.get(), &QAction::triggered, main_window, &MainWindow::exportPrimaryData);

    // step to failreason
    main_window->m_step_to_failreason_action = std::make_unique<QAction>(QObject::tr("&Step up to FailReason"));
    auto step_to_failreason_shortcut = new QShortcut(QKeySequence(QObject::tr("F2")), main_window);
    main_window->connect(step_to_failreason_shortcut, &QShortcut::activated, main_window, &MainWindow::stepToFailReason);
    main_window->connect(main_window->m_step_to_failreason_action.get(), &QAction::triggered, main_window, &MainWindow::stepToFailReason);
    main_window->m_step_to_failreason_action->setShortcut(QKeySequence(QObject::tr("F2")));

    // go to causa prima
    main_window->m_go_to_causa_prima_action = std::make_unique<QAction>(QObject::tr("&Run up to Causa Prima (i.e. repeated Step up)"));
    auto go_to_causa_prima_shortcut = new QShortcut(QKeySequence(QObject::tr("Shift+F2")), main_window);
    main_window->connect(go_to_causa_prima_shortcut, &QShortcut::activated, main_window, &MainWindow::runToFailReason);
    main_window->connect(main_window->m_go_to_causa_prima_action.get(), &QAction::triggered, main_window, &MainWindow::runToFailReason);
    main_window->m_go_to_causa_prima_action->setShortcut(QKeySequence(QObject::tr("Shift+F2")));

    // open config source
    main_window->m_edit_config_source_action = std::make_unique<QAction>(QObject::tr("&Open in Editor"));
    main_window->connect(main_window->m_edit_config_source_action.get(), &QAction::triggered, main_window, &MainWindow::openConfigSource);
    main_window->m_edit_menu->addAction(main_window->m_edit_config_source_action.get());
    main_window->m_edit_config_source_action->setShortcut(QKeySequence(QObject::tr("Ctrl+E")));

    // update treeitem
    main_window->m_update_treeitem_action = std::make_unique<QAction>(QObject::tr("&Update TreeItem"));
    auto update_treeitem_shortcut = new QShortcut(QKeySequence(QObject::tr("Ctrl+U")), main_window);
    main_window->connect(update_treeitem_shortcut, &QShortcut::activated, main_window, &MainWindow::visual_update_treeitem);
    main_window->connect(main_window->m_update_treeitem_action.get(), &QAction::triggered, main_window, &MainWindow::visual_update_treeitem);
    main_window->m_update_treeitem_action->setShortcut(QKeySequence(QObject::tr("Ctrl+U")));

    // update subtree
    main_window->m_update_subtree_action = std::make_unique<QAction>(QObject::tr("&Update Subtree"));
    auto update_subtree_shortcut = new QShortcut(QKeySequence(QObject::tr("Ctrl+T")), main_window);
    main_window->connect(update_subtree_shortcut, &QShortcut::activated, main_window, &MainWindow::visual_update_subtree);
    main_window->connect(main_window->m_update_subtree_action.get(), &QAction::triggered, main_window, &MainWindow::visual_update_subtree);
    main_window->m_update_subtree_action->setShortcut(QKeySequence(QObject::tr("Ctrl+T")));

    // invalidate action
    main_window->m_invalidate_action = std::make_unique<QAction>(QObject::tr("&Invalidate"));
    main_window->m_invalidate_action->setShortcut(QKeySequence(QObject::tr("Ctrl+I")));
    main_window->m_invalidate_action->setShortcutContext(Qt::ApplicationShortcut);
    //connect(m_invalidate_action.get(), &QAction::triggered, this, & #TODO);

    main_window->m_view_menu = std::make_unique<QMenu>(QObject::tr("&View"));
    main_window->menuBar()->addMenu(main_window->m_view_menu.get());

    main_window->m_defaultview_action = std::make_unique<QAction>(QObject::tr("Default"));
    main_window->m_defaultview_action->setIcon(QIcon::fromTheme("backward", QIcon(dms_params::default_view_icon)));
    main_window->m_defaultview_action->setStatusTip(QObject::tr("Open current selected TreeItem's default view."));
    main_window->connect(main_window->m_defaultview_action.get(), &QAction::triggered, main_window, &MainWindow::defaultView);
    main_window->m_view_menu->addAction(main_window->m_defaultview_action.get());
    main_window->m_defaultview_action->setShortcut(QKeySequence(QObject::tr("Ctrl+Alt+D")));

    // table view
    main_window->m_tableview_action = std::make_unique<QAction>(QObject::tr("&Table"));
    main_window->m_tableview_action->setStatusTip(QObject::tr("Open current selected TreeItem's in a table view."));
    main_window->m_tableview_action->setIcon(QIcon::fromTheme("backward", QIcon(dms_params::table_view_icon)));
    main_window->connect(main_window->m_tableview_action.get(), &QAction::triggered, main_window, &MainWindow::tableView);
    main_window->m_view_menu->addAction(main_window->m_tableview_action.get());
    main_window->m_tableview_action->setShortcut(QKeySequence(QObject::tr("Ctrl+D")));

    // map view
    main_window->m_mapview_action = std::make_unique<QAction>(QObject::tr("&Map"));
    main_window->m_mapview_action->setStatusTip(QObject::tr("Open current selected TreeItem's in a map view."));
    main_window->m_mapview_action->setIcon(QIcon::fromTheme("backward", QIcon(dms_params::map_view_icon)));
    main_window->connect(main_window->m_mapview_action.get(), &QAction::triggered, main_window, &MainWindow::mapView);
    main_window->m_view_menu->addAction(main_window->m_mapview_action.get());
    main_window->m_mapview_action->setShortcut(QKeySequence(QObject::tr("CTRL+M")));

    // statistics view
    main_window->m_statistics_action = std::make_unique<QAction>(QObject::tr("&Statistics"));
    main_window->m_statistics_action->setIcon(QIcon::fromTheme("backward", QIcon(dms_params::statistics_view_icon)));
    main_window->connect(main_window->m_statistics_action.get(), &QAction::triggered, main_window, &MainWindow::showStatistics);
    main_window->m_view_menu->addAction(main_window->m_statistics_action.get());

    // histogram view
//    m_histogramview_action = std::make_unique<QAction>(tr("&Histogram View"));
//    m_histogramview_action->setShortcut(QKeySequence(tr("Ctrl+H")));
    //connect(m_histogramview_action.get(), &QAction::triggered, this, & #TODO);

    // process schemes
    main_window->m_process_schemes_action = std::make_unique<QAction>(QObject::tr("&Process Schemes"));
    //connect(m_process_schemes_action.get(), &QAction::triggered, this, & #TODO);
    //m_view_menu->addAction(m_process_schemes_action.get()); // TODO: to be implemented or not..

    main_window->m_view_calculation_times_action = std::make_unique<QAction>(QObject::tr("Calculation times"));
    main_window->m_view_calculation_times_action->setIcon(main_window->getIconFromViewstyle(ViewStyle::tvsCalculationTimes));
    main_window->connect(main_window->m_view_calculation_times_action.get(), &QAction::triggered, main_window, &MainWindow::view_calculation_times);
    main_window->m_view_menu->addAction(main_window->m_view_calculation_times_action.get());

    main_window->m_view_current_config_filelist = std::make_unique<QAction>(QObject::tr("List of loaded Configuration Files"));
    main_window->m_view_current_config_filelist->setIcon(main_window->getIconFromViewstyle(ViewStyle::tvsCurrentConfigFileList));// QPixmap(":/res/images/IconCalculationTimeOverview.png"));
    main_window->connect(main_window->m_view_current_config_filelist.get(), &QAction::triggered, main_window, &MainWindow::view_current_config_filelist);
    main_window->m_view_menu->addAction(main_window->m_view_current_config_filelist.get());

    main_window->m_view_menu->addSeparator();
    main_window->m_toggle_treeview_action = std::make_unique<QAction>(QObject::tr("Toggle TreeView"));
    main_window->m_toggle_detailpage_action = std::make_unique<QAction>(QObject::tr("Toggle DetailPages area"));
    main_window->m_toggle_eventlog_action = std::make_unique<QAction>(QObject::tr("Toggle EventLog"));
    main_window->m_toggle_toolbar_action = std::make_unique<QAction>(QObject::tr("Toggle Toolbar"));
    main_window->m_toggle_currentitembar_action = std::make_unique<QAction>(QObject::tr("Toggle CurrentItemBar"));

    main_window->m_toggle_treeview_action->setCheckable(true);
    main_window->m_toggle_detailpage_action->setCheckable(true);
    main_window->m_toggle_eventlog_action->setCheckable(true);
    main_window->m_toggle_toolbar_action->setCheckable(true);
    main_window->m_toggle_currentitembar_action->setCheckable(true);

    main_window->connect(main_window->m_toggle_treeview_action.get(), &QAction::triggered, main_window, &MainWindow::toggle_treeview);
    main_window->connect(main_window->m_toggle_detailpage_action.get(), &QAction::triggered, main_window, &MainWindow::toggle_detailpages);
    main_window->connect(main_window->m_toggle_eventlog_action.get(), &QAction::triggered, main_window, &MainWindow::toggle_eventlog);
    main_window->connect(main_window->m_toggle_toolbar_action.get(), &QAction::triggered, main_window, &MainWindow::toggle_toolbar);
    main_window->connect(main_window->m_toggle_currentitembar_action.get(), &QAction::triggered, main_window, &MainWindow::toggle_currentitembar);
    main_window->m_toggle_treeview_action->setShortcut(QKeySequence(QObject::tr("Alt+0")));
    main_window->m_toggle_treeview_action->setShortcutContext(Qt::ApplicationShortcut);
    main_window->m_toggle_detailpage_action->setShortcut(QKeySequence(QObject::tr("Alt+1")));
    main_window->m_toggle_detailpage_action->setShortcutContext(Qt::ApplicationShortcut);
    main_window->m_toggle_eventlog_action->setShortcut(QKeySequence(QObject::tr("Alt+2")));
    main_window->m_toggle_eventlog_action->setShortcutContext(Qt::ApplicationShortcut);
    main_window->m_toggle_toolbar_action->setShortcut(QKeySequence(QObject::tr("Alt+3")));
    main_window->m_toggle_toolbar_action->setShortcutContext(Qt::ApplicationShortcut);
    main_window->m_toggle_currentitembar_action->setShortcut(QKeySequence(QObject::tr("Alt+4")));
    main_window->m_toggle_currentitembar_action->setShortcutContext(Qt::ApplicationShortcut);

    main_window->m_view_menu->addAction(main_window->m_toggle_treeview_action.get());
    main_window->m_view_menu->addAction(main_window->m_toggle_detailpage_action.get());
    main_window->m_view_menu->addAction(main_window->m_toggle_eventlog_action.get());
    main_window->m_view_menu->addAction(main_window->m_toggle_toolbar_action.get());
    main_window->m_view_menu->addAction(main_window->m_toggle_currentitembar_action.get());
    main_window->connect(main_window->m_view_menu.get(), &QMenu::aboutToShow, main_window, &MainWindow::updateViewMenu);

    // tools menu
    main_window->m_tools_menu = std::make_unique<QMenu>("&Tools");
    main_window->menuBar()->addMenu(main_window->m_tools_menu.get());
    main_window->connect(main_window->m_tools_menu.get(), &QMenu::aboutToShow, main_window, &MainWindow::updateToolsMenu);

    main_window->m_code_analysis_set_source_action = std::make_unique<QAction>(QObject::tr("set source"));
    main_window->connect(main_window->m_code_analysis_set_source_action.get(), &QAction::triggered, main_window, &MainWindow::code_analysis_set_source);
    main_window->m_code_analysis_set_source_action->setShortcut(QKeySequence(QObject::tr("Alt+K")));
    main_window->m_code_analysis_set_source_action->setShortcutContext(Qt::ApplicationShortcut);
    //    this->addAction(m_code_analysis_set_source_action.get());

    main_window->m_code_analysis_set_target_action = std::make_unique<QAction>(QObject::tr("set target"));
    main_window->connect(main_window->m_code_analysis_set_target_action.get(), &QAction::triggered, main_window, &MainWindow::code_analysis_set_target);
    main_window->m_code_analysis_set_target_action->setShortcut(QKeySequence(QObject::tr("Alt+B")));
    main_window->m_code_analysis_set_target_action->setShortcutContext(Qt::ApplicationShortcut);

    main_window->m_code_analysis_add_target_action = std::make_unique<QAction>(QObject::tr("add target"));
    main_window->connect(main_window->m_code_analysis_add_target_action.get(), &QAction::triggered, main_window, &MainWindow::code_analysis_add_target);
    main_window->m_code_analysis_add_target_action->setShortcut(QKeySequence(QObject::tr("Alt+N")));
    main_window->m_code_analysis_add_target_action->setShortcutContext(Qt::ApplicationShortcut);

    main_window->m_code_analysis_clr_targets_action = std::make_unique<QAction>(QObject::tr("clear target"));
    main_window->connect(main_window->m_code_analysis_clr_targets_action.get(), &QAction::triggered, main_window, &MainWindow::code_analysis_clr_targets);
    main_window->m_code_analysis_submenu = main_window->CreateCodeAnalysisSubMenu(main_window->m_tools_menu.get());

    main_window->m_eventlog_filter_toggle = std::make_unique<QAction>(QObject::tr("Eventlog filter"));
    main_window->connect(main_window->m_eventlog_filter_toggle.get(), &QAction::triggered, main_window->m_eventlog->m_event_filter_toggle.get(), &QCheckBox::click);
    main_window->m_tools_menu->addAction(main_window->m_eventlog_filter_toggle.get());

    main_window->m_open_root_config_file_action = std::make_unique<QAction>(QObject::tr("Open the root configuration file"));
    main_window->connect(main_window->m_open_root_config_file_action.get(), &QAction::triggered, main_window, &MainWindow::openConfigRootSource);
    main_window->m_tools_menu->addAction(main_window->m_open_root_config_file_action.get());

    main_window->m_expand_all_action = std::make_unique<QAction>(QObject::tr("Expand all items in the TreeView"));
    main_window->connect(main_window->m_expand_all_action.get(), &QAction::triggered, main_window, &MainWindow::expandAll);
    main_window->m_tools_menu->addAction(main_window->m_expand_all_action.get());

    // debug tools
#ifdef MG_DEBUG
    main_window->m_save_value_info_pages = std::make_unique<QAction>(QObject::tr("Debug: save value info page(s)"));
    main_window->connect(main_window->m_save_value_info_pages.get(), &QAction::triggered, main_window, &MainWindow::saveValueInfo);
    main_window->m_tools_menu->addAction(main_window->m_save_value_info_pages.get());

    main_window->m_debug_reports = std::make_unique<QAction>(QObject::tr("Debug: produce internal report(s)"));
    main_window->connect(main_window->m_debug_reports.get(), &QAction::triggered, main_window, &MainWindow::debugReports);
    main_window->m_tools_menu->addAction(main_window->m_debug_reports.get());
#endif

    // settings menu
    main_window->m_settings_menu = std::make_unique<QMenu>(QObject::tr("&Settings"));
    main_window->menuBar()->addMenu(main_window->m_settings_menu.get());
    main_window->connect(main_window->m_tools_menu.get(), &QMenu::aboutToShow, main_window, &MainWindow::updateSettingsMenu);

    main_window->m_gui_options_action = std::make_unique<QAction>(QObject::tr("&Gui Options"));
    main_window->connect(main_window->m_gui_options_action.get(), &QAction::triggered, main_window, &MainWindow::gui_options);
    main_window->m_settings_menu->addAction(main_window->m_gui_options_action.get());

    main_window->m_advanced_options_action = std::make_unique<QAction>(QObject::tr("&Local machine Options"));
    main_window->connect(main_window->m_advanced_options_action.get(), &QAction::triggered, main_window, &MainWindow::advanced_options); //TODO: change advanced options refs in local machine options
    main_window->m_settings_menu->addAction(main_window->m_advanced_options_action.get());

    main_window->m_config_options_action = std::make_unique<QAction>(QObject::tr("&Config Options"));
    main_window->connect(main_window->m_config_options_action.get(), &QAction::triggered, main_window, &MainWindow::config_options);
    main_window->m_settings_menu->addAction(main_window->m_config_options_action.get());

    // window menu
    main_window->m_window_menu = std::make_unique<QMenu>(QObject::tr("&Window"));
    main_window->menuBar()->addMenu(main_window->m_window_menu.get());

    main_window->m_win_tile_action = std::make_unique<QAction>(QObject::tr("&Tile Windows"), main_window->m_window_menu.get());
    main_window->m_win_tile_action->setShortcut(QKeySequence(QObject::tr("Ctrl+Alt+W")));
    main_window->m_win_tile_action->setShortcutContext(Qt::ApplicationShortcut);
    main_window->connect(main_window->m_win_tile_action.get(), &QAction::triggered, main_window->m_mdi_area.get(), &QDmsMdiArea::onTileSubWindows);

    main_window->m_win_cascade_action = std::make_unique<QAction>(QObject::tr("Ca&scade"), main_window->m_window_menu.get());
    main_window->m_win_cascade_action->setShortcut(QKeySequence(QObject::tr("Shift+Ctrl+W")));
    main_window->m_win_cascade_action->setShortcutContext(Qt::ApplicationShortcut);
    main_window->connect(main_window->m_win_cascade_action.get(), &QAction::triggered, main_window->m_mdi_area.get(), &QDmsMdiArea::onCascadeSubWindows);

    main_window->m_win_close_action = std::make_unique<QAction>(QObject::tr("&Close"), main_window->m_window_menu.get());
    main_window->m_win_close_action->setShortcut(QKeySequence(QObject::tr("Ctrl+W")));
    main_window->connect(main_window->m_win_close_action.get(), &QAction::triggered, main_window->m_mdi_area.get(), &QDmsMdiArea::closeActiveSubWindow);
    main_window->m_mdi_area->getTabBar()->addAction(MainWindow::TheOne()->m_win_close_action.get());
    //connect(m_win_close_action.get(), &QAction::triggered, m_mdi_area.get(), &QDmsMdiArea::testCloseSubWindow);


    //connect(m_win_close_action.get(), &QAction::triggered, m_mdi_area, &QDmsMdiArea::testCloseSubWindow);

    main_window->m_win_close_all_action = std::make_unique<QAction>(QObject::tr("Close &All"), main_window->m_window_menu.get());
    main_window->m_win_close_all_action->setShortcut(QKeySequence(QObject::tr("Ctrl+L")));
    main_window->m_win_close_all_action->setShortcutContext(Qt::ApplicationShortcut);
    main_window->connect(main_window->m_win_close_all_action.get(), &QAction::triggered, main_window->m_mdi_area.get(), &QDmsMdiArea::closeAllSubWindows);

    main_window->m_win_close_but_this_action = std::make_unique<QAction>(QObject::tr("Close All &But This"), main_window->m_window_menu.get());
    main_window->m_win_close_but_this_action->setShortcut(QKeySequence(QObject::tr("Ctrl+B")));
    main_window->m_win_close_but_this_action->setShortcutContext(Qt::ApplicationShortcut);
    main_window->connect(main_window->m_win_close_but_this_action.get(), &QAction::triggered, main_window->m_mdi_area.get(), &QDmsMdiArea::closeAllButActiveSubWindow);
    main_window->m_window_menu->addAction(main_window->m_win_tile_action.get());
    main_window->m_window_menu->addAction(main_window->m_win_cascade_action.get());
    main_window->m_window_menu->addAction(main_window->m_win_close_action.get());
    main_window->m_window_menu->addAction(main_window->m_win_close_all_action.get());
    main_window->m_window_menu->addAction(main_window->m_win_close_but_this_action.get());
    main_window->connect(main_window->m_window_menu.get(), &QMenu::aboutToShow, main_window, &MainWindow::updateWindowMenu);
    // help menu
    main_window->m_help_menu = std::make_unique<QMenu>(QObject::tr("&Help"));
    main_window->menuBar()->addMenu(main_window->m_help_menu.get());
    auto about_action = new QAction(QObject::tr("&About GeoDms"), main_window->m_help_menu.get());
    about_action->setStatusTip(QObject::tr("Show the application's About box"));
    main_window->connect(about_action, &QAction::triggered, main_window, &MainWindow::aboutGeoDms);
    main_window->m_help_menu->addAction(about_action);

    auto aboutQt_action = new QAction(QObject::tr("About &Qt"), main_window->m_help_menu.get());
    aboutQt_action->setStatusTip(QObject::tr("Show the Qt library's About box"));
    main_window->connect(aboutQt_action, &QAction::triggered, qApp, &QApplication::aboutQt);
    main_window->m_help_menu->addAction(aboutQt_action);

    auto wiki_action = new QAction(QObject::tr("&Wiki"), main_window->m_help_menu.get());
    wiki_action->setStatusTip(QObject::tr("Open the GeoDms wiki in a browser"));
    main_window->connect(wiki_action, &QAction::triggered, main_window, &MainWindow::wiki);
    main_window->m_help_menu->addAction(wiki_action);
}