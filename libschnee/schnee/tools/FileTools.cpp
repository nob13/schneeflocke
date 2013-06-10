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
	return fs::exists(fs::path(path));
}

bool isDirectory (const String & path) {
	return fs::is_directory(fs::path(path));
}

bool isSymlink (const String & path) {
	return fs::is_symlink (fs::path(path));
}

bool isRegularFile (const String & path) {
	return fs::is_regular_file (fs::path(path));
}

String fileStem (const String & path) {
	fs::path p (path);
	String parentPath (p.parent_path().string());
	if (!parentPath.empty()) return parentPath + gDirectoryDelimiter + p.stem().string();
	return p.stem().string();
}

String fileExtension (const String & path){
	return fs::path (path).extension().string();
}

String pathFilename (const String & path) {
	return fs::path(path).filename().string();
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
	} catch (fs::filesystem_error & e){
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
	} catch (fs::filesystem_error & e){
		return error::WriteError;
	}
	return NoError;
}


}
