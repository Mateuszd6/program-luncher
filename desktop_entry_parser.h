#ifndef __DESKTOP_ENTRY_PARSER_H__
#define __DESKTOP_ENTRY_PARSER_H__

#include "string.h"

typedef struct
{
    String name;
    String exec;
} DesktopEntry;

int SaveDesktopEntriesInfoToCacheFile(const char *path_to_cache_file,
                                      DesktopEntry *entries_to_save,
                                      int entries_size);

#endif // __DESKTOP_ENTRY_PARSER_H__
