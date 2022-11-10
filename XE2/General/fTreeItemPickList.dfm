object frmTreeItemPickList: TfrmTreeItemPickList
  Left = 19
  Top = 133
  Caption = 'Select'
  ClientHeight = 585
  ClientWidth = 858
  Color = clBtnFace
  Constraints.MinHeight = 387
  Constraints.MinWidth = 275
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'Tahoma'
  Font.Style = []
  OldCreateOrder = False
  Position = poScreenCenter
  Scaled = False
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  DesignSize = (
    858
    585)
  PixelsPerInch = 96
  TextHeight = 13
  object StatusBar1: TStatusBar
    Left = 0
    Top = 546
    Width = 858
    Height = 39
    Panels = <
      item
        Bevel = pbNone
        Width = 50
      end>
  end
  object btnNew: TButton
    Left = 8
    Top = 553
    Width = 75
    Height = 25
    Anchors = [akLeft, akBottom]
    Caption = 'New'
    TabOrder = 0
    Visible = False
    OnClick = btnNewClick
  end
  object btnOK: TButton
    Left = 673
    Top = 553
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = 'OK'
    Default = True
    Enabled = False
    TabOrder = 1
    OnClick = btnOKClick
  end
  object btnCancel: TButton
    Left = 758
    Top = 553
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
  end
  object pnlItemList: TPanel
    Left = 0
    Top = 0
    Width = 866
    Height = 545
    Anchors = [akLeft, akTop, akRight, akBottom]
    Caption = 'pnlItemList'
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -12
    Font.Name = 'Tahoma'
    Font.Style = []
    ParentFont = False
    TabOrder = 3
  end
end
