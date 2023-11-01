// Copyright (C) 2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "ptr/OwningPtr.h"
#include "ptr/SharedStr.h"

#include "geo/iterrangefuncs.h"
#include "geo/StringBounds.h"

#include <algorithm>

//============================= SharedStr Creators

SharedCharArray* SharedCharArray_Create(CharPtr str)
{
	if (!str)
		return SharedCharArray_CreateUndefined();
	if (!*str)
		return SharedCharArray_CreateEmpty();
	SizeT size = StrLen(str);
	SharedCharArray* result = SharedCharArray::CreateUninitialized(size+1);
	*(fast_copy(str, str+size, result->begin())) = char(0);

	return result;
}

SharedCharArray* SharedCharArray_Create(CharPtr begin, CharPtr end)
{
	if (begin == end)
		return SharedCharArray_CreateEmpty();     // else Empty string

	SharedCharArray* result = SharedCharArray::CreateUninitialized((end-begin)+1);
	*(fast_copy(begin, end, result->begin())) = char(0);

	dms_assert(result->end()[-1] == char(0));
	return result;
}

SharedCharArray* SharedCharArray_Create(TokenID id)
{
	auto range = id.AsStrRange();
	return SharedCharArray_Create(range.m_CharPtrRange.begin(), range.m_CharPtrRange.end());
}

SharedCharArray* SharedCharArray_CreateEmptyImpl()
{
	static SharedCharArray::allocator_i::value_type emptyArrayBuffer[SharedCharArray::NrAllocations(1)];
	auto result = new(emptyArrayBuffer) SharedCharArray(1, 1);
	result->IncRef(); // never destroy again.
	result->begin()[0] = CharType(0);
	return result;
}

SharedCharArray* SharedCharArray_CreateEmpty()
{
	static SharedCharArray* emptyArray = SharedCharArray_CreateEmptyImpl();
	return emptyArray;
}

//============================= SharedCharArray operators

CharPtr SharedCharArrayPtr::find(char ch)                     const { return std::find(begin(), send(), ch); }
CharPtr SharedCharArrayPtr::find(CharPtr first, CharPtr last) const { return Search(CharPtrRange(begin(), send()), CharPtrRange(first, last)); }
CharPtr SharedCharArrayPtr::find(CharPtr str)                 const { return find(str, str+StrLen(str)); }

bool SharedCharArrayPtr::operator < (SharedCharArrayPtr b) const
{
	if (!IsDefined())
		return b.IsDefined();
	if (!b.IsDefined())
		return false;
	return lex_compare(begin(), send(), b.begin(), b.send());
}

bool SharedCharArrayPtr::operator ==(SharedCharArrayPtr b) const
{
	if (!IsDefined())
		return !b.IsDefined();
	if (!b.IsDefined())
		return false;
	return equal(begin(), send(), b.begin(), b.send());
}

bool SharedCharArrayPtr::operator !=(SharedCharArrayPtr b) const
{
	if (!IsDefined())
		return b.IsDefined();
	if (!b.IsDefined())
		return true;
	return !equal(begin(), send(), b.begin(), b.send());
}

bool SharedCharArrayPtr::operator < (CharPtr b) const
{
	if (!::IsDefined(b))
		return false;
	if (!IsDefined())
		return true;
	dms_assert(b && IsDefined());
	if (empty())
		return *b;
	assert(m_Ptr);
	auto sz = m_Ptr->size();
	assert(sz);
	assert(begin()[sz-1] == char(0));
	return strnicmp(begin(), b, sz) < 0;
}

CharPtrRange SharedCharArrayPtr::AsRange() const
{ 
	if (!has_ptr())
		return {};
	return CharPtrRange(get_ptr()->begin(), get_ptr()->end() - 1);
}

bool SharedCharArrayPtr::operator ==(CharPtr b) const
{ 
	if (!IsDefined())
		return !::IsDefined(b);
	if (!::IsDefined(b))
		return false;
	dms_assert(b && IsDefined());
	if (empty())
		return !*b;
	assert(m_Ptr);
	auto sz = m_Ptr->size();
	assert(sz);
	assert(begin()[sz-1] == char(0));
	return strnicmp(begin(), b, sz) == 0;
}

bool SharedCharArrayPtr::operator !=(CharPtr b) const
{ 
	if (!IsDefined())
		return ::IsDefined(b);
	if (!::IsDefined(b))
		return true;
	dms_assert(b && IsDefined());
	if (empty())
		return *b;
	assert(m_Ptr);
	auto sz = m_Ptr->size();
	assert(sz);
	assert(begin()[sz-1] == char(0));
	return strnicmp(begin(), b, sz) != 0;
}

std::string SharedCharArrayPtr::AsStdString() const 
{ 
	auto ptr = get_ptr();
	if (!::IsDefined(ptr))
		return std::string(UNDEFINED_VALUE_STRING, UNDEFINED_VALUE_STRING_LEN);
	assert(ptr);
	assert(ptr->size());
	return std::string(ptr->begin(), ptr->size() - 1);
}

//============================= WeakStr

RTC_CALL WeakStr::operator CharPtrRange() const
{
	return { begin(), send() };
}

//============================= SharedStr mfuncs

#include "geo/SequenceArray.h"
#include "set/Token.h"

SharedStr::SharedStr() noexcept
	: base_type(SharedCharArray_CreateEmpty())
{}

SharedStr::SharedStr(const SA_ConstReference<char>& range)
	:	base_type(SharedCharArray_Create(range.begin(), range.end()) )
{}

SharedStr::SharedStr(MutableCharPtrRange range)
	: base_type(SharedCharArray_Create(range.begin(), range.end())) 
{}

SharedStr::SharedStr(CharPtrRange range) 
	: base_type(SharedCharArray_Create(range.begin(), range.end())) 
{}


SharedStr::SharedStr(TokenID id)
	:	base_type(SharedCharArray_Create(id)) 
{}

SharedStr::SharedStr(const TokenStr& str)
: base_type(SharedCharArray_Create(str.c_str()))
{}

void SharedStr::operator = (const TokenID& id)
{ 
	assign(SharedCharArray_Create(id.GetStr().c_str(), id.GetStrEnd().c_str())); 
}
void SharedStr::operator = (const SA_ConstReference<char>& range)
{ 
	assign(SharedCharArray_Create(range.begin(), range.end())); 
}

void SharedStr::MakeUnique()
{
	if (has_ptr() && get_ptr()->GetRefCount() > 1)
		assign(SharedCharArray_Create(cbegin(), csend()));
	dms_assert(!has_ptr() || get_ptr()->GetRefCount() == 1 || !ssize());
}

MutableCharPtrRange SharedStr::GetAsMutableRange() 
{ 
	MakeUnique(); 
	return MutableCharPtrRange(const_cast<char*>(begin()), const_cast<char*>(send())); 
}

void SharedStr::resize(SizeT sz)
{
	MakeUnique();
	if (sz <= ssize())
	{
		SharedCharArray* sca = GetAsMutableCharArray();
		if (sca)
			sca->erase(sca->begin()+sz, sca->end()-1);
		else
			dms_assert(sz == 0);
	}
	else 
	{
		SharedCharArray* result = SharedCharArray::CreateUninitialized(sz+1);
		assign(result);
		fast_zero(
			fast_copy(begin(), send(), result->begin())
		,	result->end()
		);
	}
}

bool SharedStr::contains(CharPtrRange subStr)
{
	assert(!subStr.empty());
	return std::search(begin(), end(), subStr.first, subStr.second) != end();
}

bool SharedStr::contains_case_insensitive(CharPtrRange subStr)
{
	bool str_contains_substr = std::search(begin(), end(), subStr.first, subStr.second
		, [](unsigned char a, unsigned char b) { 
			return std::tolower(a) == std::tolower(b); 
		}
	) != end();
	return str_contains_substr;
}

void SharedStr::insert(SizeT pos, char ch) 
{
	dms_assert(has_ptr() || !pos);
	SizeT sz = ssize();
	SharedCharArray* sca = SharedCharArray::CreateUninitialized(sz + 2); // 1 ch + 1 termination 0
	dms_assert(sca);
	SharedStr result(sca);
	CharPtr
		first = begin(), 
		mid = first + pos;
	char* r = fast_copy(first, mid, sca->begin());
	*r = ch;
	r = fast_copy(mid, first + sz, ++r); // includes terminating zero
	*r = char(0);
	dms_assert(++r ==sca->end()); // mutates res ONLY in debug mode but this cannot have side effects
	swap(result);
}

RTC_CALL SharedStr SharedStr::replace(CharPtr key, CharPtr val) const
{
	SizeT keyLen = strlen(key);
	SizeT valLen = strlen(val);
	SizeT cnt = 0;
	if (!keyLen)
		return *this;

	auto p = begin();
	while (true) {
		p = Search(CharPtrRange(p, send()), CharPtrRange(key, key + keyLen));
		if (p == send())
			break;
		++cnt;
		p += keyLen;
	}
	if (!cnt)
		return *this;

	SharedCharArray* sca = SharedCharArray::CreateUninitialized(ssize() + 1 + (valLen-keyLen)*cnt);
	SharedStr result(sca);
	dms_assert(sca);
	p = begin();
	char* r = sca->begin();
	while (true) {
		auto np = Search(CharPtrRange(p, send()), CharPtrRange(key, key + keyLen));
		r = fast_copy(p, np, r);
		if (np == send())
			break;
		r = fast_copy(val, val+valLen, r);
		p = np +keyLen;
	}
	*r = char(0);
	dms_assert(++r == sca->end()); // mutates res ONLY in debug mode but this cannot have side effects
	return result;
}

SharedStr::operator CharPtrRange() const
{ 
	return AsRange(); 
}

void SharedStr::clear() 
{ 
	assign(SharedCharArray_CreateEmpty());
}

//============================= SharedStr operators

void SharedStr::operator +=(CharPtrRange rhs)
{
	*this = (*this + rhs);
}

void SharedStr::operator +=(Char rhs)
{
	*this = (*this + rhs);
}

SharedStr operator + (CharPtrRange lhs, CharPtrRange rhs)
{
	if (!lhs.IsDefined()) return SharedStr(Undefined());
	if (!rhs.IsDefined()) return SharedStr(Undefined());

	if (lhs.empty()) return SharedStr(rhs.begin(), rhs.end());
	if (rhs.empty()) return SharedStr(lhs);

	dms_assert(lhs.size() > 0 && rhs.size() > 0);

	SharedCharArray* result = SharedCharArray::CreateUninitialized(lhs.size() + rhs.size()+1);
	SharedStr resultStr(result);

	char* resPtr = fast_copy(lhs.begin(), lhs.end(), result->begin()); // send points to NULL terminator (past string-body)
	resPtr = fast_copy(rhs.begin(), rhs.end(), resPtr); // end is past NULL terminator
	*resPtr = char(0);
	return resultStr;
}

SharedStr operator + (CharPtr lhs, WeakStr rhs)
{
	if (!lhs) return SharedStr(Undefined() );
	if (!rhs.IsDefined()) return rhs;

	if (!*lhs)       return rhs;
	if (rhs.empty()) return SharedStr(lhs);

	dms_assert(rhs.ssize());

	UInt32 lhsSize = StrLen(lhs);
	SharedCharArray* result = SharedCharArray::CreateUninitialized(lhsSize + rhs.get_ptr()->size());
	SharedStr resultStr(result);

	char* resPtr = fast_copy(lhs, lhs+lhsSize, result->begin()); // send points to NULL terminator (past string-body)
	resPtr = fast_copy(rhs.get_ptr()->begin(), rhs.get_ptr()->end(), resPtr); // end is past NULL terminator
	dms_assert(resPtr[-1] == char(0));
	return resultStr;
}

SharedStr operator + (WeakStr lhs, CharPtr rhs)
{
	if (!lhs.IsDefined()) return lhs;
	if (!rhs)             return SharedStr(Undefined());

	if (lhs.empty()) return SharedStr(rhs);
	if (!*rhs)       return lhs;

	dms_assert(lhs.ssize());
	UInt32 rhsSize = StrLen(rhs);
	SharedCharArray* result = SharedCharArray::CreateUninitialized(lhs.get_ptr()->size() + rhsSize);
	SharedStr resultStr(result);

	char* resPtr = fast_copy(lhs.get_ptr()->begin(), lhs.get_ptr()->end()-1, result->begin()); // send points to NULL terminator (past string-body)
	resPtr = fast_copy(rhs, rhs+rhsSize+1, resPtr); // end is past NULL terminator
	dms_assert(resPtr[-1] == char(0));
	return resultStr;
}

SharedStr operator + (CharPtrRange lhs, Char ch)
{
	if (!lhs.IsDefined()) return SharedStr(lhs); // Undefined

	SharedCharArray* result = SharedCharArray::CreateUninitialized(lhs.size() + 2);
	SharedStr resultStr(result);

	char* resPtr = fast_copy(lhs.begin(), lhs.end(), result->begin()); // send points to NULL terminator (past string-body)
	*resPtr = ch;
	*++resPtr = char(0);
	return resultStr;
}

SharedStr operator + (Char ch, CharPtrRange rhs)
{
	if (!rhs.IsDefined()) 
		return SharedStr(Undefined()); // Undefined

	SharedCharArray* result = SharedCharArray::CreateUninitialized(rhs.size() + 2);
	SharedStr resultStr(result);

	char* resPtr = result->begin();
	*resPtr = ch;
	resPtr = fast_copy(rhs.begin(), rhs.end(), resPtr+1); // send points to NULL terminator (past string-body)
	*resPtr = char(0);
	return resultStr;
}

//----------------------------------------------------------------------
// Section      : helper funcs
//----------------------------------------------------------------------

SharedStr substr(WeakStr str, SizeT pos)
{
	if (pos >= str.ssize())
		return SharedStr();
	CharPtr b = str.begin()+pos;
	return SharedStr(b, str.send());
}

SharedStr substr(WeakStr str, SizeT pos, SizeT len)
{
	if (pos+len >= str.ssize())
		return substr(str, pos);

	CharPtr b = str.begin()+pos;
	return SharedStr(b, b+len);
}

//----------------------------------------------------------------------
// Section      : compiler tests
//----------------------------------------------------------------------

#include "ptr/OwningPtrArray.h"

#if !defined(MG_DEBUG)

#include "geo/BitValue.h"

// check expected application of empty base class elimination (of mixin class movable).
static_assert(sizeof(ref_base<char, movable>)== sizeof(ptr_wrap<char, movable>));
static_assert(sizeof(ref_base<Bool, movable>)== sizeof(ptr_wrap<Bool, movable>));

static_assert(sizeof(ptr_wrap<char, movable>)== sizeof(sequence_traits<char>::pointer));
static_assert(sizeof(ptr_wrap<Bool, movable>)== sizeof(sequence_traits<Bool>::pointer));

static_assert(sizeof(sequence_traits<char>::pointer) == sizeof(char*));

#endif

