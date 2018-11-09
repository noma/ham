// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "ham/offload.hpp"
#include "ham/offload/stream.hpp"

#include "cereal/archives/binary.hpp"

#include <array>
#include <iostream>

struct MyData {

	char one[1024];
	char two[1024];

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive( one, two );
	}
};

// alternative: nicer, with proxy
// target
ham::offload::stream::stream_proxy offloaded_fun(ham::offload::stream::stream_proxy osp)
{
	ham::offload::stream::istream his(osp); // NOTE: data is already on the target

    MyData m1, m2, m3;
    {
        cereal::BinaryInputArchive iarchive(his); // Create an input archive

		iarchive(m1, m2, m3); // Read the data from the archive
	}

	char* bla = "0123456789";
	strcpy(m1.one, bla);
	strcpy(m1.two, bla);
	char* blub = "ABCDEFGHI";
	strcpy(m2.one, blub);
	strcpy(m2.two, blub);
	strcpy(m2.one, bla);
	strcpy(m2.two, blub);


	ham::offload::stream::ostream hos(0);

    {
        cereal::BinaryOutputArchive oarchive(hos);
        oarchive(m1, m2, m3);
    }

	auto out_proxy = hos.sync();

    return out_proxy;
}

int main(int argc, char* argv[])
{
	ham::offload::node_t target = 1;

	ham::offload::stream::ostream hos(target);

	MyData m1, m2, m3; // could be out of scope, data to be transferred

	{
		cereal::BinaryOutputArchive oarchive(hos); // Create an output archive
		oarchive(m1, m2, m3); // Write the data to the archive
	} // archive goes out of scope, ensuring all contents are flushed
	// after this scope, data from oarchive is flushed into the stream, stream can be used

	auto out_proxy = hos.sync(); // trigger transfer to target (write has other meaning with streams)

	auto in_proxy = ham::offload::sync(target, f2f(&offloaded_fun, out_proxy));

    ham::offload::stream::istream his(in_proxy);

	{
		cereal::BinaryInputArchive iarchive(his);
		iarchive(m1, m2, m3);
	}

	printf("%.10s\n", m1.one);
	printf("%.10s\n", m1.two);
	printf("%.10s\n", m2.one);
	printf("%.10s\n", m2.two);
	printf("%.10s\n", m3.one);
	printf("%.10s\n", m3.two);
	return 0;	
}

