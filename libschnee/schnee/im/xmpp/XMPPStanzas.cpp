#include "XMPPStanzas.h"

#include <schnee/tools/Base64.h>
#include <schnee/tools/Serialization.h>
#include <schnee/tools/XMLChunk.h>

namespace sf {
namespace xmpp {

bool decodeStanza (IMClient::Stanza & stanza, const XMLChunk & elem){
	stanza.from = elem.getAttribute ("from");
	stanza.to   = elem.getAttribute ("to");
	return true;
}

bool PresenceInfo::decode (const XMLChunk & chunk){
	if (!decodeStanza(*this, chunk)) return false;
	const XMLChunk * prioChunk = chunk.getHasChild ("priority");
	if (prioChunk){
		priority = atoi (prioChunk->text().c_str());
	} else {
		priority = 0;
	}
	const XMLChunk * showChild = chunk.getHasChild ("show");
	if (showChild){
		String show = showChild->text();
		if (show == "chat"){
			state = IMClient::PS_CHAT;
		} else
		if (show == "away"){
			state = IMClient::PS_AWAY;
		} else 
		if (show == "xa"){
			state = IMClient::PS_EXTENDED_AWAY;
		} else
		if (show == "dnd"){
			state = IMClient::PS_DO_NOT_DISTURB;
		} else {
			Log (LogWarning) << LOGID << "Unknown <show> state in <presence> message: " << show;
			state = IMClient::PS_UNKNOWN;
		}
	} else state = IMClient::PS_UNKNOWN; // allowed
	type = chunk.getAttribute ("type");
	if (type == "unavailable"){
		if (state != IMClient::PS_UNKNOWN){
			Log (LogError) << LOGID << "Incoherent data" << std::endl;
		}
		state = IMClient::PS_OFFLINE;
	}
	const XMLChunk * errChunk = chunk.getHasChild ("error");
	if (errChunk) {
		errorType = errChunk->getAttribute("type");
		errorText = errChunk->getChildText("text");
	}
	const XMLChunk * statusChild = chunk.getHasChild ("status");
	if (statusChild){
		status = statusChild->text();
	}
	return true;
}

static const char * encodeState (IMClient::PresenceState state) {
	switch (state){
	case IMClient::PS_UNKNOWN:
		return 0;
	case IMClient::PS_CHAT:
		return "chat";
	case IMClient::PS_AWAY:
		return "away";
	case IMClient::PS_DO_NOT_DISTURB:
		return "dnd";
	case IMClient::PS_EXTENDED_AWAY:
		return "xa";
	case IMClient::PS_OFFLINE:
		return "offline"; // ?
	default:
		Log (LogError) << LOGID << "Unknown state " << state << std::endl;
	break;
	}
	return 0;
}

String PresenceInfo::encode() const {
	std::ostringstream ss;
	ss << "<presence";
	if (!from.empty())     ss << " from='" << from << "'";
	if (!to.empty())       ss << " to='" << to << "'";
	if (!type.empty())     ss << " type='" << type << "'";
	ss << ">";
	const char * stateString = encodeState (state);
	if (stateString) {
		ss << "<show>" << stateString << "</show>";
	}
	if (priority != 0) ss << "<priority>" << priority << "</priority>";
	if (!status.empty()) ss << "<status>" << xml::encEntities (status) << "</status>";
	if (!errorType.empty() || !errorText.empty()){
		ss << "<error type='" << errorType << "'><text>" << xml::encEntities (errorText) << "</text></error>";
	}

	ss << "</presence>";
	return ss.str();
}

bool Iq::decode (const XMLChunk & chunk) {
	if (!decodeStanza(*this, chunk)) return false;
	id   = chunk.getAttribute ("id");
	if (id == ""){
		Log (LogWarning) << LOGID << "No/empty 'id' attribute in IqResult, discarding";
		return false;
	}
	type = chunk.getAttribute ("type");
	return true;
}

String Iq::encode () const {
	std::ostringstream ss;
	ss << "<iq";
	if (!id.empty()) ss << " id='" << id << "'";
	if (!type.empty()) ss << " type='" << type << "'";
	if (!from.empty()) ss << " from='" << from << "'";
	if (!to.empty()) ss << " to='" << to << "'";
	ss << ">";
	ss << encodeInner();
	ss << "</iq>";
	return ss.str();
}

String ErrorIq::encodeInner () const {
	std::ostringstream ss;
	ss << "<error";
	if (!errorType.empty()) ss << " type='" << xml::encEntities(errorType) << "'";
	ss << ">" << errorCondition << "</error>";
	return ss.str();
}

bool RosterIq::decode (const XMLChunk & chunk) {
	if (!Iq::decode(chunk)) return false;
	const XMLChunk * qr = chunk.getHasChild ("query");
	if (!qr) {
		Log (LogWarning) << LOGID << "No <query> child in " << chunk << std::endl;
		return false;
	}
	if (qr->ns() != "jabber:iq:roster"){
		Log (LogWarning) << LOGID << "Query has the wrong namespace " << qr->ns ()  << " (in chunk=" << chunk << ")";
		return false;
	}
	
	std::vector<const XMLChunk*> xitems = qr->getChildren ("item");
	for (std::vector<const XMLChunk*>::const_iterator i = xitems.begin(); i != xitems.end(); i++){
		Item item;
		item.jid  = (*i)->getAttribute("jid");
		item.name = (*i)->getAttribute("name");
		
		String subscription = (*i)->getAttribute("subscription");
		if (subscription.empty() || subscription == "none")
			item.subscription = IMClient::SS_NONE;
		if (subscription == "to")
			item.subscription = IMClient::SS_TO;
		if (subscription == "from") 
			item.subscription = IMClient::SS_FROM;
		if (subscription == "both")
			item.subscription = IMClient::SS_BOTH;
		if (subscription == "remove") {
			item.remove = true;
		}
		if ((*i)->getAttribute ("ask") == "subscribe")
			item.waitForSubscription = true;
		
		if (item.jid == ""){
			Log (LogWarning) << LOGID << "item in Roster result may not have a null jid tag, chunk=" << chunk;
			continue;
		}
		items.push_back (item);
	}
	return true;
}

bool DiscoInfoIq::decode (const XMLChunk & chunk) {
	if (!Iq::decode(chunk)) return false;

	const XMLChunk * queryNode = chunk.getHasChild("query");
	if (!queryNode) return false;
	std::vector<const XMLChunk*> identities = queryNode->getChildren("identity");
	for (std::vector<const XMLChunk*>::const_iterator i = identities.begin(); i != identities.end(); i++) {
		const XMLChunk * identity (*i);
		if (identity->getAttribute("category") == "client"){
			clientType = identity->getAttribute ("type");
			clientName = identity->getAttribute ("name");
		}
	}
	std::vector<const XMLChunk*> names = queryNode->getChildren("feature");
	for (std::vector<const XMLChunk*>::const_iterator i = names.begin(); i != names.end(); i++) {
		features.push_back((*i)->getAttribute("var"));
	}
	return true;
}

String DiscoInfoIq::encode () const {
	std::ostringstream ss;
	ss << "<iq";
	if (!id.empty())   ss << " id='" << id << "'";
	if (!to.empty())   ss << " to='" << to << "'";
	if (!type.empty()) ss << " type='" << type << "'";
	ss << "><query xmlns='http://jabber.org/protocol/disco#info'>";
	if (!clientType.empty() && !clientName.empty()){
		ss << "<identity category='client' type='" << xml::encEntities (clientType) << "' name='" << xml::encEntities(clientName) << "'/>";
	}
	for (std::vector<String>::const_iterator i = features.begin(); i != features.end(); i++) {
		ss << "<feature var='" << xml::encEntities(*i) << "'/>";
	}
	ss << "</query></iq>";
	return ss.str();
}

bool Message::decode (const XMLChunk & chunk){
	if (!decodeStanza (*this, chunk)) return false;
	String t = chunk.getAttribute ("type");
	if (t == "") {
		type = IMClient::MT_NORMAL;
	} else {
		if (t == "normal") type = IMClient::MT_NORMAL;
		else if (t == "chat") type = IMClient::MT_CHAT;
		else if (t == "error") type = IMClient::MT_ERROR;
		else if (t == "groupchat") type = IMClient::MT_GROUPCHAT;
		else if (t == "headline") type = IMClient::MT_HEADLINE;
		else {
			type = IMClient::MT_NORMAL;
		}
	}

	
	// may have multiple childs with name subject .. confusing
	// we still take the first
	subject = xml::decEntities (chunk.getChildText("subject"));
	body    = xml::decEntities (chunk.getChildText("body"));
	thread  = xml::decEntities (chunk.getChildText("thread"));
	id      = chunk.getAttribute("id");
	const XMLChunk * dataChild = chunk.getHasChild("data");
	if (dataChild){
		if (dataChild->ns() != "urn:xmpp:bob"){
			Log (LogWarning) << LOGID << "Found data child in message, but it does not have a urn:xmpp:bob namespace, ignoring";
		} else {
			data = DataBlockPtr (new DataBlock ());
			String ageS = dataChild->getAttribute("age");
			if (ageS == "") data->age = -1; else data->age = atoi (ageS.c_str());
			data->cid  = dataChild->getAttribute ("cid");
			data->type = dataChild->getAttribute ("type");
			Base64::decodeToArray (dataChild->text(), data->data);
		}
	}
	return true;
}

void Message::encode (String & target) const {
	target.reserve (512);
	std::ostringstream stream;
	stream << "<message";
	if (!from.empty()){
		stream << " from='" << xml::encEntities(from) << "'";
	}
	if (!to.empty()){
		stream << " to='" << xml::encEntities(to) << "'";
	}
	if (!id.empty()){
		stream << " id='" << xml::encEntities (id) << "'";
	}
	if (type != IMClient::MT_NORMAL){
		// note we have to be sure that messageTypeToString behaves like xmpp
		stream << " type='" << IMClient::messageTypeToString(type) << "'";
	}
	stream << ">";
	/* We decode without quotes, this is maybe not standard compatible, but faster and better to read */
	if (!subject.empty()){
		stream << "<subject>" << xml::encEntities (subject, false) << "</subject>";
	}
	if (!body.empty()){
		stream << "<body>" << xml::encEntities(body, false) << "</body>";
	}
	stream << "</message>";
	target = stream.str();
}

void serialize (Serialization & s, const DataBlockPtr & block) {
	if (block) {
		return serialize (s, *block);
	} else {
		s.insertValue ("null");
	}
}


}
}


