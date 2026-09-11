#define _GNU_SOURCE
#include "download.h"
#include "cdrive.h"
#include <time.h>    // Needed for ETA calculation
#include <stdlib.h>  // Needed for system()
#include <string.h>  // Needed for strlen
#include <math.h>    // Needed for isnan()

// Struct to hold file information for the interactive browser
typedef struct {
    char id[256];
    char name[256];
    int is_folder;
} BrowserFile;

// Struct to manage data for both file writing and progress bar
struct DownloadProgressData {
    FILE *fp;
    CURL *curl;
    time_t start_time;
    const char *filename;
    curl_off_t resume_offset;
};

// --- Forward declarations for local functions ---
static void sanitize_filename(char *dest, const char *src, size_t max_len);
static int fetch_files_for_browser(const char *folder_id, BrowserFile **files, int *count);
static int get_file_metadata(const char *file_id, char *filename_out, size_t filename_size);
static int download_file_with_progress(const char *file_id, const char *filename);
static void format_size(char *buf, size_t size, double bytes);
static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
static size_t write_file_callback(void *ptr, size_t size, size_t nmemb, void *stream);

static void sanitize_filename(char *dest, const char *src, size_t max_len) {
    if (!dest || max_len == 0) return;
    if (!src || !*src) {
        strncpy(dest, "downloaded_file", max_len - 1);
        dest[max_len - 1] = '\0';
        return;
    }
    const char *base = src;
    for (const char *p = src; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    if (*base == '\0' || strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
        strncpy(dest, "downloaded_file", max_len - 1);
        dest[max_len - 1] = '\0';
        return;
    }
    size_t i = 0;
    if (base[0] == '.' && max_len > 2) {
        dest[i++] = '_';
    }
    for (size_t j = 0; base[j] && i < max_len - 1; j++) {
        char c = base[j];
        if (c == '/' || c == '\\' || (unsigned char)c < 32) {
            dest[i++] = '_';
        } else {
            dest[i++] = c;
        }
    }
    dest[i] = '\0';
}


// --- Main Functions ---

int cdrive_pull_file_by_id(const char *file_id, const char *output_filename) {
    char final_filename[MAX_PATH_SIZE];

    if (output_filename) {
        sanitize_filename(final_filename, output_filename, sizeof(final_filename));
    } else {
        print_info("Fetching file metadata...");
        if (get_file_metadata(file_id, final_filename, sizeof(final_filename)) != 0) {
            print_error("Could not retrieve filename for the given ID.");
            return -1;
        }
        print_success("Retrieved filename:");
        printf("  %s\n", final_filename);
    }

    return download_file_with_progress(file_id, final_filename);
}

int cdrive_pull_interactive(void) {
    typedef struct {
        char id[256];
        char name[256];
    } FolderBreadcrumb;

    FolderBreadcrumb folder_stack[64];
    int stack_depth = 0;

    char current_folder_id[256] = "root";
    char current_folder_name[256] = "My Drive";

    while (1) {
        BrowserFile *files = NULL;
        int file_count = 0;

        if (fetch_files_for_browser(current_folder_id, &files, &file_count) != 0) {
            print_error("Failed to fetch files from Google Drive.");
            if (files) free(files);
            return -1;
        }

        if (file_count == 0) {
            print_info("This folder is empty. Press enter to go back.");
            getchar();
            if (files) {
                free(files);
                files = NULL;
            }
            if (stack_depth > 0) {
                stack_depth--;
                strcpy(current_folder_id, folder_stack[stack_depth].id);
                strcpy(current_folder_name, folder_stack[stack_depth].name);
                continue;
            } else {
                break;
            }
        }

        const char **options = malloc(sizeof(char *) * (file_count + 2));
        if (!options) { print_error("Memory allocation failed."); free(files); return -1; }

        char **option_strings = malloc(sizeof(char *) * file_count);
        if (!option_strings) { print_error("Memory allocation failed."); free(files); free(options); return -1; }

        for (int i = 0; i < file_count; i++) {
            option_strings[i] = malloc(512);
            if (files[i].is_folder) {
                snprintf(option_strings[i], 512, "[DIR] %s", files[i].name);
            } else {
                snprintf(option_strings[i], 512, "[FILE] %s", files[i].name);
            }
            options[i] = option_strings[i];
        }
        options[file_count] = (stack_depth > 0) ? "[..] Go Back" : "[..] Refresh";
        options[file_count + 1] = "Exit Browser";

        char menu_title[512];
        snprintf(menu_title, sizeof(menu_title), "Select a file or folder (current: %s)", current_folder_name);

        int choice = show_interactive_menu(menu_title, options, file_count + 2);

        // Terminal was already restored by disable_raw_mode() inside show_interactive_menu.
        // The stty call was removed because it does not exist on Windows.

        for (int i = 0; i < file_count; i++) { free(option_strings[i]); }
        free(option_strings);
        free(options);

        if (choice == -1 || choice == file_count + 1) { free(files); break; }

        if (choice == file_count) {
            if (stack_depth > 0) {
                stack_depth--;
                strcpy(current_folder_id, folder_stack[stack_depth].id);
                strcpy(current_folder_name, folder_stack[stack_depth].name);
            } else {
                strcpy(current_folder_id, "root");
                strcpy(current_folder_name, "My Drive");
            }
            free(files);
            continue;
        }

        if (files[choice].is_folder) {
            if (stack_depth < 64) {
                snprintf(folder_stack[stack_depth].id, sizeof(folder_stack[stack_depth].id), "%s", current_folder_id);
                snprintf(folder_stack[stack_depth].name, sizeof(folder_stack[stack_depth].name), "%s", current_folder_name);
                stack_depth++;
            }
            strcpy(current_folder_id, files[choice].id);
            strcpy(current_folder_name, files[choice].name);
        } else {
            char sanitized_name[256];
            sanitize_filename(sanitized_name, files[choice].name, sizeof(sanitized_name));
            download_file_with_progress(files[choice].id, sanitized_name);
            printf("\nPress Enter to continue...");
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF);
        }

        free(files);
    }

    return 0;
}


// --- Static Helper Functions ---

static int get_file_metadata(const char *file_id, char *filename_out, size_t filename_size) {
    APIResponse response = {0};

    char url[1024];
    snprintf(url, sizeof(url), "https://www.googleapis.com/drive/v3/files/%s?fields=name,mimeType", file_id);

    if (cdrive_api_get(url, &response) != 0) {
        return -1;
    }

    json_object *root = json_tokener_parse(response.data);
    if (!root) { free(response.data); return -1; }
    json_object *name_obj;
    if (json_object_object_get_ex(root, "name", &name_obj)) {
        sanitize_filename(filename_out, json_object_get_string(name_obj), filename_size);
    } else {
        json_object_put(root);
        free(response.data);
        return -1;
    }

    json_object *mime_obj;
    if (json_object_object_get_ex(root, "mimeType", &mime_obj)) {
        const char *mime = json_object_get_string(mime_obj);
        if (mime && strncmp(mime, "application/vnd.google-apps.", 28) == 0 &&
            strcmp(mime, "application/vnd.google-apps.folder") != 0) {
            print_warning("Note: This is a Google Workspace document (Docs/Sheets/Slides). Direct binary download may be empty or fail; export via Drive UI is required.");
        }
    }

    json_object_put(root);
    free(response.data);
    return 0;
}

static int fetch_files_for_browser(const char *folder_id, BrowserFile **files, int *count) {
    APIResponse response = {0};

    char url[1024];
    char *escaped_folder = escape_drive_query(folder_id);
    char *encoded_folder_id = url_encode(escaped_folder ? escaped_folder : folder_id);
    if (escaped_folder) free(escaped_folder);
    snprintf(url, sizeof(url),
             "%s?q=%%27%s%%27%%20in%%20parents%%20and%%20trashed=false&fields=files(id,name,mimeType)&orderBy=folder,name&pageSize=1000",
             DRIVE_API_URL, encoded_folder_id ? encoded_folder_id : folder_id);
    if (encoded_folder_id) free(encoded_folder_id);

    if (cdrive_api_get(url, &response) != 0) {
        return -1;
    }
    json_object *root = json_tokener_parse(response.data);
    if (!root) { free(response.data); return -1; }
    json_object *files_array;
    if (json_object_object_get_ex(root, "files", &files_array)) {
        *count = json_object_array_length(files_array);
        if (*count > 0) {
            *files = malloc(sizeof(BrowserFile) * (*count));
            if (!*files) {
                json_object_put(root);
                free(response.data);
                *count = 0;
                return -1;
            }
            for (int i = 0; i < *count; i++) {
                json_object *file_obj = json_object_array_get_idx(files_array, i);
                json_object *id_obj = NULL, *name_obj = NULL, *mime_obj = NULL;
                json_object_object_get_ex(file_obj, "id", &id_obj);
                json_object_object_get_ex(file_obj, "name", &name_obj);
                json_object_object_get_ex(file_obj, "mimeType", &mime_obj);

                const char *id_str = id_obj ? json_object_get_string(id_obj) : "";
                const char *name_str = name_obj ? json_object_get_string(name_obj) : "unknown";
                const char *mime_str = mime_obj ? json_object_get_string(mime_obj) : "";

                strncpy((*files)[i].id, id_str ? id_str : "", sizeof((*files)[i].id) - 1);
                (*files)[i].id[sizeof((*files)[i].id) - 1] = '\0';
                sanitize_filename((*files)[i].name, name_str ? name_str : "unknown", sizeof((*files)[i].name));
                (*files)[i].is_folder = (mime_str && strcmp(mime_str, "application/vnd.google-apps.folder") == 0);
            }
        } else {
            *files = NULL;
        }
    }
    json_object_put(root);
    free(response.data);
    return 0;
}

static void format_size(char *buf, size_t size, double bytes) {
    if (isnan(bytes) || bytes <= 0) {
        snprintf(buf, size, "0.00 B");
        return;
    }
    const char *suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    while (bytes >= 1024 && i < 4) {
        bytes /= 1024;
        i++;
    }
    snprintf(buf, size, "%.2f %s", bytes, suffixes[i]);
}

static size_t write_file_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
    struct DownloadProgressData *data = (struct DownloadProgressData *)stream;
    long http_code = 0;
    if (data && data->curl) {
        curl_easy_getinfo(data->curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code != 200 && http_code != 206) {
            return size * nmemb;
        }
    }
    return fwrite(ptr, size, nmemb, data->fp);
}

// ** THE FULLY CORRECTED PROGRESS CALLBACK **
static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;
    (void)ulnow;
    struct DownloadProgressData *data = (struct DownloadProgressData *)clientp;
    time_t now = time(NULL);
    double elapsed = difftime(now, data->start_time);

    curl_off_t actual_downloaded = (data ? data->resume_offset : 0) + dlnow;
    curl_off_t actual_total = (data ? data->resume_offset : 0) + dltotal;

    if (actual_total <= 0) return 0;

    int percentage = (int)(((double)actual_downloaded / (double)actual_total) * 100);
    if (percentage > 100) percentage = 100;
    if (percentage < 0) percentage = 0;

    char downloaded_str[32], total_str[32], speed_str[32];
    format_size(downloaded_str, sizeof(downloaded_str), (double)actual_downloaded);
    format_size(total_str, sizeof(total_str), (double)actual_total);

    double speed = (elapsed > 0) ? ((double)dlnow / elapsed) : 0;
    format_size(speed_str, sizeof(speed_str), speed);

    char eta_str[32] = "??:??";
    if (speed > 0) {
        double eta_seconds = ((double)dltotal - (double)dlnow) / speed;
        int minutes = (int)(eta_seconds / 60);
        int seconds = (int)eta_seconds % 60;
        if (minutes >= 100) {
            snprintf(eta_str, sizeof(eta_str), ">99m");
        } else {
            snprintf(eta_str, sizeof(eta_str), "%02d:%02d", minutes, seconds);
        }
    }

    // --- NEW LOGIC: Build the entire string in a buffer first ---
    char progress_bar[21];
    int bar_width = 20;
    int pos = (int)(bar_width * ((double)actual_downloaded / actual_total));
    if (pos < 0) pos = 0;
    if (pos > bar_width) pos = bar_width;
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) progress_bar[i] = '=';
        else if (i == pos) progress_bar[i] = '>';
        else progress_bar[i] = ' ';
    }
    progress_bar[bar_width] = '\0';

    char line_buffer[512];
    snprintf(line_buffer, sizeof(line_buffer),
             "%s[%s]%s Downloading '%.30s...' | [%s] %3d%% | %s / %s | %s/s | ETA: %s",
             COLOR_BLUE, "INFO", COLOR_RESET,
             data->filename,
             progress_bar,
             percentage, downloaded_str, total_str, speed_str, eta_str);

    // --- FINAL FIX: Print with \r and ANSI clear-line code \x1b[K ---
    fprintf(stderr, "\r%s\x1b[K", line_buffer);

    if (dlnow == dltotal) {
        fprintf(stderr, "\n");
    }

    fflush(stderr);
    return 0;
}

static int download_file_with_progress(const char *file_id, const char *filename) {
    CURL *curl;
    CURLcode res;
    long http_code = 0;

    // Build .part filename for resumable download
    char part_filename[MAX_PATH_SIZE + 5];
    snprintf(part_filename, sizeof(part_filename), "%s.part", filename);

    // Check if a partial download exists
    curl_off_t resume_offset = 0;
    FILE *fp = fopen(part_filename, "ab");
    if (!fp) {
        print_error("Could not open file for writing.");
        perror(part_filename);
        return -1;
    }

    // Get existing file size for resume
#ifdef _WIN32
    _fseeki64(fp, 0, SEEK_END);
    resume_offset = (curl_off_t)_ftelli64(fp);
#else
    fseeko(fp, 0, SEEK_END);
    resume_offset = (curl_off_t)ftello(fp);
#endif

    if (resume_offset > 0) {
        print_info("Resuming partial download");
        printf("  Existing bytes: %lld\n", (long long)resume_offset);
    }

    struct DownloadProgressData progress_data = { .fp = fp, .filename = filename, .resume_offset = resume_offset };

    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt > 0) {
            if (refresh_access_token(&g_tokens) != 0 || save_tokens(&g_tokens) != 0) break;
        }

        curl = curl_easy_init();
        if (!curl) break;

        char url[MAX_URL_SIZE];
        snprintf(url, sizeof(url), "https://www.googleapis.com/drive/v3/files/%s?alt=media", file_id);
        char auth_header[MAX_HEADER_SIZE];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
        struct curl_slist *headers = curl_slist_append(NULL, auth_header);
        
        progress_data.start_time = time(NULL);
        progress_data.curl = curl;

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &progress_data);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Set resume position if we have partial data
        if (resume_offset > 0) {
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, resume_offset);
        }

        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        progress_data.curl = NULL;

        int is_success = (res == CURLE_OK && (http_code == 200 || (resume_offset > 0 && (http_code == 206 || http_code == 416))));
        if (is_success) break;

        // Rollback any erroneous bytes written before retrying
        fflush(fp);
#ifdef _WIN32
        _chsize_s(_fileno(fp), resume_offset);
#else
        if (ftruncate(fileno(fp), (off_t)resume_offset) != 0) {}
#endif
#ifdef _WIN32
        _fseeki64(fp, resume_offset, SEEK_SET);
#else
        fseeko(fp, (off_t)resume_offset, SEEK_SET);
#endif

        if (res != CURLE_OK || (http_code != 401 && http_code != 403)) break;

        fprintf(stderr, "\n");
        print_warning("Authentication token expired. Refreshing and retrying...");
    }

    fclose(fp);

    int is_success = (res == CURLE_OK && (http_code == 200 || (resume_offset > 0 && (http_code == 206 || http_code == 416))));
    if (!is_success) {
        if (res == CURLE_OK) {
             fprintf(stderr, "\n");
        }
        print_error("Download failed.");
        if (http_code != 200 && http_code != 206) fprintf(stderr, "HTTP Error: %ld\n", http_code);
        if (res != CURLE_OK) fprintf(stderr, "cURL Error: %s\n", curl_easy_strerror(res));
        // Leave .part file for resumption, but notify user
        print_info("Partial download saved. Use the same command to resume.");
        printf("  Partial file: %s\n", part_filename);
        return -1;
    }

    // Rename .part to final filename on success
#ifdef _WIN32
    remove(filename); // Windows rename requires destination to not exist
#endif
    if (rename(part_filename, filename) != 0) {
        print_error("Could not rename partial file to final filename.");
        perror(filename);
        return -1;
    }

    print_success("File downloaded successfully!");
    printf("Saved as: %s\n", filename);
    return 0;
}