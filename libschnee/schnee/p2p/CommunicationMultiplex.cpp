#include "CommunicationMultiplex.h"
#include <schnee/tools/Log.h>
#include <schnee/tools/Serialization.h>
#include <schnee/tools/Deserialization.h>
#include "Datagram.h"

namespace sf {

CommunicationMultiplex::CommunicationMultiplex (){
}

CommunicationMultiplex::~CommunicationMultiplex (){
	for (ComponentSet::iterator i = mComponents.begin(); i != mComponents.end(); i++){
		delete *i;
	}
	mCommunicationMap.clear ();
	mComponents.clear();
}

void CommunicationMultiplex::setCommunicationDelegate (CommunicationDelegate * delegate) {
	mCommunicationDelegate = delegate;
	for (ComponentSet::iterator i = mComponents.begin(); i != mComponents.end(); i++){
		(*i)->setDelegate(mCommunicationDelegate);
	}
}

Error CommunicationMultiplex::addComponent (CommunicationComponent * component) {
	const char ** commands = component->commands();

	for (const char ** cmd = commands; *cmd != 0; cmd++){
		if (mCommunicationMap.find(*cmd) != mCommunicationMap.end()){
			sf::Log (LogError) << LOGID << "Command " << *cmd << " is in use, cannot insert " << component->name() << std::endl;
			return error::InvalidArgument;
		}
	}
	for (const char ** cmd = commands; *cmd != 0; cmd++){
		mCommunicationMap[*cmd] = component;
	}

	component->setDelegate (mCommunicationDelegate);
	mComponents.insert (component);
	return NoError;
}

Error CommunicationMultiplex::delComponent (CommunicationComponent * component){
	if (mComponents.count(component) == 0){
		sf::Log (LogError) << LOGID << "Protocol not found!" << std::endl;
		return error::NotFound;
	}
	const char ** commands = component->commands();
	for (const char ** cmd = commands; *cmd != 0; cmd++) {
		mCommunicationMap.erase (*cmd);
	}
	component->setDelegate (0);
	mComponents.erase (component);
	return NoError;
}

std::set<const CommunicationComponent *> CommunicationMultiplex::components () const {
	std::set<const CommunicationComponent*> result;
	for (ComponentSet::const_iterator i = mComponents.begin(); i != mComponents.end(); i++){
		result.insert (*i);
	}
	return result;
}


Error CommunicationMultiplex::dispatch (const HostId & sender, const String & cmd, const sf::Deserialization & ds, const ByteArray & data){
	if (!mCommunicationDelegate) return error::NotInitialized;
	if (ds.error()) {
		Log (LogError) << LOGID << "Could not dispatch header" << std::endl;
		return sf::error::InvalidArgument;
	}

	CommunicationMap::const_iterator i = mCommunicationMap.find (cmd);
	if (i == mCommunicationMap.end()){
		return error::NotFound;
	}
	bool suc = i->second->handleRpc (sender, cmd, ds, data);
	if (!suc) {
		Log (LogWarning) << LOGID << i->second->name() << " could not handle RPC request " << ds << std::endl;
		// do not treat this as an error, could be a bad serialization
	}
	return NoError;
}

void CommunicationMultiplex::distChannelChange (const HostId & host) {
	if (!mCommunicationDelegate) return; // do not distribute, its maybe about to close
	for (ComponentSet::const_iterator i = mComponents.begin(); i != mComponents.end(); i++){
		(*i)->onChannelChange (host);
	}
}

}
