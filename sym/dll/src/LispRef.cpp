// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "SymPch.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include <stack>

#include "LispRef.h"

#include "act/MainThread.h"
#include "dbg/debug.h"
#include "dbg/SeverityType.h"
#include "mci/Class.h"
#include "geo/BaseBounds.h"
#include "geo/iterrange.h"
#include "geo/Conversions.h"
#include "ptr/PtrBase.h"
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

struct LispCaches;

static LispComponent s_lispServiceSubscription;

/****************** Lisp streaming *******************/

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
bool     LispObj::IsUI64()     const { return false; }
UInt64   LispObj::GetUI64Val() const { ILLEGAL_ABSTRACT(this, "GetUI64Val"); }


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
	auto callFunc = [this, istr]() -> LispObj*
	{
		SetMetaThreadID();
		assert(IsMetaThread());
		return m_CreateFromStreamFunc(*istr);
	};
	auto remainingStackSpace = RemainingStackSpace();
	if (remainingStackSpace > 32768)
		obj = callFunc();
	else
	{
		auto future = std::async(callFunc);
		obj = future.get();
		SetMetaThreadID();
		assert(IsMetaThread());
	}

	assert(obj);
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
	NumbObj() = delete;
	NumbObj(const NumbObj&) = delete;
	NumbObj(NumbObj&&) = delete;

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

struct MakeNumbFunc
{
	using argument_type = Number_t;
	using result_reftype = NumbObj*;
	using result_type = LispRef;

	auto operator()(Number_t v) const -> result_reftype
	{
		return new NumbObj(Number(v));
	}
};

/******************                               *******************/
/****************** class UI64Obj                 *******************/
/******************                               *******************/


class UI64Obj : public LispObj
{
	friend LispRef::LispRef(UInt64 v);
	friend struct MakeUI64Func;

private:
	UI64Obj() = delete;
	UI64Obj(const UI64Obj&) = delete;
	UI64Obj(UI64Obj&&) = delete;

	UI64Obj(UInt64 v): m_Value(v) {}
	~UI64Obj();

	virtual bool   IsUI64()     const { return true; }
	virtual UInt64 GetUI64Val() const { return m_Value; }

	virtual void Print(FormattedOutStream& out, UInt32 level)   const { out << m_Value << "u64 "; }
	static LispObj* ReloadObj(PolymorphInpStream& ar);
	virtual void WriteObj(PolymorphOutStream& ar) const;

	UInt64 m_Value;

	DECL_RTTI(SYM_CALL, LispCls);
};

struct MakeUI64Func
{
	using argument_type = UInt64;
	using result_type = LispRef;
	using result_reftype = UI64Obj*;

	auto operator()(UInt64 v) const -> result_reftype
	{
		return new UI64Obj(v);
	}
};

IMPL_STATIC_LISPCLS(UI64Obj)

/******************                                   *******************/
/****************** class SymbObj                     *******************/
/******************                                   *******************/


class SymbObj : public LispObj
{
	friend struct MakeSymbFunc;
	friend auto GetOrCreateSymbObj(LispCaches* self, TokenID t, ChroID c) -> LispRef;

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

using SymbType = std::pair<TokenID,  ChroID>;

struct MakeSymbFunc
{	
	using argument_type = SymbType;
	using result_type = LispRef;
	using result_reftype = SymbObj*;

	auto operator()(const SymbType& v) const -> result_reftype
	{ 
		return new SymbObj(v.first, v.second); 
	} 
};

/******************                                   *******************/
/****************** class StrnObj                     *******************/
/******************                                   *******************/

struct StrnType: Couple<CharPtr>
{
	StrnType(): Couple<CharPtr>(TokenID::GetEmptyStr(), TokenID::GetEmptyStr()) {}
	StrnType(CharPtr b, CharPtr e): Couple<CharPtr>(b, e) {}
	StrnType(const StrnType& oth) : Couple<CharPtr>(oth)  {}

	SizeT size () const { return second - first;  }
	bool  empty() const { return first == second; } 

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

struct MakeStrnFunc
{
	using argument_type = StrnType;
	using result_type = LispRef;
	using result_reftype = StrnObj*;

	auto operator()(const StrnType& v) const -> result_reftype
	{
		auto len = v.size();
		assert(len); // zero-sized strings are separately provided by StrnObj::Empty()
		char* b = new char[len + 1],
			* e = b + len;
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

typedef std::pair<LispPtr, LispPtr> ListType;

struct MakeListFunc
{	
	using argument_type = ListType ;
	using result_type = LispRef;
	using result_reftype = ListObj*;
	auto operator()(const ListType& v) const -> result_reftype
	{ 
		return new ListObj(v.second, v.first);
	} 
};


/****************** LispComponent *******************/

UInt32 s_LispComponentCount = 0;

struct LispCaches {
	Cache<MakeNumbFunc, DataCompare<Number_t> > NumbObjCache;
	//	Cache<MakeUI64Func, DataCompareImpl<UInt64, false> > UI64ObjCache;
	Cache<MakeUI64Func, std::less<UInt64> > UI64ObjCache;
	Cache<MakeSymbFunc> SymbObjCache;
	Cache<MakeStrnFunc, std::less<StrnType>, const StrnType&, StrnObj* , DuplStrnData> StrnObjCache;
	Cache<MakeListFunc> ListObjCache;

	std::vector<SymbObj*> ZeroSymbObjCache;
	UInt32                nrActiveZeroSymbObj = 0;

	leveled_std_section   CS;

	LispCaches()
		: CS(item_level_type(0), ord_level_type::LispObjCache, "LispObjRegister")
	{
		assert(s_LispComponentCount);
	}

	~LispCaches()
	{
		assert(!s_LispComponentCount);

		assert(NumbObjCache.empty());
		assert(StrnObjCache.empty());
		assert(UI64ObjCache.empty());
		assert(ListObjCache.empty());
		assert(ListObjCache.empty());
		assert(!nrActiveZeroSymbObj);
	}
};

std::mutex sx_TimelessSymbolArrayLock;
auto GetOrCreateSymbObj(LispCaches* self, TokenID t, ChroID c) -> LispRef
{
	if (c)
		return self->SymbObjCache.apply(SymbType(t, c)); // has its own guard

	auto cacheLock = std::lock_guard(sx_TimelessSymbolArrayLock);

	UInt32 tnr = t.GetNr(TokenID::TokenKey());
	UInt32 reqSize = tnr + 1;

	std::size_t s = self->ZeroSymbObjCache.size();
	if (reqSize > s)
	{
		s *= 2;
		MakeMax(s, reqSize);
		self->ZeroSymbObjCache.resize(s, 0);
	}
	SymbObj*& cacheEntry = self->ZeroSymbObjCache[tnr];
	if (cacheEntry)
	{
		auto result = LispRef(LispPtr(cacheEntry), no_zombies{});
		if (result)
			return result;
	}
	cacheEntry = new SymbObj(t, 0);
	self->nrActiveZeroSymbObj++;
	return LispRef(LispPtr(cacheEntry));
}


alignas(LispCaches) Byte s_LispCacheBuffer[sizeof(LispCaches)];

LispCaches* GetLispCachesImpl()
{
	return reinterpret_cast<LispCaches*>(s_LispCacheBuffer);
}

struct LispCachesHandle : ptr_base<LispCaches, noncopyable>
{
	LispCachesHandle(LispCaches* src)
		:	ptr_base<LispCaches, noncopyable>(src)
		,   m_ScopedLock(src->CS)
	{}

	std::scoped_lock<leveled_std_section> m_ScopedLock;
};

auto GetLispCaches() -> LispCachesHandle
{
	assert(s_LispComponentCount);
	return GetLispCachesImpl();
}

LispComponent::LispComponent()
{
	if (!s_LispComponentCount++)
		new (s_LispCacheBuffer) LispCaches;
}

LispComponent::~LispComponent()
{
	if (!--s_LispComponentCount)
		GetLispCachesImpl()->~LispCaches();
}


/****************** NumbObj implementation  *******************/

LispRef::LispRef(Number value)
	: SharedPtrWrap( GetLispCaches()->NumbObjCache.apply(value) )
{}

NumbObj::~NumbObj() { GetLispCaches()->NumbObjCache.remove(m_Value); }

LispObj* NumbObj::ReloadObj(PolymorphInpStream& ar)
{
	DBG_START("NumbObj", "ReloadObj", DBG_LOADOBJ);

	double value;
	ar >> value;
	DBG_TRACE(("numb = %lf", value));
	return GetLispCaches()->NumbObjCache.apply(Number(value));
}

void NumbObj::WriteObj(PolymorphOutStream& ar) const
{
	double value = m_Value;
	ar << value;
}

IMPL_STATIC_LISPCLS(NumbObj)

/****************** UI64Obj implementation  *******************/

LispRef::LispRef(UInt64 value)
	: SharedPtrWrap( GetLispCaches()->UI64ObjCache.apply(value) )
{}

UI64Obj::~UI64Obj() { GetLispCaches()->UI64ObjCache.remove(m_Value); }

/****************** UI64Obj Serialization and rtti *******************/

void UI64Obj::WriteObj(PolymorphOutStream& ar) const
{
	UInt64 value = m_Value;
	ar << value;
}

LispObj* UI64Obj::ReloadObj(PolymorphInpStream& ar)
{
	DBG_START("UI64Obj", "ReloadObj", DBG_LOADOBJ);

	UInt64 value;
	ar >> value;
	DBG_TRACE(("ui64 = %lf", value));

	return GetLispCaches()->UI64ObjCache.apply(value);
}

/****************** SymbObj implementation  *******************/

LispRef::LispRef(TokenID t, UInt32 c)
	: SharedPtrWrap(GetOrCreateSymbObj(GetLispCaches(), t, c))
{}

LispRef::LispRef(CharPtr s, UInt32 c)
	: LispRef(GetTokenID_mt(s), c)
{}

SymbObj::~SymbObj()
{
	auto x = GetLispCaches();
	if (m_ChroID)
		x->SymbObjCache.remove(SymbType(m_TokenID, m_ChroID));
	else
	{
		auto cacheLock = std::lock_guard(sx_TimelessSymbolArrayLock);
		auto tokenNr = m_TokenID.GetNr(TokenID::TokenKey());
		assert(x->ZeroSymbObjCache.size() > tokenNr);
		auto& symbObjPtrRef = x->ZeroSymbObjCache[tokenNr];

		if (symbObjPtrRef == this)
			symbObjPtrRef = nullptr;
		x->nrActiveZeroSymbObj--;
	}
}

void SymbObj::Print(FormattedOutStream& out, UInt32 level) const
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

	return GetOrCreateSymbObj(GetLispCaches(), t, c);
}

void SymbObj::WriteObj(PolymorphOutStream& ar) const
{
	ar.WriteToken(GetSymbID());
	ar.WriteUInt32(m_ChroID);
}

IMPL_STATIC_LISPCLS(SymbObj)

/****************** StrnObj implementation  *******************/

LispRef::LispRef(CharPtr b, CharPtr e)
	: SharedPtrWrap(
		(b == e)
		? StrnObj::GetEmpty()
		: GetLispCaches()->StrnObjCache.apply(StrnType(b, e))
	)
{}

StrnObj::~StrnObj()
{
	if (!m_Data.empty())
	{
		GetLispCaches()->StrnObjCache.remove(m_Data);
		delete[] m_Data.first;
	}
}

IMPL_STATIC_LISPCLS(StrnObj)

StrnObj* StrnObj::GetEmpty()
{
	static StrnObj* empty = new StrnObj();
	static SharedPtr<LispObj> emptyHolder(empty);

	return empty;
}

void StrnObj::Print(FormattedOutStream& o, UInt32 level) const
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
		ar.m_StrnBuf.reset(new Byte[Max<UInt32>(len, 2 * ar.m_StrnBufSize)]);

	Byte
		* b = ar.m_StrnBuf.get(),
		* e = b + len;

	ReadBinRangeImpl(ar, b, e, TYPEID(directcpy_streamable_tag));

#if defined(MG_DEBUG)
	if (debugContext.m_Active)
	{
		DebugOutStream::scoped_lock lock(g_DebugStream);
		g_DebugStream->Buffer().WriteBytes(b, e - b);
	}
#endif

	return GetLispCaches()->StrnObjCache.apply(StrnType(b, e));
}

void StrnObj::WriteObj(PolymorphOutStream& ar) const
{
	ar.WriteUInt32(m_Data.size());
	WriteBinRangeImpl(ar, m_Data.first, m_Data.second, TYPEID(directcpy_streamable_tag));
}

/****************** UI64Obj implementation         *******************/


/****************** ListObj implementation  *******************/

LispRef::LispRef(LispPtr head, LispPtr tail)
	: SharedPtrWrap<LispPtr>(GetLispCaches()->ListObjCache.apply(ListType(tail, head)))
{}

using zombie_destroyer_stack = std::stack<zombie_destroyer>;

void ref_mover(zombie_destroyer_stack& nodes, LispRef& current)
{
	// Release children and push them onto the stack
	if (!current.IsRealList())
		return;
	auto resetHandle = current.delayed_reset();
	if (resetHandle)
		nodes.push(std::move(resetHandle)); // Detach and push
}

thread_local bool s_LispObjStackActive = false;

ListObj::~ListObj() 
{ 
	if (s_LispObjStackActive)
		return;

	s_LispObjStackActive = true;
	std::stack<zombie_destroyer> nodes;

	// No need to reset since release already nullifies the unique_ptrs
	GetLispCaches()->ListObjCache.remove(ListType(m_Right, m_Left));
	ref_mover(nodes, m_Left);
	ref_mover(nodes, m_Right);

	while (!nodes.empty()) {
		auto current = std::move(nodes.top());
		nodes.pop();

		auto currentLispObj = static_cast<ListObj*>(current.get());
		assert(currentLispObj);
		GetLispCaches()->ListObjCache.remove(ListType(currentLispObj->m_Right, currentLispObj->m_Left));
		ref_mover(nodes,currentLispObj->m_Left);
		ref_mover(nodes,currentLispObj->m_Right);
		// delete current
	}
	s_LispObjStackActive = false;
}

/****************** ListObj Serialization and rtti *******************/

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
	if (IsDefined(level))
	{
		++level;
		if (level > LispRef::MAX_PRINT_LEVEL)
		{
			out << "[...]";
			return;
		}
	}
	// Check if right track ends in a EndP
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

LispObj* ListObj::ReloadObj(PolymorphInpStream& ar)
{
	DBG_START("ListObj", "ReloadObj", DBG_LOADOBJ);

	LispRef head, tail;
	ar >> head;
	DBG_TRACE(("head = %s", AsString(head).c_str()));
	ar >> tail;
	DBG_TRACE(("tail = %s", AsString(tail).c_str()));
	return GetLispCaches()->ListObjCache.apply(ListType(tail, head));
}

void ListObj::WriteObj(PolymorphOutStream& ar) const
{
	ar << m_Left << m_Right;
}

/******************                                   *******************/
/****************** Print func                        *******************/
/******************                                   *******************/


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

SharedStr AsFLispSharedStr(LispPtr lispRef, FormattingFlags ff)
{
	// MUTEX, PROTECT EXCLUSIVE USE OF GLOBAL STATIC RESOURCE FOR PERFORMANCE REASONS;
	leveled_critical_section::scoped_lock lock(gc_FLispUsageGuard);
	static ExternalVectorOutStreamBuff::VectorType vector;
	MG_DEBUGCODE( SizeT cap = vector.capacity(); )
	vector.clear();
	dbg_assert(cap == vector.capacity());

	ExternalVectorOutStreamBuff vecBuf(vector);
	FormattedOutStream outStr(&vecBuf, ff);
	lispRef.PrintAsFLisp(outStr, ff == FormattingFlags::ThousandSeparator ? 0 : UNDEFINED_VALUE(UInt32));
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

