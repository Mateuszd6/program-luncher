#include <stdio.h>

#include "util.h"

// TODO: Refactor these!
#define DESKTOP_NAME_FIELD "Name="
#define DESKTOP_NAME_FIELD_SIZE (5)
#define DESKTOP_TYPE_FIELD "Type="
#define DESKTOP_TYPE_FIELD_SIZE (5)
#define DESKTOP_EXEC_FIELD "Exec="
#define DESKTOP_EXEC_FIELD_SIZE (5)
#define DESKTOP_TERM_FIELD "Terminal="
#define DESKTOP_TERM_FIELD_SIZE (9)
#define DESKTOP_NO_DISPLAY_FIELD "NoDisplay="
#define DESKTOP_NO_DISPLAY_FIELD_SIZE (10)

static int blacklisted_entries_number = 0;
static int blacklisted_entries_capacity = 0;

static int extra_entries_number = 0;
static int extra_entries_capacity = 0;

static char **blacklisted_entries;

static char **extra_entries_names;
static char **extra_entries_execs;

static char *terminal_command = "i3-sensible-terminal -e";

int SaveDesktopEntriesInfoToCacheFile(const char *path_to_cache_file,
                                      DesktopEntry *entries_to_save,
                                      int entries_size)
{
    FILE *cacheFile = fopen(path_to_cache_file, "w+b");
    if (!cacheFile)
        return 0;

    // TODO: handle errors properly!!!
    int tmp = 0;

    fwrite("MMCF", sizeof(char) * 4, 1, cacheFile);
    fwrite(&tmp, sizeof(int), 1, cacheFile);
    fwrite(&entries_size, sizeof(int), 1, cacheFile);

    for (int i = 0; i < entries_size; ++i)
    {
        char *name = StringGetText(&entries_to_save[i].name);
        size_t name_size = strlen(name);
        char *exec = StringGetText(&entries_to_save[i].exec);
        size_t exec_size = strlen(exec);

        fwrite(&name_size, sizeof(size_t), 1, cacheFile);
        fwrite(name, name_size, 1, cacheFile);
        fwrite(&exec_size, sizeof(size_t), 1, cacheFile);
        fwrite(exec, exec_size, 1, cacheFile);
    }

    fclose(cacheFile);
    return 1;
}

// TODO: Move to menu.c once it is cleaned up.
static int LoadEntriesFromDotDesktop(const char *path,
                                     DesktopEntry **result_desktop_entries,
                                     int *result_desktop_entries_size)
{
    DIR *dir;
    struct dirent *ent;
    char buffer[256];

    int result_idx = 0, result_max_len = 64;
    DesktopEntry *result = malloc(sizeof(DesktopEntry) * result_max_len);

    if ((dir = opendir(path)) != NULL)
    {
        size_t len = 256;
        char *line = malloc(sizeof(char) * len);
        int nread;

#if 0
        result[result_idx++] = (DesktopEntry){
            MakeStringCopyText("SSH MIM", strlen("SSH MIM")),
            MakeStringCopyText(
                "gnome-terminal -- ssh md394171@students.mimuw.edu.pl",
                strlen("gnome-terminal -- ssh md394171@students.mimuw.edu.pl"))};
#endif
        // TODO: Add abilitty to blacklist these.
        for (int i = 0; i < extra_entries_number; ++i)
        {
            fprintf(stderr,
                    "Adding desktop entry: %s -> %s specified in the program arguments.\n",
                    extra_entries_names[i], extra_entries_execs[i]);

            result[result_idx++] = (DesktopEntry){
                MakeStringCopyText(extra_entries_names[i], strlen(extra_entries_names[i])),
                MakeStringCopyText(extra_entries_execs[i], strlen(extra_entries_execs[i]))
            };
        }

        // print all the files and directories within directory
        while ((ent = readdir(dir)) != NULL)
            if (SuffixMatch(ent->d_name, ".desktop"))
            {
                buffer[0] = '\0';
                const char *file_path =
                    strcat(strcat(buffer, path), ent->d_name);

                FILE *file = fopen(file_path, "r");

                // TODO: Also handle as a branch. Continue?
                if (!file)
                {
                    fprintf(stderr, "Bad file: %s\n", file_path);
                    continue;
                }

                char *current_exec = NULL;
                char *current_name = NULL;

                StringMakeEmpty();
                StringMakeEmpty();

                int terminal = 0; // Is app using terminal or not (TODO: tricky)
                int header_checked = 0;    // There is [DesktopEntry] header.
                int ignore_this_entry = 0; // e.g. entry is not an application.

                while ((nread = getline(&line, &len, file)) != -1)
                {
                    if (strncmp(line, "[", 1) == 0)
                    {
                        if (strcmp(line, "[Desktop Entry]\n") == 0)
                        {
                            header_checked = 1;
                            continue;
                        }
                        else
                        {
                            // Skip other entires that [Desktop Entry]...
                            do
                            {
                                nread = getline(&line, &len, file);
                            } while (nread != -1 &&
                                     strcmp(line, "[Desktop Entry]\n") != 0);

                            if (nread == -1)
                                break;
                        }
                    }

                    if (!header_checked)
                    {
                        // TODO: Handle this case better!
                        if (strcmp(line, "[Desktop Entry]\n") == 0)
                            header_checked = 1;

                        continue;
                    }

                    if (strlen(line) > DESKTOP_TYPE_FIELD_SIZE &&
                        strncmp(line, DESKTOP_TYPE_FIELD,
                                DESKTOP_TYPE_FIELD_SIZE) == 0 &&
                        strcmp(line + DESKTOP_TYPE_FIELD_SIZE,
                               "Application\n") != 0)
                    {
                        ignore_this_entry = 1;
                        break;
                    }
                    else if (strlen(line) > DESKTOP_NAME_FIELD_SIZE &&
                             strncmp(line, DESKTOP_NAME_FIELD,
                                     DESKTOP_NAME_FIELD_SIZE) == 0)
                    {
                        char *name = line + DESKTOP_NAME_FIELD_SIZE;

                        for (int i = 0; i < blacklisted_entries_number; ++i)
                            if (PrefixMatchOfLengthN(name,
                                                     blacklisted_entries[i],
                                                     strlen(name)))
                            {
                                fprintf(stderr, "Ignoring desktop entry: %s, because it \
is on the blacklist.\n", name);
                                ignore_this_entry = 1;
                                break;
                            }

                        if (name)
                            current_name = DuplicateString(name);
                    }
                    else if (strlen(line) > DESKTOP_EXEC_FIELD_SIZE &&
                             strncmp(line, DESKTOP_EXEC_FIELD,
                                     DESKTOP_EXEC_FIELD_SIZE) == 0)
                    {
                        char *exec = line + DESKTOP_EXEC_FIELD_SIZE;
                        if (exec)
                            current_exec = DuplicateString(exec);
                    }
                    else if (strlen(line) > DESKTOP_TERM_FIELD_SIZE &&
                             strncmp(line, DESKTOP_TERM_FIELD,
                                     DESKTOP_TERM_FIELD_SIZE) == 0)
                    {
                        char *term = line + DESKTOP_TERM_FIELD_SIZE;
                        if (strcmp(term, "true\n") == 0 ||
                            strcmp(term, "true") == 0)
                            terminal = 1;
                    }
                    else if (strlen(line) > DESKTOP_NO_DISPLAY_FIELD_SIZE &&
                             strncmp(line, DESKTOP_NO_DISPLAY_FIELD,
                                     DESKTOP_NO_DISPLAY_FIELD_SIZE) == 0)
                    {
                        char *display_info =
                            line + DESKTOP_NO_DISPLAY_FIELD_SIZE;
                        if (display_info &&
                            (strcmp(display_info, "true\n") == 0 ||
                             strcmp(display_info, "true") == 0))
                            ignore_this_entry = 1;
                    }
                }

                if (header_checked && !ignore_this_entry && current_name &&
                    current_exec)
                {
                    if (terminal)
                    {
                        int termnial_command_size = strlen(terminal_command),
                            exec_original_size = strlen(current_exec);

                        // NOTE: '+1 +1' - 1 for space, 1 for trailing '\0'
                        int fixed_exec_command_size =
                            exec_original_size + termnial_command_size + 1 + 1;

                        current_exec =
                            realloc(current_exec,
                                    sizeof(char) * fixed_exec_command_size);
                        assert(current_exec);

                        // TODO: See if memcpy optimizes this (probobly not
                        // worth it).
                        for (int i = 0; i < exec_original_size; ++i)
                            current_exec[i + termnial_command_size + 1] =
                                current_exec[i];
                        for (int i = 0; i < termnial_command_size; ++i)
                            current_exec[i] = terminal_command[i];

                        current_exec[termnial_command_size] = ' ';
                        current_exec[exec_original_size +
                                     termnial_command_size + 1] = '\0';
                    }

                    // Remove %'s:
                    int fixed_idx = 0;
                    for (int i = 0;
                         current_exec[i] != '\0' && current_exec[i] != '\n';
                         ++i)
                    {
                        if (current_exec[i] == '%' &&
                            current_exec[i + 1] != '\0' &&
                            current_exec[i + 1] != '\n')
                        {
                            i += 2;
                        }

                        current_exec[fixed_idx++] = current_exec[i];
                    }
                    current_exec[fixed_idx] = '\0';

                    // Remove newline from the name:
                    for (int i = 0; current_name[i] != '\0'; ++i)
                        if (current_name[i] == '\n')
                        {
                            current_name[i] = '\0';
                            break;
                        }

                    // Remove newline from the name:
                    for (int i = 0; current_exec[i] != '\0'; ++i)
                        if (current_exec[i] == '\n')
                        {
                            current_exec[i] = '\0';
                            break;
                        }

                    if (result_idx >= result_max_len)
                    {
                        result_max_len *= 2;
                        result = realloc(result,
                                         sizeof(DesktopEntry) * result_max_len);
                    }

                    // TODO: Copy pasted.
                    result[result_idx++] = (DesktopEntry){
                        MakeStringCopyText(current_name, strlen(current_name)),
                        MakeStringCopyText(current_exec, strlen(current_exec))};
                }

                if (current_name)
                    free(current_name);
                if (current_exec)
                    free(current_exec);

                fclose(file);
            }

        closedir(dir);
        free(line);
    }
    else
    {
        fprintf(stderr, "Directory %s cannot be opened!!\n", path);
        return 0;
    }

    int CompareNameLex(const void *a, const void *b)
    {
        return strcmp((StringGetText(&((DesktopEntry *)a)->name)),
                      (StringGetText(&((DesktopEntry *)b)->name)));
    }

    qsort(result, result_idx, sizeof(DesktopEntry), CompareNameLex);

    (*result_desktop_entries) = result;
    (*result_desktop_entries_size) = result_idx;

    return 1;
}
