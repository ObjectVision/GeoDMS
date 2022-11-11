object frmExportASCGrid: TfrmExportASCGrid
  Left = 573
  Top = 221
  Caption = 'Export Ascii Grid'
  ClientHeight = 330
  ClientWidth = 524
  Color = clBtnFace
  Constraints.MinHeight = 363
  Constraints.MinWidth = 498
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = True
  Position = poScreenCenter
  Scaled = False
  OnActivate = FormActivate
  DesignSize = (
    524
    330)
  PixelsPerInch = 96
  TextHeight = 14
  object stnMain: TStatusBar
    Left = 0
    Top = 291
    Width = 524
    Height = 39
    Panels = <
      item
        Bevel = pbNone
        Width = 50
      end>
  end
  object Panel1: TPanel
    Left = 1
    Top = 2
    Width = 537
    Height = 290
    Anchors = [akLeft, akTop, akRight, akBottom]
    TabOrder = 0
    DesignSize = (
      537
      290)
    object Label1: TLabel
      Left = 16
      Top = 225
      Width = 93
      Height = 14
      Anchors = [akLeft, akBottom]
      Caption = '&Storage location:'
      FocusControl = edtFileName
    end
    object lblBaseGrid: TLabel
      Left = 16
      Top = 185
      Width = 50
      Height = 14
      Anchors = [akLeft, akBottom]
      Caption = '&BaseGrid:'
      FocusControl = cobBaseGrid
    end
    object lblSubTitle: TLabel
      Left = 16
      Top = 8
      Width = 251
      Height = 14
      Anchors = [akLeft, akTop, akRight]
      Caption = 'Container/DataItem: /Cases/EcMinMax/Results'
    end
    object Label5: TLabel
      Left = 24
      Top = 80
      Width = 4
      Height = 14
    end
    object lblDomain: TLabel
      Left = 16
      Top = 56
      Width = 44
      Height = 14
      Caption = '&Domain:'
    end
    object lblExportBoth: TLabel
      Left = 128
      Top = 161
      Width = 73
      Height = 14
      Anchors = [akLeft, akBottom]
      Caption = 'lblExportBoth'
    end
    object edtFileName: TEdit
      Left = 112
      Top = 225
      Width = 361
      Height = 22
      Anchors = [akLeft, akRight, akBottom]
      TabOrder = 5
      OnChange = edtFileNameChange
    end
    object cobBaseGrid: TComboBox
      Left = 112
      Top = 185
      Width = 385
      Height = 22
      Style = csDropDownList
      Anchors = [akLeft, akRight, akBottom]
      TabOrder = 3
    end
    object cbxUseBaseGrid: TCheckBox
      Left = 112
      Top = 32
      Width = 393
      Height = 17
      Caption = '&Use BaseGrid to export VAT DataItem'
      TabOrder = 0
      OnClick = cbxUseBaseGridClick
    end
    object lbxDomain: TListBox
      Left = 112
      Top = 56
      Width = 385
      Height = 82
      Anchors = [akLeft, akTop, akRight, akBottom]
      ItemHeight = 14
      TabOrder = 1
      OnClick = lbxDomainClick
    end
    object cbxExportBoth: TCheckBox
      Left = 112
      Top = 145
      Width = 395
      Height = 17
      Anchors = [akLeft, akRight, akBottom]
      Caption = 
        '&Export also source data (check) or calculated data only (unchec' +
        'k)'
      TabOrder = 2
      OnClick = cbxExportBothClick
    end
    object cbxProcessIdle: TCheckBox
      Left = 112
      Top = 257
      Width = 361
      Height = 17
      Anchors = [akLeft, akBottom]
      Caption = 'Do e&xport as background process (in Idle time)'
      TabOrder = 6
    end
    object btnBrowse: TButton
      Left = 477
      Top = 227
      Width = 20
      Height = 21
      Anchors = [akRight, akBottom]
      Caption = '..'
      TabOrder = 4
      OnClick = btnBrowseClick
    end
  end
  object btnOK: TButton
    Left = 254
    Top = 298
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Ok'
    TabOrder = 1
    OnClick = btnOKClick
  end
  object btnCancel: TButton
    Left = 339
    Top = 298
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Cancel = True
    Caption = '&Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object btnHelp: TButton
    Left = 424
    Top = 298
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Help'
    Enabled = False
    TabOrder = 4
  end
  object btnShowExports: TButton
    Left = 112
    Top = 298
    Width = 121
    Height = 25
    Anchors = [akLeft, akRight, akBottom]
    Caption = 'S&how x exports'
    TabOrder = 3
    OnClick = btnShowExportsClick
  end
  object dlgSave: TSaveDialog
    DefaultExt = 'asc'
    Filter = 'Ascii grids|*.asc; *.ASC'
    Options = [ofOverwritePrompt, ofHideReadOnly, ofNoChangeDir, ofEnableSizing]
    Left = 16
    Top = 304
  end
end
