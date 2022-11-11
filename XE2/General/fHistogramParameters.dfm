object frmHistogramParameters: TfrmHistogramParameters
  Left = 27
  Top = 161
  Caption = 'Histogram parameters'
  ClientHeight = 218
  ClientWidth = 435
  Color = clBtnFace
  Constraints.MaxHeight = 257
  Constraints.MinWidth = 435
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = True
  Scaled = False
  OnActivate = FormActivate
  DesignSize = (
    435
    218)
  PixelsPerInch = 96
  TextHeight = 13
  object stnMain: TStatusBar
    Left = 0
    Top = 179
    Width = 435
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
    Width = 556
    Height = 174
    Anchors = [akLeft, akTop, akRight]
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -12
    Font.Name = 'Tahoma'
    Font.Style = []
    ParentFont = False
    TabOrder = 1
    DesignSize = (
      556
      174)
    object lblFilter: TLabel
      Left = 20
      Top = 75
      Width = 30
      Height = 14
      Caption = 'Filter:'
    end
    object Label3: TLabel
      Left = 20
      Top = 28
      Width = 63
      Height = 14
      Caption = 'Classificatie:'
    end
    object lblSelection: TLabel
      Left = 20
      Top = 126
      Width = 47
      Height = 14
      Caption = 'Selectie:'
    end
    object cbxFilter: TComboBox
      Left = 88
      Top = 70
      Width = 321
      Height = 22
      Style = csDropDownList
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 0
    end
    object cbxClassification: TComboBox
      Left = 88
      Top = 23
      Width = 321
      Height = 22
      Style = csDropDownList
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 1
    end
    object cbxSelection: TComboBox
      Left = 88
      Top = 121
      Width = 321
      Height = 22
      Style = csDropDownList
      Anchors = [akLeft, akTop, akRight]
      TabOrder = 2
    end
  end
  object btnOK: TButton
    Left = 250
    Top = 186
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Ok'
    Default = True
    ModalResult = 1
    TabOrder = 0
  end
  object btnCancel: TButton
    Left = 335
    Top = 186
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Cancel = True
    Caption = '&Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object dlgSave: TSaveDialog
    DefaultExt = 'bmp'
    Options = [ofHideReadOnly, ofNoChangeDir, ofEnableSizing]
    Left = 224
    Top = 144
  end
end
