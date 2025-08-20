#define _GNU_SOURCE
#include "download.h"
#include "cdrive.h"

// Struct to hold file information for the interactive browser
typedef struct {
    char id[256];
    char name[256];
    int is_folder;
} BrowserFile;

// Forward declarations for local functions
static int fetch_files_for_browser(const char *folder_id, BrowserFile **files, int *count);
static int get_file_metadata(const char *file_id, char *filename_out, size_t filename_size);
static int download_file_with_progress(const char *file_id, const char *filename);

int cdrive_pull_file_by_id(const char *file_id, const char *output_filename) {
    char final_filename[MAX_PATH_SIZE];

    if (output_filename) {
        // User provided an output path/filename
        strncpy(final_filename, output_filename, sizeof(final_filename) - 1);
        final_filename[sizeof(final_filename) - 1] = '\0';
    } else {
        // No output path provided, fetch filename from API
        print_info("Fetching file metadata...");
        if (get_file_metadata(file_id, final_filename, sizeof(final_filename)) != 0) {
            print_error("Could not retrieve filename for the given ID. The file may not exist or you may not have permission.");
            return -1;
        }
        print_success("Retrieved filename:");
        printf("  %s\n", final_filename);
    }

    return download_file_with_progress(file_id, final_filename);
}

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

        if (response.data) {
            free(response.data);
            response.data = NULL;
            response.size = 0;
        }
    }

    if (res != CURLE_OK || http_code != 200) {
        if (response.data) free(response.data);
        return -1;
    }

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
            print_info("This folder is empty. Press any key to go back.");
            getchar(); // Simple wait
            // Logic to go back to parent folder would be needed here.
            // For now, we'll just exit the loop.
            if (strcmp(current_folder_id, "root") != 0) {
                 // A real implementation would track the parent ID.
                 // For this example, we'll just go back to root.
                strcpy(current_folder_id, "root");
                strcpy(current_folder_name, "My Drive");
                continue;
            } else {
                break;
            }
        }

        // Prepare options for the interactive menu
        const char **options = malloc(sizeof(char *) * (file_count + 2));
        if (!options) {
            print_error("Memory allocation failed for menu.");
            free(files);
            return -1;
        }

        char **option_strings = malloc(sizeof(char *) * file_count);
        if (!option_strings) {
            print_error("Memory allocation failed for menu strings.");
            free(files);
            free(options);
            return -1;
        }

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
        snprintf(menu_title, sizeof(menu_title), "Select a file or folder to navigate (current: %s)", current_folder_name);

        int choice = show_interactive_menu(menu_title, options, file_count + 2);

        // Free menu memory
        for (int i = 0; i < file_count; i++) {
            free(option_strings[i]);
        }
        free(option_strings);
        free(options);

        if (choice == -1 || choice == file_count + 1) { // Exit
            free(files);
            break;
        }

        if (choice == file_count) { // Go Back
            // This is a simplified back navigation. A real implementation
            // would use a stack to track the path.
            if (strcmp(current_folder_id, "root") != 0) {
                strcpy(current_folder_id, "root"); // Go back to root for now
                strcpy(current_folder_name, "My Drive");
            }
            free(files);
            continue;
        }

        // User selected a file or folder
        if (files[choice].is_folder) {
            // Navigate into the selected folder
            strcpy(current_folder_id, files[choice].id);
            strcpy(current_folder_name, files[choice].name);
        } else {
            // Download the selected file
            print_info("Preparing to download file...");
            download_file_with_progress(files[choice].id, files[choice].name);
            // After download, we can break or stay in the browser. Let's break for now.
            free(files);
            break;
        }

        free(files);
    }

    return 0;
}

static int fetch_files_for_browser(const char *folder_id, BrowserFile **files, int *count) {
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

        if (response.data) {
            free(response.data);
            response.data = NULL;
            response.size = 0;
        }
    }

    if (res != CURLE_OK || http_code != 200) {
        if (response.data) free(response.data);
        return -1;
    }

    // Parse JSON
    json_object *root = json_tokener_parse(response.data);
    if (!root) {
        free(response.data);
        return -1;
    }

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

struct DownloadData {
    FILE *fp;
    // You can add progress bar data here later
};

static size_t write_file_callback(void *ptr, size_t size, size_t nmemb, void *stream) {
    struct DownloadData *data = (struct DownloadData *)stream;
    return fwrite(ptr, size, nmemb, data->fp);
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

    struct DownloadData data = { .fp = fp };

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

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        // Add progress bar setup here if desired, similar to version.c

        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && http_code == 200) break;
        if (res != CURLE_OK || (http_code != 401 && http_code != 403)) break;

        // Auth error, rewind file and retry
        rewind(fp);
    }

    fclose(fp);

    if (res != CURLE_OK || http_code != 200) {
        print_error("Download failed.");
        if (http_code != 200) fprintf(stderr, "HTTP Error: %ld\n", http_code);
        if (res != CURLE_OK) fprintf(stderr, "cURL Error: %s\n", curl_easy_strerror(res));
        remove(filename); // Delete partial file
        return -1;
    }

    print_success("File downloaded successfully!");
    printf("Saved as: %s\n", filename);
    return 0;
}