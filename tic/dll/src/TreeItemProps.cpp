//<HEADER> 
/*
Data & Model Server (DMS) is a server written in C++ for DSS applications. 
Version: see srv/dms/rtc/dll/src/RtcVersion.h for version info.

Copyright (C) 1998-2004  YUSE GSO Object Vision BV. 

Documentation on using the Data & Model Server software can be found at:
http://www.ObjectVision.nl/DMS/

See additional guidelines and notes in srv/dms/Readme-srv.txt 

This library is free software; you can use, redistribute, and/or
modify it under the terms of the GNU General Public License version 2 
(the License) as published by the Free Software Foundation,
provided that this entire header notice and readme-srv.txt is preserved.

See LICENSE.TXT for terms of distribution or look at our web site:
http://www.objectvision.nl/DMS/License.txt
or alternatively at: http://www.gnu.org/copyleft/gpl.html

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
General Public License for more details. However, specific warranties might be
granted by an additional written contract for support, assistance and/or development
*/
//</HEADER>
#include "TicPCH.h"
#pragma hdrstop

#include "TreeItemProps.h"

#include "dbg/SeverityType.h"

#include "TicPropDefConst.h"
#include "TreeItemClass.h"
#include "AbstrDataItem.h"
#include "stg/AbstrStorageManager.h"
#include "AbstrUnit.h"
#include "AbstrCalculator.h"
#include "CopyTreeContext.h"
#include "SupplCache.h"
#include "StoredPropDef.h"

//----------------------------------------------------------------------
// Generic property functions
//----------------------------------------------------------------------

SharedStr TreeItemPropertyValue(const TreeItem* ti, const AbstrPropDef* pd)
{
	dms_assert(pd);
	while (ti)
	{
		SharedStr result = pd->GetValueAsSharedStr(ti);
		if (pd->CanBeIndirect() && AbstrCalculator::MustEvaluate( result.c_str() ) )
			result =  AbstrCalculator::EvaluatePossibleStringExpr(ti, result, CalcRole::Other);
		if (!result.empty())
			return result;

		MG_DEBUGCODE( const TreeItem* tiOrg = ti; )
		ti = ti->GetSourceItem();
		MG_DEBUGCODE( dms_assert(ti != tiOrg); )
	}
	return SharedStr();
}

bool TreeItemHasPropertyValue(const TreeItem* ti, const AbstrPropDef* pd)
{
	dms_assert(pd);
	while (ti)
	{
		bool result = pd->HasNonDefaultValue(ti);
		if (result)
			return true;
		MG_DEBUGCODE( const TreeItem* tiOrg = ti; )
		ti = ti->GetSourceItem();
		MG_DEBUGCODE( dms_assert(ti != tiOrg); )
	}
	return false;
}

//----------------------------------------------------------------------
// Specific property functions
//----------------------------------------------------------------------

#include "PropFuncs.h"

TokenID TreeItem_GetDialogType(const TreeItem* self) { 
	dms_assert(self);

	if (dialogTypePropDefPtr->HasNonDefaultValue(self))
		return dialogTypePropDefPtr->GetValue(self);

	return TokenID::GetEmptyID();
}

void      TreeItem_SetDialogType(TreeItem* self, TokenID dialogType)
{
	dialogTypePropDefPtr->SetValue(self, dialogType);
}

SharedStr TreeItem_GetDialogData(const TreeItem* item)
{
	return TreeItemPropertyValue(item, dialogDataPropDefPtr);
}

void TreeItem_SetDialogData(TreeItem* item, CharPtrRange dialogData)
{
	dialogDataPropDefPtr->SetValue(item, SharedStr(dialogData));
}

SharedStr TreeItem_GetViewAction(const TreeItem* self)
{
	return  viewActionPropDefPtr->GetValue(self);
}

//----------------------------------------------------------------------
// section : Specific DialogTypes
//----------------------------------------------------------------------

TokenID classificationDialogTypeID = GetTokenID_st("CLASSIFICATION"); // OBSOLETE
TokenID classBreaksDialogTypeID = GetTokenID_st("CLASS_BREAKS");

bool IsClassBreakAttr(const TreeItem* adi)
{
	TokenID dtId = TreeItem_GetDialogType( adi );
	return dtId == classificationDialogTypeID
		|| dtId == classBreaksDialogTypeID;
}

void MakeClassBreakAttr(AbstrDataItem* adi)
{
	TreeItem_SetDialogType(adi, classBreaksDialogTypeID);
}

//----------------------------------------------------------------------
// cdf support
//----------------------------------------------------------------------

bool HasCdfProp(const TreeItem* item)
{
	return TreeItemHasPropertyValue(item, cdfPropDefPtr);
}

SharedStr GetCdfProp(const TreeItem* item)
{
	return TreeItemPropertyValue(item, cdfPropDefPtr);
}

const TreeItem* GetCdfDataRef(const TreeItem* item)
{
	if (HasCdfProp(item))
	{
		SharedStr cdfValue = GetCdfProp(item);
		const TreeItem* result = item->FindItem( cdfValue );
		if (result)
			result->UpdateMetaInfo();
		return result;
	}
	return nullptr;
}

const AbstrDataItem* GetCdfAttr(const TreeItem* item)
{
	return AsDynamicDataItem( GetCdfDataRef(item) );
}

//----------------------------------------------------------------------
// Property Definitions
//----------------------------------------------------------------------

namespace { // local defs

	struct NamePropDef: ReadOnlyPropDef<TreeItem, TokenID>
	{
		NamePropDef()
			:	ReadOnlyPropDef<TreeItem, TokenID>(NAME_NAME, set_mode::construction, xml_mode::name)
		{}
		// override base class
		ApiType GetValue(const TreeItem* ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->GetID();
		}
	};

	class ExprPropDef: public PropDef<TreeItem, SharedStr>
	{
	public:
		ExprPropDef()
			:	PropDef<TreeItem, SharedStr>(EXPR_NAME, set_mode::optional, xml_mode::element, cpy_mode::expr, chg_mode::invalidate, false, true, true)
		{}
		// override base class
		ApiType GetValue(const TreeItem* ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->GetExpr();
		}
		ApiType GetRawValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->_GetExprStr();
		}
		void SetValue(TreeItem* ti, ParamType value) override
		{
			MGD_PRECONDITION(ti);
			ti->SetExpr(value);
		}

		void CopyValue(const Object* src, Object* dst) override
		{
			debug_cast<TreeItem*>(dst)->_SetExpr( debug_cast<const TreeItem*>(src)->_GetExprStr() );
		}
			
		void SetValueAsCharArray(Object* self, CharPtr value) override
		{
			TreeItem* ti = debug_cast<TreeItem*>(self);
			dms_assert(ti);
			ti->SetExpr( SharedStr(value) );
		}
	};

	struct DisableStoragePropDef: PropDef<TreeItem, PropBool>
	{
		DisableStoragePropDef()
			:	PropDef<TreeItem, PropBool>(DISABLESTORAGE_NAME, set_mode::optional, xml_mode::element, cpy_mode::expr) 
		{}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			dms_assert(ti);
			return ti->IsDisabledStorage();
		}
		void SetValue(TreeItem* ti, ParamType value) override
		{
			dms_assert(ti);
			ti->AssertPropChangeRights(DISABLESTORAGE_NAME);
			ti->DisableStorage(value);
		}
		bool HasNonDefaultValue(const Object* self) const
		{
			const TreeItem* ti = debug_cast<const TreeItem*>(self);
			dms_assert(ti);
			return GetValue(ti) && ! ti->HasConfigData();
		}
	};

	struct KeepDataPropDef: PropDef<TreeItem, PropBool>
	{
		KeepDataPropDef()
			:	PropDef<TreeItem, PropBool>(KEEPDATA_NAME, set_mode::optional, xml_mode::element, cpy_mode::expr) 
		{}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			dms_assert(ti);
			return ti->GetKeepDataState();
		}
		void SetValue(TreeItem* ti, ParamType value) override
		{
			dms_assert(ti);
			ti->AssertPropChangeRights(KEEPDATA_NAME);
			ti->SetKeepDataState(value);
		}
		bool HasNonDefaultValue(const Object* self) const
		{
			const TreeItem* ti = debug_cast<const TreeItem*>(self);
			dms_assert(ti);
			const TreeItem* parent = ti->GetTreeParent();
			return parent 
				? (GetValue(ti) != GetValue(parent))
				:  GetValue(ti);
		}
	};

	struct FreeDataPropDef: PropDef<TreeItem, PropBool>
	{
		FreeDataPropDef()
			:	PropDef<TreeItem, PropBool>(FREEDATA_NAME, set_mode::optional, xml_mode::element, cpy_mode::expr) 
		{}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			dms_assert(ti);
			return ti->GetFreeDataState();
		}
		void SetValue(TreeItem* ti, ParamType value) override
		{
			dms_assert(ti);
			ti->AssertPropChangeRights(FREEDATA_NAME);
			ti->SetFreeDataState(value);
		}
		bool HasNonDefaultValue(const Object* self) const
		{
			const TreeItem* ti = debug_cast<const TreeItem*>(self);
			dms_assert(ti);
			const TreeItem* parent = ti->GetTreeParent();
			return parent 
				? (GetValue(ti) != GetValue(parent))
				:  GetValue(ti);
		}
	};

	struct StoreDataPropDef: PropDef<TreeItem, PropBool>
	{
		StoreDataPropDef()
			:	PropDef<TreeItem, PropBool>(STOREDATA_NAME, set_mode::optional, xml_mode::element, cpy_mode::expr) 
		{}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			dms_assert(ti);
			return ti->GetStoreDataState();
		}
		void SetValue(TreeItem* ti, ParamType value) override
		{
			dms_assert(ti);
			ti->AssertPropChangeRights(STOREDATA_NAME);
			ti->SetStoreDataState(value);
		}
		bool HasNonDefaultValue(const Object* self) const
		{
			const TreeItem* ti = debug_cast<const TreeItem*>(self);
			dms_assert(ti);
			const TreeItem* parent = ti->GetTreeParent();
			return parent 
				? (GetValue(ti) != GetValue(parent))
				:  GetValue(ti);
		}
	};

	struct NrSubItemsPropDef: ReadOnlyPropDef<TreeItem, UInt32>
	{
		NrSubItemsPropDef()
			:	ReadOnlyPropDef<TreeItem, UInt32>(NRSUBITEMS_NAME) {}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->CountNrSubItems();
		}
	};

	struct CaseDirPropDef: ReadOnlyPropDef<TreeItem, SharedStr>
	{
		CaseDirPropDef()
			:	ReadOnlyPropDef<TreeItem, SharedStr>(CASEDIR_NAME)
		{}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return GetCaseDir(ti);
		}
	};

	struct ConfigFileNamePropDef: ReadOnlyPropDef<TreeItem, SharedStr>
	{
		ConfigFileNamePropDef()
			:	ReadOnlyPropDef<TreeItem, SharedStr>(CONFIGFILENAME_NAME)
		{}

		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->GetConfigFileName();
		}
	};

	struct ConfigFileLineNrPropDef: ReadOnlyPropDef<TreeItem, UInt32>
	{
		ConfigFileLineNrPropDef()
			:	ReadOnlyPropDef<TreeItem, UInt32>(CONFIGFILELINENR_NAME) 
		{}

		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->GetConfigFileLineNr();
		}
	};


	struct ConfigFileColNrPropDef: ReadOnlyPropDef<TreeItem, UInt32>
	{
		ConfigFileColNrPropDef()
			:	ReadOnlyPropDef<TreeItem, UInt32>(CONFIGFILECOLNR_NAME) {}

		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->GetConfigFileColNr();
		}
	};
 
	struct FullSourceDescrPropDef : ReadOnlyPropDef<TreeItem, SharedStr>
	{
		FullSourceDescrPropDef()
			: ReadOnlyPropDef<TreeItem, SharedStr>(FULL_SOURCE_NAME)
		{}

		ApiType GetValue(TreeItem const * ti) const override
		{
			MGD_PRECONDITION(ti);
			return TreeItem_GetSourceDescr(ti, SourceDescrMode::Configured, true);
		}
	};

	struct ReadOnlyStorageManagersPropDef : ReadOnlyPropDef<TreeItem, SharedStr>
	{
		ReadOnlyStorageManagersPropDef()
			: ReadOnlyPropDef<TreeItem, SharedStr>(READ_ONLY_SM_NAME)
		{}

		ApiType GetValue(TreeItem const * ti) const override
		{
			MGD_PRECONDITION(ti);
			return TreeItem_GetSourceDescr(ti, SourceDescrMode::ReadOnly, true);
		}
	};

	struct WriteOnlyStorageManagersPropDef : ReadOnlyPropDef<TreeItem, SharedStr>
	{
		WriteOnlyStorageManagersPropDef()
			: ReadOnlyPropDef<TreeItem, SharedStr>(READ_ONLY_SM_NAME)
		{}

		ApiType GetValue(TreeItem const * ti) const override
		{
			MGD_PRECONDITION(ti);
			return TreeItem_GetSourceDescr(ti, SourceDescrMode::WriteOnly, true);
		}
	};

	struct AllStorageManagersPropDef : ReadOnlyPropDef<TreeItem, SharedStr>
	{
		AllStorageManagersPropDef()
			: ReadOnlyPropDef<TreeItem, SharedStr>(ALL_SM_NAME)
		{}

		ApiType GetValue(TreeItem const * ti) const override
		{
			MGD_PRECONDITION(ti);
			return TreeItem_GetSourceDescr(ti, SourceDescrMode::All, true);
		}
	};

#if defined(MG_DEBUG)

	struct AddrPropDef: ReadOnlyPropDef<TreeItem,SizeT>
	{
		AddrPropDef()
			:	ReadOnlyPropDef("Addr") {}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return SizeT(ti);
		}
	};

#endif //DEBUG END

	struct LastChangePropDef: ReadOnlyPropDef<TreeItem, UInt32>
	{
		LastChangePropDef()
			:	ReadOnlyPropDef<TreeItem, UInt32>("LastChange")
		{}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->GetLastChangeTS();
		}
	};

	const cpy_mode stg_cpy_mode  = cpy_mode::expr;
	const cpy_mode odbc_cpy_mode = stg_cpy_mode;

	struct CdfPropDef : StoredPropDef<TreeItem, SharedStr>
	{
		CdfPropDef()
			:	StoredPropDef(CDF_NAME, set_mode::optional, xml_mode::element, cpy_mode::all, false) 
		{}

		ApiType GetValue(TreeItem const * ti) const override 
		{
			ApiType v = StoredPropDef::GetValue(ti);
			if (v.empty() && ti->IsKindOf(AbstrDataItem::GetStaticClass()))
				v = StoredPropDef::GetValue(debug_cast<const AbstrDataItem*>(ti)->GetAbstrValuesUnit());
			return v;
		}
	};

	struct IsHiddenPropDef: PropDef<TreeItem, PropBool>
	{
		  IsHiddenPropDef()
			  :	PropDef<TreeItem, PropBool>(ISHIDDEN_NAME, set_mode::optional, xml_mode::element, cpy_mode::expr_noroot) 
		  {}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->GetTSF(TSF_IsHidden);
		}
		void SetValue(TreeItem * ti, ParamType v) override 
		{
			MGD_PRECONDITION(ti);
			ti->AssertPropChangeRights(ISHIDDEN_NAME);
			ti->SetIsHidden(v);
		}
	};

	struct InHiddenPropDef: ReadOnlyPropDef<TreeItem, PropBool>
	{
		InHiddenPropDef()
			:	ReadOnlyPropDef<TreeItem, PropBool>(INHIDDEN_NAME) 
		{}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->GetTSF(TSF_InHidden);
		}
	};

	struct IsTemplatePropDef: PropDef<TreeItem, PropBool>
	{
		IsTemplatePropDef()
			:	PropDef<TreeItem, PropBool>(ISTEMPLATE_NAME, set_mode::optional, xml_mode::element, cpy_mode::expr_noroot)
		{}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->IsTemplate();
		}
		void SetValue(TreeItem * ti, ParamType v) override 
		{
			MGD_PRECONDITION(ti);
			if (!v)
				ti->throwItemError("IsTemplate Property can only be set to 'True'");
			ti->AssertPropChangeRights(ISTEMPLATE_NAME);
			ti->SetIsTemplate();
		}
	};

	struct InTemplatePropDef: ReadOnlyPropDef<TreeItem, PropBool>
	{
		InTemplatePropDef()
			:	ReadOnlyPropDef<TreeItem, PropBool>(INTEMPLATE_NAME)
		{}

		// override base class
		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->InTemplate();
		}
	};

	struct IsEndogenousPropDef: ReadOnlyPropDef<TreeItem, PropBool>
	{
		IsEndogenousPropDef()
			:	ReadOnlyPropDef<TreeItem, PropBool>(ISENDOGENOUS_NAME)
		{}

		ApiType GetValue(TreeItem const * ti) const override 
		{
			MGD_PRECONDITION(ti);
			return ti->IsEndogenous();
		}
	};


	// OBSOLETE, MAKE THIS an instance of StateFlagPropDef
	struct IsPassorPropDef: ReadOnlyPropDef<TreeItem, PropBool>
	{
	  public:
		IsPassorPropDef()
			:	ReadOnlyPropDef<TreeItem, PropBool>(ISPASSOR_NAME) 
		{}

		ApiType GetValue(TreeItem const * ti) const override
		{
			MGD_PRECONDITION(ti);
			return ti->IsPassor();
		}
	};

	struct UsingPropDef : PropDef<TreeItem, SharedStr>
	{
		UsingPropDef() : PropDef<TreeItem, SharedStr>(USING_NAME, set_mode::optional, xml_mode::element, cpy_mode::none) {}
		// override base class
		ApiType GetValue(const TreeItem* item) const override 
		{ 
			SharedStr result;
			UInt32 n = item->GetNrNamespaceUsages();
			UInt32 i = 0;
			while (i!=n)
			{
				const TreeItem* ns = item->GetNamespaceUsage(i++);
				if (!ns->DoesContain(item))
				{
					if (result.ssize() > 0)
						result += ';';
					result += ns->GetScriptName(item);
				}
			}
			return result; 
		}
		void SetValue(TreeItem* ti, ParamType val) override 
		{ 
			dms_assert(ti);
			ti->AssertPropChangeRights(USING_NAME);
			ti->ClearNamespaceUsage();
			if (!val.empty())
				ti->AddUsingUrls(val.begin(), val.send());
		}
	};

	struct ExplicitSuppliersPropDef : PropDef<TreeItem, SharedStr>
	{
		ExplicitSuppliersPropDef()
			:	PropDef<TreeItem, SharedStr>(EXPLICITSUPPLIERS_NAME, set_mode::optional, xml_mode::element, cpy_mode::expr, chg_mode::eval, false, true, true)
		{}
		// override base class
		ApiType GetValue(const TreeItem* item) const override 
		{ 
			if (!item->HasSupplCache())
				return SharedStr();
			return item->GetSupplCache()->GetSupplString();
		}
		void SetValue(TreeItem* ti, ParamType val) override 
		{ 
			dms_assert(ti);
			ti->AssertPropChangeRights(EXPLICITSUPPLIERS_NAME);
			ti->GetOrCreateSupplCache()->SetSupplString(val);
			ti->TriggerEvaluation();
		}
		bool HasNonDefaultValue(const Object* self) const override
		{
			return debug_cast<const TreeItem*>(self)->HasSupplCache();
		}
	};

} // end anonymous namespace

namespace {
	static NamePropDef namePropDef;
	static MF_RoPropDef<TreeItem, SharedStr> fullNameProp(FULLNAME_NAME, &TreeItem::GetFullName);
	static ExprPropDef exprPropDef;
	static DisableStoragePropDef disableStoragePropDef;
	static KeepDataPropDef keepDataPropDef;
	static FreeDataPropDef freeDataPropDef;
	static StoreDataPropDef storeDataPropDef;
	static NrSubItemsPropDef nrSubItemsPropDef;
	static CaseDirPropDef caseDirPropDef;
	static ConfigFileNamePropDef configFileNamePropDef;
	static ConfigFileLineNrPropDef configFileLineNrPropDef;
	static ConfigFileColNrPropDef configFileColNrPropDef;
	static FullSourceDescrPropDef fullSourceDescrPropDef;

	static MF_RoPropDef<TreeItem, PropBool> calcPropDef(ISCALCULABLE_NAME, &TreeItem::HasCalculator);
	static MF_RoPropDef<TreeItem, PropBool> loadPropDef(ISLOADABLE_NAME, &TreeItem::IsLoadable);
	static MF_RoPropDef<TreeItem, PropBool> storePropDef(ISSTORABLE_NAME, &TreeItem::IsStorable);
	static struct LastChangePropDef lastChangePropDef;

	static StoredPropDef<TreeItem, SharedStr> descrPropDef(DESCR_NAME, set_mode::optional, xml_mode::element, cpy_mode::expr, false);
	static StoredPropDef<TreeItem, SharedStr> integrityCheckPropDef(ICHECK_NAME, set_mode::optional, xml_mode::element, cpy_mode::expr, true);
	static StoredPropDef<TreeItem, SharedStr> storageNamePropDef(STORAGENAME_NAME, set_mode::optional, xml_mode::element, stg_cpy_mode, true, chg_mode::invalidate);
	static StoredPropDef<TreeItem, TokenID  > storageTypePropDef(STORAGETYPE_NAME, set_mode::optional, xml_mode::element, stg_cpy_mode, false, chg_mode::invalidate);
	static StoredPropDef<TreeItem, SharedStr> storageOptionsPropDef(STORAGEOPTIONS_NAME, set_mode::optional, xml_mode::element, stg_cpy_mode, true, chg_mode::invalidate);
	static StoredPropDef<TreeItem, PropBool > storageReadOnlyPropDef(STORAGEREADONLY_NAME, set_mode::optional, xml_mode::element, stg_cpy_mode, false);
	static StoredPropDef<TreeItem, TokenID  > syncModePropDef(SYNCMODE_NAME, set_mode::optional, xml_mode::element, stg_cpy_mode, false);

	static struct CdfPropDef cdfPropDef;

	static StoredPropDef<TreeItem, TokenID  > dialogTypePropDef(DIALOGTYPE_NAME, set_mode::optional, xml_mode::element, cpy_mode::all, false);
	static StoredPropDef<TreeItem, SharedStr> dialogDataPropDef(DIALOGDATA_NAME, set_mode::optional, xml_mode::element, cpy_mode::all, false);
	static StoredPropDef<TreeItem, TokenID  > configStorePropDef(CONFIGSTORE_NAME, set_mode::optional, xml_mode::none, cpy_mode::none, false);
	static StoredPropDef<TreeItem, TokenID  > paramTypePropDef(PARAMTYPE_NAME, set_mode::optional, xml_mode::element, cpy_mode::none, false);
	static StoredPropDef<TreeItem, SharedStr> paramDataPropDef(PARAMDATA_NAME, set_mode::optional, xml_mode::element, cpy_mode::none, false);
	static StoredPropDef<TreeItem, SharedStr> urlPropDef(URL_NAME, set_mode::optional, xml_mode::element, cpy_mode::all, false);
	static StoredPropDef<TreeItem, SharedStr> labelPropDef(LABEL_NAME, set_mode::optional, xml_mode::element, cpy_mode::all, false);
	static StoredPropDef<TreeItem, SharedStr> viewActionPropDef(VIEW_ACTION_NAME, set_mode::optional, xml_mode::element, cpy_mode::all, false);
	static StoredPropDef<TreeItem, SharedStr> viewDataPropDef(VIEW_DATA_NAME, set_mode::optional, xml_mode::element, cpy_mode::all, false);
	static StoredPropDef<TreeItem, SharedStr> sourceDescrPropDef(SOURCE_NAME, set_mode::optional, xml_mode::element, cpy_mode::all, false);

	// TODO: Dit zijn eigenlijk props van ODBCStorageManager en niet van TreeItem
	static StoredPropDef<TreeItem, SharedStr> sqlStringPropDef(SQLSTRING_NAME, set_mode::optional, xml_mode::element, odbc_cpy_mode, true, chg_mode::invalidate);
	static StoredPropDef<TreeItem, TokenID  > tableTypeNamePropDef(TABLETYPE_NAME, set_mode::none, xml_mode::element, cpy_mode::none, false);

	static StoredPropDef<TreeItem, SharedStr> subItemSchemaCalculatorPropDef("SubItemSchema", set_mode::optional, xml_mode::element, cpy_mode::all, false);
	static StoredPropDef<TreeItem, SharedStr> supplierSchemaCalculatorPropDef("SupplierSchema", set_mode::optional, xml_mode::element, cpy_mode::all, false);
	static StoredPropDef<TreeItem, SharedStr> calculationSchemaCalculatorPropDef("CalcSchema", set_mode::optional, xml_mode::element, cpy_mode::all, false);

	static struct IsHiddenPropDef isHiddenPropDef;
	static struct InHiddenPropDef inHiddenPropDef;
	static struct IsTemplatePropDef isTemplatePropDef;
	static struct InTemplatePropDef inTemplatePropDef;
	static struct IsEndogenousPropDef isEndogenousPropDef;

	// OBSOLETE, MAKE THIS an instance of StateFlagPropDef
	static struct IsPassorPropDef isPassorPropDef;
	static struct UsingPropDef usingPropDef;

	static struct ExplicitSuppliersPropDef explicitSupplPropDef;


#if defined(MG_DEBUG)

	static AddrPropDef addrPropDef;

#endif //DEBUG END

}

PropDef<TreeItem, SharedStr>* exprPropDefPtr           = &exprPropDef;
PropDef<TreeItem, SharedStr>* descrPropDefPtr          = &descrPropDef;
PropDef<TreeItem, SharedStr>* integrityCheckPropDefPtr = &integrityCheckPropDef;
PropDef<TreeItem, SharedStr>* explicitSupplPropDefPtr  = &explicitSupplPropDef;

PropDef<TreeItem, SharedStr>* storageNamePropDefPtr    = &storageNamePropDef;
PropDef<TreeItem, TokenID  >* storageTypePropDefPtr    = &storageTypePropDef;
PropDef<TreeItem, SharedStr>* storageOptionsPropDefPtr = &storageOptionsPropDef;
PropDef<TreeItem, PropBool >* storageReadOnlyPropDefPtr= &storageReadOnlyPropDef;
PropDef<TreeItem, TokenID  >* syncModePropDefPtr       = &syncModePropDef;

PropDef<TreeItem, TokenID  >* dialogTypePropDefPtr     = &dialogTypePropDef;
PropDef<TreeItem, SharedStr>* dialogDataPropDefPtr     = &dialogDataPropDef;
PropDef<TreeItem, TokenID  >* paramTypePropDefPtr      = &paramTypePropDef;
PropDef<TreeItem, SharedStr>* paramDataPropDefPtr      = &paramDataPropDef;
PropDef<TreeItem, SharedStr>* labelPropDefPtr          = &labelPropDef;
PropDef<TreeItem, SharedStr>* viewActionPropDefPtr     = &viewActionPropDef;
PropDef<TreeItem, TokenID  >* configStorePropDefPtr    = &configStorePropDef;
PropDef<TreeItem, SharedStr>* caseDirPropDefPtr        = &caseDirPropDef;
PropDef<TreeItem, SharedStr>* sourceDescrPropDefPtr    = &sourceDescrPropDef;
PropDef<TreeItem, SharedStr>* fullSourceDescrPropDefPtr= &fullSourceDescrPropDef;

PropDef<TreeItem, SharedStr>* sqlStringPropDefPtr      = &sqlStringPropDef;
PropDef<TreeItem, TokenID  >* tableTypeNamePropDefPtr  = &tableTypeNamePropDef;
PropDef<TreeItem, SharedStr>* cdfPropDefPtr            = &cdfPropDef;
PropDef<TreeItem, SharedStr>* urlPropDefPtr            = &urlPropDef;
