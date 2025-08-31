#define _GNU_SOURCE
#include "download.h"
#include "cdrive.h"
#include <time.h>    // Needed for ETA calculation
#include <stdlib.h>  // Needed for system()
#include <string.h>  // Needed for strlen

// Struct to hold file information for the interactive browser
typedef struct {
    char id[256];
    char name[256];
    int is_folder;
} BrowserFile;

// Struct to manage data for both file writing and progress bar
struct DownloadProgressData {
    FILE *fp;
    time_t start_time;
    const char *filename;
};


// --- Forward declarations for local functions ---
static int fetch_files_for_browser(const char *folder_id, BrowserFile **files, int *count);
static int get_file_metadata(const char *file_id, char *filename_out, size_t filename_size);
static int download_file_with_progress(const char *file_id, const char *filename);
static void format_size(char *buf, size_t size, double bytes);
static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
static size_t write_file_callback(void *ptr, size_t size, size_t nmemb, void *stream);

// --- Dummy callbacks to silence cURL's internal output ---
static size_t discard_write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    return size * nmemb;
}

static int discard_debug_callback(CURL *handle, curl_infotype type, char *data, size_t size, void *userptr) {
    return 0;
}


// --- Main Functions ---

int cdrive_pull_file_by_id(const char *file_id, const char *output_filename) {
    char final_filename[MAX_PATH_SIZE];

    if (output_filename) {
        strncpy(final_filename, output_filename, sizeof(final_filename) - 1);
        final_filename[sizeof(final_filename) - 1] = '\0';
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
            if (strcmp(current_folder_id, "root") != 0) {
                strcpy(current_folder_id, "root");
                strcpy(current_folder_name, "My Drive");
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
        options[file_count] = "[..] Go Back";
        options[file_count + 1] = "Exit Browser";

        char menu_title[512];
        snprintf(menu_title, sizeof(menu_title), "Select a file or folder (current: %s)", current_folder_name);

        int choice = show_interactive_menu(menu_title, options, file_count + 2);

        // Reset terminal settings to a normal state. This is crucial.
        system("stty sane");

        for (int i = 0; i < file_count; i++) { free(option_strings[i]); }
        free(option_strings);
        free(options);

        if (choice == -1 || choice == file_count + 1) { free(files); break; }

        if (choice == file_count) {
            if (strcmp(current_folder_id, "root") != 0) {
                strcpy(current_folder_id, "root");
                strcpy(current_folder_name, "My Drive");
            }
            free(files);
            continue;
        }

        if (files[choice].is_folder) {
            strcpy(current_folder_id, files[choice].id);
            strcpy(current_folder_name, files[choice].name);
        } else {
            download_file_with_progress(files[choice].id, files[choice].name);
        }

        free(files);
    }

    return 0;
}


// --- Static Helper Functions ---

static int get_file_metadata(const char *file_id, char *filename_out, size_t filename_size) {
    APIResponse response = {0};
    CURL *curl;
    CURLcode res;
    long http_code = 0;

    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt > 0) {
            if (refresh_access_token(&g_tokens) != 0 || save_tokens(&g_tokens) != 0) break;
        }
        curl = curl_easy_init();
        if (!curl) return -1;
        char url[1024];
        snprintf(url, sizeof(url), "https://www.googleapis.com/drive/v3/files/%s?fields=name", file_id);
        char auth_header[MAX_HEADER_SIZE];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
        struct curl_slist *headers = curl_slist_append(NULL, auth_header);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        if (res == CURLE_OK && http_code == 200) break;
        if (res != CURLE_OK || (http_code != 401 && http_code != 403)) break;
        if (response.data) { free(response.data); response.data = NULL; response.size = 0; }
    }
    if (res != CURLE_OK || http_code != 200) { if (response.data) free(response.data); return -1; }
    json_object *root = json_tokener_parse(response.data);
    if (!root) { free(response.data); return -1; }
    json_object *name_obj;
    if (json_object_object_get_ex(root, "name", &name_obj)) {
        strncpy(filename_out, json_object_get_string(name_obj), filename_size - 1);
        filename_out[filename_size - 1] = '\0';
    } else {
        json_object_put(root);
        free(response.data);
        return -1;
    }
    json_object_put(root);
    free(response.data);
    return 0;
}

static int fetch_files_for_browser(const char *folder_id, BrowserFile **files, int *count) {
    APIResponse response = {0};
    CURL *curl;
    CURLcode res;
    long http_code = 0;
    for (int attempt = 0; attempt < 2; attempt++) { if (attempt > 0) { if (refresh_access_token(&g_tokens) != 0 || save_tokens(&g_tokens) != 0) break; }
        curl = curl_easy_init();
        if (!curl) return -1;
        char url[1024];
        char *encoded_folder_id = url_encode(folder_id);
        snprintf(url, sizeof(url),
                 "%s?q='%s'%%20in%%20parents%%20and%%20trashed=false&fields=files(id,name,mimeType)&orderBy=folder,name",
                 DRIVE_API_URL, encoded_folder_id);
        free(encoded_folder_id);
        char auth_header[MAX_HEADER_SIZE];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
        struct curl_slist *headers = curl_slist_append(NULL, auth_header);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        if (res == CURLE_OK && http_code == 200) break;
        if (res != CURLE_OK || (http_code != 401 && http_code != 403)) break;
        if (response.data) { free(response.data); response.data = NULL; response.size = 0; }
    }
    if (res != CURLE_OK || http_code != 200) { if (response.data) free(response.data); return -1; }
    json_object *root = json_tokener_parse(response.data);
    if (!root) { free(response.data); return -1; }
    json_object *files_array;
    if (json_object_object_get_ex(root, "files", &files_array)) {
        *count = json_object_array_length(files_array);
        *files = malloc(sizeof(BrowserFile) * (*count));
        for (int i = 0; i < *count; i++) {
            json_object *file_obj = json_object_array_get_idx(files_array, i);
            json_object *id_obj, *name_obj, *mime_obj;
            json_object_object_get_ex(file_obj, "id", &id_obj);
            json_object_object_get_ex(file_obj, "name", &name_obj);
            json_object_object_get_ex(file_obj, "mimeType", &mime_obj);
            strncpy((*files)[i].id, json_object_get_string(id_obj), sizeof((*files)[i].id) - 1);
            strncpy((*files)[i].name, json_object_get_string(name_obj), sizeof((*files)[i].name) - 1);
            (*files)[i].is_folder = (strcmp(json_object_get_string(mime_obj), "application/vnd.google-apps.folder") == 0);
        }
    }
    json_object_put(root);
    free(response.data);
    return 0;
}

static void format_size(char *buf, size_t size, double bytes) {
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
    return fwrite(ptr, size, nmemb, data->fp);
}

// ** THE FULLY CORRECTED PROGRESS CALLBACK **
static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    struct DownloadProgressData *data = (struct DownloadProgressData *)clientp;
    time_t now = time(NULL);
    double elapsed = difftime(now, data->start_time);

    if (dltotal <= 0) return 0;

    int percentage = (int)(((double)dlnow / (double)dltotal) * 100);

    char downloaded_str[32], total_str[32], speed_str[32];
    format_size(downloaded_str, sizeof(downloaded_str), (double)dlnow);
    format_size(total_str, sizeof(total_str), (double)dltotal);

    double speed = (elapsed > 0) ? ((double)dlnow / elapsed) : 0;
    format_size(speed_str, sizeof(speed_str), speed);

    char eta_str[32] = "??:??";
    if (speed > 0) {
        double eta_seconds = ((double)dltotal - (double)dlnow) / speed;
        int minutes = (int)(eta_seconds / 60);
        int seconds = (int)eta_seconds % 60;
        snprintf(eta_str, sizeof(eta_str), "%02d:%02d", minutes, seconds);
    }

    // --- NEW LOGIC: Build the entire string in a buffer first ---
    char progress_bar[21];
    int bar_width = 20;
    int pos = (int)(bar_width * ((double)dlnow / dltotal));
    for (int i = 0; i < bar_width; ++i) {
        if (i < pos) progress_bar[i] = '=';
        else if (i == pos) progress_bar[i] = '>';
        else progress_bar[i] = ' ';
    }
    progress_bar[bar_width] = '\0';

    char line_buffer[256];
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

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        print_error("Could not open file for writing.");
        perror(filename);
        return -1;
    }

    struct DownloadProgressData progress_data = { .fp = fp, .filename = filename };

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

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &progress_data);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, discard_debug_callback);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); 
        curl_easy_setopt(curl, CURLOPT_STDERR, NULL); 
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);

        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && http_code == 200) break;
        if (res != CURLE_OK || (http_code != 401 && http_code != 403)) break;

        fprintf(stderr, "\n");
        print_warning("Authentication token expired. Refreshing and retrying...");
        rewind(fp);
    }

    fclose(fp);

    if (res != CURLE_OK || http_code != 200) {
        if (res == CURLE_OK && http_code != 200) {
             fprintf(stderr, "\n");
        }
        print_error("Download failed.");
        if (http_code != 200) fprintf(stderr, "HTTP Error: %ld\n", http_code);
        if (res != CURLE_OK) fprintf(stderr, "cURL Error: %s\n", curl_easy_strerror(res));
        remove(filename);
        return -1;
    }

    print_success("File downloaded successfully!");
    printf("Saved as: %s\n", filename);
    return 0;
}