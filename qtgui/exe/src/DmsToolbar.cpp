#include <QObject>
#include "DmsMainWindow.h"
#include "DmsViewArea.h"

void createDmsToolbar() {
    auto main_window = MainWindow::TheOne();

    main_window->addToolBarBreak();
    main_window->m_toolbar = main_window->addToolBar(QObject::tr("dmstoolbar"));
    main_window->m_toolbar->setFocusPolicy(Qt::FocusPolicy::ClickFocus);
    main_window->m_toolbar->setStyleSheet(dms_params::stylesheet_toolbar);

    main_window->m_toolbar->setIconSize(QSize(32, 32));
    main_window->m_toolbar->setMinimumSize(QSize(38, 38));

    main_window->updateDetailPagesToolbar(); // detail pages toolbar is created once
}

void clearToolbarUpToDetailPagesTools() {
    auto main_window = MainWindow::TheOne();
    for (auto action : main_window->m_current_dms_view_actions)
        main_window->m_toolbar->removeAction(action);
    main_window->m_current_dms_view_actions.clear();
}

auto getAvailableTableviewButtonIds() -> std::vector<ToolButtonID> {
    return { TB_Export, TB_TableCopy, TB_Copy, TB_Undefined,
             TB_ShowFirstSelectedRow, TB_SelectRows, TB_SelectAll, TB_SelectNone, TB_ShowSelOnlyOn, TB_Undefined,
             TB_TableGroupBy };
}

auto getAvailableMapviewButtonIds() -> std::vector<ToolButtonID> {
    return { TB_Export , TB_CopyLC, TB_Copy, TB_Undefined,
             TB_ZoomAllLayers, TB_ZoomActiveLayer, TB_Pan, TB_ZoomIn2, TB_ZoomOut2, TB_Undefined,
             TB_ZoomSelectedObj,TB_SelectObject,TB_SelectRect,TB_SelectCircle,TB_SelectPolygon,TB_SelectDistrict,TB_SelectAll,TB_SelectNone,TB_ShowSelOnlyOn, TB_Undefined,
             TB_Show_VP,TB_SP_All,TB_NeedleOn,TB_ScaleBarOn };
}

auto getToolbarButtonData(ToolButtonID button_id) -> ToolbarButtonData {
    switch (button_id) {
    case TB_Export: return { TB_Export, {"Save to file as semicolon delimited text", "Export the viewport data to bitmaps file(s) using the export settings and the current ROI"}, {TB_Export}, {":/res/images/TB_save.bmp"} };
    case TB_TableCopy: return { TB_TableCopy, {"Copy as semicolon delimited text to Clipboard",""}, {TB_TableCopy}, {":/res/images/TB_copy.bmp"} };
    case TB_Copy: return { TB_Copy, {"Copy the visible contents as image to Clipboard","Copy the visible contents of the viewport to the Clipboard"}, {TB_Copy}, {":/res/images/TB_vcopy.bmp"} };
    case TB_CopyLC: return { TB_CopyLC, {"","Copy the full contents of the LayerControlList to the Clipboard"}, {TB_CopyLC}, {":/res/images/TB_copy.bmp"} };
    case TB_ShowFirstSelectedRow: return { TB_ShowFirstSelectedRow, {"Show the first selected row","Make the extent of the selected elements fit in the ViewPort"}, {TB_ShowFirstSelectedRow}, {":/res/images/TB_table_show_first_selected.bmp"} };
    case TB_ZoomSelectedObj: return { TB_ZoomSelectedObj, {"Show the first selected row","Make the extent of the selected elements fit in the ViewPort"}, {TB_ZoomSelectedObj}, {":/res/images/TB_zoom_selected.bmp"} };
    case TB_SelectRows: return { TB_SelectRows, {"Select row(s) by mouse-click (use Shift to add or Ctrl to deselect)",""}, {TB_SelectRows}, {":/res/images/TB_table_select_row.bmp"} };
    case TB_SelectAll: return { TB_SelectAll, {"Select all rows","Select all elements in the active layer"}, {TB_SelectAll}, {":/res/images/TB_select_all.bmp"} };
    case TB_SelectNone: return { TB_SelectNone, {"Deselect all rows","Deselect all elements in the active layer"}, {TB_SelectNone}, {":/res/images/TB_select_none.bmp"} };
    case TB_ShowSelOnlyOn: return { TB_ShowSelOnlyOn, {"Show only selected rows","Show only selected elements"}, {TB_ShowSelOnlyOff, TB_ShowSelOnlyOn}, {":/res/images/TB_show_selected_features.bmp"} };
    case TB_TableGroupBy: return { TB_TableGroupBy, {"Group by the highlighted columns",""}, {TB_Neutral, TB_TableGroupBy}, {":/res/images/TB_group_by.bmp"} };
    case TB_ZoomAllLayers: return { TB_ZoomAllLayers, {"","Make the extents of all layers fit in the ViewPort"}, {TB_ZoomAllLayers}, {":/res/images/TB_zoom_all_layers.bmp"} };
    case TB_ZoomActiveLayer: return { TB_ZoomActiveLayer, {"","Make the extent of the active layer fit in the ViewPort"}, {TB_ZoomActiveLayer}, {":/res/images/TB_zoom_active_layer.bmp"} };
    case TB_ZoomIn2: return { TB_ZoomIn2, {"","Zoom in by drawing a rectangle"}, {TB_Neutral, TB_ZoomIn2}, {":/res/images/TB_zoomin_button.bmp"}, true };
    case TB_ZoomOut2: return { TB_ZoomOut2, {"","Zoom out by clicking on a ViewPort location"}, {TB_Neutral, TB_ZoomOut2}, {":/res/images/TB_zoomout_button.bmp"}, true };
    case TB_Pan: return { TB_Pan, {"","Pan by holding left mouse button down in the ViewPort and dragging"}, {TB_Neutral, TB_Pan}, {":/res/images/TB_pan_button.bmp"}, true };
    case TB_SelectObject: return { TB_SelectObject, {"","Select elements in the active layer by mouse-click(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectObject}, {":/res/images/TB_select_object.bmp"}, true };
    case TB_SelectRect: return { TB_SelectRect, {"","Select elements in the active layer by drawing a rectangle(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectRect}, {":/res/images/TB_select_rect.bmp"}, true };
    case TB_SelectCircle: return { TB_SelectCircle, {"","Select elements in the active layer by drawing a circle(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectCircle}, {":/res/images/TB_select_circle.bmp"}, true };
    case TB_SelectPolygon: return { TB_SelectPolygon, {"","Select elements in the active layer by drawing a polygon(use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectPolygon}, {":/res/images/TB_select_poly.bmp"}, true };
    case TB_SelectDistrict: return { TB_SelectDistrict, {"","Select contiguous regions in the active layer by clicking on them (use Shift to add or Ctrl to deselect)"}, {TB_Neutral, TB_SelectDistrict}, {":/res/images/TB_select_district.bmp"}, true };
    case TB_Show_VP: return { TB_Show_VP, {"","Toggle the layout of the ViewPort between{MapView only, with LayerControlList, with Overview and LayerControlList}"}, {TB_Show_VPLCOV,TB_Show_VP, TB_Show_VPLC}, {":/res/images/TB_toggle_layout_3.bmp", ":/res/images/TB_toggle_layout_1.bmp", ":/res/images/TB_toggle_layout_2.bmp"} };
    case TB_SP_All: return { TB_SP_All, {"","Toggle Palette Visibiliy between{All, Active Layer Only, None}"}, {TB_SP_All, TB_SP_Active, TB_SP_None}, {":/res/images/TB_toggle_palette.bmp"} };
    case TB_NeedleOn: return { TB_NeedleOn, {"","Show / Hide NeedleController"}, {TB_NeedleOff, TB_NeedleOn}, {":/res/images/TB_toggle_needle.bmp"} };
    case TB_ScaleBarOn: return { TB_ScaleBarOn, {"","Show / Hide ScaleBar"}, {TB_ScaleBarOff, TB_ScaleBarOn}, {":/res/images/TB_toggle_scalebar.bmp"} };
    }

    return {};
}

void updateDmsToolbar() {
    auto main_window = MainWindow::TheOne();

    // TODO: #387 separate variable for wanted toolbar state: mapview, tableview
    if (!main_window->m_toolbar) 
        createDmsToolbar();

    QMdiSubWindow* active_mdi_subwindow = main_window->m_mdi_area->activeSubWindow();
    if (!active_mdi_subwindow)
        clearToolbarUpToDetailPagesTools();

    if (main_window->m_tooled_mdi_subwindow == active_mdi_subwindow) // Do nothing
        return;

    auto view_style = ViewStyle::tvsUndefined;

    main_window->m_tooled_mdi_subwindow = active_mdi_subwindow;
    auto active_dms_view_area = dynamic_cast<QDmsViewArea*>(active_mdi_subwindow);

    DataView* dv = nullptr;
    if (active_dms_view_area) {
        // get viewstyle from dataview
        dv = active_dms_view_area->getDataView();
        if (dv)
            view_style = dv->GetViewType();
    }

    // get viewstyle from property
    if (main_window->m_tooled_mdi_subwindow && view_style == ViewStyle::tvsUndefined)
        ViewStyle view_style = static_cast<ViewStyle>(main_window->m_tooled_mdi_subwindow->property("viewstyle").value<QVariant>().toInt());

    if (view_style == ViewStyle::tvsUndefined) // No tools for undefined viewstyle
        return;

    if (view_style == main_window->m_current_toolbar_style) // Do nothing
        return;

    main_window->m_current_toolbar_style = view_style;

    // disable/enable coordinate tool
    auto is_mapview = view_style == ViewStyle::tvsMapView;
    auto is_tableview = view_style == ViewStyle::tvsTableView;
    main_window->m_statusbar_coordinates->setVisible(is_mapview || is_tableview);
    clearToolbarUpToDetailPagesTools();

    static std::vector<ToolButtonID> available_table_buttons = getAvailableTableviewButtonIds();
    static std::vector<ToolButtonID> available_map_buttons = getAvailableMapviewButtonIds();

    std::vector < ToolButtonID>* buttons_array_ptr = nullptr;

    switch (view_style) {
    case ViewStyle::tvsTableView: buttons_array_ptr = &available_table_buttons; break;
    case ViewStyle::tvsMapView:   buttons_array_ptr = &available_map_buttons; break;
    }
    if (buttons_array_ptr == nullptr)
        return;

    assert(dv);
    auto first_toolbar_detail_pages_action = main_window->m_dms_toolbar_spacer_action.get();
    for (auto button_id : *buttons_array_ptr) {
        if (button_id == TB_Undefined) {
            QWidget* spacer = new QWidget(main_window);
            spacer->setMinimumSize(dms_params::toolbar_button_spacing);
            main_window->m_current_dms_view_actions.push_back(main_window->m_toolbar->insertWidget(first_toolbar_detail_pages_action, spacer));
            spacer->setFocusPolicy(Qt::FocusPolicy::NoFocus);
            continue;
        }

        auto button_data = getToolbarButtonData(button_id);
        auto button_icon = QIcon(button_data.icons[0]);
        auto action = new DmsToolbuttonAction(button_id, button_icon, view_style == ViewStyle::tvsTableView ? button_data.text[0] : button_data.text[1], main_window->m_toolbar, button_data, view_style);
        main_window->m_current_dms_view_actions.push_back(action);
        auto is_command_enabled = dv->OnCommandEnable(button_id) == CommandStatus::ENABLED;
        if (!is_command_enabled)
            action->setDisabled(true);

        main_window->m_toolbar->insertAction(first_toolbar_detail_pages_action, action);

        // connections
        main_window->connect(action, &DmsToolbuttonAction::triggered, action, &DmsToolbuttonAction::onToolbuttonPressed);
    }
}