#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"
using namespace rapidxml;

#include  <iostream>
#include <string.h>
#include <assert.h>

#include <schnee/schnee.h>
#include <schnee/tools/XMLChunk.h>

/*
 * Tests the rapidxml library and the XML completion detection which is used when
 * parsing XML streams.
 */



/// Parsed den Text, gibt true bei erfolg zurück und gibt es aus
bool parseAndPrint (const char* txt, bool assumeError = false){
	char * cpyWork = (char*) malloc (strlen (txt) + 1);
	strcpy (cpyWork, txt);
	xml_document<> document; // default type = char
	const int parameters = 0 | parse_validate_closing_tags; // 0 steht für default parsing type...
	try {
		document.parse<parameters> (cpyWork);
	} catch (parse_error err) {
		free (cpyWork);
		if (!assumeError){
			std::cerr << "Parsing Error: " << err.what() << std::endl;
		} else {
			std::cout << "Assumed Error, got one: " << err.what() << " (Okay)" << std::endl;
		}
		return false;
	}
	print (std::cout, document);
	free (cpyWork);
	if (assumeError) {
		std::cerr << "Assumed an error, got none!" << std::endl;
	}
	return true;
}

bool cmpLength (const char * x, bool assumeError, bool assumeUnclosed) {
	int el = sf::xml::completionDetection(sf::ByteArray (x));
	int l  = strlen (x);
	if (el < 0) {
		if (!assumeError) 
			std::cerr << "Found an error=" << el<<" (but did not assumed one) in " << x << std::endl;
		return false;
	}
	if (el == 0) {
		if (!assumeUnclosed)
			std::cerr << "Found no closing (but did not assuming it) in " << x << std::endl;
		return false;
	}
	if (el > l) {
		std::cerr << "Part is too big in " << x << std::endl;
		assert (false); // this is really critical
		return false;
	}
	if (assumeError) {
		std::cerr << "Assumed an error, and did not find one" << std::endl;
		return false;
	}
	if (assumeUnclosed) {
		std::cerr << "Assumed no closing, but did find one" << std::endl;
	}
	std::cout << "  Got length " << el <<  "(of=" << l << ")" << std::endl;
	char * part = (char*) malloc (el + 1);
	part[el] = 0;
	memcpy (part, x, el);
	std::cout << "  Part of " << x << " is " << part << std::endl;
	free (part);
	return true;
}

int main (int argc, char * argv[]){
	/*
	Ein paar Tests wie man das Qt XML Gedöns durch andere Bibliotheken ersetzen kann
	- probieren von RapidXML, soll wohl sehr schnell sein, braucht nur includiert werden
	- Schauen ob es XML-Teile geparsed werden können
	- Schauen wie man solange warten kann, bis ein vollständiger XML-Block vorhanden ist, ohne erst den Parser anzuwerfen
	- Schauen wie man das ganze dann API-kompatibel mit XMLChunk bekommt
	- Schauen wie man den ganzen eröffnungsquatsch abfangen kann, ohne einen eigenen Parser dafür zu schreiben...
	
	- Diskutierbar: Für uns ist es wohl besser, wenn wir non-destructive verwenden, da ansonsten der speicher überschrieben wird und wir eine Kopie der Daten machen müssten, falls jemand an die Originaldaten ranwill (will dass den jemand)
	- Auffallend, der validiert nicht besonders gut (muss man stück für stück einschalten)
	- den header scheint er erstmal wegzulassen...
	*/
	
	const char * testcase1 = "<message id=\"bla\"><body tag='hochkomma'><!-- comment -->\nLalala</body></message>"; // shall be Ok
	const char * testcase2 = "<message><id></message></id>"; // should give an error
	const char * testcase3 = "<message><id>bla</id><!-- commentary --></id>"; // sudden end error
	const char * testcase4 = "<presence from='enob2@shodan/shodan' to='enob1@shodan/schneeflocke' xml:lang='en'><show>chat</show><status>bla</status><priority>5</priority><c xmlns='http://jabber.org/protocol/caps' node='http://psi-im.org/caps' ver='0.12.1' ext='cs ep-notify html'/></presence>"; // real live example with namespaces
	const char * testcase5 = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\
<verzeichnis>\
     <titel>Wikipedia Städteverzeichnis</titel>\
     <eintrag>\
          <stichwort>Genf</stichwort>\
          <eintragstext>Genf ist der Sitz von ...</eintragstext>\
     </eintrag>\
     <eintrag>\
          <stichwort>Köln</stichwort>\
          <eintragstext>Köln ist eine Stadt, die ...</eintragstext>\
     </eintrag>\
</verzeichnis>\
";
	const char * testcase6  = "<part id=\"commentary' grerg' \">rubbel</xmlr>"; // shall find the part, also its not well
	const char * testcase7  = "<aafdsfsdfk><!-- Kommentare--><shortened /></aafdsfsdfk <!--Commentary -->><multiple/>";
	const char * testcase8  = "<shortened/>";
	const char * testcase9  = "<short with <? special ?>/></bla>"; // shall bring no error
	const char * testcase10 = "<long <long <long>>>"; // shall bring an error
	const char * testcase11 = "</only closing>"; // shall bring an error
	const char * testcase12 = "<opening><anotheropening><short/></anotheropening></opening><rest>"; // shall work
	
	const char * testcase13 = "<opening name='bab'> <id> bla </id> <x name=\"...\"></x>"; // incomplete
	const char * testcase14 = "<opening name='bab'> <id> bla </id> <x name=\"...\"><"; // incomplete
	const char * testcase15 = "<opening name='bab'> <id> bla </id> <x name=\".."; // incomplete
	const char * testcase16 = "<opening name='bab'> <id> bla </id> <x name"; // incomplete
	const char * testcase17 = "<opening name='bab'> <id> bla </id>"; // incomplete
	const char * testcase18 = "<opening name='bab'> <id"; // incomplete
	

	parseAndPrint (testcase1, false);
	parseAndPrint (testcase2, true);
	parseAndPrint (testcase3, true);
	parseAndPrint (testcase4, false);
	parseAndPrint (testcase5, false);
	
	
	
	cmpLength (testcase1, false, false);
	cmpLength (testcase2, false, false); // it doesn't check the right names of the tags
	cmpLength (testcase3, false, false); // also doesn't check the names
	cmpLength (testcase4, false, false);
	cmpLength (testcase5, false, false);
	cmpLength (testcase6, false, false);
	cmpLength (testcase7, false, false);
	
	
	cmpLength (testcase8, false, false);
	cmpLength (testcase9, false, false);
	cmpLength (testcase10, true, false);
	cmpLength (testcase11, true, false);
	cmpLength (testcase12, false, false);
	
	cmpLength (testcase13, false, true);	
	cmpLength (testcase14, false, true);	
	cmpLength (testcase15, false, true);	
	cmpLength (testcase16, false, true);	
	cmpLength (testcase17, false, true);	
	cmpLength (testcase18, false, true);	

	return 0;
}

