// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_util_time_hpp
#define ham_util_time_hpp

#include <chrono>
#include <cmath>
#include <ratio>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <type_traits>
#include <vector>

namespace ham {
namespace util {
namespace time {

typedef double rep;
typedef std::nano period;
typedef std::chrono::duration<rep, period> duration;
// TODO(future): Intel MPSS <= 3.3 come with gcc 4.7 headers that do not fully implement std::chrono
//typedef std::conditional<std::chrono::high_resolution_clock::is_steady, std::chrono::high_resolution_clock, std::chrono::steady_clock>::type clock;
typedef std::chrono::high_resolution_clock clock;
typedef clock::time_point time_point;

// the code below assumes floating point arithmetic on rep (retuned by duration::count())
static_assert(std::chrono::treat_as_floating_point<duration::rep>::value, "rep is required to be a floating point type");

//class timer
//{
//public:
//	timer() : start(clock::now()) {}

//	duration elapsed() const
//	{
//		// NOTE: conversion from clock's duration type to ours (see above)
//		return std::chrono::duration_cast<duration>(clock::now() - start); 
//	}
//	
//private:
//	time_point start;
//};

class timer
{
public:
	timer()
	{
		clock_gettime(CLOCK_REALTIME, &start_time);
	}

	// TODO(): measure overhead with different compilers
	//duration elapsed() const
	rep elapsed() const
	{
		struct timespec stop_time;
		clock_gettime(CLOCK_REALTIME, &stop_time);
		// TODO(): measure overhead with different compilers
		//return duration(static_cast<rep>((stop_time.tv_sec - start_time.tv_sec) * 1000000000 +  (stop_time.tv_nsec - start_time.tv_nsec)));
		return static_cast<rep>((stop_time.tv_sec - start_time.tv_sec) * 1000000000 +  (stop_time.tv_nsec - start_time.tv_nsec));
	}
	
private:
	struct timespec start_time;
};

class statistics
{
public:
	statistics() : mean(0.0), var(0.0), k(0) {}
	statistics(size_t n, size_t warmup = 0) : statistics() { this->warmup = warmup, times.reserve(n); }
	
	// add a timer
	void add(timer const& t) { add(t.elapsed()); }

	// add a duration
	// TODO(): measure overhead with different compilers
	//void add(duration val)
	void add(rep r)
	{
		// ignore warmup values
		if (warmup > 0) // NOTE: decrement
		{
			warmup--;
			return;
		}
		duration val(r);
		times.push_back(val);
		++k;
		duration delta = val - mean;
		mean = mean + duration(delta.count() / k);
		var = var + duration(delta.count() * delta.count());

		if (k == 1)
		{
			min_time = val;
			max_time = val;
		}
		else
		{
			min_time = val < min_time ? val : min_time;
			max_time = val > max_time ? val : max_time;
		}
	}

	size_t count() const { return k; }

	duration average() const { return mean; }

	duration median() const
	{
		// NOTE: when comparing this with the mathematical definition, keep in mind our indices start with 0
		const size_t n = times.size();
		if (n == 0)
		{
			return duration(0.0);
		}
		else if ((n % 2) == 0) // even number of vaules
		{
			// average the two median elements, round the result, and convert it back to a duration
			return duration(0.5 * (times[(n / 2) - 1].count() + times[n / 2].count()));
		}
		else // uneven number of values
		{
			return times[n / 2];
		}
	}

	duration min() const { return min_time; }

	duration max() const { return max_time; }

	duration variance() const 
	{ 
		return duration((k <= 1) ? 0.0 : var.count() / rep(k - 1)); 
	}

	// standard error
	duration std_error() const
	{
		return duration(std::sqrt(var.count()) / k);
	}

	// relative error (to repeat measurements until small enough)
	duration relative_std_error() const
	{
		return duration(std_error().count() / average().count());
	}

	// delta value for the 95% confidence interval
	// (not student's t-test but normal distribution)
	// [average - error, average + error]
	duration conf95_error() const
	{ 
		return 1.96 * std_error();
	}

	// relative error (to repeat measurements until small enough)
	duration relative_conf95_error() const
	{
		return duration(conf95_error().count() / average().count());
	}


	// returns the header for the string() method
	static std::string header_string()
	{
		std::stringstream ss;
		ss << "average" << "\t"
		   << "median" << "\t"
		   << "min" << "\t"
		   << "max" << "\t"
		   << "variance" << "\t"
		   << "std_error" << "\t"
		   << "relative_std_error" << "\t"
		   << "conf95_error" << "\t"
		   << "relative_conf95_error" << "\t"
		   << "count";
		return ss.str();
	}

	// returns all data in one line separated by tabs
	std::string string() const
	{
		std::stringstream ss;
		ss << std::scientific // << std::fixed
		   << this->average().count() << "\t"
		   << this->median().count() << "\t"
		   << this->min().count() << "\t"
		   << this->max().count()  << "\t"
		   << this->variance().count() << "\t"
		   << this->std_error().count() << "\t"
		   << this->relative_std_error().count() << "\t"
		   << this->conf95_error().count() << "\t"
		   << this->relative_conf95_error().count() << "\t"
		   << this->count();
		return ss.str();
	}

	// writes the raw data to a file (one duration per line)
	void to_file(std::string filename) const
	{
		std::ofstream file(filename.c_str());
		file << std::scientific;

		for (size_t i = 0; i < times.size(); ++i)
		{
			file << times[i].count() << std::endl;
		}

		file.close();
	}

	duration sum() const
	{
		duration result(0.0);
		for (size_t i = 0; i < times.size(); ++i)
			result += times[i];
		return result;
	}

private:
	duration mean; // average
	duration var; // variance
	size_t k; // event counter
	size_t warmup; // number of values to drop before taking data
	std::vector<duration> times; // all measured values
	duration min_time; // global maximum
	duration max_time; //  global minimum
};

} // namespace time
} // namespace util
} // namespace ham

#endif // ham_util_time_hpp
