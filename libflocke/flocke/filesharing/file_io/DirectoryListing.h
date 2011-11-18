#pragma once
#include <schnee/sftypes.h>
#include <sfserialization/autoreflect.h>
#include <assert.h>

namespace sf {

/// Represents a directory listing
struct DirectoryListing {
	enum EntryType { File, Directory };

	/// One Dirlisting Entry
	struct Entry {
		Entry () : type (File),size(0) {}
		String name;
		EntryType type;
		int64_t size;
		bool operator != (const Entry & other) const {
			return name != other.name ||
				   type != other.type ||
				   size != other.size;
		}
		SF_AUTOREFLECT_SD;
	};

	typedef std::vector<Entry> EntryVector;
	EntryVector entries;	///< Directory entries

	SF_AUTOREFLECT_SD;
};
SF_AUTOREFLECT_ENUM (DirectoryListing::EntryType);

/// Generate a directory listing
Error listDirectory (const String & directory, DirectoryListing * out);


}
