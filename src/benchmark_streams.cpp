// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"
#include "ham/offload/stream.hpp"

#include <boost/program_options.hpp>

#include "cereal/archives/binary.hpp"

#include "ham/util/time.hpp"

#include <array>
#include <iostream>

using namespace std;
using namespace ham::util::time;
using namespace ham;

// this is set in main. locally for host, through offload for target
// it is ugly, but is used to remove the allocation of the user-buffer from the benchmarked time,
// because we want to measure the overhead of the streaming abstraction, not how long it takes to instantiate user data
class cheese {
public:
	static char* d1;
	static size_t cheese_size;
};
char* cheese::d1 = nullptr;
size_t cheese::cheese_size = 0;

void set_cheese(size_t size) {
	posix_memalign((void**)&cheese::d1, constants::CACHE_LINE_SIZE, size);
	cheese::cheese_size = size;
}

ham::offload::stream::stream_proxy offloaded_fun(ham::offload::stream::stream_proxy osp)
{
	ham::offload::stream::istream his(osp);

    {
        cereal::BinaryInputArchive iarchive(his);

		iarchive(cereal::binary_data(cheese::d1, sizeof(char)*cheese::cheese_size));
	}

	//if(cheese::d1[1337] == 'a') cheese::d1[1337] = 'b';
	ham::offload::stream::ostream hos(0, cheese::cheese_size);
    {
        cereal::BinaryOutputArchive oarchive(hos);
        oarchive(cereal::binary_data(cheese::d1, sizeof(char)*cheese::cheese_size));
    }
	auto out_proxy = hos.sync();
    return out_proxy;
}

int main(int argc, char* argv[])
{
	// option defaults
	unsigned int warmup_runs = 1;
	unsigned int runs = 1000;
	size_t data_size = 1024*1024;

	// command line options
	boost::program_options::options_description desc("Supported options");
	desc.add_options()
			("help,h", "Shows this message")
			("runs,r", boost::program_options::value(&runs)->default_value(runs), "number of identical inner runs for which the average time will be computed")
			("warmup-runs", boost::program_options::value(&warmup_runs)->default_value(warmup_runs), "number of number of additional warmup runs before times are measured")
			("size,s", boost::program_options::value(&data_size)->default_value(data_size), "size of transferred data in byte (multiple of 4)")
			;

	boost::program_options::variables_map vm;

	boost::program_options::store(boost::program_options::command_line_parser(argc, argv).options(desc).allow_unregistered().run(), vm);
	boost::program_options::notify(vm);

	ham::offload::node_t target = 1;

	// used to avoid benchmarking memory allocation for the target object on the target side
	set_cheese(data_size);
	ham::offload::ping(target, f2f(&set_cheese, data_size));

	statistics comp_time(runs, warmup_runs);
	statistics put_time(runs, warmup_runs);
	statistics call_time(runs, warmup_runs);
	statistics get_time(runs, warmup_runs);
	statistics os_time(runs, warmup_runs);
	statistics is_time(runs, warmup_runs);

	for (int i = 0; i < (runs + warmup_runs) ; ++i) {
		//cheese::d1[1337] = 'a';
		timer comp;
		ham::offload::stream::ostream hos(target, cheese::cheese_size);
		timer ost;
		{
			cereal::BinaryOutputArchive oarchive(hos);
			oarchive(cereal::binary_data(cheese::d1, sizeof(char)*data_size)); //sizeof(char)*data_size)
		}
		os_time.add(ost);
		timer put;
		auto out_proxy = hos.sync();
		put_time.add(put);
		timer call;
		auto in_proxy = ham::offload::sync(target, f2f(&offloaded_fun, out_proxy));
		call_time.add(call);
		timer get;
		ham::offload::stream::istream his(in_proxy);
		get_time.add(get);
		timer ist;
		{
			cereal::BinaryInputArchive iarchive(his);
			iarchive(cereal::binary_data(cheese::d1, sizeof(char)*data_size));
		}
		is_time.add(ist);
		comp_time.add(comp);
		//assert(cheese::d1[1337] == 'b');
	}

	std::string header_string = "name\t" + statistics::header_string() + "\tdata_size";

	cout << endl <<"HAM-Offload stream overall: " << endl
	     << header_string << endl
	     << "stream:\t" << comp_time.string() << "\t" << data_size << endl << endl;
	cout << "HAM-Offload streams ostream: " << endl
	     << header_string << endl
	     << "stream:\t" << os_time.string() << "\t" << data_size << endl << endl;
	cout << "HAM-Offload streams copy-in: " << endl
	     << header_string << endl
	     << "stream:\t" << put_time.string() << "\t" << data_size << endl << endl;
	cout << "HAM-Offload streamed call: " << endl
	     << header_string << endl
	     << "stream:\t" << call_time.string() << "\t" << data_size << endl << endl;
	cout << "HAM-Offload streamed copy-out: " << endl
	     << header_string << endl
	     << "stream:\t" << get_time.string() << "\t" << data_size << endl << endl;
	cout << "HAM-Offload streamed istream: " << endl
	     << header_string << endl
	     << "stream:\t" << is_time.string() << "\t" << data_size << endl << endl;


	statistics str_time(1, 0);

	ham::offload::stream::ostream hos(target, cheese::cheese_size);
	{
		cereal::BinaryOutputArchive oarchive(hos);
		oarchive(cereal::binary_data(cheese::d1, sizeof(char)*data_size)); //sizeof(char)*data_size)
	}
	timer str_tim;

		string tmp = hos.rdbuf()->str();

	str_time.add(str_tim);
	statistics cpy_time(1, 0);
	statistics cstr_time(1, 0);
	timer cpy_tim;
	memcpy((void *) cheese::d1, tmp.c_str(), cheese::cheese_size);
	cpy_time.add(cpy_tim);
	timer cstr_tim;
	const char* asdf = tmp.c_str();
	cstr_time.add(cstr_tim);
	cout << str_time.string() << endl;
	cout << cpy_time.string() << endl;
	cout << cstr_time.string() << endl;
	return 0;
}

