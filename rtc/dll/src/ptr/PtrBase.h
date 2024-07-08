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
#pragma once

#if !defined(__PTR_PTR_BASE_H)
#define __PTR_PTR_BASE_H

#include <boost/noncopyable.hpp>

#include "RtcBase.h"
#include "dbg/Check.h"
//#include "geo/SequenceTraits.h"
#include "set/BitVector.h"

//  -----------------------------------------------------------------------
// movable (kind of boost::noncopyable
//  -----------------------------------------------------------------------

using noncopyable = boost::noncopyable;

struct copyable {};

struct movable : noncopyable
{
protected:
	movable() noexcept {}
	~movable() noexcept {}

private:   // emphasize that the following members should not be default generated to favor move operator with rvalue references
	void operator = (movable&);
	movable(movable&);
};


// ============================
// ptr_wrap acts as a base class for smart pointers, providing elementary operations that are
// - non mutating
// - not providing access to the pointee
// which can be defined by derived classes in various ways
// ============================

template <typename T, typename CTorBase>
struct ptr_wrap : CTorBase
{
	typedef typename sequence_traits<T>::pointer pointer;
	typedef typename sequence_traits<T>::reference reference;
	typedef typename sequence_traits<T>::const_pointer const_pointer;
	typedef typename sequence_traits<T>::const_reference const_reference;
	using value_type = T;

	ptr_wrap(pointer ptr = pointer() ): m_Ptr(ptr) {}

	bool operator <  (pointer right) const { return m_Ptr <  right; }
	bool operator == (pointer right) const { return m_Ptr == right; }
	bool operator != (pointer right) const { return m_Ptr != right; }

	bool has_ptr() const { return m_Ptr != nullptr; }
	bool is_null() const { return m_Ptr == nullptr; }

	pointer   get_ptr() const { return m_Ptr; } // TODO G8: REMOVE and replace by get()
	pointer   get() const { return m_Ptr; }
	reference get_ref() const { MG_CHECK(m_Ptr != nullptr); return *m_Ptr; }

protected:
	pointer m_Ptr = nullptr;
};


// ============================
// ptr_base grants debug-checked access to the pointee by
// -	type conversion
// -	dereference operator 
// -	access operator
// constness: pointer semantics, so a const ptr cannot be reassigned but non-const access to the pointee can be granted
// ============================

template <class T, typename CTorBase>
struct ptr_base : ptr_wrap<T, CTorBase>
{
	using base_type = ptr_wrap<T, CTorBase>;

	using typename base_type::pointer;
	using typename base_type::reference;

	ptr_base(pointer ptr = pointer() ): base_type(ptr) {}

	operator pointer () const { return this->m_Ptr; }

	pointer   operator ->() const { return &this->get_ref(); }
	reference operator * () const { return  this->get_ref(); }

	void swap(ptr_base& oth) { std::swap(this->m_Ptr, oth.m_Ptr); }
};

// ============================
// ref_base grants debug-checked access to the pointee by
// -   
// -	dereference operator 
// -	access operator
// constness: value semantics, so a const ptr cannot be reassigned and can only grant const access to the pointee
// ============================

template <class T, typename CTorBase>
struct ref_base : ptr_wrap<T, CTorBase>
{
	using typename ptr_wrap<T, CTorBase>::pointer;
	using typename ptr_wrap<T, CTorBase>::reference;
	using typename ptr_wrap<T, CTorBase>::const_pointer;
	using typename ptr_wrap<T, CTorBase>::const_reference;

	ref_base(pointer ptr = pointer() ): ptr_wrap<T, CTorBase>(ptr) {}

	explicit operator bool () const { return this->has_ptr(); }

	pointer         operator ->()       { return &this->get_ref(); }
	const_pointer   operator ->() const { return &this->get_ref(); }
	reference       operator * ()       { return  this->get_ref(); }
	const_reference operator * () const { return  this->get_ref(); }

	void swap(ref_base& oth) { std::swap(this->m_Ptr, oth.m_Ptr); }
};


//  -----------------------------------------------------------------------
// Serialization support 
//  -----------------------------------------------------------------------

struct PolymorphOutStream;

template <typename T, typename CTorBase> inline
PolymorphOutStream& operator <<(PolymorphOutStream& ar, const ptr_base<T, CTorBase>& rPtr)
{
	ar << rPtr.get_ptr();
	return ar;
}

//  -----------------------------------------------------------------------

#endif // __PTR_PTR_BASE_H
