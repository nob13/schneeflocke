#include "BoshNodeBuilder.h"

namespace sf {

/*
 * Content Insertion algorithm:
 * I found no way to insert pure content into rapidxml without fully parsing
 * traversing and inserting each element by itself.
 *
 * I also wanted not to change the rapidxml implementation to intrudoce a new node type.
 *
 * So I use a magic text word which gets replaced by the content.
 * If it happens that anyone inserts that magic keyword as an attribute
 * it gets not inserted. This is not a perfect solution but shall work.
 */
const char * g_MagicContentMarker = "CONTENTCOMESHERE"; // very unusual as attribute or attribute value

BoshNodeBuilder::BoshNodeBuilder () {
		mBody = mDoc.allocate_node(rapidxml::node_element, "body", g_MagicContentMarker);
		mDoc.append_node(mBody);
	}

void BoshNodeBuilder::addAttribute (const String & name, const String & value) {
	if (name == g_MagicContentMarker || value == g_MagicContentMarker) {
		assert (!"This name is reserved!");
		return;
	}
	mBody->append_attribute (mDoc.allocate_attribute(mDoc.allocate_string(name.c_str()), mDoc.allocate_string(value.c_str())));
}

void BoshNodeBuilder::addContent (const ByteArrayPtr & content) {
	mContents.push_back (content);
}

String BoshNodeBuilder::toString () const {
	// serializing XML
	std::ostringstream ss;
	ss << mDoc;

	// serializing content
	std::ostringstream serializedContent;
	for (std::vector<ByteArrayPtr>::const_iterator i = mContents.begin(); i != mContents.end(); i++) {
		const ByteArray & ba (*(*i));
		serializedContent << String (ba.const_c_array(), ba.size());
	}

	// replacing the magic marker
	std::string base = ss.str();
	size_t p = base.find(g_MagicContentMarker);
	if (p == base.npos){
		assert (!"Should not happen");
		return String();
	}
	base.replace(p, strlen (g_MagicContentMarker), serializedContent.str());
	return base;
}

}
