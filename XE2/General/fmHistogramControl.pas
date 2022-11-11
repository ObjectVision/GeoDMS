//<HEADER>
(*
GeoDmsGui.exe
Copyright (C) Object Vision BV.
http://www.objectvision.nl/geodms/

This is open source software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2
(the License) as published by the Free Software Foundation.

Have a look at our web-site for licensing conditions:
http://www.objectvision.nl/geodms/software/license-and-disclaimer

You can use, redistribute and/or modify this library source code file
provided that this entire header notice is preserved.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*)
//</HEADER>

unit fmHistogramControl;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, OleCtrls, ExtCtrls, Buttons,
  fmBasePrimaryDataControl,
  Configuration,
  uDMSInterfaceTypes, TeeProcs, TeEngine, Chart, uGenLib, uDmsUtils;

type
   THistogramParameters = record
      tiClassification,
      tiFilter,
      tiSelection       : TTreeItem;
   end;

   TfraHistogramControl = class(TfraBasePrimaryDataControl)
      chartDistribution: TChart;
   public
      class function GetHistogramViewEnabled(const ti: TTreeItem): boolean;
      class function AllInfoPresent(const tiSource: TTreeItem; ct: TTreeItemViewStyle): boolean; override;
//      class function AssertControlData: Boolean;  override;

      constructor CreateNew(AOwner: TComponent; tiView: TTreeItem; ct: TTreeItemViewStyle; sm: TShvSyncMode); override;
      destructor  Destroy; override;

      procedure   SetVisible(pKey: Pointer; bVisible: Boolean); override;
      procedure   Remove(pKey: Pointer); override;
      procedure   Move(pKeySrc, pKeyDst: Pointer; bBefore: Boolean); override;
      function    InitFromDesktop(tiView: TTreeItem): Boolean; override;
      function    LoadTreeItem(tiSource: TTreeItem; loadContext: PLoadContext): Boolean; override;
      function    IsLoadable(tiSource: TTreeItem): Boolean; override;
//      procedure   Update; override;
  end;

implementation

{$R *.DFM}

uses
   Math,
   Series,
   uDmsInterface,
   fHistogramParameters
;

{******************************************************************************}
//                        TThemaInfo
{******************************************************************************}
type
 TThemaInfo = class
 private
    tiDataItem,
    tiHistogramCount     :  TTreeItem;
    HistogramParameters  :  THistogramParameters;
 public
    constructor Create(const tid: TTreeItem; const hp: THistogramParameters);

    function    HistogramCountItemCreate(const tiParent: TTreeItem): TTreeItem;
    procedure   HistogramCountChartTitles(var main, left, bottom: string);
    function    HistogramCountItemRetrieve(ds: TChartSeries): Boolean;
 end;

constructor TThemaInfo.Create(const tid: TTreeItem; const hp: THistogramParameters);
begin
   inherited   Create;
   tiDataItem           := tid;
   HistogramParameters  := hp;
end;

function    TThemaInfo.HistogramCountItemCreate(const tiParent: TTreeItem): TTreeItem;
var
   calcRule       :  ItemString;
   domain_unit,
   classid_unit   :  TAbstrUnit;
begin
   calcrule := TreeItem_GetFullName_Save(tiDataItem);

   with HistogramParameters do
   begin
      if Assigned(tiClassification) then
      begin
         calcrule       := 'classify(' + calcrule + ',' + TreeItem_GetFullName_Save(tiClassification) + ')';
         classid_unit   := DMS_DataItem_GetDomainUnit(tiClassification);
      end else
         classid_unit   := DMS_DataItem_GetValuesUnit(tiDataItem);
   end;

   domain_unit := DMS_GetDefaultUnit(DMS_UnitClass_Find(DMS_ValueType_GetCrdClass(DMS_Unit_GetValueType(DMS_DataItem_GetDomainUnit(tiDataItem)))));
   Result      := DMS_CreateDataItem(tiParent, 'HistogramCount', classid_unit, domain_unit, VC_Single);

   DMS_TreeItem_SetExpr(Result, PItemChar('pcount(' + calcrule + ')'));
end;

procedure   TThemaInfo.HistogramCountChartTitles(var main, left, bottom: string);
begin
   main     := TreeItemDescrOrName(tiDataItem);
//   with HistogramParameters do
   bottom   := DMS_TreeItem_GetName(DMS_DataItem_GetDomainUnit(tiHistogramCount)); // + ' -> ' + DMS_TreeItem_GetName(DMS_DataItem_GetValuesUnit(tiHistogramCount));
   left     := 'Count[' + DMS_TreeItem_GetName(DMS_DataItem_GetDomainUnit(tiDataItem)) + ']';
end; // HistogramCountChartTitles

// count := DMS_Unit_GetCount(DMS_DataItem_GetDomainUnit(tiHistogramCount));
function    TThemaInfo.HistogramCountItemRetrieve(ds :  TChartSeries): Boolean;
var
   float64buffer  :  TFloat64Array;
   position       :  SizeT;
   readcnt,
   i              :  Cardinal;
   labelTxt       :  SourceString;
   labelAttr      :  TAbstrDataItem;
begin
   Result   := Assigned(tiHistogramCount);

   if not Result then Exit;

   DMS_TreeItem_IncInterestCount(tiHistogramCount); try
   labelAttr := DMS_Unit_GetLabelAttr(DMS_DataItem_GetDomainUnit(tiHistogramCount));
   if Assigned(labelAttr) then DMS_TreeItem_IncInterestCount(labelAttr); try
   with ds do
      Clear;

      position := 0;
      labelTxt := EMPTYSTRING;
      while true do
      begin
         readcnt := NumericDenseDataItem_GetFloat64Data(tiHistogramCount, float64Buffer, position, FLOAT64_BLOCK_SIZE);

         if readcnt = 0 then
            Break;

         for i := 0 to readcnt - 1 do
         begin
            if Assigned(labelAttr) then
            begin
               if float64buffer[i] <> 0 then
                 labelTxt := AnyDataItem_GetValueAsString(labelAttr, position + i)
               else
                 Str(i, labelTxt);
            end;

            ds.AddXY(position + i, float64buffer[i], labelTxt, clBlue);
         end;

         Inc(position, readcnt);
   end;
   finally if Assigned(labelAttr) then DMS_TreeItem_DecInterestCount(labelAttr); end
   finally DMS_TreeItem_DecInterestCount(tiHistogramCount); end;
end; // HistogramCountItemRetrieve

{******************************************************************************}
//                        TfraHistogramControl
{******************************************************************************}

class function TfraHistogramControl.AllInfoPresent(const tiSource: TTreeItem; ct: TTreeItemViewStyle): boolean;
begin
   Result := IsDataItem(tiSource) and (not IsParameter(tiSource))
        and (IsDomainUnit(DMS_DataItem_GetValuesUnit(tiSource)) or CanBeDisplayed(tiSource));
end;

class function TfraHistogramControl.GetHistogramViewEnabled(const ti: TTreeItem): boolean;
var vt: TValueTypeID;
begin
   Result := IsDataItem(ti) and not IsParameter(ti);
   if not Result then exit;
   vt := GetValuesUnitValueTypeID(ti);
   Result := ValueTypeId_IsNumeric(vt) or ValueTypeId_IsBoolean(vt);
end;

constructor TfraHistogramControl.CreateNew(AOwner: TComponent; tiView: TTreeItem; ct: TTreeItemViewStyle; sm: TShvSyncMode);
begin
  inherited _Create(AOwner, tiView);
end;

destructor TfraHistogramControl.Destroy;
begin
   inherited;
end;

procedure   TfraHistogramControl.SetVisible(pKey: Pointer; bVisible: Boolean);
begin
end;

procedure   TfraHistogramControl.Remove(pKey: Pointer);
begin
end;

procedure   TfraHistogramControl.Move(pKeySrc, pKeyDst: Pointer; bBefore: Boolean);
begin
end;

function    TfraHistogramControl.InitFromDesktop(tiView: TTreeItem): Boolean;
begin
   Result   := true;
end;

function    TfraHistogramControl.LoadTreeItem(tiSource: TTreeItem; loadContext: PLoadContext): Boolean;

   procedure   DistributionSeriesShow(const cs: TChartSeries);
   var   i  :  Integer;
   begin
      with TBarSeries.Create(Self) do
      begin

         for i := 0 to cs.XValues.Count - 1 do
            AddXY(cs.XValues[i], cs.YValues[i], cs.XLabel[i], clBlue);
      end;
   end;

var
   hp       :  THistogramParameters;
   bs       :  TBarSeries;
   mtitle,
   ltitle,
   btitle   :  string;
begin
   Result   := TfrmHistogramParameters.ShowForm(tiSource, hp);

   if not Result then
      Exit;

   bs := TBarSeries.Create(Self);
   bs.ParentChart      := chartDistribution;
   bs.Marks.Visible    := false;

   with TThemaInfo.Create(tiSource, hp) do
   try
      tiHistogramCount := HistogramCountItemCreate(m_tiView);
      HistogramCountChartTitles(mtitle, ltitle, btitle);
      with chartDistribution do
      begin
         Title.Text.Text            := mtitle;
         LeftAxis.Title.Caption     := ltitle;
         BottomAxis.Title.Caption   := btitle;
      end;
      HistogramCountItemRetrieve(bs);
   finally
      Free;
   end;

   FrameCaption   := 'HistogramView';
end;

function    TfraHistogramControl.IsLoadable(tiSource: TTreeItem): Boolean;
begin
   Result   := true;
end;


end.



