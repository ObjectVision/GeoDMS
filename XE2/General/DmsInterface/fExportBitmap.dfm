object frmExportBitmap: TfrmExportBitmap
  Left = 17
  Top = 127
  Caption = 'Export Bitmap'
  ClientHeight = 218
  ClientWidth = 497
  Color = clBtnFace
  Constraints.MaxHeight = 257
  Constraints.MinHeight = 251
  Constraints.MinWidth = 435
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = True
  Scaled = False
  OnActivate = FormActivate
  DesignSize = (
    497
    218)
  PixelsPerInch = 96
  TextHeight = 14
  object stnMain: TStatusBar
    Left = 0
    Top = 179
    Width = 497
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
    Width = 503
    Height = 175
    Anchors = [akLeft, akTop, akRight]
    TabOrder = 1
    DesignSize = (
      503
      175)
    object Label1: TLabel
      Left = 16
      Top = 16
      Width = 93
      Height = 14
      Caption = '&Storage location:'
      FocusControl = edFileName
    end
    object Label2: TLabel
      Left = 16
      Top = 73
      Width = 54
      Height = 14
      Caption = '&Relate to:'
      FocusControl = cbxBasisGrid
    end
    object Label3: TLabel
      Left = 16
      Top = 111
      Width = 70
      Height = 14
      Caption = '&Classification:'
      FocusControl = cbxClassification
    end
    object Label4: TLabel
      Left = 16
      Top = 148
      Width = 43
      Height = 14
      Caption = '&Palette:'
      FocusControl = cbxPalette
    end
    object edFileName: TEdit
      Left = 16
      Top = 32
      Width = 425
      Height = 22
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 0
    end
    object cbxBasisGrid: TComboBox
      Left = 104
      Top = 69
      Width = 369
      Height = 22
      Style = csDropDownList
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 1
    end
    object cbxClassification: TComboBox
      Left = 103
      Top = 107
      Width = 370
      Height = 22
      Style = csDropDownList
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 2
      OnChange = cbxClassificationChange
    end
    object cbxPalette: TComboBox
      Left = 103
      Top = 144
      Width = 369
      Height = 22
      Style = csDropDownList
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 3
    end
  end
  object btnBrowse: TButton
    Left = 451
    Top = 34
    Width = 21
    Height = 21
    Anchors = [akTop, akRight]
    Caption = '..'
    TabOrder = 0
    OnClick = btnBrowseClick
  end
  object btnOK: TButton
    Left = 227
    Top = 186
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Ok'
    Default = True
    TabOrder = 2
    OnClick = btnOKClick
  end
  object btnCancel: TButton
    Left = 312
    Top = 186
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Cancel = True
    Caption = '&Cancel'
    ModalResult = 2
    TabOrder = 3
  end
  object btnHelp: TButton
    Left = 397
    Top = 186
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Help'
    Enabled = False
    TabOrder = 4
  end
  object dlgSave: TSaveDialog
    DefaultExt = 'bmp'
    Filter = 'Bitmaps|*.bmp; *.BMP'
    Options = [ofHideReadOnly, ofNoChangeDir, ofEnableSizing]
    Left = 8
    Top = 200
  end
end
