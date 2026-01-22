// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

#include "RtcPCH.h"

#if defined(CC_PRAGMAHDRSTOP)
#pragma hdrstop
#endif //defined(CC_PRAGMAHDRSTOP)

#include "act/MainThread.h"
#include "ptr/OwningPtr.h"
#include "ptr/SharedStr.h"

#include "geo/iterrangefuncs.h"
#include "geo/StringBounds.h"

#include <algorithm>

//============================= SharedStr Creators

SharedCharArray* SharedCharArray_Create(CharPtr str MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	if (!str)
		return SharedCharArray_CreateUndefined();
	if (!*str)
		return SharedCharArray_CreateEmpty();
	SizeT size = StrLen(str);
	SharedCharArray* result = SharedCharArray::CreateUninitialized(size+1 MG_DEBUG_ALLOCATOR_SRC_PARAM);
	*(fast_copy(str, str+size, result->begin())) = char(0);

	return result;
}

SharedCharArray* SharedCharArray_Create(CharPtr begin, CharPtr end MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	if (begin == end)
		return SharedCharArray_CreateEmpty();     // else Empty string

	SharedCharArray* result = SharedCharArray::CreateUninitialized((end-begin)+1 MG_DEBUG_ALLOCATOR_SRC_PARAM);
	*(fast_copy(begin, end, result->begin())) = char(0);

	assert(result->end()[-1] == char(0));
	return result;
}

SharedCharArray* SharedCharArray_Create(TokenID id MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	RequestMainThreadOperProcessingBlocker saveNotificationAfterAssignment;

	auto range = id.AsStrRange();
	return SharedCharArray_Create(range.m_CharPtrRange.begin(), range.m_CharPtrRange.end() MG_DEBUG_ALLOCATOR_SRC_PARAM);
}

SharedCharArray* SharedCharArray_CreateEmptyImpl()
{
	static SharedCharArray::allocator_i::value_type emptyArrayBuffer[SharedCharArray::NrAllocations(1)];
	auto result = new(emptyArrayBuffer) SharedCharArray(1, 1);
	result->AdoptRef(); // never destroy again.
	result->begin()[0] = CharType(0);
	return result;
}

SharedCharArray* SharedCharArray_CreateEmpty()
{
	static SharedCharArray* emptyArray = SharedCharArray_CreateEmptyImpl();
	return emptyArray;
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
	: base_type(SharedCharArray_CreateEmpty(), no_zombies{})
{}

SharedStr::SharedStr(const SA_ConstReference<char>& range MG_DEBUG_ALLOCATOR_SRC_ARG)
	:	base_type(SharedCharArray_Create(range.begin(), range.end() MG_DEBUG_ALLOCATOR_SRC_PARAM), newly_obj{})
{}

SharedStr::SharedStr(MutableCharPtrRange range MG_DEBUG_ALLOCATOR_SRC_ARG)
	: base_type(SharedCharArray_Create(range.begin(), range.end() MG_DEBUG_ALLOCATOR_SRC_PARAM), newly_obj{})
{}

SharedStr::SharedStr(CharPtrRange range MG_DEBUG_ALLOCATOR_SRC_ARG) 
	: base_type(SharedCharArray_Create(range.begin(), range.end() MG_DEBUG_ALLOCATOR_SRC_PARAM), newly_obj{})
{}

SharedStr::SharedStr(TokenID id MG_DEBUG_ALLOCATOR_SRC_ARG)
	:	base_type(SharedCharArray_Create(id MG_DEBUG_ALLOCATOR_SRC_PARAM), newly_obj{})
{}

SharedStr::SharedStr(const TokenStr& str MG_DEBUG_ALLOCATOR_SRC_ARG)
: base_type(SharedCharArray_Create(str.c_str() MG_DEBUG_ALLOCATOR_SRC_PARAM), newly_obj{})
{}

void SharedStr::operator = (const TokenID& id)
{ 
	reset(SharedCharArray_Create(id.GetStr().c_str(), id.GetStrEnd().c_str() MG_DEBUG_ALLOCATOR_SRC("SharedStr::operator = ")));
}
void SharedStr::operator = (const SA_ConstReference<char>& range)
{ 
	reset(SharedCharArray_Create(range.begin(), range.end() MG_DEBUG_ALLOCATOR_SRC("SharedStr::operator = ")));
}

void SharedStr::MakeUnique()
{
	if (has_ptr() && get_ptr()->GetRefCount() > 1)
		reset(SharedCharArray_Create(cbegin(), csend() MG_DEBUG_ALLOCATOR_SRC("SharedStr::MakeUnique ")));
	assert(!has_ptr() || get_ptr()->GetRefCount() == 1 || !ssize());
}

MutableCharPtrRange SharedStr::GetAsMutableRange() 
{ 
	MakeUnique(); 
	return MutableCharPtrRange(const_cast<char*>(begin()), const_cast<char*>(send())); 
}

void SharedStr::resize(SizeT sz MG_DEBUG_ALLOCATOR_SRC_ARG)
{
	MakeUnique();
	if (sz <= ssize())
	{
		SharedCharArray* sca = GetAsMutableCharArray();
		if (sca)
			sca->erase(sca->begin()+sz, sca->end()-1);
		else
			assert(sz == 0);
	}
	else 
	{
		SharedCharArray* result = SharedCharArray::CreateUninitialized(sz+1 MG_DEBUG_ALLOCATOR_SRC_PARAM);
		reset(result);
		fast_zero(
			fast_copy(begin(), send(), result->begin())
		,	result->end()
		);
	}
}

bool SharedStr::contains(CharPtrRange subStr) const
{
	assert(!subStr.empty());
	return std::search(begin(), end(), subStr.first, subStr.second) != end();
}

bool SharedStr::contains_case_insensitive(CharPtrRange subStr) const
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
	assert(has_ptr() || !pos);
	SizeT sz = ssize();
	SharedCharArray* sca = SharedCharArray::CreateUninitialized(sz + 2 MG_DEBUG_ALLOCATOR_SRC("SharedStr::insert")); // 1 ch + 1 termination 0
	assert(sca);
	SharedStr result(sca);
	CharPtr
		first = begin(), 
		mid = first + pos;
	char* r = fast_copy(first, mid, sca->begin());
	*r = ch;
	r = fast_copy(mid, first + sz, ++r); // includes terminating zero
	*r = char(0);
	assert(++r ==sca->end()); // mutates res ONLY in debug mode but this cannot have side effects
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

	SharedCharArray* sca = SharedCharArray::CreateUninitialized(ssize() + 1 + (valLen-keyLen)*cnt MG_DEBUG_ALLOCATOR_SRC("SharedStr::replace"));
	SharedStr result(sca);
	assert(sca);
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
	assert(++r == sca->end()); // mutates res ONLY in debug mode but this cannot have side effects
	return result;
}

SharedStr::operator CharPtrRange() const
{ 
	return AsRange(); 
}

void SharedStr::clear() 
{ 
	reset(SharedCharArray_CreateEmpty());
}

//============================= SharedStr operators

SharedStr operator + (CharPtrRange lhs, CharPtrRange rhs)
{
	if (!lhs.IsDefined()) return SharedStr(Undefined());
	if (!rhs.IsDefined()) return SharedStr(Undefined());

	if (lhs.empty()) return SharedStr(CharPtrRange(rhs.begin(), rhs.end()));
	if (rhs.empty()) return SharedStr(lhs);

	assert(lhs.size() > 0 && rhs.size() > 0);

	SharedCharArray* result = SharedCharArray::CreateUninitialized(lhs.size() + rhs.size()+1 MG_DEBUG_ALLOCATOR_SRC("operator +"));
	SharedStr resultStr(result);

	char* resPtr = fast_copy(lhs.begin(), lhs.end(), result->begin()); // send points to NULL terminator (past string-body)
	resPtr = fast_copy(rhs.begin(), rhs.end(), resPtr); // end is past NULL terminator
	*resPtr = char(0);
	return resultStr;
}

SharedStr operator + (CharPtr lhs, WeakStr rhs)
{
	if (!lhs) 
		return SharedStr(Undefined() );
	if (!rhs.IsDefined()) 
		return rhs;

	if (!*lhs)       
		return rhs;
	if (rhs.empty()) 
		return SharedStr(lhs MG_DEBUG_ALLOCATOR_SRC("operator +"));

	assert(rhs.ssize());

	UInt32 lhsSize = StrLen(lhs);
	SharedCharArray* result = SharedCharArray::CreateUninitialized(lhsSize + rhs.get_ptr()->size() MG_DEBUG_ALLOCATOR_SRC("operator +"));
	SharedStr resultStr(result);

	char* resPtr = fast_copy(lhs, lhs+lhsSize, result->begin()); // send points to NULL terminator (past string-body)
	resPtr = fast_copy(rhs.get_ptr()->begin(), rhs.get_ptr()->end(), resPtr); // end is past NULL terminator
	assert(resPtr[-1] == char(0));
	return resultStr;
}

SharedStr operator + (WeakStr lhs, CharPtr rhs)
{
	if (!lhs.IsDefined()) return lhs;
	if (!rhs)             return SharedStr(Undefined());

	if (lhs.empty()) return SharedStr(rhs MG_DEBUG_ALLOCATOR_SRC("operator +"));
	if (!*rhs)       return lhs;

	assert(lhs.ssize());
	UInt32 rhsSize = StrLen(rhs);
	SharedCharArray* result = SharedCharArray::CreateUninitialized(lhs.get_ptr()->size() + rhsSize MG_DEBUG_ALLOCATOR_SRC("operator +"));
	SharedStr resultStr(result);

	char* resPtr = fast_copy(lhs.get_ptr()->begin(), lhs.get_ptr()->end()-1, result->begin()); // send points to NULL terminator (past string-body)
	resPtr = fast_copy(rhs, rhs+rhsSize+1, resPtr); // end is past NULL terminator
	assert(resPtr[-1] == char(0));
	return resultStr;
}


SharedStr operator + (CharPtrRange lhs, Char ch)
{
	if (!lhs.IsDefined()) 
		return SharedStr(lhs); // Undefined

	SharedCharArray* result = SharedCharArray::CreateUninitialized(lhs.size() + 2 MG_DEBUG_ALLOCATOR_SRC("operator +"));
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

	SharedCharArray* result = SharedCharArray::CreateUninitialized(rhs.size() + 2 MG_DEBUG_ALLOCATOR_SRC("operator +"));
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
	return SharedStr(CharPtrRange(b, str.send()));
}

SharedStr substr(WeakStr str, SizeT pos, SizeT len)
{
	if (pos+len >= str.ssize())
		return substr(str, pos);

	CharPtr b = str.begin()+pos;
	return SharedStr(CharPtrRange(b, b+len));
}

//----------------------------------------------------------------------
// Section      : equality and hashers 
//----------------------------------------------------------------------

//#include <string_view>
//#include <cstddef>
//#include <cctype>

static char32_t ascii_case_fold(char32_t cp) noexcept {
	// Only ASCII a-zA-Z for now; you can extend with Unicode tables
	if (cp >= U'A' && cp <= U'Z') return cp + 32;
	return cp;
}

static size_t decode_utf8(const char* ptr, const char* end, char32_t& out) noexcept {
	unsigned char c = static_cast<unsigned char>(*ptr);
	if (c < 0x80) {
		out = c;
		return 1;
	}
	else if ((c >> 5) == 0x6 && ptr + 1 < end) { // 2-byte
		out = ((c & 0x1F) << 6) | (static_cast<unsigned char>(ptr[1]) & 0x3F);
		return 2;
	}
	else if ((c >> 4) == 0xE && ptr + 2 < end) { // 3-byte
		out = ((c & 0x0F) << 12) |
			((static_cast<unsigned char>(ptr[1]) & 0x3F) << 6) |
			(static_cast<unsigned char>(ptr[2]) & 0x3F);
		return 3;
	}
	else if ((c >> 3) == 0x1E && ptr + 3 < end) { // 4-byte
		out = ((c & 0x07) << 18) |
			((static_cast<unsigned char>(ptr[1]) & 0x3F) << 12) |
			((static_cast<unsigned char>(ptr[2]) & 0x3F) << 6) |
			(static_cast<unsigned char>(ptr[3]) & 0x3F);
		return 4;
	}
	else {
		out = 0xFFFD; // replacement char
		return 1;
	}
}

bool Utf8CaseInsensitiveEqual::operator()(CharPtrRange a, CharPtrRange b) const noexcept {
	if (a.size() != b.size())
		return false;

	const char* p1 = a.first;
	const char* p2 = b.first;
	const char* end1 = a.second;
	const char* end2 = b.second;

	while (p1 < end1 && p2 < end2) {
		char32_t cp1, cp2;
		size_t n1 = decode_utf8(p1, end1, cp1);
		size_t n2 = decode_utf8(p2, end2, cp2);
		p1 += n1;
		p2 += n2;

		if (ascii_case_fold(cp1) != ascii_case_fold(cp2)) {
			return false;
		}
	}

	return (p1 == end1 && p2 == end2); // both must end at same time
}
 
std::size_t Utf8CaseInsensitiveHasher::operator()(CharPtrRange input) const noexcept {
	std::size_t hash = 0xcbf29ce484222325;
	constexpr std::size_t prime = 0x100000001b3;

	const char* ptr = input.first;
	const char* end = input.second;

	// Decode UTF-8 one codepoint at a time
	while (ptr < end) {
		char32_t cp;
		size_t bytes = decode_utf8(ptr, end, cp);
		ptr += bytes;

		// Apply simple case fold (ASCII only for now)
		cp = ascii_case_fold(cp);

		// Update hash
		hash ^= static_cast<std::size_t>(cp);
		hash *= prime;
	}

	return hash;
}

#include <immintrin.h>
#include <cstdint>

using chunk_t = __m128i;

// Fold ASCII uppercase letters in a 16-byte __m128i chunk
inline __m128i fold_ascii_uppercase(chunk_t chunk) noexcept {
	// Set up constants
	const __m128i A = _mm_set1_epi8('A'); // 0x41
	const __m128i Z = _mm_set1_epi8('Z'); // 0x5A
	const __m128i AZ_range = _mm_set1_epi8('Z' - 'A' + 1); // 26
	const __m128i lowercase_bit = _mm_set1_epi8(0x20);     // bit 5

	// Compute: mask = (chunk - 'A') < 26
	__m128i shifted = _mm_subs_epu8(chunk, A);              // saturate at 0
	__m128i is_upper = _mm_cmplt_epi8(shifted, AZ_range);   // signed < 26

	// Apply lowercase folding where mask is set
	return _mm_or_si128(chunk, _mm_and_si128(is_upper, lowercase_bit));
}


inline bool chunks_equal(__m128i a, __m128i b) noexcept {
	__m128i cmp = _mm_cmpeq_epi8(a, b);             // returns 0xFF in each byte if equal
	return _mm_movemask_epi8(cmp) == 0xFFFF;        // all 16 bytes must be equal
}

inline __m128i load_tail(const char* ptr, std::size_t size) noexcept {
	alignas(16) char buffer[16] = {};  // zero-initialized
	std::memcpy(buffer, ptr, size);   // copies only the valid tail
	return _mm_load_si128(reinterpret_cast<const __m128i*>(buffer));
}

RTC_CALL bool GenericEqual::operator()(CharPtrRange a, CharPtrRange b) const noexcept
{
	auto aSize = a.size();
	if (aSize != b.size())
		return false;

	while (aSize >= sizeof(chunk_t)) {
		chunk_t chunkA, chunkB;
		chunkA = _mm_loadu_si128(reinterpret_cast<const chunk_t*>(a.first));
		chunkB = _mm_loadu_si128(reinterpret_cast<const chunk_t*>(b.first));

		if (!chunks_equal(chunkA, chunkB))
			return false;
		a.first += sizeof(chunk_t);
		b.first += sizeof(chunk_t);
		aSize -= sizeof(chunk_t);
	}

	if (aSize > 0) {
		chunk_t chunkA = load_tail(a.first, aSize);
		chunk_t chunkB = load_tail(b.first, aSize);
		if (!chunks_equal(chunkA, chunkB))
			return false;
	}
	return true;
}

RTC_CALL bool AsciiFoldedCaseInsensitiveEqual::operator()(CharPtrRange a, CharPtrRange b) const noexcept
{
	auto aSize = a.size();
	if (aSize != b.size())
		return false;

	while (aSize >= sizeof(chunk_t)) {
		chunk_t chunkA, chunkB;
		chunkA = _mm_loadu_si128(reinterpret_cast<const chunk_t*>(a.first));
		chunkB = _mm_loadu_si128(reinterpret_cast<const chunk_t*>(b.first));
		chunkA = fold_ascii_uppercase(chunkA);
		chunkB = fold_ascii_uppercase(chunkB);

		if (!chunks_equal(chunkA, chunkB))
			return false;
		a.first += sizeof(chunk_t);
		b.first += sizeof(chunk_t);
		aSize -= sizeof(chunk_t);
	}

	if (aSize > 0) {
		chunk_t chunkA = fold_ascii_uppercase(load_tail(a.first, aSize));
		chunk_t chunkB = fold_ascii_uppercase(load_tail(b.first, aSize));
		if (!chunks_equal(chunkA, chunkB))
			return false;
	}
	return true;
}

static std::size_t avalanche(std::size_t h) noexcept {
	// David Stafford's mix13 (variant of Murmur3 finalizer)
	h ^= h >> 33;
	h *= 0xff51afd7ed558ccd;
	h ^= h >> 33;
	h *= 0xc4ceb9fe1a85ec53;
	h ^= h >> 33;
	return h;
}

void hash_in(std::size_t& hash, __m128i v)
{
	std::uint64_t low = static_cast<std::uint64_t>(_mm_extract_epi64(v, 0));
	std::uint64_t high = static_cast<std::uint64_t>(_mm_extract_epi64(v, 1));
	hash ^= low;
	hash ^= high;
}

std::size_t GenericHasher::operator()(CharPtrRange str) const noexcept {
	const char* ptr = str.first;
	const char* end = str.second;

	std::size_t hash = 0; // 0xcbf29ce484222325; // FNV-1a offset basis
//	constexpr std::size_t prime = 0x0305070b0d111317; // swirl prime for 8-byte chunks

	// Process full word chunks
	while (end - ptr >= sizeof(chunk_t)) {
		chunk_t chunk = _mm_loadu_si128(reinterpret_cast<const chunk_t*>(ptr));
		hash_in(hash, chunk);
		ptr += sizeof(chunk_t);
	}

	// Process tail bytes
	UInt32 tailSize = end - ptr;
	if (tailSize > 0) {
		chunk_t chunk = load_tail(ptr, tailSize);
		hash_in(hash, chunk);
	}
	return avalanche(hash);
}

std::size_t AsciiFoldedChunkedCaseInsensitiveHasher::operator()(CharPtrRange str) const noexcept {
	const char* ptr = str.first;
	const char* end = str.second;

	std::size_t hash = 0; // 0xcbf29ce484222325; // FNV-1a offset basis
	//	constexpr std::size_t prime = 0x0305070b0d111317; // swirl prime for 8-byte chunks

		// Process full word chunks
	while (end - ptr >= sizeof(chunk_t)) {
		chunk_t chunk = _mm_loadu_si128(reinterpret_cast<const chunk_t*>(ptr));
		hash_in(hash, fold_ascii_uppercase(chunk));
		ptr += sizeof(chunk_t);
	}

	// Process tail bytes
	UInt32 tailSize = end - ptr;
	if (tailSize > 0) {
		chunk_t chunk = load_tail(ptr, tailSize);
		hash_in(hash, fold_ascii_uppercase(chunk));
	}
	return avalanche(hash);
}

size_t SharedStr::cs_hasher::operator()(const SharedStr& str) const noexcept
{
	CharPtrRange::hasher csHasherFunc;
	return csHasherFunc(CharPtrRange{ str.begin(), str.send() } );
}

size_t SharedStr::ci_hasher::operator()(const SharedStr& str) const noexcept
{
	AsciiFoldedChunkedCaseInsensitiveHasher ciHasherFunc;
	return ciHasherFunc(CharPtrRange{ str.begin(), str.send() });
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

