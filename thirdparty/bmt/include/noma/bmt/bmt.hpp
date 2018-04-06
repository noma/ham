// Copyright (c) 2013-2017 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef noma_bmt_bmt_hpp
#define noma_bmt_bmt_hpp

#include <chrono>
#include <cmath>
#include <ratio>
#include <string>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <type_traits>
#include <vector>

namespace noma {
namespace bmt {

using rep = double;
using period = std::nano;
using duration = std::chrono::duration<rep, period>;

// make sure we have a steady clock, if possible one with a high resolution
using clock = std::conditional<std::chrono::high_resolution_clock::is_steady, std::chrono::high_resolution_clock, std::chrono::steady_clock>::type;
using time_point = clock::time_point;

// convenience duration types
using nanoseconds = std::chrono::duration<rep, std::nano>;
using microseconds = std::chrono::duration<rep, std::micro>;
using milliseconds = std::chrono::duration<rep, std::milli>;
using seconds = std::chrono::duration<rep, std::ratio<1>>;
using minutes = std::chrono::duration<rep, std::ratio<60>>;
using hours = std::chrono::duration<rep, std::ratio<3600>>;


// NOTE: the code below assumes floating point arithmetic on rep (the type retuned by duration::count())
static_assert(std::chrono::treat_as_floating_point<duration::rep>::value, "rep is required to be a floating point type");

class timer
{
public:
	timer() : start(clock::now()) {}

	duration elapsed() const
	{
		// NOTE: conversion from clock's duration type to ours (see above)
		return std::chrono::duration_cast<duration>(clock::now() - start);
	}
	
private:
	time_point start;
};

class statistics
{
public:
	statistics() = default;

	/**
	 * Ctor with name and pre-allocation of internal vector of timings measured.
	 * If the name is used a first column is added for table output.
	 * If the number of measurements is known, it should be used to 
	 * avoid re-allocating memory while benchmarking.
	 * Optionally, a number of ignored warm-up values can be specified.
	 */
	statistics(const std::string& name, size_t expected_count, size_t warmup_count = 0) : warmup_count_(warmup_count), name_(name)
	{
		times_.reserve(expected_count);
	}

	/**
	 * Same as above with a name, that adds a leading column to the table output.
	 */
	statistics(size_t count, size_t warmup_count = 0) : statistics("", count, warmup_count) { }
	
	// add a timer
	void add(const timer& t) { add(t.elapsed()); }

	// add a duration
	void add(const duration& value)
	{
		// ignore warmup values
		if (warmup_count_ > 0) // NOTE: decrement
		{
			--warmup_count_;
			return;
		}

		times_.push_back(value);
		++count_;
		duration delta = value - average_;
		average_ = average_ + duration(delta.count() / count_);
		variance_ = variance_ + duration(delta.count() * delta.count());

		if (count_ == 1)
		{
			min_ = value;
			max_ = value;
		}
		else
		{
			min_ = std::min(min_, value); //;value < min_ ? value : min_;
			max_ = std::max(max_, value); //value > max_ ? value : max_;
		}
	}

	size_t count() const { return count_; }

	duration average() const { return average_; }

	duration median() const
	{
		// NOTE: when comparing this with the mathematical definition, keep in mind our indices start with 0
		const size_t n = times_.size();
		if (n == 0)
		{
			return duration(0.0);
		}
		else if ((n % 2) == 0) // even number of vaules
		{
			// average the two median elements, round the result, and convert it back to a duration
			return duration(0.5 * (times_[(n / 2) - 1].count() + times_[n / 2].count()));
		}
		else // uneven number of values
		{
			return times_[n / 2];
		}
	}

	duration min() const { return min_; }

	duration max() const { return max_; }

	const std::string& name() const { return name_; }

	duration variance() const 
	{ 
		return duration((count_ <= 1) ? 0.0 : variance_.count() / rep(count_ - 1)); 
	}

	// standard error
	duration std_error() const
	{
		return duration(std::sqrt(variance_.count()) / count_);
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
	static std::string header_string(bool name_column)
	{
		std::stringstream ss;

		// add name column if name was set
		if (name_column)
			ss << "name" << "\t";
		
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

	std::string header_string() const
	{
		return header_string(!name_.empty());
	}

	// returns all data in one line separated by tabs
	std::string string() const
	{
		std::stringstream ss;
		
		// add name column if name was set
		if (!name_.empty())
			ss << name_ << "\t";
		
		ss << std::scientific // << std::fixed
		   << average().count() << "\t"
		   << median().count() << "\t"
		   << min().count() << "\t"
		   << max().count()  << "\t"
		   << variance().count() << "\t"
		   << std_error().count() << "\t"
		   << relative_std_error().count() << "\t"
		   << conf95_error().count() << "\t"
		   << relative_conf95_error().count() << "\t"
		   << count();
		return ss.str();
	}

	// writes the raw data to a file (one duration per line)
	void to_file(std::string filename) const
	{
		std::ofstream file(filename.c_str());
		file << std::scientific;

		for (size_t i = 0; i < times_.size(); ++i)
		{
			file << times_[i].count() << std::endl;
		}

		file.close();
	}

	duration sum() const
	{
		duration result(0.0);
		for (size_t i = 0; i < times_.size(); ++i)
			result += times_[i];
		return result;
	}

private:
	size_t count_ { 0 }; // event counter
	size_t warmup_count_ { 0 }; // number of values to drop before taking data
	duration average_ { 0.0 }; // average
	duration variance_ { 0.0 }; // variance
	duration min_ { 0.0 }; // global maximum
	duration max_ { 0.0 }; //  global minimum
	std::vector<duration> times_; // all measured values
	std::string name_;
};

} // namespace bmt
} // namespace noma

#endif // noma_bmt_bmt.hpp
