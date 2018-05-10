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

char *terminal_command = "i3-sensible-terminal -e";

// TODO.
#if 0
void SaveDesktopEntriesInfoToCacheFile(const char *path_to_cache_file,
                                       DesktopEntry *entries_to_save,
                                       int entries_size)
{

}
#endif

// TODO: Move to menu.c once it is cleaned up.
static void LoadEntriesFromDotDesktop(const char *path,
                                      DesktopEntry **result_desktop_entries,
                                      int *result_desktop_entries_size)
{
    DIR *dir;
    struct dirent *ent;
    char buffer[256];

    int result_idx = 0,
        result_max_len = 64;
    DesktopEntry *result = malloc(sizeof(DesktopEntry) * result_max_len);

    if ((dir = opendir(path)) != NULL)
    {
        size_t len = 256;
        char *line = malloc(sizeof(char) * len);
        int nread;

        // print all the files and directories within directory
        while ((ent = readdir (dir)) != NULL)
            if (SuffixMatch(ent->d_name, ".desktop"))
            {
                buffer[0] = '\0';
                const char *file_path = strcat(strcat(buffer, path), ent->d_name);

                FILE *file = fopen(file_path, "r");
                assert(file);

                // TODO: This is very unintuitive way to do it, just make to char *'s.
                DesktopEntry info = { NULL, NULL };

                int terminal = 0; // Is app using terminal or not (TODO: tricky)
                int header_checked = 0; // There is [DesktopEntry] header.
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
                            } while (nread != -1 && strcmp(line, "[Desktop Entry]\n") != 0);

                            if (nread == -1)
                                break;
                        }
                    }

                    if (!header_checked)
                    {
                        // TODO: Handle this case better!
                        assert(strcmp(line, "[Desktop Entry]\n") == 0);
                        header_checked = 1;
                        continue;
                    }

                    if (strlen(line) > DESKTOP_TYPE_FIELD_SIZE
                        && strncmp(line, DESKTOP_TYPE_FIELD, DESKTOP_TYPE_FIELD_SIZE) == 0
                        && strcmp(line + DESKTOP_TYPE_FIELD_SIZE, "Application\n") != 0)
                    {
                        ignore_this_entry = 1;
                        break;
                    }
                    else if (strlen(line) > DESKTOP_NAME_FIELD_SIZE
                             && strncmp(line, DESKTOP_NAME_FIELD, DESKTOP_NAME_FIELD_SIZE) == 0)
                    {
                        char *name = line + DESKTOP_NAME_FIELD_SIZE;
                        if (name)
                            info.name = DuplicateString(name);
                    }
                    else if (strlen(line) > DESKTOP_EXEC_FIELD_SIZE
                             && strncmp(line, DESKTOP_EXEC_FIELD, DESKTOP_EXEC_FIELD_SIZE) == 0)
                    {
                        char *exec = line + DESKTOP_EXEC_FIELD_SIZE;
                        if (exec)
                            info.exec = DuplicateString(exec);
                    }
                    else if (strlen(line) > DESKTOP_TERM_FIELD_SIZE
                             && strncmp(line, DESKTOP_TERM_FIELD, DESKTOP_TERM_FIELD_SIZE) == 0)
                    {
                        char *term = line + DESKTOP_TERM_FIELD_SIZE;
                        if (strcmp(term, "true\n") == 0 || strcmp(term, "true") == 0)
                            terminal = 1;
                    }
                    else if (strlen(line) > DESKTOP_NO_DISPLAY_FIELD_SIZE
                             && strncmp(line, DESKTOP_NO_DISPLAY_FIELD, DESKTOP_NO_DISPLAY_FIELD_SIZE) == 0)
                    {
                        char *display_info = line + DESKTOP_NO_DISPLAY_FIELD_SIZE;
                        if (display_info
                            && (strcmp(display_info, "true\n") == 0
                                || strcmp(display_info, "true") == 0))
                            ignore_this_entry = 1;
                    }
                }

                if (header_checked && !ignore_this_entry && info.name && info.exec)
                {
                    if (terminal)
                    {
                        int termnial_command_size = strlen(terminal_command),
                            exec_original_size = strlen(info.exec);

                        // NOTE: '+1 +1' - 1 for space, 1 for trailing '\0'
                        info.exec = realloc(info.exec, sizeof(char) *
                                            (exec_original_size + termnial_command_size + 1 + 1));

                        // TODO: See if memcpy optimizes this (probobly not worth it).
                        for (int i = 0; i < exec_original_size; ++i)
                            info.exec[i + termnial_command_size+1] = info.exec[i];
                        for (int i = 0; i < termnial_command_size; ++i)
                            info.exec[i] = terminal_command[i];

                        info.exec[termnial_command_size] = ' ';
                        info.exec[exec_original_size + termnial_command_size + 1] = '\0';
                    }

                    // Remove %'s:
                    int fixed_idx = 0;
                    for (int i = 0;
                         info.exec[i] != '\0' && info.exec[i] != '\n';
                         ++i)
                    {
                        if (info.exec[i] == '%'
                            && info.exec[i+1] != '\0'
                            && info.exec[i+1] != '\n')
                        {
                            i += 2;
                        }

                        info.exec[fixed_idx++] = info.exec[i];
                    }
                    info.exec[fixed_idx] = '\0';

                    // Remove newline from the name:
                    for (int i = 0; info.name[i] != '\0'; ++i)
                        if (info.name[i] == '\n')
                        {
                            info.name[i] = '\0';
                            break;
                        }

                    // Remove newline from the name:
                    for (int i = 0; info.exec[i] != '\0'; ++i)
                        if (info.exec[i] == '\n')
                        {
                            info.exec[i] = '\0';
                            break;
                        }

                    if (result_idx >= result_max_len)
                    {
                        result_max_len *= 2;
                        result = realloc(result, sizeof(DesktopEntry) * result_max_len);
                    }

                    result[result_idx++] = (DesktopEntry){ info.name, info.exec };
                }
                else
                {
                    if (info.name)
                        free(info.name);
                    if (info.exec)
                        free(info.exec);
                }

                fclose(file);
            }

        closedir(dir);
        free(line);
    }


    int CompareNameLex (const void * a, const void * b)
    {
        return strcmp(((DesktopEntry *)a)->name, ((DesktopEntry *)b)->name);
    }

    // TODO: Do we need to sort?
    qsort(result, result_idx, sizeof(DesktopEntry), CompareNameLex);

    for (int i = 0; i < result_idx; ++i)
        AddEntry(result[i].name, strlen(result[i].name));

    (* result_desktop_entries) = result;
    (* result_desktop_entries_size) = result_idx;
}
