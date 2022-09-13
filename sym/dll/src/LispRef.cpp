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
#include "SymPch.h"
#pragma hdrstop

#include "LispRef.h"

#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "mci/Class.h"
#include "geo/BaseBounds.h"
#include "geo/IterRange.h"
#include "ser/AsString.h"
#include "ser/DebugOutStream.h"
#include "ser/PolyStream.h"
#include "ser/StringStream.h"
#include "ser/VectorStream.h"
#include "set/Cache.h"
#include "set/DataCompare.h"
#include "utl/Quotes.h"
#include "utl/Encodes.h"
#include "xct/DmsException.h"
#include "LockLevels.h"

#include <typeinfo>

#define DBG_LOADOBJ false

inline FormattedOutStream& operator <<(FormattedOutStream& output, const LispObj& obj)
{
	obj.Print(output, 0);
	return output;
}

#define ILLEGAL_ABSTRACT(OBJ, METHOD) throwIllegalAbstract(MG_POS, OBJ, METHOD);

#include "LispList.h" // thats where LispError is defined

void LispError(CharPtr msg, LispPtr ref)
{
	throwErrorF("Lisp", "%s with:\n%s", msg, AsString(ref).c_str());
}

/****************** class LispObj                  *******************/

inline LispObj::LispObj()  {}
inline LispObj::~LispObj() {}

bool     LispObj::IsNumb()     const { return false; }
Number   LispObj::GetNumbVal() const { ILLEGAL_ABSTRACT(this, "GetNumbVal");}

bool     LispObj::IsSymb()     const { return false; }
TokenStr LispObj::GetSymbStr() const { ILLEGAL_ABSTRACT(this, "GetSymbStr"); }
TokenStr LispObj::GetSymbEnd() const { ILLEGAL_ABSTRACT(this, "GetSymbEnd"); }
TokenID  LispObj::GetSymbID()  const { ILLEGAL_ABSTRACT(this, "GetSymbId" ); }
ChroID   LispObj::GetChroID()  const { ILLEGAL_ABSTRACT(this, "GetChroId" ); }
bool     LispObj::IsVar()      const { return false; }

bool     LispObj::IsStrn()     const { return false; }
CharPtr  LispObj::GetStrnBeg() const { ILLEGAL_ABSTRACT(this, "GetStrngStr"); }
CharPtr  LispObj::GetStrnEnd() const { ILLEGAL_ABSTRACT(this, "GetStrngStr"); }


bool     LispObj::IsList()     const { return false; }
LispPtr  LispObj::Left()       const { ILLEGAL_ABSTRACT(this, "Left" ); }
LispPtr  LispObj::Right()      const { ILLEGAL_ABSTRACT(this, "Right"); }
void     LispObj::PrintAsFLisp(FormattedOutStream& os, UInt32 level) const
{
	Print(os, level);
}

/******************                               *******************/
/****************** struct LispCls                *******************/
/******************                               *******************/

typedef LispObj* (*CreateFromStreamFunc)(PolymorphInpStream& istr);

class LispCls : public Class
{
public:
	LispCls(
		CreateFromStreamFunc createFromStreamFunc, 
		TokenID clsNameID)
		:	Class(0, LispObj::GetStaticClass(), clsNameID)
		,	m_CreateFromStreamFunc(createFromStreamFunc)
	{}

	virtual Object* CreateObj(PolymorphInpStream*) const override;
private:
	CreateFromStreamFunc m_CreateFromStreamFunc;
};

/****************** Avoid stack overflow *******************/

#include <future>

Object* LispCls::CreateObj(PolymorphInpStream* istr) const
{
	SizeT n = istr->m_ObjReg.size();
	istr->m_ObjReg.push_back(nullptr);


	Object* obj = nullptr;
	auto callFunc = [this, istr]() {
		return m_CreateFromStreamFunc(*istr);
	};
	auto remainingStackSpace = RemainingStackSpace();
	if (remainingStackSpace > 32768)
		obj = callFunc();
	else
	{
		auto future = std::async(callFunc);
		obj = future.get();
	}

	dms_assert(obj);
	istr->m_ObjReg[n] = obj;
	return obj;
}


//----------------------------------------------------------------------
// Serialization and rtti
//----------------------------------------------------------------------

IMPL_CLASS(LispObj, 0)

#define IMPL_LISPCLS(cls, createFromStreamFunc) \
	const LispCls* cls::GetStaticClass() \
	{ \
		static LispCls s_Cls(createFromStreamFunc, GetTokenID_st(#cls) ); \
		return &s_Cls; \
	}  \

#define IMPL_STATIC_LISPCLS(cls) IMPL_RTTI(cls, LispCls) IMPL_LISPCLS(cls, cls::ReloadObj)


/******************                               *******************/
/****************** class NumbObj                 *******************/
/******************                               *******************/


class NumbObj: public LispObj
{
	friend LispRef::LispRef(Number v);
	friend struct MakeNumbFunc;

private:
	NumbObj() = delete; // REMOVE { NeverLinkThis(); }
	NumbObj(const NumbObj&) = delete; // REMOVE { NeverLinkThis(); }

	NumbObj(Number v)
		:	m_Value(v) 
	{}
   ~NumbObj();

	virtual bool   IsNumb()     const { return true; }
	virtual Number GetNumbVal() const { return m_Value;    }

	virtual void Print(FormattedOutStream& out, UInt32 level)   const { out << m_Value << ' '; }
	static LispObj* ReloadObj(PolymorphInpStream& ar);
	virtual void WriteObj(PolymorphOutStream& ar) const;

	Number m_Value;

	DECL_RTTI(SYM_CALL, LispCls);
};

/****************** ctor/dtor                         *******************/

struct MakeNumbFunc
{	
	using argument_type = Number_t;
	using result_type = LispObj*;

	LispObj* operator()(Number_t v) const
	{ 
		return new NumbObj(Number(v)); 
	} 
};

StaticCache<MakeNumbFunc, DataCompare<Number_t> > s_NumbObjCache;

LispRef::LispRef(Number value) 
	: SharedPtrWrap(
		s_NumbObjCache(value)
	)
{}

NumbObj::~NumbObj() { s_NumbObjCache.remove(m_Value); }


/****************** Serialization and rtti            *******************/

IMPL_STATIC_LISPCLS(NumbObj)

LispObj* NumbObj::ReloadObj(PolymorphInpStream& ar)
{
	DBG_START("NumbObj", "ReloadObj", DBG_LOADOBJ);

	double value;
	ar >> value;
	DBG_TRACE(("numb = %lf", value));

	return s_NumbObjCache(Number(value));
}

void NumbObj::WriteObj(PolymorphOutStream& ar) const
{
	double value = m_Value;
	ar << value;
}

/******************                                   *******************/
/****************** class SymbObj                     *******************/
/******************                                   *******************/


class SymbObj : public LispObj
{
	friend struct MakeSymbFunc;
	friend SymbObj* GetOrCreateSymbObj(TokenID t, ChroID c);

private:
	SymbObj()              : m_TokenID(TokenID::GetUndefinedID()) { NeverLinkThis(); }
	SymbObj(const SymbObj&): m_TokenID(TokenID::GetUndefinedID()) { NeverLinkThis(); }

	SymbObj (TokenID  t, ChroID c) : m_TokenID(t), m_ChroID(c) {}

  ~SymbObj();

	bool     IsSymb()     const override { return true;  }
	TokenID  GetSymbID()  const override { return m_TokenID;     }
	ChroID   GetChroID()  const override { return m_ChroID;      }
	TokenStr GetSymbStr() const override { return GetTokenStr   (m_TokenID); }
	TokenStr GetSymbEnd() const override { return GetTokenStrEnd(m_TokenID); }
	bool     IsVar()      const override { return m_ChroID != 0; }

	void Print       (FormattedOutStream& out, UInt32 level)  const override;
	void PrintAsFLisp(FormattedOutStream& out, UInt32 level)  const override;
	void WriteObj(PolymorphOutStream& ar) const override ;
	static LispObj* ReloadObj (PolymorphInpStream& ar);

	TokenID m_TokenID;
	ChroID  m_ChroID = 0;

	DECL_RTTI(SYM_CALL, LispCls);
};

/****************** ctor/dtor                         *******************/

typedef std::pair<TokenID,  ChroID>  SymbType;

struct MakeSymbFunc
{	
	typedef SymbType argument_type;
	typedef SymbObj* result_type;

	SymbObj* operator()(const SymbType& v) const
	{ 
		return new SymbObj(v.first, v.second); 
	} 
};

StaticCache<MakeSymbFunc>          s_SymbObjCache;

static_ptr<std::vector<SymbObj*> > s_ZeroSymbObjCache;
UInt32                             s_nrActiveZeroSymbObj = 0;
leveled_std_section           s_SymbObjSection(item_level_type(0), ord_level_type::SymbObjSection, "SymbObjRegister");

inline SymbObj* GetOrCreateSymbObj(TokenID t, ChroID c)
{
	if (c) 
		return s_SymbObjCache(SymbType(t, c));
	UInt32 tnr     = t.GetNr(TokenID::TokenKey());
	UInt32 reqSize = tnr + 1;

	leveled_std_section::scoped_lock lock(s_SymbObjSection);
	if (s_ZeroSymbObjCache.is_null())
		s_ZeroSymbObjCache.assign(new std::vector<SymbObj*>(reqSize));
	std::size_t s = s_ZeroSymbObjCache->size();
	if (reqSize > s) 
	{
		s *= 2;
		MakeMax(s, reqSize);
		s_ZeroSymbObjCache->resize(s, 0);
	}
	SymbObj*& cacheEntry = (*s_ZeroSymbObjCache)[tnr]; 
	if (!cacheEntry)
	{
		cacheEntry = new SymbObj(t, 0);
		s_nrActiveZeroSymbObj++;
	}
	return cacheEntry;
}

LispRef::LispRef(TokenID t, UInt32 c) : SharedPtrWrap(GetOrCreateSymbObj(t,c))
{}

LispRef::LispRef(CharPtr s, UInt32 c) : SharedPtrWrap(GetOrCreateSymbObj(GetTokenID_mt(s), c))
{}

SymbObj::~SymbObj() 
{ 
	if (m_ChroID)
		s_SymbObjCache.remove(SymbType(m_TokenID, m_ChroID)); 
	else
	{
		leveled_std_section::scoped_lock lock(s_SymbObjSection);

		dms_assert(s_ZeroSymbObjCache->size() > m_TokenID.GetNr(TokenID::TokenKey()) 
			&& (*s_ZeroSymbObjCache)[m_TokenID.GetNr(TokenID::TokenKey())] == this);
		(*s_ZeroSymbObjCache)[m_TokenID.GetNr(TokenID::TokenKey())] = nullptr;
		if (!--s_nrActiveZeroSymbObj)
			s_ZeroSymbObjCache.reset();
	}
}

/****************** Serialization and rtti            *******************/

IMPL_STATIC_LISPCLS(SymbObj)

void SymbObj::Print   (FormattedOutStream& out, UInt32 level) const
{ 
	auto symbStr = GetSymbStr();
	if (itemName_test(symbStr.c_str()))
		out << symbStr.c_str();
	else
		SingleQuote(out, symbStr.c_str());
	if (m_ChroID) 
		out << ":" << m_ChroID; 
	out << " "; 
}

void SymbObj::PrintAsFLisp(FormattedOutStream& out, UInt32 level) const
{ 
	out << GetSymbStr().c_str();
	if (m_ChroID) 
		out << ":" << m_ChroID; 
}

LispObj* SymbObj::ReloadObj(PolymorphInpStream& ar)
{
	DBG_START("SymbObj", "ReloadObj", DBG_LOADOBJ);
	TokenID t = ar.ReadToken();
	DBG_TRACE(("Token=%s", t.GetStr().c_str()));

	UInt32  c;	
	c = ar.ReadUInt32();

	return GetOrCreateSymbObj(t, c);
}

void SymbObj::WriteObj(PolymorphOutStream& ar) const
{
	ar.WriteToken(GetSymbID());
	ar.WriteUInt32(m_ChroID);
}

/******************                                   *******************/
/****************** class StrnObj                     *******************/
/******************                                   *******************/

struct StrnType: Couple<CharPtr>
{
	StrnType(): Couple<CharPtr>(TokenID::GetEmptyStr(), TokenID::GetEmptyStr()) {}
	StrnType(CharPtr b, CharPtr e): Couple<CharPtr>(b, e) {}
	StrnType(const StrnType& oth) : Couple<CharPtr>(oth)  {}

	UInt32 size () const { return second - first;  }
	bool   empty() const { return first == second; } 

	bool operator < (const StrnType& rhs) const
	{
		UInt32 sz1 = size(), sz2 = rhs.size();
		int cmpRes = strncmp(first, rhs.first, Min<UInt32>(sz1, sz2) );
		return (cmpRes < 0)
			|| (cmpRes == 0 && sz1 < sz2);
	}
};

class StrnObj : public LispObj
{
	friend struct MakeStrnFunc;
	friend struct DuplStrnData;
	friend struct LispRef;

	static StrnObj* GetEmpty();

private:
	StrnObj() { }
	StrnObj(const StrnObj& oth): m_Data(oth.m_Data) { NeverLinkThis(); }

	StrnObj(CharPtr b, CharPtr e) : m_Data(b, e) {}
	~StrnObj();

	bool    IsStrn()     const override { return true;          }
	CharPtr GetStrnBeg() const override { return m_Data.first;  }
	CharPtr GetStrnEnd() const override { return m_Data.second; }

	void Print     (FormattedOutStream& o, UInt32 level)  const override;
	void WriteObj  (PolymorphOutStream& ar) const override;
	static LispObj* ReloadObj(PolymorphInpStream& ar);

	const StrnType& GetConstData() const { return m_Data; }

	StrnType m_Data; 

	DECL_RTTI(SYM_CALL, LispCls);
};

/****************** ctor/dtor                         *******************/

struct MakeStrnFunc
{	
	typedef StrnType argument_type;
	typedef StrnObj* result_type;

	StrnObj* operator()(const StrnType& v) const
	{ 
		UInt32 len = v.size();
		dms_assert(len); // zero-sized strings are separately provided by StrnObj::Empty()
		char *b = new char[len+1], 
		     *e = b+len;
		strncpy(b, v.first, len);
		*e = 0;
		return new StrnObj(b, e);
	} 
};

struct DuplStrnData {
	const StrnType& operator()(const StrnType&, StrnObj* res) const 
	{ 
		return res->GetConstData();
	}
};


StaticCache<MakeStrnFunc, std::less<StrnType>, DuplStrnData> s_StrnObjCache;

LispRef::LispRef(CharPtr b, CharPtr e)
:	SharedPtrWrap(
		(b ==e )
		?	StrnObj::GetEmpty()
		:	s_StrnObjCache(StrnType(b, e))
	)
{}

StrnObj::~StrnObj() 
{ 
	if (!m_Data.empty())
	{
		s_StrnObjCache.remove(m_Data); 
		delete [] m_Data.first;
	}
}


/****************** Serialization and rtti            *******************/

IMPL_STATIC_LISPCLS(StrnObj)

StrnObj* StrnObj::GetEmpty()
{
	static StrnObj* empty = new StrnObj();
	static SharedPtr<LispObj> emptyHolder(empty);

	return empty;
}

void StrnObj::Print   (FormattedOutStream& o, UInt32 level) const
{ 
	DoubleQuote(o, GetStrnBeg(), GetStrnEnd());
	o << " "; 
}

LispObj* StrnObj::ReloadObj(PolymorphInpStream& ar)
{
	DBG_START("StrnObj", "ReloadObj", DBG_LOADOBJ);

	UInt32 len = ar.ReadUInt32();
	if (!len)
		return StrnObj::GetEmpty();

	if (ar.m_StrnBufSize < len)
		ar.m_StrnBuf.reset( new Byte[Max<UInt32>(len, 2 * ar.m_StrnBufSize)] );

	Byte
		*b = ar.m_StrnBuf.get(),
		*e = b+len;

	ReadBinRangeImpl(ar, b, e, TYPEID(directcpy_streamable_tag));

#if defined(MG_DEBUG)
	if (debugContext.m_Active)
	{
		DebugOutStream::scoped_lock lock(g_DebugStream);
		g_DebugStream->Buffer().WriteBytes(b, e-b);
	}
#endif

	return s_StrnObjCache(
		StrnType(
			b,
			e
		)
	);
}

void StrnObj::WriteObj(PolymorphOutStream& ar) const
{
	ar.WriteUInt32( m_Data.size() );
	WriteBinRangeImpl(ar, m_Data.first, m_Data.second, TYPEID(directcpy_streamable_tag));
}

/******************                                   *******************/
/****************** class ListObj                     *******************/
/******************                                   *******************/


class ListObj: public LispObj
{
	friend LispRef::LispRef(LispPtr head, LispPtr tail);
	friend struct MakeListFunc;

private:
	ListObj()                { NeverLinkThis(); }
	ListObj(const ListObj& ) { NeverLinkThis(); }

	ListObj (LispPtr head, LispPtr tail):	m_Left(head), m_Right(tail) {}
   ~ListObj();

	bool   IsList() const override { return true; }
	LispPtr Left () const override { return m_Left;  }
	LispPtr Right() const override { return m_Right; }

	void Print       (FormattedOutStream&, UInt32 level) const override;
	void PrintAsFLisp(FormattedOutStream&, UInt32 level) const override;

	static LispObj* ReloadObj (PolymorphInpStream& ar);
	void WriteObj(PolymorphOutStream& ar) const override;

	LispRef m_Left;
	LispRef m_Right;

	DECL_RTTI(SYM_CALL, LispCls);
};

/****************** ctor/dtor                         *******************/

typedef std::pair<LispPtr, LispPtr> ListType;

struct MakeListFunc
{	
	typedef ListType argument_type;
	typedef LispObj* result_type;

	LispObj* operator()(const ListType& v) const
	{ 
		return new ListObj(v.second, v.first); 
	} 
};

StaticCache<MakeListFunc> s_ListObjCache;

LispRef::LispRef(LispPtr head, LispPtr tail) 
	:	SharedPtrWrap<LispPtr>(
			s_ListObjCache(
				ListType(tail, head)
			)
		)
{}

ListObj::~ListObj() { s_ListObjCache.remove(ListType(m_Right, m_Left)); }

/****************** Serialization and rtti            *******************/

IMPL_STATIC_LISPCLS(ListObj)

UInt32 LispPtr::MAX_PRINT_LEVEL = 5;

#include "utl/IncrementalLock.h"

void ListObj::Print(FormattedOutStream& out, UInt32 level) const
{
	// Check if right track ends in a EndP
	DynamicIncrementalLock<> lock(level);

	if (level > LispRef::MAX_PRINT_LEVEL)
	{
		out << "[...]";
	}
	else
	{
		LispPtr cursor = m_Right;
		while (cursor.IsRealList()) 
			cursor = cursor.Right();
		if (cursor.EndP())
		{
			out << '('; 
			m_Left.Print(out, level);

			cursor = m_Right;
			while (cursor.IsRealList())
			{
				out << " "; cursor.Left().Print(out, level);
				cursor = cursor.Right();
			}
			out << ')';
		}
		else
		{
			out << '['; m_Left.Print(out, level);
			out << " "; m_Right.Print(out, level); 
			out << "]";
		}
	}
}

void ListObj::PrintAsFLisp(FormattedOutStream& out, UInt32 level) const
{
	// Check if right track ends in a EndP
	DynamicIncrementalLock<> lock(level);

	if (level > LispRef::MAX_PRINT_LEVEL)
	{
		out << "[...]";
	}
	else
	{
		LispPtr cursor = m_Right;
		while (cursor.IsRealList())
			cursor = cursor->Right();
		if (cursor.EndP())
		{
			m_Left.PrintAsFLisp(out, level);
			out << '(';
			cursor = m_Right;
			while (cursor.IsRealList())
			{
				dms_assert(cursor->GetRefCount() >= 1);
				if (cursor != m_Right)
					out << ", "; // comma after first element
				cursor->Left().PrintAsFLisp(out, level);
				cursor = cursor->Right();
			}
			out << ')';
		}
		else
		{
			out << '[';  m_Left.PrintAsFLisp(out, level);
			out << ", "; m_Right.PrintAsFLisp(out, level);
			out << "]";
		}
	}
}

LispObj* ListObj::ReloadObj(PolymorphInpStream& ar)
{
	DBG_START("ListObj", "ReloadObj", DBG_LOADOBJ);

	LispRef head, tail;
	ar >> head;
	DBG_TRACE(("head = %s", AsString(head).c_str()));
	ar >> tail;
	DBG_TRACE(("tail = %s", AsString(tail).c_str()));
	return 
		s_ListObjCache(
			ListType(tail, head)
		);
}

void ListObj::WriteObj(PolymorphOutStream& ar) const
{
	ar << m_Left << m_Right;
}

/******************                                   *******************/
/****************** class LispRef                     *******************/
/******************                                   *******************/

/****************** Print func                *******************/

void LispPtr::Print(FormattedOutStream& out, UInt32 level) const 
{ 
	if (get_ptr()) 
		get_ptr()->Print(out, level); 
	else 
		out << "() "; 
}

void LispPtr::PrintAsFLisp(FormattedOutStream& out, UInt32 level) const
{ 
	if (get_ptr()) 
		get_ptr()->PrintAsFLisp(out, level);
	else
		out << "() ";
}


leveled_critical_section gc_FLispUsageGuard(item_level_type(0), ord_level_type::FLispUsageCache, "FLispUsageCache");

SharedStr AsFLispSharedStr(LispPtr lispRef)
{
	// MUTEX, PROTECT EXCLUSIVE USE OF GLOBAL STATIC RESOURCE FOR PERFORMANCE REASONS;
	leveled_critical_section::scoped_lock lock(gc_FLispUsageGuard);
	static ExternalVectorOutStreamBuff::VectorType vector;
	MG_DEBUGCODE( SizeT cap = vector.capacity(); )
	vector.clear();
	dbg_assert(cap == vector.capacity());

	ExternalVectorOutStreamBuff vecBuf(vector);
	FormattedOutStream outStr(&vecBuf, FormattingFlags::ThousandSeparator);
	lispRef.PrintAsFLisp(outStr, 0);
	outStr << char('\0');
	return SharedStr(begin_ptr(vector), end_ptr(vector)-1);
}

/****************** IsExpr *******************/

bool IsExprList(LispPtr e)
{
	if (!e.IsList())
		return false;
	if (e.EndP())
		return true;
	if (!IsExpr(e.Left()))
		return false;
	return IsExprList(e.Right());
}

bool IsExpr(LispPtr e)
{
	if (!e.IsList())
		return true;
	if (e.EndP())
		return false;
	if (!e.Left().IsSymb())
		return false;
	return IsExprList(e.Right());
}


//REMOVE const LispPtr LispPtr::null;

