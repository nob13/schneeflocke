# A interactive test which cannot be automated
macro (add_interactive_test name)
	string (REPLACE "/" "_" exec_name ${name})
	message (STATUS " - Adding: ${name}  --> ${exec_name}")
	
	file (GLOB_RECURSE src_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${name}/*.cpp)
	file (GLOB_RECURSE header_files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${name}/*.h)
	
	set (all_files ${src_files} ${header_files})
	sfautoreflect_decorate_auto (all_files)

	add_executable (${exec_name} ${all_files})
	target_link_libraries (${exec_name} ${LIBS})
endmacro ()

# A automatable testcase
macro (add_automatic_test name)
	add_interactive_test (${name}) # links and builds it, saves exec-name in ${exec_name}
	add_test (${exec_name} ${exec_name})
endmacro ()

# Automatic Tests
add_automatic_test (schnee/tools/delegate_test)
add_automatic_test (schnee/tools/log_test)
add_automatic_test (schnee/tools/parse_xml_test)
add_automatic_test (schnee/tools/async_ops)
add_automatic_test (schnee/tools/path)
add_automatic_test (schnee/tools/bind_demo)	
add_automatic_test (schnee/net/tcptest)
add_automatic_test (schnee/net/udpechoclient)
add_automatic_test (schnee/net/udptest)
add_automatic_test (schnee/net/udtsocket)
add_automatic_test (schnee/net/tls)
add_automatic_test (schnee/net/http)

add_automatic_test (schnee/im/xmpp_contacts)
add_automatic_test (schnee/im/xml_stream)
add_automatic_test (schnee/im/xmpp_stream)
add_automatic_test (schnee/im/xmpp_connection)

add_automatic_test (schnee/p2p/channels)
add_automatic_test (schnee/p2p/channelholder)
add_automatic_test (schnee/p2p/interplex)
add_automatic_test (schnee/p2p/transmission_test)
add_automatic_test (schnee/p2p/datasharingbasics)
add_automatic_test (schnee/p2p/async_stream)

add_automatic_test (flocke/tools/globtest)
add_automatic_test (flocke/sharedlists/sharedlists)
add_automatic_test (flocke/filesharing/filesharing)

# Interactive Tests
add_interactive_test (schnee/im/xmpp_bosh)
