#pragma once

#include <map>
#include <vector>
#include <string>
#include <sstream>

#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_enum.hpp>

/**
 * @file
 * Various generic toString methods for STL data types.
 */

namespace sf {

/// Simple conversion to string if there is an output operator
/// Disabled for enums as they should have their own const char* toString methods.
template <typename T>
 typename boost::disable_if< boost::is_enum<T>, std::string>::type
toString (const T & t){
	std::ostringstream stream;
	stream << t;
	return stream.str();
}

/// Simple conversion to string for sets.
/// This is not defined as an output operator, to not
/// harm users of this library who defined them for theirself.
template <class T> std::string toString (const std::set<T> & s) {
	std::ostringstream stream;
	stream << "(";
	if (!s.empty()) {
		typename std::set<T>::const_iterator i = s.begin();
		stream << *i;
		i++;
		for (; i != s.end(); i++){
			stream << "," << *i;
		}
	}
	stream << ")";
	return stream.str();
}

/// Simple conversion to string for vectors.
/// This is not defined as an output operator, to not
/// harm users of this library who defined them for theirself.
template <class T> std::string toString (const std::vector<T> & s) {
	std::ostringstream stream;
	stream << "[";
	if (!s.empty()) {
		typename std::vector<T>::const_iterator i = s.begin();
		stream << *i;
		i++;
		for (; i != s.end(); i++){
			stream << "," << *i;
		}
	}
	stream << "]";
	return stream.str();
}

template <class A, class B>
std::string toString (const std::multimap<A,B> & multimap){
	std::ostringstream stream;
	bool first = true;
	stream << "{";
	for (typename std::multimap<A,B>::const_iterator i = multimap.begin(); i != multimap.end(); i++){
		if (!first) stream << ", ";
		stream << i->first << " -> " << i->second;
		first = false;
	}
	stream << "}";
	return stream.str();
}

template <class A, class B>
std::string toString (const std::map<A,B> & map) {
	std::ostringstream stream;
	bool first = true;
	stream << "{";
	for (typename std::map<A,B>::const_iterator i = map.begin(); i != map.end(); i++) {
		if (!first) stream << ", ";
		stream << i->first << "->" << i->second;
		first = false;
	}
	stream << "}";
	return stream.str();
}

}
