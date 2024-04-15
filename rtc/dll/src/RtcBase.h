// Copyright (C) 1998-2023 Object Vision b.v. 
// License: GNU GPL 3
/////////////////////////////////////////////////////////////////////////////

// include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if defined(_MSC_VER)
#pragma once
#endif

#if !defined(__RTC_CALL_H)
#define __RTC_CALL_H

//----------------------------------------------------------------------
// Trigger MG_DEBUG mode
//----------------------------------------------------------------------

#if defined( _DEBUG )
#	define MG_DEBUG
// // NOW ALSO DEBUG RELEASE
// #else
// #	define MG_DEBUG 
#endif


//----------------------------------------------------------------------
// avoid horrible Windows min & max definitions
//----------------------------------------------------------------------

#define NOMINMAX

//----------------------------------------------------------------------
// Used modules
//----------------------------------------------------------------------
#define BOOST_MPL_LIMIT_VECTOR_SIZE 50

#include "cpc/CompChar.h"
#include "cpc/Types.h"

using dms_thread_id = UInt32;

#include <memory>
#include <vector>

//----------------------------------------------------------------------
// BitValue support
//----------------------------------------------------------------------

using bit_size_t = int;
using bit_block_t = UInt32 ;

template <bit_size_t N> struct bit_value;
template <bit_size_t N, typename Block> struct bit_reference;

using Bool = bit_value<1>;
using UInt2 = bit_value<2>;
using UInt4 = bit_value<4>;

template <bit_size_t N, typename Block = bit_block_t, typename Allocator = std::allocator<Block> >
struct BitVector;

template <bit_size_t N, typename Block = bit_block_t>
struct bit_iterator;

template <bit_size_t N, typename Block = bit_block_t>
struct bit_info;

template <bit_size_t N, typename Block = bit_block_t>
struct bit_sequence;

//----------------------------------------------------------------------
// C style Interface DLL export support 
//----------------------------------------------------------------------

#if defined(_MSC_VER)
#	define DMS_CONV __cdecl
#else
#	define DMS_CONV
#	undef DMRTC_EXPORTS
#	define DMRTC_STATIC
#endif

#if defined(DMRTC_EXPORTS)
#	define RTC_CALL __declspec(dllexport)
#else
#	if defined(DMRTC_STATIC)
#		define RTC_CALL
#	else
#		define RTC_CALL __declspec(dllimport)
#	endif
#endif

//----------------------------------------------------------------------
// CONDITIONAL DEFINES FOR SPECIFIC DEBUGGING
//----------------------------------------------------------------------
// DEBUG

#if defined(MG_DEBUG)

// Defines for determining the debug level

#	if defined(_MSC_VER)
#		define MG_CRTLOG
#	else
#		undef MG_CRTLOG
#	endif

#	define MG_DEBUG_DATA
//#	define MG_DEBUG_UPDATESOURCE
//#	define MG_DEBUG_ALLOCATOR
#	define MG_DEBUG_INTERESTSOURCE 
#	define MG_DEBUG_DATASTORELOCK


// Defines for keeping obsolete functions
//#   define MG_OBSOLETE

#else
//#	define MG_DEBUG_INTERESTSOURCE 
#	undef MG_DEBUG_DATA
#endif

#if defined(MG_DEBUG_INTERESTSOURCE) || defined(MG_DEBUG_ALLOCATOR)
#	define MG_DEBUGREPORTER
#	define MG_DEBUG_INTERESTSOURCE_LOGGING
#	define MG_DEBUG_DATA
#   define MG_UNIT_TESTING
#else
#	undef MG_DEBUG_INTERESTSOURCE_LOGGING
#endif

#if defined(MG_DEBUG)
#	define release_protected public
#	define release_private public
#else
#	define release_protected protected
#	define release_private private
#endif

#if defined(MG_DEBUG_DATASTORELOCK)
#	define MG_DEBUG_SOURCE_INFO
#endif

#if defined(MG_DEBUG_SOURCE_INFO)
#	define MG_SOURCE_INFO_CODE(x) x,
#	define MG_SOURCE_INFO_DECL  CharPtr srcInfo, 
#	define MG_SOURCE_INFO_USE   srcInfo, 
#else
#	define MG_SOURCE_INFO_DECL
#	define MG_SOURCE_INFO_USE 
#	define MG_SOURCE_INFO_CODE(x)
#endif
//----------------------------------------------------------------------
// Forward references 
//----------------------------------------------------------------------

class  Object;
class  AbstrValue;
class  AbstrPropDef;
class  Class;
class  ValueClass;
class  SharedObj;
class  PersistentSharedObj;
using zombie_destroyer = std::unique_ptr<SharedObj>;

struct IString;
struct Undefined;

enum   class SeverityTypeID  : UInt8;
enum   class ValueClassID    : UInt8;
enum   class ValueComposition: UInt8;
enum   class dms_rw_mode     : Int8; // can have negative values

struct SafeFileWriterArray;

//----------------------------------------------------------------------
// StreamBuff types
//----------------------------------------------------------------------

using streamsize_t = std::size_t;

struct BinaryInpStream;
struct BinaryOutStream;
struct PolymorphInpStream;
struct PolymorphOutStream;
struct FormattedInpStream;
struct FormattedOutStream;
struct OutStreamBase;

class OutStreamBuff;
	class FileOutStreamBuff;
	class MemoOutStreamBuff;
	class VectorOutStreamBuff;
	class CallbackOutStreamBuff;
		typedef void* ClientHandle;
		typedef void (DMS_CONV *CallbackStreamFuncType)(ClientHandle clientHandle, const Byte* data, streamsize_t size);

const int utf8CP = 65001;
using vos_buffer_type = std::vector<Byte>;

//----------------------------------------------------------------------
// Common typedefs
//----------------------------------------------------------------------

typedef const Class*   ClassCPtr;
typedef const IString* IStringHandle;
struct TokenID;
struct TokenStr;

//----------------------------------------------------------------------
// Callback function typedefs
//----------------------------------------------------------------------

typedef void (DMS_CONV *TCppExceptionTranslator)(CharPtr msg);

//----------------------------------------------------------------------
// C style enumeration types for PropDefs
//----------------------------------------------------------------------

enum class xml_mode;
enum class set_mode;
enum class cpy_mode;
enum class chg_mode;

//=======================================
// scattered traits
//=======================================

template<typename T> struct minmax_traits;
template<typename T> T MinValue();
template<typename T> T MaxValue();

//=======================================
// forward reference
//=======================================

template <typename T> struct Range;
template <typename T> struct Point;
template <typename T, typename U> struct Pair;
template <typename T> struct Couple;
template <typename T> struct IndexRange;

template <typename T> struct elem_traits;
template <typename T> struct sequence_traits;
template <typename V> struct sequence_obj;
template <typename T> struct sequence_array;
template <typename T> struct sequence_vector;
template <typename T> struct sequence_array_ref;
template <typename T> struct sequence_array_cref;
template <typename T> struct SA_ConstReference;
template <typename T> struct SA_Reference;
template <typename T> struct SA_ConstIterator;
template <typename T> struct SA_Iterator;
template <typename Iter> struct IterRange;
template <typename T> class abstr_sequence_provider;

//=======================================
// specific sequence_arrays
//=======================================

typedef sequence_array <char> StringArray;
typedef sequence_vector<char> StringVector;

typedef SA_ConstReference<char> StringCRef;
typedef SA_Reference     <char> StringRef;

#define SEQ_SUFFIX "seq.dmsdata" 

using tile_id = UInt32;
using tile_offset = UInt32;
using row_id = UInt64;
using tile_loc = std::pair<tile_id, tile_offset>;

const tile_id no_tile   = -1;
const tile_id all_tiles = -2;

const UInt8 log2_default_tile_size = 8;
const UInt8 log2_default_segment_size = log2_default_tile_size * 2;

//=======================================
// RefCounted pointers to objects or arrays
//=======================================

template <typename T, typename CTorBase> struct ptr_base;
template <typename T, typename CTorBase> struct ref_base;

template <typename T> struct WeakPtr;
template <typename T> struct SharedPtr;
template <typename T> struct SharedArray;
template <typename T> struct OwningPtr;
template <typename T> struct OwningPtrSizedArray;
template <typename P> struct InterestPtr;
typedef char                  CharType;
typedef SharedArray<CharType> SharedCharArray;
struct SharedStr;
struct WeakStr;

namespace std {
	template <typename T> class shared_ptr;
	template <typename T> class weak_ptr;
}

//======================================= pointer destillation

struct TileBase;
using TileRef = SharedPtr < SharedObj >;
using TileCRef = SharedPtr < const SharedObj >;

//----------------------------------------------------------------------
// metafunc : pointer_traits
//----------------------------------------------------------------------

template <typename T> struct pointer_traits_helper {
	typedef T value_type; 
	typedef T* ptr_type; 
	typedef T& ref_type; 
	static T* get_ptr(T* ptr) { return ptr; }
//	const T* get_ptr(const T* ptr) { return ptr; }
};

template <typename P> struct pointer_traits;
template <typename T> struct pointer_traits<T*>             : pointer_traits_helper<T> {};
template <typename T> struct pointer_traits<SharedPtr<T>  > : pointer_traits_helper<T> {};
template <typename T> struct pointer_traits<OwningPtr<T>  > : pointer_traits_helper<T> {};
template <typename T> struct pointer_traits<WeakPtr  <T>  > : pointer_traits_helper<T> {};
template <typename P> struct pointer_traits<InterestPtr<P>> : pointer_traits<P> {};
template <typename T> struct pointer_traits<std::shared_ptr<T>  > : pointer_traits_helper<T> {
	static T* get_ptr(const std::shared_ptr<T>& ptr) { return ptr.get(); }
};
template <typename T> struct pointer_traits<std::weak_ptr<T>  > : pointer_traits_helper<T> {};

template <typename P> struct raw_ptr { using type = typename pointer_traits<P>::ptr_type; };

//=======================================
// common std declarations
//=======================================

namespace std
{ 
//	template <typename V> class allocator;
//	template <typename V, typename Alloc = std::allocator<V>> class vector;
	template<class T> struct less;
	template <typename F> class function;
}


extern bool RTC_CALL g_IsTerminating;


#endif // __RTC_CALL_H
