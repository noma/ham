// Copyright (c) 2013-2018 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_misc_options
#define ham_misc_options

#include <CLI/CLI11.hpp>
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
		CLI::App app("HAM Options");
		app.add_option("--ham-cpu-affinity", cpu_affinity_, "Per process value for the CPU affinity.");

		// NOTE: no try-catch here to avoid exceptions, that cause offload-dependencies to boost exception in the MIC code
		const char* options_env = std::getenv("HAM_OPTIONS");
		if(options_env)
		{
			char split_character = ' ';
			if (std::getenv("HAM_OPTIONS_NO_SPACES")) // value does not matter
				split_character = '_';
			// parse from environment
			// NOTE: get a vector strings from the environment options
			auto args = split(std::string(options_env), split_character);
			app.parse(args);
		}
		else
		{
			// parse from command line
			try {
				app.parse(argc, argv);
			} catch(const CLI::ParseError &e) {
				app.exit(e);
			}
			//app.parse(argc, argv);
		}
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
