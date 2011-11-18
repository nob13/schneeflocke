#include "FileTools.h"

#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

namespace sf {

#ifdef WIN32
const char gDirectoryDelimiter = '\\';
#else
const char gDirectoryDelimiter = '/';
#endif

bool fileExists  (const String & path) {
	return fs::exists(path);
}

bool isDirectory (const String & path) {
	return fs::is_directory(path);
}

bool isSymlink (const String & path) {
	return fs::is_symlink (path);
}

bool isRegularFile (const String & path) {
	return fs::is_regular_file (path);
}

String fileStem (const String & path) {
	fs::path p (path);
	String parentPath (p.parent_path().string());
	if (!parentPath.empty()) return parentPath + gDirectoryDelimiter + p.stem();
	return p.stem();
}

String fileExtension (const String & path){
	return fs::path (path).extension();
}

String pathFilename (const String & path) {
	return fs::path(path).filename();
}

String parentPath (const String & path) {
	return fs::path(path).parent_path().string();
}

String toSystemPath (const String & internPath) {
	String toUse = internPath;
#ifdef WIN32
	boost::replace_all (toUse, "/", "\\");
#endif
	return toUse;
}

static bool contains (const String & s, const char * pattern){
	return s.find (pattern) != s.npos;
}

bool checkDangerousPath (const String & path) {
	if (contains (path, "/../")) return true; 
	// Note: Win32 needs probably more checking in here
	if (contains (path, "\\..\\")) return true;
	return false;
	
}

Error createDirectory (const String & directory) {
	try {
		fs::create_directory (fs::path(directory));
	} catch (fs::basic_filesystem_error<fs::path> & e){
		return error::WriteError;
	}
	return NoError;
}

Error createDirectoriesForFilePath (const String & path) {
	fs::path p (path);
	fs::path parent = p.parent_path();
	if (parent.empty()) return NoError;
	try {
		fs::create_directories (parent);
	} catch (fs::basic_filesystem_error<fs::path> & e){
		return error::WriteError;
	}
	return NoError;
}


}
