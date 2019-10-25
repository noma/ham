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

std::vector<std::string>& split(const std::string &s, char delim, std::vector<std::string> &elems);
std::vector<std::string> split(const std::string &s, char delim);

class options {
public:
	options(int* argc_ptr, char** argv_ptr[])
	 : app_("HAM-Offload Options"), argc_ptr_(argc_ptr), argv_ptr_(argv_ptr), cpu_affinity_(-1)
	{
		// basic command line options
		app_.allow_extras(); // ignore other options
		app_.set_help_flag("--ham-help", "Print list of HAM-Offload command line options.");
		app_.add_option("--ham-cpu-affinity", cpu_affinity_, "Per process value for the CPU affinity.");
	}

	void parse()
	{
#ifndef HAM_COMM_VE
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
			app_.parse(args);
		}
		else
		{
			// parse from command line
			try {
				app_.parse(*argc_ptr_, *argv_ptr_);
			} catch(const CLI::ParseError &e) {
				app_.exit(e);
			}
			//app.parse(*argc_ptr, *argv_ptr);

			// TODO: fix command line for remaining application
			// TODO: NOTE: we do not get the application command line options on the VE-end
			// fix argc and argv, see C++ standard, see notes
		}
#endif
	}

	bool parsed() { return app_.parsed(); }

	// raw access
	int* argc_ptr() { return argc_ptr_; }
	char*** argv_ptr() { return argv_ptr_; }

	// command line argument getters
	const int& cpu_affinity() const { return cpu_affinity_; }

protected:
	CLI::App app_;

private:
	int* argc_ptr_;
	char*** argv_ptr_;

	int cpu_affinity_;
};

} // namespace ham
} // namespace detail


#endif // ham_misc_options
