#include <assert.h>
#include <vector>
#include <algorithm>

using ClassBreakValueType = double;
using break_array = std::vector<ClassBreakValueType>;
typedef std::size_t                  SizeT;
typedef SizeT                        CountType;
typedef std::pair<double, CountType> ValueCountPair;
typedef std::vector<ValueCountPair>  ValueCountPairContainer;

// helper funcs
template <typename T> T Min(T a, T b) { return (a<b) ? a : b; } 

SizeT GetTotalCount(const  ValueCountPairContainer& vcpc)
{
	SizeT sum = 0;
	ValueCountPairContainer::const_iterator 
		i = vcpc.begin(),
		e = vcpc.end();
	for(sum = 0; i!=e; ++i)
		sum += (*i).second;
	return sum;
}

// helper struct JenksFisher

struct JenksFisher  // captures the intermediate data and methods for the calculation of Natural Class Breaks.
{
	JenksFisher(const ValueCountPairContainer& vcpc, SizeT k)
		:	m_M(vcpc.size())
		,	m_K(k)
		,	m_BufSize(vcpc.size()-(k-1))
		,	m_PrevSSM(m_BufSize)
		,	m_CurrSSM(m_BufSize)
		,	m_CB(m_BufSize * (m_K-1))
		,	m_CBPtr()
	{
		m_CumulValues.reserve(vcpc.size());
		double cwv=0;
		CountType cw = 0, w;

		for(SizeT i=0; i!=m_M; ++i)
		{
			assert(!i || vcpc[i].first > vcpc[i-1].first); // PRECONDITION: the value sequence must be strictly increasing

			w   = vcpc[i].second;
			assert(w > 0); // PRECONDITION: all weights must be positive

			cw  += w; 
			assert(cw > w || !i); // No overflow? No loss of precision?

			cwv += w * vcpc[i].first;
			m_CumulValues.push_back(ValueCountPair(cwv, cw));
			if (i < m_BufSize)
				m_PrevSSM[i] = cwv * cwv / cw; // prepare SSM for first class. Last (k-1) values are omitted
		}
	}

	double GetW(SizeT b, SizeT e)
	// Gets sum of weighs for elements b..e.
	{
		assert(b<=e);
		assert(e<m_M);

		double res  = m_CumulValues[e].second;
		if (b) res -= m_CumulValues[b-1].second;
		return res;
	}

	double GetWV(SizeT b, SizeT e)
	// Gets sum of weighed values for elements b..e
	{
		assert(b<=e);
		assert(e<m_M);

		double res  = m_CumulValues[e].first;
		if (b) res -= m_CumulValues[b-1].first;
		return res;
	}

	double GetSSM(SizeT b, SizeT e)
	// Gets the Squared Mean for elements b..e, multiplied by weight.
	// Note that n*mean^2 = sum^2/n when mean := sum/n
	{
		double res = GetWV(b,e);
		return res * res / GetW(b,e);
	}

	SizeT FindMaxBreakIndex(SizeT i, SizeT bp, SizeT ep)
	// finds CB[i+m_NrCompletedRows] given that 
	// the result is at least bp+(m_NrCompletedRows-1)
	// and less than          ep+(m_NrCompletedRows-1)
	// Complexity: O(ep-bp) <= O(m)
	{
		assert(bp < ep);
		assert(bp <= i);
		assert(ep <= i+1);
		assert(i  <  m_BufSize);
		assert(ep <= m_BufSize);

		double minSSM = m_PrevSSM[bp] + GetSSM(bp+m_NrCompletedRows, i+m_NrCompletedRows);
		SizeT foundP = bp;
		while (++bp < ep)
		{
			double currSSM = m_PrevSSM[bp] + GetSSM(bp+m_NrCompletedRows, i+m_NrCompletedRows);
			if (currSSM > minSSM)
			{
				minSSM = currSSM;
				foundP = bp;
			}
		}
		m_CurrSSM[i] = minSSM;
		return foundP;
	}

	void CalcRange(SizeT bi, SizeT ei, SizeT bp, SizeT ep)
	// find CB[i+m_NrCompletedRows]
	// for all i>=bi and i<ei given that
	// the results are at least bp+(m_NrCompletedRows-1)
	// and less than            ep+(m_NrCompletedRows-1)
	// Complexity: O(log(ei-bi)*Max((ei-bi),(ep-bp))) <= O(m*log(m))
	{
		assert(bi <= ei);

		assert(ep <= ei);
		assert(bp <= bi);

		if (bi == ei)
			return;
		assert(bp < ep);

		SizeT mi = (bi + ei)/2;
		SizeT mp = FindMaxBreakIndex(mi, bp, Min<SizeT>(ep, mi+1));

		assert(bp <= mp);
		assert(mp <  ep);
		assert(mp <= mi);
		
		// solve first half of the sub-problems with lower 'half' of possible outcomes
		CalcRange(bi, mi, bp, Min<SizeT>(mi, mp+1)); 

		m_CBPtr[ mi ] = mp; // store result for the middle element.

		// solve second half of the sub-problems with upper 'half' of possible outcomes
		CalcRange(mi+1, ei, mp, ep);
	}


	void CalcAll()
	// complexity: O(m*log(m)*k)
	{
		if (m_K>=2)
		{
			m_CBPtr = m_CB.begin();
			for (m_NrCompletedRows=1; m_NrCompletedRows<m_K-1; ++m_NrCompletedRows)
			{
				CalcRange(0, m_BufSize, 0, m_BufSize); // complexity: O(m*log(m))

				m_PrevSSM.swap(m_CurrSSM);
				m_CBPtr += m_BufSize;
			}
		}
	}

	SizeT                   m_M, m_K, m_BufSize;
	ValueCountPairContainer m_CumulValues;

	std::vector<double> m_PrevSSM;
	std::vector<double> m_CurrSSM;
	std::vector<SizeT>               m_CB;
	std::vector<SizeT>::iterator     m_CBPtr;

	SizeT                  m_NrCompletedRows;
};

// GetValueCountPairs
// 
// GetValueCountPairs sorts chunks of values and then merges them in order to minimize extra memory and work when many values are equal. 
// This is done recursively while retaining used intermediary buffers in order to minimize heap allocations.

const SizeT BUFFER_SIZE = 1024;

void GetCountsDirect(ValueCountPairContainer& vcpc, const double* values, SizeT size)
{
	assert(size <= BUFFER_SIZE);
	assert(size > 0);
	assert(vcpc.empty());

	double buffer[BUFFER_SIZE];

	std::copy(values, values+size, buffer);
	size = std::remove(buffer, buffer+size, std::numeric_limits<double>::quiet_NaN()) - buffer;

	std::sort(buffer, buffer+size);

	double currValue = buffer[0];
	SizeT     currCount = 1;
	for (SizeT index = 1; index != size; ++index)
	{
		if (currValue < buffer[index])
		{
			vcpc.push_back(ValueCountPair(currValue, currCount));
			currValue = buffer[index];
			currCount = 1;
		}
		else
			++currCount;
	}
	vcpc.push_back(ValueCountPair(currValue, currCount));
}

struct CompareFirst
{
	bool operator () (const ValueCountPair& lhs, const ValueCountPair& rhs)
	{
		return lhs.first < rhs.first;
	}
};

void MergeToLeft(ValueCountPairContainer& vcpcLeft, const ValueCountPairContainer& vcpcRight, ValueCountPairContainer& vcpcDummy)
{
	assert(vcpcDummy.empty());
	vcpcDummy.swap(vcpcLeft);
	vcpcLeft.resize(vcpcRight.size() + vcpcDummy.size());

	std::merge(vcpcRight.begin(), vcpcRight.end(), vcpcDummy.begin(), vcpcDummy.end(), vcpcLeft.begin(), CompareFirst());

	ValueCountPairContainer::iterator 
		currPair = vcpcLeft.begin(),
		lastPair = vcpcLeft.end();


	ValueCountPairContainer::iterator index = currPair+1;
	while (index != lastPair && currPair->first < index->first)
	{
		currPair = index;
		++index;
	}

	double currValue = currPair->first;
	SizeT     currCount = currPair->second;
	for (; index != lastPair;++index)
	{
		if (currValue < index->first)
		{
			*currPair++ = ValueCountPair(currValue, currCount);
			currValue = index->first;
			currCount = index->second;
		}
		else
			currCount += index->second;
	}
	*currPair++ = ValueCountPair(currValue, currCount);
	vcpcLeft.erase(currPair, lastPair);

	vcpcDummy.clear();
}

struct ValueCountPairContainerArray : std::vector<ValueCountPairContainer>
{
	void resize(SizeT k)
	{
		assert(capacity() >= k);
		while (size() < k)
		{
			push_back(ValueCountPairContainer());
			back().reserve(BUFFER_SIZE);
		}
	}

	void GetValueCountPairs(ValueCountPairContainer& vcpc, const double* values, SizeT size, unsigned int nrUsedContainers)
	{
		assert(vcpc.empty());
		if (size <= BUFFER_SIZE)
			GetCountsDirect(vcpc, values, size);
		else
		{
			resize(nrUsedContainers+2);

			unsigned int m = size / 2;

			GetValueCountPairs(vcpc, values, m, nrUsedContainers);
			GetValueCountPairs(begin()[nrUsedContainers], values + m, size - m, nrUsedContainers+1);

			MergeToLeft(vcpc, begin()[nrUsedContainers], begin()[nrUsedContainers+1]);
			begin()[nrUsedContainers].clear();
		}
		assert(GetTotalCount(vcpc) == size);
	}
};

void GetValueCountPairs(ValueCountPairContainer& vcpc, const double* values, SizeT n)
{
	vcpc.clear();

	if(n)
	{
		ValueCountPairContainerArray vcpca;
		// max nr halving is log2(max cardinality / BUFFER_SIZE); max cardinality is SizeT(-1)
		vcpca.reserve(3+8*sizeof(SizeT)-10); 
		vcpca.GetValueCountPairs(vcpc, values, n, 0);

		assert(vcpc.size());
	}
}

template<typename CB> using LimitsContainer = std::vector<CB>;

void ClassifyJenksFisherFromValueCountPairs(LimitsContainer<ClassBreakValueType>& breaksArray, SizeT k, const ValueCountPairContainer& vcpc)
{
	breaksArray.resize(k);
	SizeT m  = vcpc.size();

	assert(k <= m); // PRECONDITION

	if (!k)
		return;

	JenksFisher jf(vcpc,  k);

	if (k > 1)
	{
		jf.CalcAll();

		SizeT lastClassBreakIndex = jf.FindMaxBreakIndex(jf.m_BufSize-1, 0, jf.m_BufSize);

		while (--k)
		{
			breaksArray[k] = vcpc[lastClassBreakIndex+k].first;
			assert(lastClassBreakIndex < jf.m_BufSize);
			if (k > 1)
			{
				jf.m_CBPtr -= jf.m_BufSize;
				lastClassBreakIndex = jf.m_CBPtr[lastClassBreakIndex];
			}
		}
		assert(jf.m_CBPtr == jf.m_CB.begin());
	}
	assert( k == 0 );
	breaksArray[0] = vcpc[0].first; // break for the first class is the minimum of the dataset.
}


// test code

#include <boost/random.hpp>


int main(int c, char** argv) 
{ 
	const double rangeMin = 0.0; 
	const double rangeMax = 10.0; 
	typedef boost::uniform_real<double> NumberDistribution; 
	typedef boost::mt19937 RandomNumberGenerator; 
	typedef boost::variate_generator<RandomNumberGenerator&, NumberDistribution> Generator; 

	NumberDistribution distribution(rangeMin, rangeMax); 
	RandomNumberGenerator generator; 
	generator.seed(0); // seed with the current time 
	Generator numberGenerator(generator, distribution); 
 
	const int n = 1000000;
	const int k = 10;

	std::cout << "Generating random numbers..." << std::endl;

	std::vector<double> values;
	values.reserve(n);
	for (int i=0; i!=n; ++i)
	{
		double v = numberGenerator();
		values.push_back(v*v); //populate a distribuiton slightly more interesting than uniform, with a lower density at higher values.
	}
	assert(values.size() == n);

	std::cout << "Generating sortedUniqueValueCounts ..." << std::endl;
	ValueCountPairContainer sortedUniqueValueCounts;
	GetValueCountPairs(sortedUniqueValueCounts, &values[0], n);

	std::cout << "Finding Jenks ClassBreaks..." << std::endl;
	LimitsContainer< ClassBreakValueType> resultingbreaksArray;
	ClassifyJenksFisherFromValueCountPairs(resultingbreaksArray, k, sortedUniqueValueCounts);

	std::cout << "Reporting results..." << std::endl;
	for (double breakValue: resultingbreaksArray)
		std::cout << breakValue << std::endl << std::endl; 
 
	std::cout << "Press a char and <enter> to terminate" << std::endl;
	char ch;
	std::cin >> ch; // wait for user to enter a key
} // main 