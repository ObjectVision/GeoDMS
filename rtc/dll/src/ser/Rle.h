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

#if !defined(__RTC_SET_RLE_H)
#define __RTC_SET_RLE_H


// *****************************************************************************
//									rle structures
// *****************************************************************************

template <typename T> struct rle_traits {};
template <> struct rle_traits<UInt32> { typedef Int32 count_type; };
template <> struct rle_traits< Int32> { typedef Int32 count_type; };
template <> struct rle_traits<UInt16> { typedef Int16 count_type; };
template <> struct rle_traits< Int16> { typedef Int16 count_type; };
template <> struct rle_traits<UInt8>  { typedef Int16 count_type; };
template <> struct rle_traits< Int8>  { typedef Int16 count_type; };
template <> struct rle_traits<Float32> { typedef Int32 count_type; };
template <> struct rle_traits<Float64> { typedef Int64 count_type; };

template <typename T>
struct rle_type
{
	typedef rle_traits<T>::count_type count_t;

	count_t m_RunLength;
	T       m_Value[1];
};

template <typename T>
struct rle_const_iter : std::iterator<std::input_iterator_tag, T>
{
	typedef rle_type<T>::count_t count_t;

	rle_const_iter(const rle_type<T>* data): m_Ptr(data->m_Value), m_Count(data->m_RunLength) 
	{ 
		BOOST_MPL_ASSERT(sizeof(count_t) % sizeof(T) == 0);
		dms_assert(data); 
	}

	rle_const_iter<T> operator ++(int) // postfix
	{
		rle_iter<T> tmp = *this;
		++*this;
		return tmp;
	}

	rle_const_iter<T>& operator ++()
	{
		dms_assert(!AtEnd()); // atEnd iters should not be incremented anymore

		if (count > 0)
			--count;
		else 
		{
			dms_assert(count < 0);
			++count;
			++m_ValuePtr;
		}
		if (!count)
		{
			count = reinterpret_cast<count_t*>(m_Ptr);
			m_Ptr += sizeof(count_t) / sizeof(T);
		}
		return this;
	}

	const T& operator *()  const { dms_assert(!AtEnd()); return *m_Ptr; }
	const T* operator ->() const { dms_assert(!AtEnd()); return  m_Ptr; }

	operator non_promotional_bool () const { return make_non_promotional(!AtEnd()); }

	bool operator ==(const rle_const_iter<T>& rhs) { return m_Ptr == rhs.m_Ptr || (AtEnd() && rhs.AtEnd()); }

protected:
	bool AtEnd() const { return m_Count == 0; }

private:
	const T* m_Ptr;
	count_T  m_Count;
};

template <typename T>
struct rle_iter : std::iterator<std::output_iterator_tag, T>
{
	typedef rle_type<T>::count_t count_t;

	rle_iter(rle_type<T>* data)
		:	m_CountPtr(&data->m_Count) 
		,	m_ValuePtr(&data->m_Value)
	{
		dms_assert(data); 
		*m_CountPtr = 0; 
		--m_ValuePtr; // pre increment at assignment in order to do fast comparison, thus if(*m_CountPtr) then *m_ValuePtr indicates last assigned value
	}
	~rle_iter()
	{
		forward();
		*m_CountPtr = 0;
	}
/* NOP
	rle_iter<T> operator ++(int) // postfix
	{
		rle_iter<T> tmp = *this;
		++*this;
		return tmp;
	}

	rle_const_iter<T>& operator ++()
	{
		dms_assert(!AtEnd()); // atEnd iters should not be incremented anymore
	}
*/
	rle_iter<T>& operator *()  { return this; }

	void operator = (const T& v) 
	{
		dms_assert(m_CountPtr);
		dms_assert(m_ValuePtr);

		count_t c = *m_CountPtr;
		if (c >= 2)
		{
			dms_assert(m_ValuePtr == &(reinterpret_cast<rle_type<T>*>(m_CountPtr)->m_Value));

			if (*m_ValuePtr == v)
			{
				if (c < MAX_VALUE(count_t))
				{
					++*m_CountPtr;
					dms_assert(*m_CountPtr > 0);
				}
				else
				{
					forward_after_const();
					*m_CountPtr = 1;
					*m_ValuePtr = v;
				}
			}
			else // *m_ValuePtr != v && m_CountPtr >= 0
			{
//				count_t c = m_Ptr->m_Count; // remove last value
//				++reinterpret_cast<count_t*>(m_CountPtr);
//				reinterpret_cast<T*>(m_CountPtr) += c;

				forward_after_const();
				*m_CountPtr = 1;
				*m_ValuePtr = v;
			}
		}
		else // *m_CountPtr < 2
		{
			if (!c || *m_ValuePtr != v)
			{
				if (c > MIN_VALUE(count_t))
				{
					if (c == 1)
						*m_CountPtr = -2;
					else
						*m_CountPtr = --c;
					*++m_ValuePtr = v;
				}
				else
				{
					forward_after_var(-c);
					*m_CountPtr = 1;
					*m_ValuePtr = v;
				}
			}
			else // countPtr <= 2 &&  (*m_ValuePtr == v)
			{
				if (c<0)
				{
					dms_assert(c!=-1);
					++*m_CountPtr; // remove last value
					dms_assert(1-c == -*m_CountPtr);
					forward_after_var(1-c);
					*m_CountPtr = 2;
					*m_ValuePtr = v;
				}
				else
				{
					dms_assert(c==0 || c==1);
					++*m_CountPtr;
					if (!c)
						*m_ValuePtr = v;
					dms_assert(*m_ValuePtr == v);
				}
			}
		}
	}
	bool operator ==(const rle_iter<T>& rhs) { return m_Ptr == rhs.m_Ptr || (AtEnd() && rhs.AtEnd()); }

protected:

	void forward() 
	{ 
		dms_assert(m_CountPtr); 
		count_t c = *m_CountPtr;
		if (c)
		{
			if (c > 0)
				forward_after_const() 
			else
				forward_after_var(-c)
		}
	}

	void forward_after_var(count_t c) 
	{ 
		++m_CountPtr;
		reinterpret_cast<T*>(m_CountPtr) += c;
		m_ValuePtr += (sizeof(count_t) / sizeof(T));
		dms_assert(m_ValuePtr == &(reinterpret_cast<rle_type<T>*>(m_CountPtr)->m_Value));
	}
	void forward_after_const() 
	{ 
		++m_CountPtr;
		++reinterpret_cast<T*>(m_CountPtr);
		m_ValuePtr += (sizeof(count_t) / sizeof(T));
		dms_assert(m_ValuePtr == &(reinterpret_cast<rle_type<T>*>(m_CountPtr)->m_Value));
	}

//	bool AtEnd() const { return m_Count == 0; }

private:
	count_t* m_CountPtr;
	T*       m_ValuePtr;
};

#endif

