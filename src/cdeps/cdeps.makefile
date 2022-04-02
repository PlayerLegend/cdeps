C_PROGRAMS += bin/cdeps
SH_PROGRAMS += test/run-cdeps

bin/cdeps: \
	src/cdeps/cdeps.util.o \
	src/log/log.o \
	src/window/alloc.o \
	src/range/streq.o \
	src/range/string_tokenize.o \
	src/convert/getline.o \
	src/convert/source.o \
	src/convert/fd/source.o \
	src/range/string_init.o \
	src/range/path.o \
	src/range/strchr.o \
	src/range/strstr.o \
	src/window/path.o \
	src/window/string.o \
	src/range/strdup.o \
	src/range/streq_string.o \
	src/table/pointer.o \
	src/table/string.o \
	src/range/alloc.o \

test/run-cdeps: src/cdeps/test/run.sh

cdeps-utils: bin/cdeps

depend: cdeps-depend
cdeps-depend:
	cdeps src/cdeps > src/cdeps/depends.makefile

run-tests: run-cdeps-tests
run-cdeps-tests:
	DEPENDS=cdeps-utils sh run-tests.sh test/run-cdeps
