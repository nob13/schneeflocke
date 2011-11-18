#include "BoshNodeParser.h"
#include <schnee/tools/Log.h>

#include <string.h>
namespace sf {

BoshNodeParser::BoshNodeParser () {
	mBody = 0;
	mData = 0;
}

BoshNodeParser::~BoshNodeParser () {
	delete [] mData;
}

Error BoshNodeParser::parse (const char * content, size_t length) {
	if (mData) return error::WrongState;
	mData = new char [length + 1];
	memcpy (mData, content, length);
	mData[length] = 0;

	// NOTE: Strings will NOT be zero-terminated!!
	// Array will not changed by RapidXML.
	// The good thing: we could easily extract the content
	const int flags = rapidxml::parse_validate_closing_tags | rapidxml::parse_non_destructive;
	try {
		mDocument.parse<flags>(mData);
	} catch (rapidxml::parse_error & error){
		String message = error.what ();
		Log (LogWarning) << LOGID << "Parsing error " << error.what() << std::endl;
		return error::BadProtocol;
	}
	mBody = mDocument.first_node("body");
	if (!mBody || mBody->type() != rapidxml::node_element) {
		Log (LogWarning) << LOGID << "Found no body element" << std::endl;
		return error::BadProtocol;
	}
	return NoError;
}

String BoshNodeParser::attribute (const char * name) const {
	assert (mBody);
	if (!mBody) return String();
	rapidxml::xml_attribute<> * attr = mBody->first_attribute(name);
	if (!attr) return String();
	return String (attr->value(), attr->value_size());
}


String BoshNodeParser::content () const {
	assert (mBody);
	if (!mBody) return String();
	// Note: This function could be faster if we would parse by hand
	std::ostringstream ss;
	rapidxml::xml_node<> * n = mBody->first_node();
	while (n) {
		ss << *n;
		n = n->next_sibling();
	}
	return ss.str();
}


}
