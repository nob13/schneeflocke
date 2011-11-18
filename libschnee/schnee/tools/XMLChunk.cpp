#include "XMLChunk.h"
#include <schnee/tools/Log.h>
#include <assert.h>
#include <cstring>

#include <rapidxml/rapidxml.hpp>
#include "rapidxml/rapidxml_print.hpp"

namespace sf {

bool XMLChunk::hasChildWithName (const String & name) const {
	for (ChunkVector::const_iterator i = mChildren.begin(); i != mChildren.end(); i++){
		const XMLChunk & child = *i;
		if (child.name() == name) return true;
	}
	return false;
}

const XMLChunk * XMLChunk::getHasChild (const String & name) const {
	for (ChunkVector::const_iterator i = mChildren.begin(); i != mChildren.end(); i++){
		const XMLChunk & child = *i;
		if (child.name() == name) return &child;
	}
	return 0;
}

XMLChunk XMLChunk::getChild (const String & name) const {
	if (mError) return XMLChunk::errChunk();
	const XMLChunk * child = getHasChild (name);
	if (!child) return XMLChunk::errChunk ();
	return *child;
}

std::vector <const XMLChunk *> XMLChunk::getChildren (const String & name) const {
	std::vector<const XMLChunk*> result;
	for (ChunkVector::const_iterator i = mChildren.begin(); i != mChildren.end(); i++){
		if (i->name() == name){
			result.push_back (&(*i));
		}
	}
	return result;
}

String XMLChunk::getChildText (const String & name, bool * found) const {
	const XMLChunk * child = getHasChild (name);
	if (found) *found = (child != 0);
	if (!child) return "";
	return child->text();
}

void XMLChunk::serialize (sf::Serialization & s) const {
	s ("name", mName);
	s ("ns", mNs);
	s ("attributes", mAttributes);
	s ("text", mText);
	s ("error", mError);
	s ("children", mChildren);
}


/// converts a xml-node tree into XMLChunks...
static XMLChunk convertTree (rapidxml::xml_node<> * node){
	rapidxml::node_type type = node->type();
	if (type != rapidxml::node_document && type != rapidxml::node_element){
		Log (LogError) << LOGID << "Wrong node type " << type << std::endl;
		return XMLChunk::errChunk ();
	}
	
	XMLChunk target;
	target.setName (node->name());
	
	// attributes (iteration until attr is zero)
	for (rapidxml::xml_attribute<> * attr = node->first_attribute(); attr; attr = attr->next_attribute()){
		String name  = attr->name();
		String value = attr->value ();
		if (name == "xmlns"){
			target.setNs(value);
		} else {
			target.attributes().insert (std::pair<String, String> (name, value));
		}
	}
	
	// subelements (iteration until subnode == 0)
	for (rapidxml::xml_node<> * subnode = node->first_node(); subnode; subnode = subnode->next_sibling()){
		switch (subnode->type()){
			case rapidxml::node_document: // won't happen
			case rapidxml::node_element: {
				XMLChunk subchunk = convertTree (subnode);
				if (subchunk.error()) return XMLChunk::errChunk();
				target.children().push_back (subchunk);
			}
			break;
			case rapidxml::node_data:
				target.setText (target.text() + String (subnode->value()));
			break;
			case rapidxml::node_comment:
			case rapidxml::node_declaration:
			case rapidxml::node_doctype:
			case rapidxml::node_pi:
				continue; // ignoring
				break;
			default:
				assert (false); // unknown subtype?
			break;
		}
	}
	return target;
}

String xml::encEntities (const String & data, bool withQuotes) {
	// there is a sample function here: http://doc.trolltech.com/qq/qq05-generating-xml.html
	String s = data;
	boost::algorithm::replace_all (s, "&", "&amp;" );
	boost::algorithm::replace_all (s, ">", "&gt;" );
	boost::algorithm::replace_all (s, "<", "&lt;" );
	if (withQuotes) boost::algorithm::replace_all (s, "\"", "&quot;" );
	if (withQuotes) boost::algorithm::replace_all (s, "\'", "&apos;" );
	return s;
}

String xml::decEntities (const String & data){
	String s = data;
	boost::algorithm::replace_all (s, "&amp;", "&" );
	boost::algorithm::replace_all (s, "&gt;", ">" );
	boost::algorithm::replace_all (s, "&lt;", "<" );
	boost::algorithm::replace_all (s, "&quot;", "\"" );
	boost::algorithm::replace_all (s, "&apos;", "\'" );
	return s;
}


XMLChunk xml::parseDocument (const ByteArray & array) {
	return parseDocument (array.const_c_array(), array.size());
}

XMLChunk xml::parseDocument (const char * data, size_t len) {
	char * begin = (char*) malloc (len + 1);
	memcpy (begin, data, len);
	begin[len] = 0;
	rapidxml::xml_document<> document;
	try {
		const int flags = rapidxml::parse_validate_closing_tags;
		document.parse<flags> (begin);
	} catch (rapidxml::parse_error & error){
		String message = error.what ();
		Log (LogInfo) << LOGID << "Parsing error during parsing of " << begin << ":" << message << std::endl;
		free (begin);
		return XMLChunk::errChunk();
	}

	// Log (LogInfo) << LOGID << "Parsed the document " << array << " to " << std::endl;
	// rapidxml::print (std::cout, document);

	// as we parse a whole document, it's ok, that there are multiple XML elements inside
	// so chunk won't be just one element
	XMLChunk chunk = convertTree (&document);
	free (begin);
	return chunk;
}


/// Finds first non-white-space character
static size_t beginOfNext (const ByteArray & txt){
	size_t length = txt.size();
	size_t b = 0;
	for (b = 0; b < length && (txt[b] == ' ' || txt[b] == '\t' || txt[b] == '\n'); b++);
	return b;
}

int xml::completionDetection (const ByteArray & txt) {
	size_t length = txt.size ();
	
	size_t b = beginOfNext(txt); // skipping whitespace at the beginning
	
	if (b == length) return 0; //  no begin   (no full element)
	if (txt[b] != '<') return -2;     // no opening (cannot be an element)
	if (txt[b + 1] == 0) return 0; // -3; // to short
	
	bool inCommentary = false; // whether we are in a commentary block (<!-- ... -->)
	bool inText       = false; // whether we are in a text block like ".." or '..'
	bool inTextType   = false; // false means current text type is "..."; true means '...'
	bool inTag        = false;
	bool inSpecialTag = false; // specialTags are like <? .. ?> (e.g. the XML beginning), we will ignore them
	
	int depth  = 0;
	for (size_t i = b; i < length; i++){
		int rest = length - i;
		char c = txt[i];
		char d = (rest > 1) ? txt[i+1] : 0; // maybe \0
		char e = (rest > 2) ? txt[i+2] : 0;
		char f = (rest > 3) ? txt[i+3] : 0;
		
		// std::cout << "Iteration " << i << " current=" << c << " next=" << d << e << f << " depth=" << depth << " inCommentary = " << inCommentary << " inText = " << inText << " inTextType=" << inTextType << " inTag=" << inTag << " inSpecialTag=" << inSpecialTag << std::endl;
		
		if (c == 0) return -8; // no \NULL allowed
		
		// ignorieren of comments
		if (inCommentary) {
			if (c == '-' && d == '-' && e =='>'){
				inCommentary = false;
				i+=2;
			}
			continue;
		}
		// ignoring of special tags
		if (inSpecialTag) {
			if (c == '?' && d == '>'){
				inSpecialTag = false;
				i+=1;
			}
			continue;
		}
		// ignoring text
		if (inText) {
			if ((c == '\"' && !inTextType) || (c == '\'' && inTextType)) {
				inText = false;
			}
			continue;
		}
		// begin of comments
		if (c == '<' && d == '!' && e == '-' && f == '-') {
			inCommentary = true;
			i+=3;
			continue;
		}
		// begin of special tags
		if (c == '<' && d == '?') {
			inSpecialTag = true;
			i+=1;
			continue;
		}
		// begin of text, type ".."
		if (c == '\"'){
			inText = true;
			inTextType = false;
			continue;
		}
		// end von text, typ ''
		if (c == '\''){
			inText = true;
			inTextType = true;
			continue;
		}
		// starting a tag
		if (c == '<'){
			if (inTag) return -4; // starting tag inside tag
			if (d == '/'){
				depth--;
			} else depth++;
			inTag = true;
		}
		// closing symbol without being in a tag
		if (c == '>' && !inTag) {
			return -5; // closing '>' without being in a tag
		}
		// closing a shortened tag
		if (c == '/' && d == '>'){
			if (!inTag) return -6; // closing '/>' without being in a tag
			i++;
			depth--;
			inTag = false;
		}
		// closing a tag (regular)
		if (c == '>' && inTag){
			inTag = false;
		}

		if (depth < 0) return -7; // more closing tags than opening tags?
		if (depth == 0 && !inTag) return (i+1); // ok, we are ready
	}
	return 0;
}

int xml::fullTag (const ByteArray & text){
	size_t length = text.size();
	size_t b = beginOfNext (text); // beginning
	// searching beginning
	if (b == length) return 0; // only newspaces
	if (text[b] != '<') return -1; // invalid starting
	bool inText   = false;
	bool textType = false; // false means "..", true means '..'
	for (size_t i = b; i < length; i++){
		char c = text[i];
		if (c == '\"'){
			if (inText && !textType) inText = false;
			else if (!inText) { inText = true; textType = false; }
		}
		if (c== '\''){
			if (inText && textType)  inText = false;
			else if (!inText) { inText = true; textType = true; }
		}
		if (c == '>' && !inText){
			return i + 1; // ok, we are ready
		}
	}
	return 0;
}

int xml::xmlBeginning (const ByteArray & text){
	size_t b = beginOfNext (text);
	size_t e = fullTag (text);
	if (e < 0 || e == 0) return e;
	size_t l = e - b;
	if (l < 6) return -1; // to short
	const char * begin = text.const_c_array() + b;
	const char * end   = text.const_c_array() + l - 2;
	if (std::strncmp (begin, "<?xml", 5) != 0) return -2; // no right beginning
	if (std::strncmp (end, "?>", 2) != 0) return -3; // no right ending
	return l;
}


}

std::ostream & operator<< (std::ostream & stream, const sf::XMLChunk & chunk){
	stream << sf::toJSONEx (chunk, sf::COMPRESS);
	return stream;
}


