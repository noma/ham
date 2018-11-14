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
		archive( cereal::binary_data( one, sizeof(char)*1024), cereal::binary_data( two, sizeof(char)*1024));
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

	printf("tin: 1.1 %.10s\n", m1.one);
	printf("tin: 1.2 %.10s\n", m1.two);
	printf("tin: 2.1 %.10s\n", m2.one);
	printf("tin: 2.2 %.10s\n", m2.two);
	printf("tin: 3.1 %.10s\n", m3.one);
	printf("tin: 3.2 %.10s\n", m3.two);

	char* bla = "0123456789";
	strcpy(m1.one, bla);
	strcpy(m1.two, bla);
	char* blub = "ABCDEFGHI";
	strcpy(m2.one, blub);
	strcpy(m2.two, blub);
	strcpy(m3.one, bla);
	strcpy(m3.two, blub);

	printf("tout: 1.1 %.10s\n", m1.one);
	printf("tout: 1.2 %.10s\n", m1.two);
	printf("tout: 2.1 %.10s\n", m2.one);
	printf("tout: 2.2 %.10s\n", m2.two);
	printf("tout: 3.1 %.10s\n", m3.one);
	printf("tout: 3.2 %.10s\n", m3.two);

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

	char* bla = "9876543210";
	strcpy(m1.one, bla);
	strcpy(m1.two, bla);
	char* blub = "IHGFEDCBA";
	strcpy(m2.one, blub);
	strcpy(m2.two, blub);
	strcpy(m3.one, bla);
	strcpy(m3.two, blub);
	
	printf("hout: 1.1 %.10s\n", m1.one);
	printf("hout: 1.2 %.10s\n", m1.two);
	printf("hout: 2.1 %.10s\n", m2.one);
	printf("hout: 2.2 %.10s\n", m2.two);
	printf("hout: 3.1 %.10s\n", m3.one);
	printf("hout: 3.2 %.10s\n", m3.two);

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
	printf("hin: 1.1 %.10s\n", m1.one);
	printf("hin: 1.2 %.10s\n", m1.two);
	printf("hin: 2.1 %.10s\n", m2.one);
	printf("hin: 2.2 %.10s\n", m2.two);
	printf("hin: 3.1 %.10s\n", m3.one);
	printf("hin: 3.2 %.10s\n", m3.two);
	return 0;	
}

