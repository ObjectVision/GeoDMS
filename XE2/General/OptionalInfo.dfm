object frmOptionalInfo: TfrmOptionalInfo
  Left = 165
  Top = 230
  Caption = 'Information'
  ClientHeight = 114
  ClientWidth = 550
  Color = clBtnFace
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  Scaled = False
  DesignSize = (
    550
    114)
  PixelsPerInch = 96
  TextHeight = 14
  object stnMain: TStatusBar
    Left = 0
    Top = 75
    Width = 550
    Height = 39
    Panels = <
      item
        Bevel = pbNone
        Width = 50
      end>
    ExplicitTop = 367
    ExplicitWidth = 551
  end
  object cbxHide: TCheckBox
    Left = 8
    Top = 86
    Width = 344
    Height = 17
    Anchors = [akLeft, akRight, akBottom]
    Caption = 'Do not show this information in the future'
    TabOrder = 0
    ExplicitTop = 378
    ExplicitWidth = 345
  end
  object btnOK: TButton
    Left = 366
    Top = 82
    Width = 75
    Height = 25
    Anchors = [akLeft, akBottom]
    Caption = '&Ok'
    Default = True
    ModalResult = 1
    TabOrder = 1
    ExplicitTop = 374
  end
  object Panel1: TPanel
    Left = 0
    Top = 0
    Width = 550
    Height = 74
    Align = alTop
    Anchors = [akLeft, akTop, akRight, akBottom]
    Caption = 'Panel1'
    TabOrder = 2
    ExplicitWidth = 551
    ExplicitHeight = 366
    object Label1: TLabel
      Left = 1
      Top = 1
      Width = 460
      Height = 29
      Align = alClient
      Alignment = taCenter
      Caption = 'Information is shown in a browser window.'
      Font.Charset = ANSI_CHARSET
      Font.Color = clWindowText
      Font.Height = -24
      Font.Name = 'Tahoma'
      Font.Style = []
      ParentFont = False
    end
  end
  object btnCancel: TButton
    Left = 451
    Top = 82
    Width = 75
    Height = 25
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = '&Cancel'
    Default = True
    ModalResult = 2
    TabOrder = 3
    ExplicitTop = 374
  end
end
