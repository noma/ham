// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_misc_options
#define ham_misc_options

#include <boost/program_options.hpp>
#include <cstdlib>

#include "ham/util/log.hpp"

namespace ham {
namespace detail {

class options {
public:
	options(int argc, char* argv[])
	 : cpu_affinity_(-1)
	{
		// command line options
		boost::program_options::options_description desc("HAM Options");
		desc.add_options()
//			("ham-help", "Shows this message")
			("ham-cpu-affinity", boost::program_options::value(&cpu_affinity_)->default_value(cpu_affinity_), "Per process value for the CPU affinity.")
		;

		boost::program_options::variables_map vm;

		// NOTE: no try-catch here to avoid exceptions, that cause offload-dependencies to boost exception in the MIC code
		const char* options_env = std::getenv("HAM_OPTIONS");
		if(options_env)
		{
			// parse from environment
			boost::program_options::store(boost::program_options::command_line_parser(split(std::string(options_env), ' ')).options(desc).allow_unregistered().run(), vm);
		}
		else
		{
			// parse from command line
			boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
		}

		boost::program_options::notify(vm);

//		if(vm.count("ham-help"))
//		{
//			std::cout << desc << std::endl;
//			exit(0);
//		}
	}

	int cpu_affinity() const { return cpu_affinity_; }

	static std::vector<std::string>& split(const std::string &s, char delim, std::vector<std::string> &elems)
	{
		std::stringstream ss(s);
		std::string item;
		while (std::getline(ss, item, delim)) {
			elems.push_back(item);
		}
		return elems;
	}

	static std::vector<std::string> split(const std::string &s, char delim)
	{
		std::vector<std::string> elems;
		split(s, delim, elems);
		return elems;
	}

private:
	int cpu_affinity_;
};

} // namespace ham
} // namespace detail


#endif // ham_misc_options
