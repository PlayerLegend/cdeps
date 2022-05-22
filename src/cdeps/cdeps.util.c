#include "../convert/getline.h"
#include "../convert/fd/source.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include "../range/string.h"
#include "../range/path.h"
#include "../window/string.h"
#include "../window/path.h"
#include "../window/alloc.h"
#include "../log/log.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include "../table/string.h"
#include "../table/pointer.h"
#include <string.h>

#define PATH_SEPARATOR '/'

typedef enum {
    PATH_ERROR,
    PATH_FILE,
    PATH_DIR,
    PATH_UNKNOWN,
}
    path_target;

map_string_type_declare(path);
#define depends_value_clear(arg)
map_pointer_def(depends, path_pair*, struct{});
typedef depends_table path_value;
map_string_type_define(path);
void path_value_clear(path_value * value)
{
    depends_table_clear(value);
}
map_string_function_define(path);

typedef struct {
    map_string_query * origin;
    map_string_query * target;
}
    dependency_item;

range_typedef(dependency_item, dependency_item);
window_typedef(dependency_item, dependency_item);

typedef struct {
    path_table table;
    window_dependency_item pairs;
}
    dependency_list;

void dependency_list_add(dependency_list * dest, const range_const_char * origin, const range_const_char * target)
{
    path_pair * origin_pair = path_include_range(&dest->table, origin);
    path_pair * target_pair = path_include_range(&dest->table, target);

    depends_link ** set_point = depends_seek(&origin_pair->value, target_pair);

    if (*set_point)
    {
	return;
    }

    *set_point = depends_link_alloc(&origin_pair->value, target_pair);

    *window_push(dest->pairs) = (dependency_item) { &origin_pair->query, &target_pair->query };
}

int dependency_compar(const void * a, const void * b)
{
    const dependency_item * a_item = a;
    const dependency_item * b_item = b;

    int retval = strcmp (a_item->origin->key.string, b_item->origin->key.string);

    if (retval)
    {
	return retval;
    }
    else
    {
	return strcmp (a_item->target->key.string, b_item->target->key.string);
    }
}

void dependency_list_print(dependency_list * dest)
{
    qsort(dest->pairs.region.begin, range_count(dest->pairs.region), sizeof(*dest->pairs.region.begin), dependency_compar);

    const dependency_item * item;

    for_range(item, dest->pairs.region)
    {
	log_normal (RANGE_FORMSPEC ".o: " RANGE_FORMSPEC, range_count(item->origin->key.range) - 2, item->origin->key.range.begin, RANGE_FORMSPEC_ARG(item->target->key.range));
    }
}

void dependency_list_clear(dependency_list * target)
{
    path_table_clear(&target->table);
    window_clear(target->pairs);
}

path_target path_stat(const char * path)
{
    struct stat s;

    if (stat (path, &s) < 0)
    {
	perror(path);
	return PATH_ERROR;
    }

    if (S_ISREG(s.st_mode))
    {
	return PATH_FILE;
    }
    else if (S_ISDIR(s.st_mode))
    {
	return PATH_DIR;
    }
    else
    {
	return PATH_UNKNOWN;
    }
}

void toggle_comment (bool * comment, range_const_char * text)
{
    range_const_char text_less = { .begin = text->begin, .end = text->end - 1 };
    const char * i;

    if (range_count(*text) < 2 || (text->begin[0] == '/' && text->begin[1] == '/'))
    {
	return;
    }
    
    for_range (i, text_less)
    {
	if (*comment == false && i[0] == '/' && i[1] == '*')
	{
	    *comment = true;
	}

	if (*comment == true && i[0] == '*' && i[1] == '/')
	{
	    *comment = false;
	    text->begin = i + 2;
	}
    }
}

bool list_depends(dependency_list * output, const range_const_char * path, const range_const_char * origin)
{
    window_unsigned_char contents = {0};

    fd_source fd_source = fd_source_init (open (path->begin, O_RDONLY), &contents);

    range_const_char end_sequence = { .begin = "\n", .end = end_sequence.begin + 1 };

    range_const_char line = {0};

    status status;

    range_const_char token;

    range_const_char keyword_include;

    range_string_init(&keyword_include, "#include");

    window_char next_path = {0};

    bool retval = false;
    
    if (fd_source.fd < 0)
    {
	perror (path->begin);
	log_fatal ("Could not open source file " RANGE_FORMSPEC, RANGE_FORMSPEC_ARG(*path));
    }

    bool is_comment = false;

    while ( (status = convert_getline(&line, &fd_source.source, &end_sequence)) == STATUS_UPDATE )
    {
	toggle_comment (&is_comment, &line);

	if (is_comment)
	{
	    continue;
	}
	
	if (!range_string_tokenize(&token, ' ', &line))
	{
	    continue;
	}

	if (!range_streq(&token, &keyword_include))
	{
	    continue;
	}

	if (!range_string_tokenize(&token, ' ', &line))
	{
	    log_fatal ("#include has no argument");
	}

	if (range_count(token) < 2)
	{
	    log_fatal ("#include argument is too short");
	}

	if (*token.begin != '"')
	{
	    continue;
	}
	
	token.begin++;
	token.end--;

	window_strcpy_range(&next_path, path);
	range_dirname(&next_path.region.const_cast, PATH_SEPARATOR);
	window_path_cat(&next_path, PATH_SEPARATOR, &token);
	window_path_resolve(&next_path, PATH_SEPARATOR);

	assert(range_count(*origin) >= 2);

	dependency_list_add(output, origin, &next_path.region.const_cast);
        
	if (!list_depends(output, &next_path.region.const_cast, origin))
	{
	    log_fatal ("Failed dependency of " RANGE_FORMSPEC, RANGE_FORMSPEC_ARG(*path));
	}
    }

    if (status == STATUS_ERROR)
    {
	log_fatal ("Error reading " RANGE_FORMSPEC, RANGE_FORMSPEC_ARG(*path));
    }

    retval = true;
    goto end;
    
fail:

    retval = false;
    goto end;

end:
    convert_source_clear(&fd_source.source);
    window_clear(contents);
    window_clear(next_path);
    return retval;
}

bool list_depends_from_path(dependency_list * output, const range_const_char * path)
{
    if (range_count(*path) < 2 || path->end[-2] != '.' || path->end[-1] != 'c')
    {
	return true;
    }

    return list_depends(output, path, path);
}

bool for_file_sub (dependency_list * output, const char * root_path, window_char * path)
{
    DIR * dir = opendir(root_path);

    if (!dir)
    {
	perror(root_path);
	log_fatal ("Could not open dir %s", root_path);
    }

    struct dirent * ent;

    errno = 0;

    range_const_char dir_name;

    while ( (ent = readdir(dir)) )
    {
	if (ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0')))
	{
	    continue;
	}
	
	range_string_init (&dir_name, ent->d_name);
	window_path_cat(path, PATH_SEPARATOR, &dir_name);
	{
	    switch (path_stat(path->region.begin))
	    {
	    case PATH_FILE:
		if (!list_depends_from_path(output, &path->region.const_cast))
		{
		    log_fatal ("File callback failed for %s", path->region.const_cast);
		}
		break;
		
	    case PATH_DIR:
		if (!for_file_sub(output, path->region.const_cast.begin, path))
		{
		    goto fail;
		}
		break;
		
	    case PATH_ERROR:
		log_fatal ("Error determining path type");

	    case PATH_UNKNOWN:
	    default:
		break;
	    }
	}
	range_dirname (&path->region.const_cast, PATH_SEPARATOR);
    }

    closedir (dir);

    if (errno)
    {
	perror(root_path);
	return false;
    }
    else
    {
	return true;
    }

fail:
    return false;
}

bool for_file (dependency_list * output, const char * root_path)
{
    window_char path = {0};

    window_strcpy_string(&path, root_path);

    bool result = for_file_sub (output, root_path, &path);

    window_clear(path);

    return result;
}

int main(int argc, char * argv[])
{
    if (argc < 2)
    {
	log_fatal ("Usage: %s [path]", argv[0]);
    }

    dependency_list list = {0};

    for (int i = 1; i < argc; i++)
    {
	if (!for_file(&list, argv[i]))
	{
	    log_fatal ("Failed to list dependencies");
	}
    }
	
    dependency_list_print(&list);

    dependency_list_clear(&list);

    return 0;

fail:
    return 1;
}
