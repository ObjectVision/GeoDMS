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

unit uTreeViewUtilities;
(*
 * Contents: DMS specific Serializer for TreeView
 *
 * Author  : Mathieu van Loon
 *
 * Notes:
 * See http://www.delphi-gems.com/VirtualTreeview/VT.php for extensive information
 * on the VirtualTree component
 *
 * History
 * MTA 30-08-2007 support for TVirtualStringTree removed
 *)


interface

uses
   ComCtrls,
   uDMSInterface;

type

   TTreeViewSerializer = class
   private
      FTree : TTreeView;
      class function  LocateSibling(tn : TTreeNode; name : String): TTreeNode;
   public
      constructor Create(const tree : TTreeView);
      function  SaveExpandedState: String;
      procedure LoadExpandedState(const code: String);

      function  SaveFocusedNode: String;
      procedure LoadFocusedNode(const path: String);

      class procedure ActivateFocusedNode(const path: String; tnFirstChild: TTreeNode);
   end;


implementation

uses ContNrs, sysUtils;

const SEP = ';';

function NextChar(str: PChar): PChar; forward;
function NextSeparator(str: PChar): PChar; forward;
function CopyBetween(S, E: PChar) : String; forward;


//
// TTreeViewSerializer
//

constructor TTreeViewSerializer.Create(const tree : TTreeView);
begin
   FTree := tree;
end;


function NodeText(tn : TTreeNode): String;
begin
  if ASsigned(tn.Data)
    then Result := DMS_TreeItem_GetName(tn.Data)
    else Result := tn.Text;
end;


(*
 * Return a string which represents tree's expanded nodes
 *)
function  TTreeViewSerializer.SaveExpandedState: String;
   function RecurseNode(tn : TTreeNode): String;
   begin
      Result := '';
      while Assigned(tn) do
      begin
         if tn.Expanded then
            Result := Result + NodeText(tn) + SEP + RecurseNode(tn.GetFirstChild) + SEP;

         tn := tn.GetNextSibling;
      end;
   end;

begin
   Result := RecurseNode(FTree.Items[0]);
end;

(*
 * Open those nodes, which are stored in code.
 * The code parameter should be produced by SaveExpandedState
 *)
procedure TTreeViewSerializer.LoadExpandedState(const code: String);

   function RecurseNode(tn: TTreeNode; PStart : PChar): PChar;
   var
      PEnd : PChar;
      node : TTreeNode;
   begin
      while true do
      begin
         Result := NextSeparator(PStart);
         if not Assigned(Result) then exit;

         PEnd := Result;
         Result := NextChar(Result);
         if PStart = PEnd then exit; // the sequence ;; means we must ascend one level

         node := LocateSibling(tn, CopyBetween(PStart, PEnd));

         if Assigned(node) then
         begin
            node.Expanded := True;
            PStart := RecurseNode(node.GetFirstChild, Result);
         end
         else
         begin
            // we can't find the correct node, but we need to keep recursing
            // to chop the missing node's children from the string before
            // continuing
            PStart := RecurseNode(nil, Result);
         end;
      end;
   end;

begin
   FTree.FullCollapse;

   RecurseNode(FTree.Items[0], PChar(code));
end;


(*
 * Return a string representing the currently focused node
 * @see LoadFocusedNode
 *)
function  TTreeViewSerializer.SaveFocusedNode: String;
var tn : TTreeNode;
begin
   tn := FTree.Selected;
   Result := '';

   while Assigned(tn) do
   begin
      Result := NodeText(tn) + SEP + Result;
      tn     := tn.Parent;
   end;
end;

(*
 * Set the tree's focused node to the node represented by path
 * @see SaveFocusedNode
 * even when the full path doesn't exist, try to select an item most close to the given path.
 *)

class procedure TTreeViewSerializer.ActivateFocusedNode(const path: String; tnFirstChild: TTreeNode);
var
   PSeparator, PStart : PChar;
   tn           : TTreeNode;
begin
   PStart  := PChar(path);
   tn      := nil;

   PSeparator := NextSeparator(PStart);

   while Assigned(PSeparator) and (PSeparator <> PStart) do
   begin
      tnFirstChild := LocateSibling(tnFirstChild, CopyBetween(PStart, PSeparator));
      if not Assigned(tnFirstChild) then break;
      tn           := tnFirstChild;
      tnFirstChild := tn.GetFirstChild;

      PStart      := NextChar(PSeparator);
      PSeparator  := NextSeparator(PStart);
   end;

   if Assigned(tn) then
      tn.Selected := True;
end;

procedure TTreeViewSerializer.LoadFocusedNode(const path: String);
begin
   ActivateFocusedNode(path, FTree.Items[0]);
end;


(*
 * Find vn's sibling which has the caption name.
 * Returns nil if vn is nil
 *)

class function  TTreeViewSerializer.LocateSibling(tn : TTreeNode; name : String): TTreeNode;
begin
   Result := tn;
   name := Uppercase(name);

   while Assigned(Result) do
   begin
      if Uppercase(NodeText(Result)) = name then
         Exit;

      Result := Result.GetNextSibling;
   end;

   // if we arrive here there is no child with the given name
   // we return nil
end;



//
// Helper functions
//

// return a pointer to the second character in the given string
function NextChar(str: PChar): PChar;
begin
   Result := str+1;
end;

// Return a pointer to the next occurence of SEP in the given string
function NextSeparator(str: PChar): PChar;
begin
   if Assigned(str)
     then Result := StrScan(str, SEP)
     else Result := Nil;
end;


// S(tart) and E(nd) are both pointers into the same string.
// Result will be the string contained between the two pointers
function CopyBetween(S, E: PChar) : String;
begin
   Result := '';
   while (S <> E) do
   begin
      Result := Result + S[0];
      INC(S);
   end;
end;


end.
