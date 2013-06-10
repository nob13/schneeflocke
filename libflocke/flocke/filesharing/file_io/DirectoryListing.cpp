#include "DirectoryListing.h"

#include <schnee/tools/FileTools.h>
#include <schnee/tools/Log.h>
#include <boost/filesystem/operations.hpp>
#include <boost/system/error_code.hpp>

namespace fs = boost::filesystem;

namespace sf {

Error listDirectory (const String & directory, DirectoryListing * out) {
	if (!out) return error::InvalidArgument;
	if (!sf::isDirectory(directory)) return error::InvalidArgument;
	
	fs::directory_iterator end;
	int errors = 0;

	fs::path p (directory);
	boost::system::error_code ec;
	fs::directory_iterator i (p, ec);
	if (ec) return error::NotFound;
	for (; i != end; i++){
		DirectoryListing::Entry e;
		try {
			e.name = i->path().filename().string();
			if (fs::is_regular_file (i->status())){
				e.type = DirectoryListing::File;
				e.size = fs::file_size (*i);
			} else
				if (fs::is_directory (i->status())){
					e.type = DirectoryListing::Directory;
					e.size = 0;
				} else continue; // no support for other types yet
		} catch (fs::filesystem_error & err){
			if (err.code().value() == ELOOP){
				Log (LogWarning) << LOGID << " too many symbilic links; ignoring" << std::endl; // ignoring
				continue;
			}
			Log (LogWarning) << LOGID << "Could not read sub path of " << directory << " " << err.what() << std::endl;
			errors++;
			continue;
		}
		out->entries.push_back (e);
	}
	if (errors > 0) return error::MultipleErrors;
	return NoError;
}


}
