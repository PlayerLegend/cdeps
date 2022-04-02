# CDEPS

This program finds C source file dependencies resulting from relative path includes and writes the corresponding makefile dependency rules to stdout.

# DESCRIPTION

Usage: cdeps [directory] [directory] ...

Given one or more directories as arguments, cdeps will search each directory for .c files and print out makefile dependency rules.

The purpose of this program is to provide a similar utility to makedepend but without some of makedepend's idiosynrasies, namely its unordered output and inability to write to a terminal rather than a file.

# Dependencies

The standard C library