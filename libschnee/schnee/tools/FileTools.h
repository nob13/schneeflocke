#pragma once
#include <schnee/sftypes.h>

/**
 * @file
 * Some common file operations (mostly wrapped from boost)
 */

namespace sf {

/// Directory delimiter on the platform
extern const char gDirectoryDelimiter;

///@name Working on logical paths
///@{


/// Converts internal path representation (with '/') to System path representation (possibly with '\\')
String toSystemPath (const String & internPath);

///@}

///@name Working on system paths
///@{

/// Returns true if file (or directory) exists
bool fileExists  (const String & path);

/// Returns true if path is a directory
bool isDirectory (const String & path);

/// Path is a symlink
bool isSymlink (const String & path);

/// Returns true if path is a regular file
bool isRegularFile (const String & path);

/// Returns the given path without the extension (and without the '.')
/// Note: in contrast to boost::file_system::path.stem () it also includes the
/// file path (so /foo/bar.tgz.xy will give you /foo/bar.tgz)
String fileStem (const String & path);

/// Returns the (last) file extension, including the .
String fileExtension (const String & path);

/// Returns just the filename of a given path
String pathFilename (const String & path);

/// Returns just the parent path of a given path
String parentPath (const String & path);

/// Check a itnernal path for potential dangerous things (like /..)
/// Returns true on potential dangerous content
bool checkDangerousPath (const String & path);

/// Creates a directory (like mkdir)
Error createDirectory (const String & directory);

/// Create the directories for a given file path
/// (All directories shall be created but not pathFilename (path))
Error createDirectoriesForFilePath (const String & path);



///@}
}
