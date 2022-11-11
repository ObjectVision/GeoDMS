object frmErrorMsg: TfrmErrorMsg
  Left = 231
  Top = 113
  HorzScrollBar.Visible = False
  VertScrollBar.Visible = False
  ActiveControl = btnOk
  BorderIcons = []
  Caption = 'Error'
  ClientHeight = 293
  ClientWidth = 531
  Color = clBtnFace
  Constraints.MinHeight = 144
  Constraints.MinWidth = 508
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  KeyPreview = True
  OldCreateOrder = False
  Scaled = False
  OnClose = FormClose
  OnCreate = FormCreate
  OnKeyUp = FormKeyUp
  OnShow = FormShow
  DesignSize = (
    531
    293)
  PixelsPerInch = 96
  TextHeight = 13
  object stnMain: TStatusBar
    Left = 0
    Top = 254
    Width = 531
    Height = 39
    Panels = <
      item
        Bevel = pbNone
        Width = 50
      end>
  end
  object btnOk: TButton
    Left = 91
    Top = 261
    Width = 80
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&OK'
    Default = True
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -12
    Font.Name = 'Tahoma'
    Font.Style = []
    ModalResult = 1
    ParentFont = False
    TabOrder = 0
  end
  object btnIgnore: TButton
    Left = 176
    Top = 261
    Width = 80
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Ignore'
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -12
    Font.Name = 'Tahoma'
    Font.Style = []
    ModalResult = 5
    ParentFont = False
    TabOrder = 1
  end
  object btnMailMsg: TButton
    Left = 346
    Top = 261
    Width = 80
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Mail'
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -12
    Font.Name = 'Tahoma'
    Font.Style = []
    ParentFont = False
    TabOrder = 4
    OnClick = btnMailMsgClick
  end
  object mmLongMsg: TMemo
    Left = 0
    Top = 0
    Width = 539
    Height = 253
    Anchors = [akLeft, akTop, akRight, akBottom]
    Color = clBtnFace
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -12
    Font.Name = 'Courier New'
    Font.Style = []
    Lines.Strings = (
      'm_longMsg')
    ParentFont = False
    ReadOnly = True
    TabOrder = 5
    OnDblClick = mmLongMsgDblClick
  end
  object btnTerminate: TButton
    Left = 261
    Top = 261
    Width = 80
    Height = 25
    Hint = 'Try to end the '
    Anchors = [akRight, akBottom]
    Caption = '&Terminate'
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -12
    Font.Name = 'Tahoma'
    Font.Style = []
    ParentFont = False
    TabOrder = 2
    OnClick = btnTerminateClick
  end
  object btnReopen: TButton
    Left = 431
    Top = 261
    Width = 80
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = '&Reopen'
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -12
    Font.Name = 'Tahoma'
    Font.Style = []
    ParentFont = False
    TabOrder = 3
    OnClick = btnReopenClick
  end
end
