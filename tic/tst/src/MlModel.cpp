#include "cpc/Types.h"
#include "dbg/Debug.h"
#include "geo/BaseBounds.h"
#include "ptr/OwningPtrSizedArray.h"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/uniform_01.hpp>


// ============== RandomFill

template <typename Iter>
void random_uniform_fill(Iter first, Iter last, UInt32 seed)
{
	typedef typename std::iterator_traits<Iter>::value_type value_type;

	boost::mt19937::result_type randomSeed = seed;
	boost::uniform_01< boost::mt19937, value_type> uniformEngine =
		boost::uniform_01< boost::mt19937,  value_type>( boost::mt19937(randomSeed) );
//	boost::uniform_01< boost::mt19937, value_type> uniformEngine( boost::mt19937(randomSeed) );

	std::generate(first, last, uniformEngine);
}

template <typename Iter>
void random_discrete_fill(Iter first, Iter last, UInt32 ub, UInt32 seed)
{
	typedef typename std::iterator_traits<Iter>::value_type value_type;

	boost::mt19937::result_type randomSeed = seed;
	boost::uniform_01< boost::mt19937,  double> uniformEngine =
		boost::uniform_01< boost::mt19937,  double>( boost::mt19937(randomSeed) );
//	boost::uniform_01< boost::mt19937,  double> uniformEngine( boost::mt19937(randomSeed) );

	for (; first != last; ++first)
		*first = uniformEngine() * ub;
}

// ============== RandomFill

#include "set/RangeFuncs.h"

template <typename T> inline 
typename std::enable_if< raw_constructed< T >::value, OwningPtrSizedArray<T> >::type
new_zero_array(UInt32 n)
{
	auto result = OwningPtrSizedArray<T>(n MG_DEBUG_ALLOCATOR_SRC_EMPTY);
	fast_zero(result.begin(), result.end());
	return result;
}
/*
template <typename T> inline 
typename std::enable_if< !raw_constructed< T >::value, T* >::type
new_zero_array(UInt32 n)
{
	return new T[n];
}
*/
// ============== RandomMlDataProvider

struct RandomMlDataProvider
{
	typedef Float64                      value_type;
	typedef UInt32                       obs_index;
	typedef UInt32                       choice_index;
	typedef UInt32                       param_index;

	using value_array = OwningPtrSizedArray<value_type>;
	using  choice_array = OwningPtrSizedArray<choice_index>;

	RandomMlDataProvider(obs_index i, choice_index j, param_index k)
		:	m_nrI(i)
		,	m_nrJ(j)
		,	m_nrK(k)
	{
		DBG_START("RandomMlDataProvider", "RandomMlDataProvider", true);

		UInt32 nrD = i*j*k;

		dms_assert(nrD / i == j*k);
		dms_assert(nrD < 0x1FFFFFFF / sizeof(value_type) );

		m_D = value_array( nrD MG_DEBUG_ALLOCATOR_SRC_EMPTY);
		m_J = choice_array( i MG_DEBUG_ALLOCATOR_SRC_EMPTY);
		random_uniform_fill (m_D.begin(), m_D.begin()+nrD,  1);
		random_discrete_fill(m_J.begin(), m_J.begin()+i, j, 2);
	}

	choice_index GetJ(obs_index i) const { dms_assert(i < m_nrI); return m_J[i]; }
	value_type   GetD(obs_index i, choice_index j, param_index k) const 
	{
		dms_assert(i < m_nrI); 
		dms_assert(j < m_nrJ); 
		dms_assert(k < m_nrK); 

		i *= m_nrJ;
		i += j;
		i *= m_nrK;
		i += k;

		return m_D[i]; 
	}

	obs_index    NrI() const { return m_nrI; }
	choice_index NrJ() const { return m_nrJ; }
	param_index  NrK() const { return m_nrK; }

private:
	obs_index    m_nrI;
	choice_index m_nrJ;
	param_index  m_nrK;

	value_array  m_D;
	choice_array m_J;
};

// ============== MlModel

template <typename Dataprovider>
struct MlModel
{
	typedef typename Dataprovider::value_type value_type;
	typedef OwningPtrSizedArray<value_type>   value_array;

	typedef typename Dataprovider::obs_index    obs_index;
	typedef typename Dataprovider::choice_index choice_index;
	typedef typename Dataprovider::param_index  param_index;

	typedef value_type* param_space;

	MlModel(const Dataprovider& dataProvider, const param_space p)
		:	m_DataProvider(dataProvider)
		,	m_Likelihood()
		,	m_Score (new_zero_array<value_type>(    dataProvider.NrK() ) )
		,	m_Fisher(new_zero_array<value_type>(Sqr(dataProvider.NrK())) )
	{

		DBG_START("MlModel", "MlModel", true);

		value_array currP   ( new_zero_array<value_type>(dataProvider.NrJ()) );
		value_array currExpD( new_zero_array<value_type>(dataProvider.NrK()) );

		obs_index    nrI = dataProvider.NrI();
		choice_index nrJ = dataProvider.NrJ();
		param_index  nrK = dataProvider.NrK();

		for (obs_index i = nrI; i--; )
		{
			choice_index currJ = m_DataProvider.GetJ(i);
			dms_assert(currJ < nrJ);

			value_type currTQ = 0;

		//	Calculate Sij += Dijk * Bk; Qij = exp(Sij); Qi += Qij; Pij = Qij/Qi
			for (choice_index j = nrJ; j--; )
			{
				value_type currSj = value_type();

				param_space pPtr = p;

				for (param_index k = 0; k != nrK; ++pPtr, ++k)
					currSj += m_DataProvider.GetD(i, j, k) * *pPtr;

				if (j == currJ)
					m_Likelihood += currSj; //	adjust likelihood based on current choice

				value_type currQj = exp(currSj);
				currP[j] = currQj;
				currTQ += currQj;
			}
			for (choice_index j = nrJ; j--; )
				currP[j] /= currTQ;

		//	============== Likelihood calculation

			m_Likelihood -= log( currTQ ); //	adjust likelihood based on choice weight

		//	============== first  derivative
			// calcualte ExpDik += Dijk * Pij, m_Score += (ExpDik - ActDik)
			for (param_index k = nrK; k--;)
			{
				value_type currExpDk = value_type();
				for (choice_index j = nrJ; j--;)
					currExpDk += m_DataProvider.GetD(i, j, k) * currP[j];
				currExpD[k] = currExpDk;
				m_Score[k] += (currExpDk - m_DataProvider.GetD(i, currJ, k));
			}

		//	============== second derivative

			value_type* fisherRow = &(m_Fisher[0]);
			for (param_index k = 0; k!=nrK; ++k)
			{
//				for (param_index kk = 0; kk!=nrK; ++fisherRow, ++kk)
				param_index kk = k;
				{

					value_type result = value_type();
					for (choice_index j = m_DataProvider.NrJ(); j--;)
						result 
							+=	currP[j] 
							*	( m_DataProvider.GetD(i, j,  k) - currExpD[k ] )
							*	( m_DataProvider.GetD(i, j, kk) - currExpD[kk] );

					fisherRow[k] += result;
				}
				fisherRow += k;
			}
		}
	}

	const Dataprovider& GetDataProvider() const { return m_DataProvider; }

    value_type GetL   ()                             const { return m_Likelihood; }
	value_type GetLk (param_index k)                 const { return -m_Score[k]; }
	value_type GetHkk(param_index k, param_index kk) const { return m_Fisher[SizeT(k) * m_DataProvider.NrK() + kk]; }

private:
	const Dataprovider& m_DataProvider;

	value_type  m_Likelihood;
	value_array m_Score, m_Fisher;
};


// ============== TestML

template <typename DataProvider>
void printML(const MlModel<DataProvider>& estimator)
{
	std::cout << std::endl << "Likelihood: " << estimator.GetL() << std::endl;

	std::cout << "First derivatives: ";
	typename DataProvider::param_index nrParam = estimator.GetDataProvider().NrK();
	for (typename DataProvider::param_index k = 0; k!=nrParam; ++k)
		std::cout << " " << estimator.GetLk(k);
	std::cout << std::endl;

	std::cout << "Second derivatives " << std::endl;
	for (typename DataProvider::param_index k = 0; k!=nrParam; ++k)
	{
//		for (UInt32 kk = 0; kk!=nrParam; ++kk)
		UInt32 kk = k;
			std::cout << " " << estimator.GetHkk(k, kk);
//		std::cout << std::endl;
	}
	std::cout << std::endl;
}

//#include <valarray>

bool testML()
{
	typedef MlModel<RandomMlDataProvider> LikelihoodModel;

//	const UInt32 nrObs    =   1000;
//	const UInt32 nrChoice =     10;
//	const UInt32 nrParam  =      5;
	const UInt32 nrObs    =  10000;
	const UInt32 nrChoice =     10;
	const UInt32 nrParam  =     10;

	DBG_START("TestML", "All", true);

	RandomMlDataProvider mlData(nrObs, nrChoice, nrParam);

	RandomMlDataProvider::value_type params [nrParam] = {1,2,3,4,5};
	RandomMlDataProvider::value_type deltas [nrParam] = {};
	RandomMlDataProvider::value_type newpars[nrParam] = {};
	RandomMlDataProvider::value_type a = 1.0, xx = 0, prevL = MinValue<RandomMlDataProvider::value_type>(), currL;

	for (UInt32 i=0; true; ++i)
	{
		std::cout << std::endl << "Iteration " << i;
		std::cout << std::endl << "Params( " << a << "):";
		for (UInt32 k = 0; k!=nrParam; ++k)
		{
			newpars[k] = params[k] + a * deltas[k];
			std::cout << " " << newpars[k];
		}
		std::cout << std::endl;

		LikelihoodModel estimator(mlData, newpars);
		printML(estimator);
		currL = estimator.GetL();
		if (prevL >= currL + 0.20 * a * xx)
		{
			std::cout << "a = "<< a << std::endl;
			a *= 0.4;
			if (a < 1e-6 && prevL == currL)
				break;
		}
		else
		{
			dms_assert(prevL <= currL);

			xx = 0;
			a = 1.0;
			prevL = currL;
			std::cout << "Deltas: ";
			for (UInt32 k=0; k!=nrParam; ++k)
			{
				params[k] = newpars[k];

				dms_assert( estimator.GetHkk(k, k) > 0 );

				deltas[k] = estimator.GetLk(k) / estimator.GetHkk(k, k);
				xx -= deltas[k] * estimator.GetLk(k);
				std::cout << " " << deltas[k];
			}
			dms_assert(xx <= 0);
			std::cout << std::endl;
		}
	}
	return true;
}

