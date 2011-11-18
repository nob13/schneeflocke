#include <schnee/schnee.h>
#include <schnee/im/xmpp/XMLStreamDecoder.h>
#include <schnee/tools/async/MemFun.h>
#include <schnee/test/test.h>

using namespace sf;


struct Collector {
	std::vector<XMLChunk> received;
	std::vector<XMLStreamDecoder::State> stateChanges;
	typedef XMLStreamDecoder::State State;

	Collector () {
		stream.chunkRead() = memFun (this, &Collector::onChunk);
		stream.stateChange () = memFun (this, &Collector::onStateChange);
	}

	void onChunk (const XMLChunk & chunk) {
		received.push_back (chunk);
	}

	void onStateChange () {
		stateChanges.push_back (stream.state());
	}
	void write (const String & s) {
		stream.onWrite(ByteArray(s));
	}

	State state () { return stream.state(); }
	const XMLChunk & opener () { return stream.opener(); }
private:
	XMLStreamDecoder stream;

};

int validStart () {
	Collector c;
	tcheck (c.state() == XMLStreamDecoder::XS_Start, "Start State");
	c.write ("<?xml version=\"");
	tcheck (c.state() == XMLStreamDecoder::XS_Start, "");
	c.write ("1.0\"?>");
	tcheck (c.state() == XMLStreamDecoder::XS_ReadXmlBegin, "");
	c.write ("<open");
	tcheck (c.state() == XMLStreamDecoder::XS_ReadXmlBegin, "");
	c.write ("er>");
	tcheck (c.state() == XMLStreamDecoder::XS_ReadOpener, "");
	tcheck (c.opener().name() == "opener", "");
	c.write ("<element>Hi!</element>");
	tcheck (c.received.size() == 1 && c.received[0].name() == "element", "Could not read first element");
	c.write ("<and>Bla<sub>yeah</sub></and>");
	tcheck (c.received.size() == 2 && c.received[1].name() == "and", "Could not read 2nd element");
	c.write ("</opener>");
	tcheck (c.state() == XMLStreamDecoder::XS_Closed, "Did not detect closing");
	return 0;
}

int strangeProtocol () {
	Collector c;
	c.write ("brevlmlm");
	tcheck (c.state() == XMLStreamDecoder::XS_Error, "");
	Collector c2;
	c2.write ("<?xml version=\"1.0\"?>  renterntknt4kntkntkln");
	tcheck (c2.state() == XMLStreamDecoder::XS_Error, "");
	return 0;
}

int whiteSpaces () {
	Collector c;
	c.write ("  \t \n   <?xml   \t \n version=\"1.0\"   ?>    \t \n <opener      >  \t \n  <element/>  </opener>");
	tcheck (c.state() == XMLStreamDecoder::XS_Closed, "Was valid!");
	tcheck (c.received.size() == 1 && c.received[0].name() == "element", "");
	return 0;
}

int main (int argc, char * argv[]){
	sf::schnee::SchneeApp app (argc, argv);
	int ret = 0;
	testcase (validStart());
	testcase (strangeProtocol());
	testcase (whiteSpaces());
	return ret;
}
