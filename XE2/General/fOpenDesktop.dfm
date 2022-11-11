object frmOpenDesktop: TfrmOpenDesktop
  Left = 90
  Top = 149
  Caption = 'Open desktop'
  ClientHeight = 537
  ClientWidth = 787
  Color = clBtnFace
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  Scaled = False
  DesignSize = (
    787
    537)
  PixelsPerInch = 96
  TextHeight = 14
  object stnMain: TStatusBar
    Left = 0
    Top = 498
    Width = 787
    Height = 39
    Panels = <
      item
        Bevel = pbNone
        Width = 50
      end>
  end
  object pnlMain: TPanel
    Left = 5
    Top = 5
    Width = 755
    Height = 493
    Anchors = [akLeft, akTop, akRight, akBottom]
    Caption = ' '
    TabOrder = 0
    DesignSize = (
      755
      493)
    object lvDesktops: TListView
      Tag = 303
      Left = 0
      Top = 0
      Width = 759
      Height = 497
      Anchors = [akLeft, akTop, akRight, akBottom]
      Columns = <
        item
          Caption = 'Name'
          Width = 754
        end>
      HideSelection = False
      ReadOnly = True
      RowSelect = True
      SortType = stText
      TabOrder = 0
      ViewStyle = vsReport
      OnDblClick = lvDesktopsDblClick
      OnSelectItem = lvDesktopsSelectItem
    end
  end
  object btnOK: TButton
    Left = 602
    Top = 505
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&OK'
    Default = True
    Enabled = False
    ModalResult = 1
    TabOrder = 1
  end
  object btnCancel: TButton
    Left = 687
    Top = 505
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Cancel = True
    Caption = '&Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object btnHelp: TButton
    Left = 813
    Top = 504
    Width = 92
    Height = 29
    Anchors = [akRight, akBottom]
    Caption = '&Help'
    Enabled = False
    TabOrder = 3
  end
end
