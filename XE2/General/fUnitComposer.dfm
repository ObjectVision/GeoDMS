object frmUnitComposer: TfrmUnitComposer
  Left = 299
  Top = 151
  BorderIcons = [biSystemMenu]
  Caption = 'Insert Entity/Unit'
  ClientHeight = 592
  ClientWidth = 739
  Color = clBtnFace
  Constraints.MinHeight = 508
  Constraints.MinWidth = 582
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  Scaled = False
  DesignSize = (
    739
    592)
  PixelsPerInch = 96
  TextHeight = 14
  object stnMain: TStatusBar
    Left = 0
    Top = 553
    Width = 739
    Height = 39
    Panels = <
      item
        Bevel = pbNone
        Width = 50
      end>
  end
  object btnOK_Next: TButton
    Left = 554
    Top = 561
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = 'OK/Next >'
    Default = True
    TabOrder = 2
    OnClick = btnOK_NextClick
  end
  object btnCancel: TButton
    Left = 469
    Top = 561
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 3
  end
  object btnHelp: TButton
    Left = 639
    Top = 561
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Help'
    Enabled = False
    TabOrder = 4
  end
  object pcMain: TPageControl
    Left = 5
    Top = 5
    Width = 724
    Height = 548
    ActivePage = tsAdvancedUnit
    Anchors = [akLeft, akTop, akRight, akBottom]
    TabOrder = 0
    object tsSelectType: TTabSheet
      TabVisible = False
      OnShow = tsSelectTypeShow
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      DesignSize = (
        716
        538)
      object Label8: TLabel
        Left = 10
        Top = 10
        Width = 131
        Height = 18
        Caption = 'Select the unit type:'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
      object Label9: TLabel
        Left = 10
        Top = 46
        Width = 47
        Height = 16
        Caption = 'Domain'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -13
        Font.Name = 'MS Sans Serif'
        Font.Style = [fsUnderline]
        ParentFont = False
      end
      object Label10: TLabel
        Left = 10
        Top = 149
        Width = 42
        Height = 16
        Caption = 'Values'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -13
        Font.Name = 'MS Sans Serif'
        Font.Style = [fsUnderline]
        ParentFont = False
      end
      object rbTableUnit: TRadioButton
        Left = 20
        Top = 80
        Width = 139
        Height = 21
        Caption = '&Table unit'
        TabOrder = 0
        OnClick = SelectUnitType
      end
      object rbGridUnit: TRadioButton
        Left = 20
        Top = 114
        Width = 139
        Height = 21
        Caption = '&Grid unit'
        TabOrder = 1
        OnClick = SelectUnitType
      end
      object rbQuantityUnit: TRadioButton
        Left = 20
        Top = 183
        Width = 139
        Height = 21
        Caption = '&Quantity unit'
        Checked = True
        TabOrder = 2
        TabStop = True
        OnClick = SelectUnitType
      end
      object rbShapeUnit: TRadioButton
        Left = 20
        Top = 252
        Width = 139
        Height = 21
        Caption = '&Shape unit'
        TabOrder = 4
        OnClick = SelectUnitType
      end
      object rbStringUnit: TRadioButton
        Left = 20
        Top = 321
        Width = 139
        Height = 21
        Caption = 'St&ring unit'
        TabOrder = 6
        OnClick = SelectUnitType
      end
      object rbBoolUnit: TRadioButton
        Left = 20
        Top = 356
        Width = 139
        Height = 21
        Caption = '&Boolean unit'
        TabOrder = 7
        OnClick = SelectUnitType
      end
      object rbAdvancedUnit: TRadioButton
        Left = 20
        Top = 425
        Width = 139
        Height = 21
        Caption = '&Advanced'
        TabOrder = 9
        Visible = False
        OnClick = SelectUnitType
      end
      object rbPointUnit: TRadioButton
        Left = 20
        Top = 218
        Width = 139
        Height = 21
        Caption = '&Point unit'
        TabOrder = 3
        OnClick = SelectUnitType
      end
      object rbCodeUnit: TRadioButton
        Left = 20
        Top = 287
        Width = 139
        Height = 21
        Caption = '&Code unit'
        TabOrder = 5
        OnClick = SelectUnitType
      end
      object rbFocalPointMatrix: TRadioButton
        Left = 20
        Top = 390
        Width = 139
        Height = 21
        Caption = '&Focal point matrix'
        TabOrder = 8
        OnClick = SelectUnitType
      end
      object reDescription: TRichEdit
        Left = 200
        Top = 46
        Width = 501
        Height = 432
        Anchors = [akTop, akRight, akBottom]
        BorderStyle = bsNone
        BorderWidth = 2
        Enabled = False
        Font.Charset = ANSI_CHARSET
        Font.Color = clBlue
        Font.Height = -13
        Font.Name = 'Tahoma'
        Font.Style = []
        Lines.Strings = (
          'No type selected.')
        ParentColor = True
        ParentFont = False
        TabOrder = 10
      end
    end
    object tsTableUnit: TTabSheet
      ImageIndex = 1
      TabVisible = False
      OnShow = ShowTabSheet
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      DesignSize = (
        716
        538)
      object Label1: TLabel
        Left = 10
        Top = 10
        Width = 64
        Height = 18
        Caption = 'Table unit'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
      object Bevel3: TBevel
        Left = 10
        Top = 223
        Width = 321
        Height = 91
        Shape = bsFrame
      end
      object Bevel4: TBevel
        Left = 341
        Top = 296
        Width = 363
        Height = 240
        Anchors = [akLeft, akTop, akRight, akBottom]
        Shape = bsFrame
      end
      object Bevel5: TBevel
        Left = 341
        Top = 159
        Width = 364
        Height = 131
        Anchors = [akLeft, akTop, akRight]
        Shape = bsFrame
      end
      object Bevel6: TBevel
        Left = 10
        Top = 320
        Width = 321
        Height = 216
        Anchors = [akLeft, akTop, akBottom]
        Shape = bsFrame
      end
      object Label33: TLabel
        Left = 34
        Top = 255
        Width = 62
        Height = 14
        Caption = 'S&ource TU:'
      end
      object Label34: TLabel
        Left = 34
        Top = 284
        Width = 30
        Height = 14
        Caption = '&Filter:'
      end
      object Label35: TLabel
        Left = 362
        Top = 194
        Width = 62
        Height = 14
        Caption = 'Sou&rce TU:'
      end
      object Label36: TLabel
        Left = 362
        Top = 224
        Width = 30
        Height = 14
        Caption = 'F&ilter:'
      end
      object Label37: TLabel
        Left = 346
        Top = 324
        Width = 61
        Height = 14
        Caption = 'Tar&get VU:'
      end
      object Bevel7: TBevel
        Left = 10
        Top = 159
        Width = 321
        Height = 58
        Shape = bsFrame
      end
      object Label38: TLabel
        Left = 26
        Top = 188
        Width = 32
        Height = 14
        Caption = '&Type:'
        FocusControl = cbxTableBaseType
      end
      object Label39: TLabel
        Left = 189
        Top = 188
        Width = 37
        Height = 14
        Caption = '&Count:'
        FocusControl = edTableCount
      end
      object sbUniqueFilter: TSpeedButton
        Left = 595
        Top = 220
        Width = 20
        Height = 24
        Caption = '...'
        Enabled = False
        OnClick = sbUniqueFilterClick
      end
      object sbUniqueSourceTU: TSpeedButton
        Left = 595
        Top = 191
        Width = 20
        Height = 23
        Caption = '...'
        Enabled = False
        OnClick = sbUniqueSourceTUClick
      end
      object sbSourceTU: TSpeedButton
        Left = 260
        Top = 251
        Width = 20
        Height = 23
        Caption = '...'
        Enabled = False
        OnClick = sbSourceTUClick
      end
      object sbSubsetFilter: TSpeedButton
        Left = 260
        Top = 281
        Width = 20
        Height = 23
        Caption = '...'
        Enabled = False
        OnClick = sbSubsetFilterClick
      end
      object sbTargetVU: TSpeedButton
        Left = 579
        Top = 320
        Width = 20
        Height = 23
        Caption = '...'
        Enabled = False
        OnClick = sbTargetVUClick
      end
      object rbSubsetUnit: TRadioButton
        Left = 18
        Top = 226
        Width = 92
        Height = 21
        Caption = '&Subset'
        TabOrder = 3
        OnClick = TableSelectStyle
      end
      object rbUnion: TRadioButton
        Left = 346
        Top = 300
        Width = 95
        Height = 21
        Caption = 'U&nion'
        TabOrder = 8
        OnClick = TableSelectStyle
      end
      object rbUnique: TRadioButton
        Left = 346
        Top = 162
        Width = 91
        Height = 21
        Caption = '&Unique'
        TabOrder = 6
        OnClick = TableSelectStyle
      end
      object rbCombine: TRadioButton
        Left = 18
        Top = 324
        Width = 92
        Height = 21
        Caption = 'Co&mbine'
        TabOrder = 4
        OnClick = TableSelectStyle
      end
      object dlbUnion: TDualListBox
        Left = 346
        Top = 347
        Width = 344
        Height = 174
        Anchors = [akLeft, akTop, akRight, akBottom]
        Constraints.MinHeight = 115
        Constraints.MinWidth = 229
        Enabled = False
        TabOrder = 9
        SourceCaption = 'Available'
        DestinationCaption = 'Selected'
        MaxSelection = 5
        OnSelectionChanged = DataChanged
        DesignSize = (
          344
          174)
      end
      object dlbCombine: TDualListBox
        Left = 18
        Top = 347
        Width = 303
        Height = 174
        Anchors = [akLeft, akTop, akBottom]
        Constraints.MinHeight = 115
        Constraints.MinWidth = 229
        Enabled = False
        TabOrder = 5
        SourceCaption = 'A&vailable'
        DestinationCaption = 'Se&lected'
        MaxSelection = 8
        OnSelectionChanged = DataChanged
        DesignSize = (
          303
          174)
      end
      object chbCount: TCheckBox
        Left = 346
        Top = 255
        Width = 138
        Height = 21
        Caption = 'Cr&eate countitem'
        Enabled = False
        TabOrder = 7
      end
      object rbTableBaseUnit: TRadioButton
        Left = 18
        Top = 162
        Width = 92
        Height = 21
        Caption = 'B&ase unit'
        Checked = True
        TabOrder = 0
        TabStop = True
        OnClick = TableSelectStyle
      end
      object cbxTableBaseType: TComboBox
        Left = 62
        Top = 183
        Width = 114
        Height = 22
        Style = csDropDownList
        TabOrder = 1
        OnChange = DataChanged
      end
      object edTableCount: TEdit
        Left = 228
        Top = 183
        Width = 97
        Height = 22
        TabOrder = 2
        OnChange = DataChanged
      end
      object edUniqueFilter: TEdit
        Left = 429
        Top = 219
        Width = 156
        Height = 22
        Enabled = False
        ParentShowHint = False
        ReadOnly = True
        ShowHint = True
        TabOrder = 10
        OnChange = DataChanged
      end
      object edUniqueSourceTU: TEdit
        Left = 429
        Top = 190
        Width = 156
        Height = 22
        Enabled = False
        ParentShowHint = False
        ReadOnly = True
        ShowHint = True
        TabOrder = 11
        OnChange = edUniqueSourceTUChange
      end
      object edSourceTU: TEdit
        Left = 100
        Top = 250
        Width = 157
        Height = 22
        Enabled = False
        ParentShowHint = False
        ReadOnly = True
        ShowHint = True
        TabOrder = 12
        OnChange = edSourceTUChange
      end
      object edSubsetFilter: TEdit
        Left = 100
        Top = 279
        Width = 157
        Height = 22
        Enabled = False
        ParentShowHint = False
        ReadOnly = True
        ShowHint = True
        TabOrder = 13
        OnChange = DataChanged
      end
      object edTargetVU: TEdit
        Left = 413
        Top = 319
        Width = 156
        Height = 22
        Enabled = False
        ParentShowHint = False
        ReadOnly = True
        ShowHint = True
        TabOrder = 14
        OnChange = edTargetVUChange
      end
    end
    object tsGridUnit: TTabSheet
      ImageIndex = 2
      TabVisible = False
      OnShow = ShowTabSheet
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      DesignSize = (
        716
        538)
      object Label2: TLabel
        Left = 10
        Top = 10
        Width = 53
        Height = 18
        Caption = 'Grid unit'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
      object Bevel1: TBevel
        Left = 10
        Top = 159
        Width = 690
        Height = 108
        Anchors = [akLeft, akTop, akRight]
        Shape = bsFrame
      end
      object Bevel2: TBevel
        Left = 10
        Top = 273
        Width = 690
        Height = 256
        Anchors = [akLeft, akTop, akRight, akBottom]
        Shape = bsFrame
      end
      object Label23: TLabel
        Left = 18
        Top = 313
        Width = 85
        Height = 14
        Caption = '&Reference unit:'
      end
      object Label24: TLabel
        Left = 18
        Top = 373
        Width = 68
        Height = 14
        Caption = '&Scale factor:'
      end
      object Label25: TLabel
        Left = 18
        Top = 402
        Width = 95
        Height = 14
        Caption = '(0,0) &Coordinate:'
      end
      object Label26: TLabel
        Left = 18
        Top = 432
        Width = 52
        Height = 14
        Caption = 'To&p-Left:'
      end
      object Label27: TLabel
        Left = 18
        Top = 462
        Width = 77
        Height = 14
        Caption = 'Botto&m-Right:'
      end
      object Label28: TLabel
        Left = 126
        Top = 345
        Width = 8
        Height = 16
        Caption = 'x'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -13
        Font.Name = 'MS Sans Serif'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object Label29: TLabel
        Left = 288
        Top = 345
        Width = 9
        Height = 16
        Caption = 'y'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -13
        Font.Name = 'MS Sans Serif'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object Label30: TLabel
        Left = 18
        Top = 206
        Width = 52
        Height = 14
        Caption = '&Top-Left:'
        FocusControl = edBaseTlX
      end
      object Label31: TLabel
        Left = 18
        Top = 235
        Width = 77
        Height = 14
        Caption = 'B&ottom-Right:'
      end
      object Label22: TLabel
        Left = 126
        Top = 181
        Width = 8
        Height = 16
        Caption = 'x'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -13
        Font.Name = 'MS Sans Serif'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object Label32: TLabel
        Left = 288
        Top = 181
        Width = 9
        Height = 16
        Caption = 'y'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -13
        Font.Name = 'MS Sans Serif'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object rbGridBaseUnit: TRadioButton
        Left = 18
        Top = 162
        Width = 92
        Height = 21
        Caption = '&Base unit'
        Checked = True
        TabOrder = 0
        TabStop = True
        OnClick = GridSelectBaseUnit
      end
      object rbGridGridsetUnit: TRadioButton
        Left = 18
        Top = 281
        Width = 140
        Height = 21
        Caption = '&Gridset unit'
        TabOrder = 5
        OnClick = GridSelectBaseUnit
      end
      object cbxGridReferenceUnit: TComboBox
        Left = 126
        Top = 308
        Width = 178
        Height = 22
        Style = csDropDownList
        Enabled = False
        Sorted = True
        TabOrder = 6
        OnChange = DataChanged
      end
      object edSfX: TEdit
        Left = 126
        Top = 368
        Width = 148
        Height = 22
        Enabled = False
        TabOrder = 7
        OnChange = DataChanged
      end
      object ed0X: TEdit
        Left = 126
        Top = 398
        Width = 148
        Height = 22
        Enabled = False
        TabOrder = 9
        OnChange = DataChanged
      end
      object edTlX: TEdit
        Left = 126
        Top = 427
        Width = 148
        Height = 22
        Enabled = False
        TabOrder = 11
        OnChange = DataChanged
      end
      object edSfY: TEdit
        Left = 288
        Top = 368
        Width = 149
        Height = 22
        Enabled = False
        TabOrder = 8
        OnChange = DataChanged
      end
      object ed0Y: TEdit
        Left = 288
        Top = 398
        Width = 149
        Height = 22
        Enabled = False
        TabOrder = 10
        OnChange = DataChanged
      end
      object edTlY: TEdit
        Left = 288
        Top = 427
        Width = 149
        Height = 22
        Enabled = False
        TabOrder = 12
        OnChange = DataChanged
      end
      object edBrX: TEdit
        Left = 126
        Top = 457
        Width = 148
        Height = 22
        Enabled = False
        TabOrder = 13
        OnChange = DataChanged
      end
      object edBrY: TEdit
        Left = 288
        Top = 457
        Width = 149
        Height = 22
        Enabled = False
        TabOrder = 14
        OnChange = DataChanged
      end
      object edBaseTlX: TEdit
        Left = 126
        Top = 201
        Width = 148
        Height = 22
        TabOrder = 1
        OnChange = DataChanged
      end
      object edBaseBrX: TEdit
        Left = 126
        Top = 230
        Width = 148
        Height = 22
        TabOrder = 3
        OnChange = DataChanged
      end
      object edBaseBrY: TEdit
        Left = 288
        Top = 230
        Width = 149
        Height = 22
        TabOrder = 4
        OnChange = DataChanged
      end
      object edBaseTlY: TEdit
        Left = 288
        Top = 201
        Width = 149
        Height = 22
        TabOrder = 2
        OnChange = DataChanged
      end
    end
    object tsQuantityUnit: TTabSheet
      ImageIndex = 3
      TabVisible = False
      OnShow = ShowTabSheet
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      DesignSize = (
        716
        538)
      object Label3: TLabel
        Left = 10
        Top = 10
        Width = 83
        Height = 18
        Caption = 'Quantity unit'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
      object lblExpression: TLabel
        Left = 18
        Top = 271
        Width = 57
        Height = 14
        Caption = 'E&xpression'
        FocusControl = mmQuantityExpression
      end
      object bvlQuantityBaseUnit: TBevel
        Left = 10
        Top = 155
        Width = 690
        Height = 73
        Anchors = [akLeft, akTop, akRight]
        Shape = bsFrame
      end
      object Label19: TLabel
        Left = 18
        Top = 194
        Width = 32
        Height = 14
        Caption = '&Type:'
      end
      object bvlQuantityDerivedUnit: TBevel
        Left = 10
        Top = 234
        Width = 690
        Height = 252
        Anchors = [akLeft, akTop, akRight, akBottom]
        Shape = bsFrame
      end
      object Label20: TLabel
        Left = 18
        Top = 420
        Width = 33
        Height = 14
        Anchors = [akLeft, akBottom]
        Caption = 'Metric'
      end
      object lblMetric: TLabel
        Left = 98
        Top = 420
        Width = 28
        Height = 14
        Anchors = [akLeft, akBottom]
        Caption = '       '
      end
      object Label21: TLabel
        Left = 18
        Top = 462
        Width = 42
        Height = 14
        Anchors = [akLeft, akBottom]
        Caption = '&Format:'
        FocusControl = edQuantityFormat
        Visible = False
      end
      object btnCheck: TSpeedButton
        Left = 661
        Top = 271
        Width = 29
        Height = 27
        Anchors = [akTop, akRight]
        Enabled = False
        Glyph.Data = {
          76010000424D7601000000000000760000002800000020000000100000000100
          04000000000000010000120B0000120B00001000000000000000000000000000
          8000008000000080800080000000800080008080000080808000C0C0C0000000
          FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00555555555555
          555555555555555555555555555555555555555555FF55555555555550055555
          55555555577FF5555555555500005555555555557777F5555555555500005555
          555555557777FF5555555550000005555555555777777F555555550000000555
          5555557777777FF5555557000500005555555777757777F55555700555500055
          55557775555777FF5555555555500005555555555557777F5555555555550005
          555555555555777FF5555555555550005555555555555777FF55555555555570
          05555555555555777FF5555555555557005555555555555777FF555555555555
          5000555555555555577755555555555555555555555555555555}
        NumGlyphs = 2
        OnClick = btnCheckClick
      end
      object rbQuantityBaseUnit: TRadioButton
        Left = 18
        Top = 162
        Width = 140
        Height = 21
        Caption = '&Base unit'
        Checked = True
        TabOrder = 0
        TabStop = True
        OnClick = QuantitySelectBaseUnit
      end
      object rbQuantityDerivedUnit: TRadioButton
        Left = 18
        Top = 241
        Width = 140
        Height = 21
        Caption = 'D&erived unit'
        TabOrder = 2
        OnClick = QuantitySelectBaseUnit
      end
      object mmQuantityExpression: TMemo
        Left = 98
        Top = 271
        Width = 555
        Height = 138
        Anchors = [akLeft, akTop, akRight, akBottom]
        Enabled = False
        TabOrder = 3
        OnChange = mmQuantityExpressionChange
      end
      object cbxQuantityType: TComboBox
        Left = 64
        Top = 190
        Width = 178
        Height = 22
        Style = csDropDownList
        TabOrder = 1
        OnChange = DataChanged
      end
      object edQuantityFormat: TEdit
        Left = 100
        Top = 457
        Width = 139
        Height = 22
        Anchors = [akLeft, akBottom]
        TabOrder = 4
        Visible = False
        OnChange = DataChanged
      end
    end
    object tsPointUnit: TTabSheet
      ImageIndex = 4
      TabVisible = False
      OnShow = ShowTabSheet
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      object Label11: TLabel
        Left = 10
        Top = 10
        Width = 59
        Height = 18
        Caption = 'Point unit'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
      object Label40: TLabel
        Left = 10
        Top = 161
        Width = 32
        Height = 14
        Caption = '&Type:'
        FocusControl = cbxPointType
      end
      object Label41: TLabel
        Left = 10
        Top = 215
        Width = 52
        Height = 14
        Caption = 'T&op-Left:'
        FocusControl = edPointTLX
      end
      object Label42: TLabel
        Left = 10
        Top = 245
        Width = 77
        Height = 14
        Caption = 'Botto&m-Right:'
        FocusControl = edPointBRX
      end
      object Label43: TLabel
        Left = 98
        Top = 191
        Width = 8
        Height = 16
        Caption = 'x'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -13
        Font.Name = 'MS Sans Serif'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object Label44: TLabel
        Left = 261
        Top = 191
        Width = 9
        Height = 16
        Caption = 'y'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -13
        Font.Name = 'MS Sans Serif'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object cbxPointType: TComboBox
        Left = 98
        Top = 156
        Width = 179
        Height = 22
        Style = csDropDownList
        TabOrder = 0
        OnChange = DataChanged
      end
      object edPointTLX: TEdit
        Left = 98
        Top = 210
        Width = 149
        Height = 22
        TabOrder = 1
        OnChange = DataChanged
      end
      object edPointBRX: TEdit
        Left = 98
        Top = 240
        Width = 149
        Height = 22
        TabOrder = 3
        OnChange = DataChanged
      end
      object edPointBRY: TEdit
        Left = 261
        Top = 240
        Width = 149
        Height = 22
        TabOrder = 4
        OnChange = DataChanged
      end
      object edPointTLY: TEdit
        Left = 261
        Top = 210
        Width = 149
        Height = 22
        TabOrder = 2
        OnChange = DataChanged
      end
    end
    object tsShapeUnit: TTabSheet
      ImageIndex = 5
      TabVisible = False
      OnShow = ShowTabSheet
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      object Label12: TLabel
        Left = 10
        Top = 10
        Width = 68
        Height = 18
        Caption = 'Shape unit'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
      object Label45: TLabel
        Left = 10
        Top = 161
        Width = 32
        Height = 14
        Caption = '&Type:'
        FocusControl = cbxShapeType
      end
      object Label46: TLabel
        Left = 10
        Top = 215
        Width = 52
        Height = 14
        Caption = 'T&op-Left:'
        FocusControl = edShapeTLX
      end
      object Label47: TLabel
        Left = 10
        Top = 245
        Width = 77
        Height = 14
        Caption = 'Botto&m-Right:'
        FocusControl = edShapeBRX
      end
      object Label48: TLabel
        Left = 98
        Top = 191
        Width = 8
        Height = 16
        Caption = 'x'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -13
        Font.Name = 'MS Sans Serif'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object Label49: TLabel
        Left = 261
        Top = 191
        Width = 9
        Height = 16
        Caption = 'y'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -13
        Font.Name = 'MS Sans Serif'
        Font.Style = [fsBold]
        ParentFont = False
      end
      object Label50: TLabel
        Left = 10
        Top = 289
        Width = 42
        Height = 14
        Caption = '&Format:'
      end
      object cbxShapeType: TComboBox
        Left = 98
        Top = 156
        Width = 149
        Height = 22
        Style = csDropDownList
        TabOrder = 0
        OnChange = DataChanged
      end
      object edShapeTLX: TEdit
        Left = 98
        Top = 210
        Width = 149
        Height = 22
        TabOrder = 1
        OnChange = DataChanged
      end
      object edShapeBRX: TEdit
        Left = 98
        Top = 240
        Width = 149
        Height = 22
        TabOrder = 3
        OnChange = DataChanged
      end
      object edShapeBRY: TEdit
        Left = 261
        Top = 240
        Width = 149
        Height = 22
        TabOrder = 4
        OnChange = DataChanged
      end
      object edShapeTLY: TEdit
        Left = 261
        Top = 210
        Width = 149
        Height = 22
        TabOrder = 2
        OnChange = DataChanged
      end
      object cbxShapeFormat: TComboBox
        Left = 98
        Top = 284
        Width = 149
        Height = 22
        Style = csDropDownList
        TabOrder = 5
        OnChange = DataChanged
        Items.Strings = (
          ''
          'arc'
          'polygon')
      end
    end
    object tsCodeUnit: TTabSheet
      ImageIndex = 6
      TabVisible = False
      OnShow = ShowTabSheet
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      object Label13: TLabel
        Left = 10
        Top = 10
        Width = 61
        Height = 18
        Caption = 'Code unit'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
      object Label51: TLabel
        Left = 10
        Top = 161
        Width = 32
        Height = 14
        Caption = '&Type:'
        FocusControl = cbxCodeType
      end
      object Label52: TLabel
        Left = 10
        Top = 196
        Width = 38
        Height = 14
        Caption = '&Range:'
        FocusControl = edCodeFrom
      end
      object Label53: TLabel
        Left = 254
        Top = 196
        Width = 12
        Height = 14
        Caption = 't&o'
        FocusControl = edCodeTo
      end
      object cbxCodeType: TComboBox
        Left = 98
        Top = 156
        Width = 149
        Height = 22
        Style = csDropDownList
        TabOrder = 0
        OnChange = cbxCodeTypeChange
      end
      object edCodeFrom: TEdit
        Left = 98
        Top = 191
        Width = 149
        Height = 22
        TabOrder = 1
        OnChange = DataChanged
      end
      object edCodeTo: TEdit
        Left = 272
        Top = 191
        Width = 149
        Height = 22
        TabOrder = 2
        OnChange = DataChanged
      end
    end
    object tsStringUnit: TTabSheet
      ImageIndex = 7
      TabVisible = False
      OnShow = ShowTabSheet
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      object Label14: TLabel
        Left = 10
        Top = 10
        Width = 64
        Height = 18
        Caption = 'String unit'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
    end
    object tsBoolUnit: TTabSheet
      ImageIndex = 8
      TabVisible = False
      OnShow = ShowTabSheet
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      object Label15: TLabel
        Left = 10
        Top = 10
        Width = 79
        Height = 18
        Caption = 'Boolean unit'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
    end
    object tsFocalPointMatrix: TTabSheet
      ImageIndex = 9
      TabVisible = False
      OnShow = ShowTabSheet
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      object Label16: TLabel
        Left = 10
        Top = 10
        Width = 214
        Height = 18
        Caption = 'Focal point matrix (1/(d^2 + 2))'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
      object Label55: TLabel
        Left = 10
        Top = 161
        Width = 50
        Height = 14
        Caption = 'D&istance:'
        FocusControl = edFPDist
      end
      object Label54: TLabel
        Left = 10
        Top = 196
        Width = 83
        Height = 14
        Caption = '&Subitems type:'
        FocusControl = cbxSubitemsType
      end
      object edFPDist: TEdit
        Left = 98
        Top = 156
        Width = 149
        Height = 22
        TabOrder = 0
        OnChange = DataChanged
      end
      object cbxSubitemsType: TComboBox
        Left = 98
        Top = 191
        Width = 149
        Height = 22
        Style = csDropDownList
        TabOrder = 1
        OnChange = DataChanged
      end
    end
    object tsAdvancedUnit: TTabSheet
      ImageIndex = 10
      TabVisible = False
      OnShow = ShowTabSheet
      ExplicitLeft = 0
      ExplicitTop = 0
      ExplicitWidth = 0
      ExplicitHeight = 0
      DesignSize = (
        716
        538)
      object lblName: TLabel
        Left = 10
        Top = 48
        Width = 35
        Height = 14
        Caption = '&Name:'
        FocusControl = edName
      end
      object lblDisplayName: TLabel
        Left = 10
        Top = 82
        Width = 32
        Height = 14
        Caption = '&Label:'
        FocusControl = edDisplayName
      end
      object lblDescription: TLabel
        Left = 10
        Top = 117
        Width = 64
        Height = 14
        Caption = '&Description:'
        FocusControl = edDescription
      end
      object Label4: TLabel
        Left = 10
        Top = 161
        Width = 63
        Height = 14
        Caption = '&Value type:'
        FocusControl = cbxValueType
      end
      object Label5: TLabel
        Left = 464
        Top = 161
        Width = 42
        Height = 14
        Anchors = [akTop, akRight]
        Caption = '&Format:'
        FocusControl = cbxFormat
      end
      object Label6: TLabel
        Left = 10
        Top = 255
        Width = 61
        Height = 14
        Caption = '&Expression:'
        FocusControl = mmExpression
      end
      object Label7: TLabel
        Left = 575
        Top = 209
        Width = 12
        Height = 14
        Anchors = [akTop, akRight]
        Caption = '&to'
        FocusControl = edTo
      end
      object Label17: TLabel
        Left = 10
        Top = 10
        Width = 64
        Height = 18
        Caption = 'Advanced'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -15
        Font.Name = 'Tahoma'
        Font.Style = []
        ParentFont = False
      end
      object edName: TEdit
        Left = 98
        Top = 43
        Width = 602
        Height = 22
        Anchors = [akLeft, akTop, akRight]
        TabOrder = 0
        OnChange = DataChanged
      end
      object edDisplayName: TEdit
        Left = 98
        Top = 78
        Width = 602
        Height = 22
        Anchors = [akLeft, akTop, akRight]
        TabOrder = 1
      end
      object edDescription: TEdit
        Left = 98
        Top = 112
        Width = 602
        Height = 22
        Anchors = [akLeft, akTop, akRight]
        TabOrder = 2
      end
      object cbxValueType: TComboBox
        Left = 98
        Top = 156
        Width = 179
        Height = 22
        Style = csDropDownList
        TabOrder = 3
        OnChange = DataChanged
      end
      object chbBaseUnit: TCheckBox
        Left = 298
        Top = 159
        Width = 82
        Height = 21
        Caption = 'B&ase unit'
        TabOrder = 4
        OnClick = chbBaseUnitClick
      end
      object cbxFormat: TComboBox
        Left = 519
        Top = 156
        Width = 181
        Height = 22
        Style = csDropDownList
        Anchors = [akTop, akRight]
        TabOrder = 5
        Items.Strings = (
          'polygon')
      end
      object mmExpression: TMemo
        Left = 98
        Top = 250
        Width = 602
        Height = 109
        Anchors = [akLeft, akTop, akRight]
        TabOrder = 11
      end
      object rbNumberItems: TRadioButton
        Left = 98
        Top = 207
        Width = 130
        Height = 21
        Caption = 'N&umber elements:'
        Checked = True
        TabOrder = 6
        TabStop = True
        OnClick = NumberRangeClick
      end
      object rbRange: TRadioButton
        Left = 388
        Top = 207
        Width = 67
        Height = 21
        Anchors = [akTop, akRight]
        Caption = '&Range:'
        TabOrder = 8
        OnClick = NumberRangeClick
      end
      object seNumberItems: TSpinEdit
        Left = 231
        Top = 206
        Width = 100
        Height = 23
        MaxValue = 0
        MinValue = 0
        TabOrder = 7
        Value = 0
      end
      object edFrom: TEdit
        Left = 463
        Top = 206
        Width = 101
        Height = 22
        Anchors = [akTop, akRight]
        Enabled = False
        TabOrder = 9
      end
      object edTo: TEdit
        Left = 599
        Top = 206
        Width = 101
        Height = 22
        Anchors = [akTop, akRight]
        Enabled = False
        TabOrder = 10
      end
    end
  end
  object btnBack: TButton
    Left = 384
    Top = 561
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '< &Back'
    Enabled = False
    TabOrder = 1
    OnClick = btnBackClick
  end
end
