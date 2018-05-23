#include <time.h>
#include <pthread.h>

#include "desktop_entry_parser.h"
#include "string.h"
#include "util.h"

struct _Entry
{
    String text;

    struct _Entry *next_displayed;
    struct _Entry *prev_displayed;
};
typedef struct _Entry Entry;

#define NUMBER_OF_ENTRIES_IN_BLOCK (256)
struct _EntriesBlock
{
    Entry entries[NUMBER_OF_ENTRIES_IN_BLOCK];
    int number_of_entries;

    struct _EntriesBlock *next;
};
typedef struct _EntriesBlock EntriesBlock;

struct
{
    // These are used only when reading .desktop files.
    DesktopEntry *desktop_entries;
    int desktop_entries_size;

    int dirty;
} desktop_state;

// The thread that will work in a background and check if desktop entries are
// up-to-date.
static pthread_t update_desktop_entries_thread;
static pthread_t save_desktop_entries_thread;
static pthread_mutex_t desktop_state_lock;

static void *SaveDesktopEntriesThreadMain(void *args)
{
    if (args)
    {
    }

    pthread_mutex_lock(&desktop_state_lock);

    // NOTE: Testing. Make sure program exits properly when saving this file takes longer than it should.
#if 0
    sleep(5);
#endif

    int save_feedback = SaveDesktopEntriesInfoToCacheFile(
        "/home/mateusz/mmcache",
        desktop_state.desktop_entries,
        desktop_state.desktop_entries_size);

    // TODO: Don't assert, just handle this case!!!
    assert(save_feedback);

    pthread_mutex_unlock(&desktop_state_lock);
    return NULL;
}

typedef enum
{
    UMF_NONE = 0,
    UMF_REDRAW = 1,
    UMF_RESET_SELECT = 2,
    UMF_RESET_AFTER_APPEND = 4
} UpdateMenuFeedback;

// TODO(possibly): move all of them to one strucutre?

// The first block which will be usually enought for all entries.
// If there are dozens of them, next blocks will be allocated.
static EntriesBlock first_entries_block = {};
static EntriesBlock *last_block = &first_entries_block;

static Entry *first_displayed_entry;

// A pointer to the selected entry, must be in a printed list!
static Entry *current_select;

// Which rect (from the top) is highlighted?
static int current_select_idx = 0;

// How many rects (from the top) we must skip to see selected one?
static int current_select_offset = 0;

// TODO: Realloc inserted text!
static char inserted_text[256];
static int inserted_text_idx = 0;

// TODO: Make prompt as long as the user wants it to be!
static char prompt[256];
static int prompt_length = 0;

inline static int EntryMatch(Entry *entry, const char *text)
{
    if (PrefixMatchWithTrimmedPattern(StringGetText(&entry->text), text))
        return 2;
    else if (TextMatch(StringGetText(&entry->text), text))
        return 1;
    else
        return 0;
}

// TODO: this neeeds testing!
static void AddEntry(char *value, int length, EntriesBlock *last_entries_block)
{
    if (last_entries_block->number_of_entries == NUMBER_OF_ENTRIES_IN_BLOCK)
    {
        // TODO: Ugly. This is used when we reset the entires!
        if (!last_entries_block->next)
        {
            last_entries_block = last_entries_block->next;
            last_entries_block->number_of_entries = 0;
        }
        else
        {
            // We must allocate new new_entry block.
            EntriesBlock *block = malloc(sizeof(EntriesBlock));
            block->next = NULL;
            block->number_of_entries = 0;

            last_entries_block->next = block;
            last_entries_block = block;
        }
    }

    Entry *new_entry =
        &(last_entries_block->entries[last_entries_block->number_of_entries++]);

    new_entry->text = MakeStringCopyText(value, length);
    new_entry->next_displayed = NULL;
}

static void AddEntryToMatchList(Entry *current_entry,
                                Entry **prefix_matched_head,
                                Entry **prefix_matched_tail,
                                Entry **non_full_matched_head,
                                Entry **non_full_matched_tail)
{
    int match_level = EntryMatch(current_entry, inserted_text);

    switch (match_level)
    {
        // Prefix match; Add to the list of fixes.
        case 2:
        {
            if (*prefix_matched_tail)
                (*prefix_matched_tail)->next_displayed = current_entry;
            else
                (*prefix_matched_head) = current_entry;

            current_entry->prev_displayed = (*prefix_matched_tail);
            current_entry->next_displayed = NULL;
            (*prefix_matched_tail) = current_entry;
        }
        break;

        // Weaker match. Append to the back.
        case 1:
        {
            if (!(*non_full_matched_tail))
            {
                assert(!(*non_full_matched_head));
                (*non_full_matched_head) = current_entry;
                (*non_full_matched_tail) = current_entry;
                current_entry->next_displayed = NULL;
                current_entry->prev_displayed = NULL;
            }
            else
            {
                assert(*non_full_matched_head);
                (*non_full_matched_tail)->next_displayed = current_entry;
                current_entry->next_displayed = NULL;
                current_entry->prev_displayed = (*non_full_matched_tail);
                (*non_full_matched_tail) = current_entry;
            }
        }
        break;

        // No match; do nothing.
        case 0:
            break;

        default:
            assert(!"Unsupported match level!");
    }
}

static void UpdateDisplayedEntries()
{
    first_displayed_entry = NULL;

    current_select = NULL;

    Entry *prev_entry = NULL;
    EntriesBlock *current_block = &first_entries_block;

    Entry *first_with_not_full_match = NULL;
    Entry *last_with_not_full_match = NULL;

    do
    {
        for (int i = 0; i < current_block->number_of_entries; ++i)
        {
            Entry *current_entry = &(current_block->entries[i]);

            AddEntryToMatchList(current_entry, &first_displayed_entry,
                                &prev_entry, &first_with_not_full_match,
                                &last_with_not_full_match);
        }
        current_block = current_block->next;
    } while (current_block);

    if (prev_entry)
    {
        assert(first_displayed_entry);
        assert(!prev_entry->next_displayed);
        prev_entry->next_displayed = first_with_not_full_match;
    }
    else
    {
        assert(!first_displayed_entry);
        first_displayed_entry = first_with_not_full_match;
    }

    current_select = first_displayed_entry;
    current_select_idx = 0;
    current_select_offset = 0;
}

// Same as above, but does it faster, as it only check already matched text. It
// can be used after appending a letter (letters) to the inserted text, but
// after deleteion, this function cannot be used.
static void UpdateDisplayedEntriesAfterAppendingCharacter()
{
    assert(inserted_text[0] != '\0');

    Entry *prev_entry = NULL;
    Entry *next_entry_candidate = first_displayed_entry;

    Entry *first_with_not_full_match = NULL;
    Entry *last_with_not_full_match = NULL;

    first_displayed_entry = NULL;

    while (next_entry_candidate)
    {
        Entry *current_entry = next_entry_candidate;
        next_entry_candidate = next_entry_candidate->next_displayed;

        AddEntryToMatchList(current_entry, &first_displayed_entry, &prev_entry,
                            &first_with_not_full_match,
                            &last_with_not_full_match);
    }

    if (prev_entry)
    {
        assert(first_displayed_entry);
        assert(!prev_entry->next_displayed);
        prev_entry->next_displayed = first_with_not_full_match;
    }
    else
    {
        assert(!first_displayed_entry);
        first_displayed_entry = first_with_not_full_match;
    }

    // TODO: This is a copy paste from the function below.
    current_select = first_displayed_entry;
    current_select_idx = 0;
    current_select_offset = 0;
}

// TODO: This must be thread-safe!
static void HandleAppLuncherOutput(const char *output)
{
    pthread_mutex_lock(&desktop_state_lock);
    int found_in_entries_list = 0;

    // Iterate over desktop entries and find the matching one.
    for (int i = 0; i < desktop_state.desktop_entries_size; ++i)
        if (strcmp(output, StringGetText(&desktop_state.desktop_entries[i].name)) ==
            0)
        {
            found_in_entries_list = 1;

            // TODO: Calculate the length properly.
            char buffer[strlen(StringGetText(&desktop_state.desktop_entries[i].exec)) +
                        strlen(" i3-msg exec ''")];
            buffer[0] = '\0';

            // TODO: Add ability to specify command by the user (use % to
            // replace a command to call or something).
            strcat(strcat(strcat(buffer, "i3-msg exec '"),
                          StringGetText(&desktop_state.desktop_entries[i].exec)), "'");

            fprintf(stderr, "Buffer: %s\n", buffer);
            system(buffer);

            break;
        }

    if (!found_in_entries_list)
    {
        // If nothing was found, FOR NOW we just try to exec it. TODO: Clean
        // up this super-bad code! this doenst have to be here TBH...
        size_t inserted_text_len = strlen(inserted_text);
        if (inserted_text_len)
        {
            // TODO: This has probobly some bugs.
            char buffer[inserted_text_len + 3 + strlen("i3-msg  ' '")];
            buffer[0] = '\0';
            strcat(strcat(strcat(buffer, "i3-msg '"), inserted_text), "'");

            fprintf(stderr, "Executing command not from the list: %s\n", buffer);
            system(buffer);
        }
    }

    pthread_mutex_unlock(&desktop_state_lock);
}

static void HandeOutput(const char *output)
{
    if (output[0] == '\0')
        return;

    switch (menu_mode)
    {
        case MM_DMENU:
            printf("%s\n", output);
            break;

        case MM_APP_LUNCHER:
            HandleAppLuncherOutput(output);
            break;

        default:
            assert(!"Not supported menu type!\n");
    }
}

static void LoadEntriesFromStdin()
{
    size_t len = 256;
    int nread;

    // We allocate it once, so that we can reuse it. It must be allocated with
    // malloc because getline will try to reallocate it when gets more that out
    // character limit.
    char *line = malloc(sizeof(char) * 256);

    while ((nread = getline(&line, &len, stdin)) != -1)
    {
        // Remove the newline, and calculate the size
        int text_len = 0;
        for (int i = 0; line[i] != '\0'; ++i)
            if (line[i] == '\n' && line[i + 1] == '\0')
            {
                text_len = i;
                line[i] = ('\0');
                break;
            }

        AddEntry(line, text_len, last_block);
    }

    free(line);
}

// TODO: Once its ready, move this function here.
static int LoadEntriesFromDotDesktop(const char *path,
                                     DesktopEntry **result_desktop_entries,
                                     int *result_desktop_entries_size);

// TODO: Get rid of not needed asserts!
static int
LoadDotDesktopEntriesFromCacheFile(const char *path,
                                   DesktopEntry **result_desktop_entries,
                                   int *result_desktop_entries_size)
{
    FILE *file = fopen(path, "r");
    if (file)
    {
        char title[5];
        int fread_feedback = fread(title, sizeof(char) * 4, 1, file);
        assert(fread_feedback);
        title[4] = '\0';
        if (strcmp(title, "MMCF") != 0)
            assert("Bad header!");

        // TODO: version stuff.
        int ver = 0;
        fread_feedback = fread(&ver, sizeof(int), 1, file);
        assert(fread_feedback);
        assert(ver == 0);

        int number_of_entries;
        fread_feedback = fread(&number_of_entries, sizeof(int), 1, file);
        assert(fread_feedback);
        (*result_desktop_entries) =
            malloc(sizeof(DesktopEntry) * number_of_entries);

        int current_entry_idx = 0;

        size_t len;
        while (fread(&len, sizeof(size_t), 1, file))
        {
            // TODO: collapse!
            {
                char buffer[len + 2];
                fread_feedback = fread(buffer, sizeof(char) * len, 1, file);
                assert(fread_feedback);
                buffer[len] = '\0';

                (*result_desktop_entries)[current_entry_idx].name =
                    MakeStringCopyText(buffer, len);
            }
            fread_feedback = fread(&len, sizeof(size_t), 1, file);
            assert(fread_feedback);

            {
                char buffer[len + 2];
                fread_feedback = fread(buffer, sizeof(char) * len, 1, file);
                assert(fread_feedback);
                buffer[len] = '\0';

                (*result_desktop_entries)[current_entry_idx].exec =
                    MakeStringCopyText(buffer, len);
            }

            current_entry_idx++;
        }

        (*result_desktop_entries_size) = number_of_entries;
        assert(current_entry_idx == number_of_entries);

        fclose(file);
    }
    else
    {
        fprintf(stderr, "File %s cannot be opened!!\n", path);
        return 0;
    }

    return 1;
}

// TODO: rename becasue this name is missleading.
void *CheckIfEntriesAreUpToDate(void *args)
{
    // NOTE: Silience the warrning about unused argument.
    if (args)
    {
    }

    DesktopEntry *result_desktop_entries;
    int result_desktop_entries_size = 0;

#ifdef DEBUG
    PQUERY_START_TIMER("");
#endif

    // TODO: dont assert, just handle this case!
    int entries_loaded_from_dot_desktop =
        LoadEntriesFromDotDesktop("/usr/share/applications/",
                                  &result_desktop_entries,
                                  &result_desktop_entries_size);

    assert(entries_loaded_from_dot_desktop);

#ifdef DEBUG
    PQUERY_STOP_TIMER("Total load time of reading from dot desktop:");
#endif

    pthread_mutex_lock(&desktop_state_lock);

    int arrays_differ = 0;

    if (result_desktop_entries_size != desktop_state.desktop_entries_size)
        arrays_differ = 1;
    else
        for (int i = 0; i < result_desktop_entries_size; ++i)
            if (StringCompare(&result_desktop_entries[i].name,
                              &desktop_state.desktop_entries[i].name) != 0
                || StringCompare(&result_desktop_entries[i].exec,
                                 &desktop_state.desktop_entries[i].exec) != 0)
            {
                arrays_differ = 1;
                break;
            }

    // TODO: result_desktop_entries leaks, but nobody cares.
    if (arrays_differ)
    {
        fprintf(stderr, "Displayed stuff differs with the current one. Updating...\n");
        desktop_state.desktop_entries = result_desktop_entries;
        desktop_state.desktop_entries_size = result_desktop_entries_size;
        desktop_state.dirty = 1;

        // .
    }

    pthread_mutex_unlock(&desktop_state_lock);

    // TODO: Check if loaded entries table differs with the one that has been
    // read from cache and if it is: chagne displayed one to this one and save
    // this one to cache file!!!!

    return NULL;
}

// TODO: This must be done by the main thread!!!!!!
// Or everything must be locked!
static void MakeEntryArrayFromDesktopStateData()
{
    // NOTE: Reset entry blocks!
    last_block = &first_entries_block;
    last_block->number_of_entries = 0;

    // Add created list to the display list.
    for (int i = 0; i < desktop_state.desktop_entries_size; ++i)
    {
        AddEntry(
            StringGetText(&(desktop_state.desktop_entries[i].name)),
            strlen(StringGetText(&(desktop_state.desktop_entries[i].name))),
            last_block);
    }
}

static void MenuInit()
{
    switch (menu_mode)
    {
        case MM_DMENU:
        {
            LoadEntriesFromStdin();
        } break;

        case MM_APP_LUNCHER:
        {
            // TODO: This code is not thread save, make sure it is, when we add
            // more guys touching destkop_state!

            desktop_state.desktop_entries_size = 0;
            desktop_state.desktop_entries = NULL;

            // We load some entries from the cache file, and then check if
            // cached data is up to date with the original desktop file foleder.

            int data_was_loaded_from_cache = 0;

#ifdef DEBUG
            PQUERY_START_TIMER("");
#endif
            if (LoadDotDesktopEntriesFromCacheFile(
                    "/home/mateusz/mmcache",
                    &desktop_state.desktop_entries,
                    &desktop_state.desktop_entries_size))
            {
#ifdef DEBUG
                PQUERY_STOP_TIMER("Files read from cache in:");
#endif

                data_was_loaded_from_cache = 1;
            }
            else
            {
                data_was_loaded_from_cache = 0;
                fprintf(stderr, "No cache file, loading from dot desktop.\n");

                // TODO: dont assert, just handle this case!
                // TODO: Does it have to be "/usr/share/applications/"?
                int entries_loaded_from_dot_desktop =
                    LoadEntriesFromDotDesktop("/usr/share/applications/",
                                              &desktop_state.desktop_entries,
                                              &desktop_state.desktop_entries_size);

                assert(entries_loaded_from_dot_desktop);
            }

            MakeEntryArrayFromDesktopStateData();
            desktop_state.desktop_entries = desktop_state.desktop_entries;
            desktop_state.desktop_entries_size = desktop_state.desktop_entries_size;

            if (data_was_loaded_from_cache)
            {
                // Create a second thread which updates the data, checks if
                // there are some differences and if so swaps datas, and save
                // new one to the cache file.
                if (pthread_create(&update_desktop_entries_thread, NULL,
                                   CheckIfEntriesAreUpToDate, NULL))
                {
                    fprintf(stderr, "Error creating thread\n");
                }
            }
            else
            {
                // TODO: Add ability for the user, not to create cache file,
                // also add ability to specify the path to it!
                int desktop_entries_saved_into_cache_file =
                    SaveDesktopEntriesInfoToCacheFile(
                        "/home/mateusz/mmcache",
                        desktop_state.desktop_entries,
                        desktop_state.desktop_entries_size);

                assert(desktop_entries_saved_into_cache_file);
            }
        }
        break;

        default:
            assert(!"Not supported menu type!\n");
    }
}

static UpdateMenuFeedback MenuCompleteAtCurrent()
{
    if (current_select)
    {
        char *current_complete = StringGetText(&current_select->text);
        int curr_complete_size = GetLettersCount(current_complete);
        for (int i = 0; i < curr_complete_size; ++i)
            inserted_text[i] = current_complete[i];
        inserted_text_idx = curr_complete_size;
    }
    else
    {
        // TODO: Make sure this assert is correct!
        assert(!first_displayed_entry);
        fprintf(stderr, "No option to complete. TAB ignored...\n");
    }

    return UMF_REDRAW | UMF_RESET_SELECT;
}

// If 1 is returned windows requires the redraw, is 0 it does not.
static UpdateMenuFeedback MenuMoveUp()
{
    if (current_select->prev_displayed)
    {
        // If there is previous entry either current_select_idx
        // or current_select_offset is > 0.
        assert(current_select_offset > 0 || current_select_idx > 0);

        current_select = current_select->prev_displayed;
        current_select_idx > 0 ? current_select_idx-- : current_select_offset--;

        return UMF_REDRAW;
    }

    return UMF_NONE;
}

// TODO: try to move number_of_fields_drawned to this module so that it does not
// need to be passed as an argument.
static UpdateMenuFeedback MenuMoveDown(int number_of_fields_drawned)
{
    if (current_select->next_displayed)
    {
        current_select = current_select->next_displayed;
        current_select_idx++;

        // First -1 because one field holds inserted
        // text. Second -1 is because we want some space, and
        // the last field might be cuted.
        while (current_select_idx >= number_of_fields_drawned - 1 - 1)
        {
            current_select_offset++;
            current_select_idx--;
        }
        return UMF_REDRAW;
    }

    return UMF_NONE;
}

static UpdateMenuFeedback MenuAddCharacter(const char ch)
{
    inserted_text[inserted_text_idx++] = ch;
    inserted_text[inserted_text_idx] = '\0';

    return UMF_REDRAW | UMF_RESET_AFTER_APPEND;
}

static UpdateMenuFeedback MenuRemoveCharacter()
{
    if (inserted_text_idx)
    {
        inserted_text_idx--;
        inserted_text[inserted_text_idx] = '\0';
    }

    return UMF_REDRAW | UMF_RESET_SELECT;
}
