object frmEditProperty: TfrmEditProperty
  Left = 40
  Top = 166
  Anchors = [akLeft, akTop, akRight]
  BorderStyle = bsDialog
  Caption = 'frmEditProperty'
  ClientHeight = 74
  ClientWidth = 395
  Color = clBtnFace
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  Scaled = False
  ExplicitWidth = 320
  PixelsPerInch = 96
  TextHeight = 14
  object edPropertyValue: TEdit
    Left = 10
    Top = 10
    Width = 369
    Height = 22
    TabOrder = 0
    OnKeyUp = edPropertyValueKeyUp
  end
  object btnOk: TButton
    Left = 226
    Top = 43
    Width = 75
    Height = 23
    Caption = '&Ok'
    ModalResult = 1
    TabOrder = 1
  end
  object btnCancel: TButton
    Left = 304
    Top = 43
    Width = 75
    Height = 23
    Caption = '&Cancel'
    ModalResult = 2
    TabOrder = 2
  end
end
