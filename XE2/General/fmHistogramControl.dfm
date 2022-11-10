inherited fraHistogramControl: TfraHistogramControl
  Width = 451
  Height = 304
  Align = alClient
  Font.Charset = ANSI_CHARSET
  Font.Height = -12
  ParentFont = False
  ExplicitWidth = 435
  ExplicitHeight = 266
  inherited pnlToolbar: TPanel
    Left = 48
    Top = 232
    ExplicitLeft = 48
    ExplicitTop = 232
  end
  object chartDistribution: TChart
    Left = 0
    Top = 0
    Width = 435
    Height = 266
    BackWall.Brush.Color = clWhite
    BackWall.Brush.Style = bsClear
    Legend.Visible = False
    Title.Font.Charset = ANSI_CHARSET
    Title.Font.Color = clBlack
    Title.Font.Height = -15
    Title.Font.Name = 'Tahoma'
    Title.Text.Strings = (
      'Title')
    BottomAxis.Title.Caption = 'title x axis'
    BottomAxis.Title.Font.Charset = ANSI_CHARSET
    BottomAxis.Title.Font.Height = -12
    BottomAxis.Title.Font.Name = 'Tahoma'
    DepthAxis.Title.Font.Charset = ANSI_CHARSET
    DepthAxis.Title.Font.Height = -12
    DepthAxis.Title.Font.Name = 'Tahoma'
    LeftAxis.Title.Caption = 'Title y axis'
    LeftAxis.Title.Font.Charset = ANSI_CHARSET
    LeftAxis.Title.Font.Height = -12
    LeftAxis.Title.Font.Name = 'Tahoma'
    RightAxis.Title.Font.Charset = ANSI_CHARSET
    RightAxis.Title.Font.Height = -12
    RightAxis.Title.Font.Name = 'Tahoma'
    TopAxis.Title.Font.Charset = ANSI_CHARSET
    TopAxis.Title.Font.Height = -12
    TopAxis.Title.Font.Name = 'Tahoma'
    View3D = False
    Align = alClient
    TabOrder = 1
    ColorPaletteIndex = 13
  end
end
