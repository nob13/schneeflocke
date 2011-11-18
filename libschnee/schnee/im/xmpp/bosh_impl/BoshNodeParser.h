#pragma once
#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_print.hpp>
#include <schnee/sftypes.h>

///@cond DEV

namespace sf {

class BoshNodeParser {
public:
	BoshNodeParser ();
	~BoshNodeParser ();

	/// Parses the of an Bosh-Chunk
	/// Note: can only be done once.
	Error parse (const String & content) {
		return parse (content.c_str(), content.length());
	}

	/// Parses the of an Bosh-Chunk
	/// Note: can only be done once.
	Error parse (const char * content, size_t length);

	/// Returns an attribute
	String attribute (const char * name) const;

	/// Returns the (unparsed) contents
	String content () const;

private:
	BoshNodeParser (const BoshNodeParser &);
	BoshNodeParser& operator= (const BoshNodeParser&);

	rapidxml::xml_document<> mDocument;
	rapidxml::xml_node<> * mBody;
	char * mData;
};

}

///@endcond DEV

