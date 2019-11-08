// Copyright (c) 2013-2014 Matthias Noack (ma.noack.pr@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef ham_msg_msg_handler_registry_abi_hpp
#define ham_msg_msg_handler_registry_abi_hpp

#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <ostream>
#include <typeinfo>
#include <vector>

namespace ham {
namespace msg {

/**
 * Implementation of the msg_handler_registry concept. It allows constant
 * time mapping between message types and handlers in both directions.
 * It supports message exchange between heterogeneous binaries using a 
 * compatible ABI, i.e. the used compilers generate the same typeid name 
 * (clang, gcc and icc use the same ABI, that defines the mangled function
 * name to be returned by typeid(T)->name().
 *
 * On static data initialisation, each message type is registered via its
 * typeid name, and its handler address in a map.
 * When init() is called (in the programme's main function), the handler
 * addresses are copied into a vector in the alphabetical order of their
 * names. The alphabetical order of the unique names guarantees that all
 * binaries, generated from the same source, will have the addresses for the
 * same handlers at the same indices. Accordingly the key for this message
 * registry implementation is a size_t index.
 *
 * After initialisation, each message type holds its handler index in a static
 * member that is used on construction. Thus type-to-handler-key lookups are
 * O(1). On the remote side, the index is used to get the handler from the
 * vector in O(1).
 *
 * This class is a monostate (i.e. everything is static).
 */
class msg_handler_registry_abi {
public:
	using key_type = size_t;
	using handler_type = void (*)(void*);
	using set_key_type = void (*)(key_type);
	using handler_map_value_type = std::pair<handler_type, set_key_type>;
	using handler_map_type = std::map<std::string, handler_map_value_type>;
	using handler_vector_type = std::vector<handler_type>;

	static void init()
	{
		// we use the map order defined by std::less<handler_map_type::key_type> to build a simple array which can be accessed in O(1) by index
		// the index stored in active_msg::handler_vector_index_static by using a function pointer to the static setter
		handler_map_type& handler_map = get_handler_map();
		handler_vector_type& handler_vector = get_handler_vector();
		size_t index = 0;
		for (auto& key_value_pair : handler_map)
		{
			handler_vector.push_back(key_value_pair.second.first); // store the handler
			key_value_pair.second.second(index); // this actually calls active_msg::set_handler_index()
			++index;
		}
	}

	static handler_type get_handler(key_type index)
	{
		return get_handler_vector()[index];
	}

	template<class Msg>
	static key_type register_handler() {
		get_handler_map().insert(std::pair<std::string,handler_map_value_type>(typeid(Msg).name(),
							   handler_map_value_type(&Msg::handler, &Msg::set_handler_key)));
		return INVALID_KEY_VALUE;
	}

	static void print_handler_map(std::ostream& out)
	{
		handler_map_type& handler_map = get_handler_map();
		out << "==================== BEGIN HANDLER MAP =====================" << std::endl;
		for (auto& key_value_pair : handler_map)
		{
			out << "typeid_name: " << key_value_pair.first << ",\t"
				      << "handler_address: " << key_value_pair.second.first << std::endl;
		}
		out << "==================== END HANDLER MAP =======================" << std::endl;
	}

	static void print_handler_vector(std::ostream& out)
	{
		handler_vector_type& handler_vector = get_handler_vector();
		out << "==================== BEGIN HANDLER VECTOR ==================" << std::endl;
		for (size_t i = 0; i < handler_vector.size(); ++i)
		{
			out << "index: " << i << ",\t"
				      << "handler_address: " << handler_vector[i] << std::endl;
		}
		out << "==================== END HANDLER VECTOR ====================" << std::endl;
	}



protected:
	/**
	 * "Construct On First Use"-idiom.
	 * Maps from typeid-name to handler address. Filled prior to main when
	 * ActiveMsg::handler_vector_index_static is initialised. Needed to build
	 * handler_vector.
	 */
	static handler_map_type& get_handler_map() {
		static handler_map_type handler_map;
		return handler_map;
	}
	
	/**
	 * "Construct On First Use"-idiom.
	 * Vector (array) of all handlers. Used to map a handler index in constant
	 * time to the corresponding handler address. Filled by 
	 * active_msg_base::init().
	 */
	static handler_vector_type& get_handler_vector() {
		static handler_vector_type handler_vector;
		return handler_vector;
	}

	static const key_type INVALID_KEY_VALUE { std::numeric_limits<key_type>::max() };
};

} // namespace msg
} // namespace ham

#endif // ham_msg_msg_handler_registry_abi_hpp
