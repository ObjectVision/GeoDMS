object TreeItemPickList: TTreeItemPickList
  Left = 0
  Top = 0
  Width = 451
  Height = 304
  Align = alClient
  AutoSize = True
  Constraints.MinHeight = 250
  Constraints.MinWidth = 275
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Tahoma'
  Font.Style = []
  ParentFont = False
  TabOrder = 0
  object stnMain: TStatusBar
    Left = 0
    Top = 285
    Width = 451
    Height = 19
    Panels = <
      item
        Bevel = pbNone
        Text = 'Filter: <NO FILTER>'
        Width = 640
      end>
    ExplicitTop = 247
    ExplicitWidth = 435
  end
  object lvTreeItems: TListView
    Left = 0
    Top = 0
    Width = 451
    Height = 285
    Align = alClient
    Columns = <>
    HideSelection = False
    ReadOnly = True
    RowSelect = True
    TabOrder = 1
    ViewStyle = vsReport
    OnColumnClick = lvTreeItemsColumnClick
    OnColumnRightClick = lvTreeItemsColumnRightClick
    OnCompare = lvTreeItemsCompare
    OnDblClick = lvTreeItemsDblClick
    OnSelectItem = lvTreeItemsSelectItem
    ExplicitWidth = 435
    ExplicitHeight = 247
  end
  object pmColumnHeaders: TPopupMenu
    Left = 24
    Top = 24
    object miSetfilter: TMenuItem
      Caption = 'Set filter'
      OnClick = miSetfilterClick
    end
  end
end
