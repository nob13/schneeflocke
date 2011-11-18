#pragma once

#include <schnee/sftypes.h>
#include <map>
#include <schnee/tools/Serialization.h>

namespace sf {

/** An short parsed XML snipped based on rapid XML
	Don't feed it with large XML data, as it will copy each small element in std::string data...
	
	Note: the XML Chunk does not decode any XML entities; use encXmlEntities and decXmlEntities for that.
*/
class XMLChunk {
public:
	/// A map containing XML element attributes
	typedef std::map<String, String> XMLAttributes;
	/// Multiple (child) XML elements
	typedef std::vector<XMLChunk> ChunkVector;
	
	XMLChunk () : mError (false) {}
	
	/// Constructs an XMLChunk with error flag set
	static XMLChunk errChunk () { XMLChunk c; c.setError (true); return c; }
	
	/// returns name
	const String& name () const { return mName;}
	/// returns namespace
	const String& ns () const { return mNs; }
	/// returns attributes
	const XMLAttributes& attributes () const { return mAttributes; }
	/// write access to the attributes...
	XMLAttributes & attributes () { return mAttributes; }
	/// returns text field
	const String & text () const { return mText; }
	/// returns error flag
	bool error () const { return mError; }

	/// Returns the children of the element
	const ChunkVector& children () const { return mChildren; }
	/// write access to the children...
	ChunkVector & children () { return mChildren; }
	
	
	/// sets name
	void setName (const String & name) { mName = name; }
	/// sets namespace
	void setNs (const String & ns) { mNs = ns; }
	/// sets attributes
	void setAttributes (const XMLAttributes & attr) { mAttributes = attr;}
	/// sets the text field
	void setText (const String & text) { mText = text; }
	/// sets error flag
	void setError (bool err) { mError = err; }
	
	
	// convenience functions
	
	/** Returns one attribute, returns a null string if attribute doesn't exist
		@param name   - name of the attribute
		@param wasSet - if you pass in a pointer to a bool, you will know, whether the attribute was set.
		@return - the value of the attribute or "" if not found 
	*/
	String getAttribute (const String & name, bool * wasSet = 0) const {
		XMLAttributes::const_iterator i = mAttributes.find (name);
		if (i == mAttributes.end()){
			if (wasSet) *wasSet = false;
			return "";
		}
		if (wasSet) *wasSet = true;
		return i->second;
	}
	
	/// searches for a child with a specific name, returns true if it exists
	bool hasChildWithName (const String & name) const;
	
	/// returns a pointer to the child (if it exists, or 0 if no existance).
	/// this may be faster than the other get/has operations
	const XMLChunk * getHasChild (const String & name) const;

	/// returns a pointer to the first child found with the name
	/// if no child was found a Chunk with error flag set is returned
	/// this function is not very fast
	XMLChunk getChild (const String & name) const;
	
	/// returns all children with specific name
	std::vector<const XMLChunk*> getChildren (const String & name) const;
	
	/// Short getting the child's text (returns "" if child not found) 
	/// @param name of the child (the first will be used)
	/// @param found if not null, state (child found or not) will be written in here
	String getChildText (const String & name, bool * found = 0) const;
	
	void serialize (sf::Serialization & s) const;
private:
	String mName;
	String mNs;
	String mText;
	XMLAttributes mAttributes;
	
	bool mError;						///< error during parsing
	ChunkVector mChildren;	///< children elements
};

namespace xml {

/// encode xml entities (like '<')
/// If withQuotes = true, then it will also encode \" to &quot; and \' to &apos;
String encEntities (const String &, bool withQuotes = true);
/// decode xml entities (like '<')
String decEntities (const String &);


/// Parses a whole document
XMLChunk parseDocument (const ByteArray & array);

/// Parses a whole document
XMLChunk parseDocument (const char * data, size_t len);

/// Shall test if a XML Element is fully presented (until closing tag) inside the XML stream
/// returns the number of characters until full detection or <0 in case of an error or 0 if no full xml element was found
int completionDetection (const ByteArray & text);

/// Scans for a full XML Tag (begins with '<' and ends with '>' and ignores inside text; not verifying)
/// @return <0: error; ==0 incomplete; >0 number of characters until end of the tag.
int fullTag (const ByteArray & text);

/// Scans for a begin of a XML Document
/// @return <0: error; ==0 incomplete; >0 number of characters until end of XML beginning
int xmlBeginning (const ByteArray & text);

}

}

/// (debug) output operator
std::ostream & operator<< (std::ostream & stream, const sf::XMLChunk & chunk);


