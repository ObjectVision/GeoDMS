object frmSettings: TfrmSettings
  Left = 179
  Top = 177
  BorderIcons = [biSystemMenu]
  BorderWidth = 2
  Caption = 'Options'
  ClientHeight = 470
  ClientWidth = 510
  Color = clBtnFace
  Constraints.MinHeight = 502
  Constraints.MinWidth = 522
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = True
  Position = poScreenCenter
  Scaled = False
  OnCreate = FormCreate
  DesignSize = (
    510
    470)
  PixelsPerInch = 96
  TextHeight = 14
  object Label2: TLabel
    Left = 7
    Top = 27
    Width = 77
    Height = 14
    Caption = '&No-data color:'
  end
  object SpeedButton1: TSpeedButton
    Left = 223
    Top = 22
    Width = 23
    Height = 22
    Glyph.Data = {
      76010000424D7601000000000000760000002800000020000000100000000100
      0400000000000001000000000000000000001000000000000000000000000000
      8000008000000080800080000000800080008080000080808000C0C0C0000000
      FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
      33333333333333333333333333333333333333FFFFFFFFFFFF33300000000000
      003337777777777777F330AAA0EEE0DDD03337888788878887F330AAA0EEE0DD
      D03337888788878887F330AAA0EEE0DDD03337888788878887F3300000000000
      003337777777777777F3307770FFF000003337777788877777F3307770FFF000
      003337777788877777F3307770FFF000003337777788877777F3300000000000
      003337777777777777F3309990BBB0CCC03337888788878887F3309990BBB0CC
      C03337888788878887F3309990BBB0CCC03337888788878887F3300000000000
      0033377777777777773333333333333333333333333333333333}
    NumGlyphs = 2
  end
  object Label3: TLabel
    Left = 15
    Top = 35
    Width = 77
    Height = 14
    Caption = '&No-data color:'
  end
  object SpeedButton2: TSpeedButton
    Left = 231
    Top = 30
    Width = 23
    Height = 22
    Glyph.Data = {
      76010000424D7601000000000000760000002800000020000000100000000100
      0400000000000001000000000000000000001000000000000000000000000000
      8000008000000080800080000000800080008080000080808000C0C0C0000000
      FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
      33333333333333333333333333333333333333FFFFFFFFFFFF33300000000000
      003337777777777777F330AAA0EEE0DDD03337888788878887F330AAA0EEE0DD
      D03337888788878887F330AAA0EEE0DDD03337888788878887F3300000000000
      003337777777777777F3307770FFF000003337777788877777F3307770FFF000
      003337777788877777F3307770FFF000003337777788877777F3300000000000
      003337777777777777F3309990BBB0CCC03337888788878887F3309990BBB0CC
      C03337888788878887F3309990BBB0CCC03337888788878887F3300000000000
      0033377777777777773333333333333333333333333333333333}
    NumGlyphs = 2
  end
  object Label4: TLabel
    Left = 15
    Top = 115
    Width = 77
    Height = 14
    Caption = '&No-data color:'
  end
  object SpeedButton3: TSpeedButton
    Left = 231
    Top = 110
    Width = 23
    Height = 22
    Glyph.Data = {
      76010000424D7601000000000000760000002800000020000000100000000100
      0400000000000001000000000000000000001000000000000000000000000000
      8000008000000080800080000000800080008080000080808000C0C0C0000000
      FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00333333333333
      33333333333333333333333333333333333333FFFFFFFFFFFF33300000000000
      003337777777777777F330AAA0EEE0DDD03337888788878887F330AAA0EEE0DD
      D03337888788878887F330AAA0EEE0DDD03337888788878887F3300000000000
      003337777777777777F3307770FFF000003337777788877777F3307770FFF000
      003337777788877777F3307770FFF000003337777788877777F3300000000000
      003337777777777777F3309990BBB0CCC03337888788878887F3309990BBB0CC
      C03337888788878887F3309990BBB0CCC03337888788878887F3300000000000
      0033377777777777773333333333333333333333333333333333}
    NumGlyphs = 2
  end
  object Label17: TLabel
    Left = 16
    Top = 384
    Width = 356
    Height = 14
    Caption = 'Minimum size for DataItem specific swapfiles in CalcCache:'
    Font.Charset = ANSI_CHARSET
    Font.Color = clGreen
    Font.Height = -12
    Font.Name = 'Tahoma'
    Font.Style = [fsBold]
    ParentFont = False
  end
  object Label18: TLabel
    Left = 459
    Top = 384
    Width = 30
    Height = 14
    Caption = 'bytes'
  end
  object StatusBar1: TStatusBar
    Left = 0
    Top = 431
    Width = 510
    Height = 39
    Panels = <
      item
        Bevel = pbNone
        Width = 50
      end>
  end
  object PageControl1: TPageControl
    Left = 4
    Top = 8
    Width = 498
    Height = 424
    ActivePage = tsGUI
    Anchors = [akLeft, akTop, akRight, akBottom]
    TabOrder = 0
    object tsGUI: TTabSheet
      Caption = 'GUI'
      ImageIndex = 1
      object lblMapviewSettings: TLabel
        Left = 20
        Top = 188
        Width = 149
        Height = 14
        Caption = 'Map View Color settings'
        Font.Charset = ANSI_CHARSET
        Font.Color = clGreen
        Font.Height = -12
        Font.Name = 'Tahoma'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object Label5: TLabel
        Left = 39
        Top = 85
        Width = 29
        Height = 14
        Caption = '&Valid:'
      end
      object Label7: TLabel
        Left = 39
        Top = 117
        Width = 79
        Height = 14
        Caption = '&NotCalculated:'
      end
      object Label8: TLabel
        Left = 39
        Top = 149
        Width = 34
        Height = 14
        Caption = '&Failed:'
      end
      object Label9: TLabel
        Left = 20
        Top = 276
        Width = 209
        Height = 14
        Caption = 'Default Classification Ramp Colors'
        Font.Charset = ANSI_CHARSET
        Font.Color = clGreen
        Font.Height = -12
        Font.Name = 'Tahoma'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object Label10: TLabel
        Left = 20
        Top = 319
        Width = 27
        Height = 14
        Caption = '&Start'
      end
      object Label11: TLabel
        Left = 20
        Top = 351
        Width = 25
        Height = 14
        Caption = '&End:'
      end
      object Label12: TLabel
        Left = 20
        Top = 227
        Width = 64
        Height = 14
        Caption = '&Background'
      end
      object stValidColor: TEdit
        Left = 124
        Top = 83
        Width = 121
        Height = 22
        Cursor = crArrow
        Hint = 
          'This Color is used to represent items that are successfully calc' +
          'ulated'
        Color = clGray
        HideSelection = False
        ReadOnly = True
        TabOrder = 1
        OnClick = stColorClick
      end
      object stInvalidatedColor: TEdit
        Left = 124
        Top = 113
        Width = 121
        Height = 22
        Cursor = crArrow
        Hint = 
          'This Color is used to represent items that are currently not yet' +
          ' calculated or invalidated by a recent change'
        Color = clGray
        HideSelection = False
        ReadOnly = True
        TabOrder = 2
        OnClick = stColorClick
      end
      object stFailedColor: TEdit
        Left = 124
        Top = 145
        Width = 121
        Height = 22
        Cursor = crArrow
        Hint = 
          'This Color is used to represent items for which the calculation ' +
          'of the data failed'
        Color = clGray
        HideSelection = False
        ReadOnly = True
        TabOrder = 3
        OnClick = stColorClick
      end
      object stRampStart: TEdit
        Left = 124
        Top = 318
        Width = 121
        Height = 22
        Cursor = crArrow
        Hint = 
          'This Color is used to represent items for which the calculation ' +
          'of the data failed'
        Color = clGray
        HideSelection = False
        ReadOnly = True
        TabOrder = 4
        OnClick = stColorClick
      end
      object stRampEnd: TEdit
        Left = 124
        Top = 346
        Width = 121
        Height = 22
        Cursor = crArrow
        Hint = 
          'This Color is used to represent items for which the calculation ' +
          'of the data failed'
        Color = clGray
        HideSelection = False
        ReadOnly = True
        TabOrder = 5
        OnClick = stColorClick
      end
      object stBackgroundColor: TEdit
        Left = 124
        Top = 224
        Width = 121
        Height = 22
        Cursor = crArrow
        Hint = 'This Color is used as the background in a geographic map'
        Color = clGray
        HideSelection = False
        ReadOnly = True
        TabOrder = 0
        OnClick = stColorClick
      end
      object chbShowStateColors: TCheckBox
        Left = 20
        Top = 60
        Width = 231
        Height = 17
        Caption = 'Sh&ow State Colors in TreeView'
        TabOrder = 6
        OnClick = SettingChange
      end
      object chbShowThousandSeparator: TCheckBox
        Left = 20
        Top = 37
        Width = 193
        Height = 17
        Caption = 'Show T&housand Separators'
        TabOrder = 7
        OnClick = SettingChange
      end
      object chbAdminMode: TCheckBox
        Left = 20
        Top = 14
        Width = 161
        Height = 17
        Caption = '&Show Hidden Items'
        TabOrder = 8
        OnClick = chbAdminModeClick
      end
    end
    object tsAdvanced: TTabSheet
      Caption = 'Advanced'
      DesignSize = (
        490
        395)
      object lblExternalPrograms: TLabel
        Left = 8
        Top = 115
        Width = 112
        Height = 14
        Hint = 'Path references used in the Main Menu form'
        Caption = 'External programs'
        Font.Charset = ANSI_CHARSET
        Font.Color = clGreen
        Font.Height = -12
        Font.Name = 'Tahoma'
        Font.Style = [fsBold]
        ParentFont = False
        ParentShowHint = False
        ShowHint = True
      end
      object Bevel1: TBevel
        Left = 0
        Top = 88
        Width = 490
        Height = 13
        Anchors = [akLeft, akTop, akRight]
        Shape = bsBottomLine
      end
      object Bevel2: TBevel
        Left = 0
        Top = 284
        Width = 490
        Height = 13
        Anchors = [akLeft, akTop, akRight]
        Shape = bsBottomLine
      end
      object lblDMSEditor: TLabel
        Left = 8
        Top = 143
        Width = 64
        Height = 14
        Caption = '&DMS editor:'
        FocusControl = edDMSeditor
      end
      object btnDMSEditor: TSpeedButton
        Left = 459
        Top = 138
        Width = 23
        Height = 22
        Anchors = [akTop, akRight]
        Glyph.Data = {
          76010000424D7601000000000000760000002800000020000000100000000100
          04000000000000010000120B0000120B00001000000000000000000000000000
          800000800000008080008000000080008000808000007F7F7F00BFBFBF000000
          FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00555555555555
          5555555555555555555555555555555555555555555555555555555555555555
          555555555555555555555555555555555555555FFFFFFFFFF555550000000000
          55555577777777775F55500B8B8B8B8B05555775F555555575F550F0B8B8B8B8
          B05557F75F555555575F50BF0B8B8B8B8B0557F575FFFFFFFF7F50FBF0000000
          000557F557777777777550BFBFBFBFB0555557F555555557F55550FBFBFBFBF0
          555557F555555FF7555550BFBFBF00055555575F555577755555550BFBF05555
          55555575FFF75555555555700007555555555557777555555555555555555555
          5555555555555555555555555555555555555555555555555555}
        NumGlyphs = 2
        OnClick = btnDMSEditorClick
      end
      object TLabel
        Left = 8
        Top = 186
        Width = 234
        Height = 14
        Hint = 'Path references used in the Main Menu form'
        Caption = 'Multi Tasking (fka: Parallel Processing)'
        Font.Charset = ANSI_CHARSET
        Font.Color = clGreen
        Font.Height = -12
        Font.Name = 'Tahoma'
        Font.Style = [fsBold]
        ParentFont = False
        ParentShowHint = False
        ShowHint = True
      end
      object Bevel3: TBevel
        Left = 0
        Top = 165
        Width = 490
        Height = 13
        Anchors = [akLeft, akTop, akRight]
        Shape = bsBottomLine
      end
      object Label14: TLabel
        Left = 8
        Top = 14
        Width = 35
        Height = 14
        Hint = 'Path references used in the Main Menu form'
        Caption = 'Paths'
        Font.Charset = ANSI_CHARSET
        Font.Color = clGreen
        Font.Height = -12
        Font.Name = 'Tahoma'
        Font.Style = [fsBold]
        ParentFont = False
        ParentShowHint = False
        ShowHint = True
      end
      object Label15: TLabel
        Left = 14
        Top = 47
        Width = 70
        Height = 14
        Caption = '&LocalDataDir:'
        FocusControl = edDMSeditor
      end
      object Label16: TLabel
        Left = 14
        Top = 71
        Width = 81
        Height = 14
        Caption = '&SourceDataDir:'
        FocusControl = edDMSeditor
      end
      object Label13: TLabel
        Left = 8
        Top = 312
        Width = 356
        Height = 14
        Caption = 'Minimum size for DataItem specific swapfiles in CalcCache:'
        Font.Charset = ANSI_CHARSET
        Font.Color = clGreen
        Font.Height = -12
        Font.Name = 'Tahoma'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object bytes: TLabel
        Left = 451
        Top = 312
        Width = 30
        Height = 14
        Caption = 'bytes'
      end
      object Label19: TLabel
        Left = 8
        Top = 346
        Width = 286
        Height = 14
        Caption = 'Threshold for Memory Flushing wait procedure'
        Font.Charset = ANSI_CHARSET
        Font.Color = clGreen
        Font.Height = -12
        Font.Name = 'Tahoma'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object percent: TLabel
        Left = 451
        Top = 344
        Width = 12
        Height = 14
        Caption = '%'
      end
      object edDMSeditor: TEdit
        Left = 94
        Top = 139
        Width = 356
        Height = 22
        Hint = 
          'Use: %F for the file, %L for the linenumber, %C for the columnnu' +
          'mber.'
        Anchors = [akLeft, akTop, akRight]
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnChange = SettingChange
      end
      object chbSuspendForGUI: TCheckBox
        Left = 14
        Top = 215
        Width = 239
        Height = 17
        Anchors = [akLeft, akTop, akRight]
        Caption = '0: Sus&pend View Updates to favour GUI'
        TabOrder = 3
        OnClick = SettingChange
      end
      object chbTraceLogFile: TCheckBox
        Left = 8
        Top = 366
        Width = 161
        Height = 17
        Caption = '&TraceLogFile'
        TabOrder = 1
        OnClick = SettingChange
      end
      object edLocalDataDir: TEdit
        Left = 100
        Top = 43
        Width = 384
        Height = 22
        Hint = 
          'Use: %F for the file, %L for the linenumber, %C for the columnnu' +
          'mber.'
        Anchors = [akLeft, akTop, akRight]
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
        OnChange = SettingChange
      end
      object edSourceDataDir: TEdit
        Left = 100
        Top = 67
        Width = 384
        Height = 22
        Hint = 
          'Use: %F for the file, %L for the linenumber, %C for the columnnu' +
          'mber.'
        Anchors = [akLeft, akTop, akRight]
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
        OnChange = SettingChange
      end
      object chbMultiThreading: TCheckBox
        Left = 14
        Top = 235
        Width = 193
        Height = 17
        Caption = '1: Tile/segment tasks'
        TabOrder = 2
        OnClick = SettingChange
      end
      object chbMT2: TCheckBox
        Left = 14
        Top = 255
        Width = 281
        Height = 17
        Hint = 'When enabled, independent operations may run simultaneously'
        Caption = '&2: Multiple operations simultaneously'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 9
        OnClick = SettingChange
      end
      object chbMT3: TCheckBox
        Left = 14
        Top = 275
        Width = 281
        Height = 17
        Hint = 'When enabled, many operations are processed tile by tile'
        Caption = '&3: Pipelined Tile operations'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
        OnClick = SettingChange
      end
      object txtSwapfileMinSize: TEdit
        Left = 376
        Top = 309
        Width = 65
        Height = 22
        BiDiMode = bdLeftToRight
        ParentBiDiMode = False
        TabOrder = 7
        Text = '2000000000'
        OnChange = SettingChange
      end
      object txtFlushThreshold: TEdit
        Left = 376
        Top = 341
        Width = 65
        Height = 22
        BiDiMode = bdLeftToRight
        ParentBiDiMode = False
        TabOrder = 8
        Text = '90'
        OnChange = SettingChange
      end
    end
    object tsConfigSettings: TTabSheet
      Hint = 
        'Press OK or Apply if you want to save changes to the User Specif' +
        'ic Settings in the Registry to override the configured value on ' +
        'this machine; Press Cancel if you want to keep using the current' +
        ' values.'
      Caption = 'Configuration'
      ImageIndex = 2
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
    end
  end
  object btnCancel: TButton
    Left = 410
    Top = 438
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Cancel = True
    Caption = '&Cancel'
    ModalResult = 2
    TabOrder = 1
  end
  object btnOk: TButton
    Left = 240
    Top = 438
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Ok'
    Default = True
    Enabled = False
    ParentShowHint = False
    ShowHint = True
    TabOrder = 2
    OnClick = btnOkClick
  end
  object btnApply: TButton
    Left = 325
    Top = 438
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Apply'
    Enabled = False
    ParentShowHint = False
    ShowHint = True
    TabOrder = 3
    OnClick = btnApplyClick
  end
end
