// Copyright (c) 2015-2017 Matthias Noack <ma.noack.pr@gmail.com>
//
// See accompanying file LICENSE and README for further information.

#ifndef noma_memory_memory_hpp
#define noma_memory_memory_hpp

#include <algorithm>
#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <stdlib.h> // posix_memalign

#include "noma/debug/debug.hpp"

namespace noma {
namespace memory {

template<typename T>
void update_max(std::atomic<T>& max, T const& value) noexcept
{
	T prev_max = max;
	while (prev_max < value && !max.compare_exchange_weak(prev_max, value));
}

class allocation_counter
{
public:
	allocation_counter()
	{
		allocated_byte_.store(0);
		allocated_byte_max_.store(0);
		allocations_.store(0);
		allocations_max_.store(0);
	}

	size_t allocated_byte() const
	{ return allocated_byte_.load(); }

	size_t allocated_byte_max() const
	{ return allocated_byte_max_.load(); }

	size_t allocations() const
	{ return allocations_.load(); }

	size_t allocations_max() const
	{ return allocations_max_.load(); }

protected:
	void add_allocation(size_t size) noexcept
	{
		allocated_byte_.fetch_add(size);
		++allocations_;
		update_max(allocated_byte_max_, allocated_byte_.load());
		update_max(allocations_max_, allocations_.load());
	}

	void sub_allocation(size_t size) noexcept
	{
		allocated_byte_.fetch_sub(size);
		--allocations_;
	}

private:
	std::atomic<size_t> allocated_byte_;
	std::atomic<size_t> allocated_byte_max_;
	std::atomic<size_t> allocations_;
	std::atomic<size_t> allocations_max_;
};


#ifndef NOMA_MEMORY_DEFAULT_ALIGNMENT
#define NOMA_MEMORY_DEFAULT_ALIGNMENT 64
#endif

/**
 * Custom exception class for memory errors.
 */
class memory_error : public std::runtime_error
{
public:
	memory_error(const std::string& msg) : runtime_error("memory::memory_error: " + msg)
	{};
};

/**
 * Convert posix_memalign error return code to string.
 */
const std::string posix_memalign_error_to_string(int err);

/**
 * Single instance class for aligned memory allocation and statistics.
 */
class aligned : public allocation_counter
{
public:
	static aligned& instance()
	{
		static aligned inst; // created on first use
		return inst;
	}

	~aligned()
	{
		DEBUG_ONLY(if (!allocated_sizes_.empty())
			           std::cout << "memory::~aligned(): warning: there were unfreed allocations." << std::endl;)
	}

	void* allocate_byte(size_t size_byte, size_t alignment = default_alignment_)
	{
		int err = 0;
		void* ptr = nullptr;
		if ((err = posix_memalign(&ptr, alignment, size_byte)))
			throw memory_error(posix_memalign_error_to_string(err));

		// keep count
		add_allocation(size_byte);

		ASSERT_ONLY(bool inserted =)
		allocated_sizes_.insert(std::make_pair(ptr, size_byte))ASSERT_ONLY(.second);
		assert(inserted); // make sure ptr is unique

		return ptr;
	}

	template<typename T>
	T* allocate(size_t size, size_t alignment = default_alignment_)
	{
		return reinterpret_cast<T*>(allocate_byte(size * sizeof(T), alignment));
	}

	template<typename T>
	void free(T* ptr)
	{
		void* vptr = reinterpret_cast<void*>(ptr);
		std::free(vptr);

		sub_allocation(allocated_sizes_.at(vptr));
		allocated_sizes_.erase(vptr);
	}

	template<typename T>
	std::function<void(T*)> deleter()
	{
		return std::bind(&aligned::free<T>, this, std::placeholders::_1);
	}

private:
	aligned() = default;

	static constexpr const size_t default_alignment_ = NOMA_MEMORY_DEFAULT_ALIGNMENT;
	std::map<void*, size_t> allocated_sizes_;
};

} // namespace memory
} // namespace noma

#endif // noma_memory_memory_hpp
