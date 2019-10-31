// Copyright (c) 2015-2017 Matthias Noack <ma.noack.pr@gmail.com>
//
// See accompanying file LICENSE and README for further information.

#ifndef noma_memory_array_to_file_hpp
#define noma_memory_array_to_file_hpp

#include <string>
#include <fstream>

namespace noma {
namespace misc {

/**
Outputs an array as simple text file. One value per line.
*/
template<typename T>
void array_to_file(std::string filename, T array, size_t size)
{
	std::ofstream ofs(filename);
	ofs.precision(17);
	ofs << std::scientific;
	for (size_t i = 0; i < size; ++i) {
		ofs << array[i];
		ofs << std::endl;
	}
	ofs.close();
};

} // namespace misc
} // namespace noma

#endif // noma_misc_array_to_file_hpp
