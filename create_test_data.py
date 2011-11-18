#!/usr/bin/python
import os;
import random;

prefix = "testbed"

# Directories
def createDirectories():
	dirs = [ prefix, prefix + "/dir1", prefix + "/dir2", prefix + "/dir3", prefix + "/dir4" ];
	for d in dirs:
		os.mkdir (d)
	
	subdirs = ["dir1", "dir2", "dir3", "dir4" ]
	for d in subdirs:
		os.mkdir (prefix + "/dir1/"+d);

# Testfiles
def createFile (name, length):
	print "    Create ", name, " with length of ", length;
	f = open (prefix + "/" + name, "wb");
	for i in range (0, length):
		f.write (chr (random.randint (0,255)))
	f.close()
	
print "1. Create Directories";
createDirectories ()
print "2. Create Files";
createFile ("a", 128);
createFile ("b", 23434);
createFile ("emptyfile", 0);
createFile ("dir1/a", 342);
createFile ("dir1/b", 2345670);
createFile ("dir1/c", 3425455);
createFile ("dir1/d", 2323244);
createFile ("dir1/dir2/filea.bla", 213213);
createFile ("dir1/dir2/fileb.bla", 213213);
createFile ("dir1/dir2/filec.bla", 213213);
createFile ("dir1/dir2/filed.bla", 213213);
createFile ("dir1/dir2/filee.bla", 213213);

print "3. Create Save folder";
# for copying operations
os.mkdir (prefix + "/save")

print "4. Create Glob test directory loop";
# for Globber test (symlink loop)
os.mkdir (prefix +   "/globber");
os.mkdir (prefix +   "/globber/subdir");
os.mkdir (prefix +   "/globber/other");
os.mkdir (prefix +   "/globber/a");
os.mkdir (prefix +   "/globber/subdir/subdir2");
os.mkdir (prefix +   "/globber/subdir/subdir2/subdir3");
os.symlink ("../../..", prefix + "/globber/subdir/subdir2/subdir3/backtohell");
# God sake; no hardlink support for directories anymore (http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=136621)

