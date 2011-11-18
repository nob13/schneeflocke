#pragma once
#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_print.hpp>
#include <schnee/sftypes.h>

///@cond DEV

namespace sf {

/// Helps building bosh nodes
class BoshNodeBuilder {
public:
	BoshNodeBuilder ();

	/// Adds an attribute to the body tag
	void addAttribute (const String & name, const String & value);

	/// Add (XML-parseable!) content
	/// The content must be a string (it's type is just for compatibility with Channel class)
	void addContent (const ByteArrayPtr & content);

	/// Serializes the whole node into the final Bosh XML Snippet.
	String toString () const;

private:
	rapidxml::xml_document<> mDoc;
	rapidxml::xml_node<> * mBody;
	std::vector<ByteArrayPtr> mContents;
};

}

///@endcond DEV
