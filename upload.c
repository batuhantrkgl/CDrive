#define _GNU_SOURCE
#include "cdrive.h"

const char *get_file_mime_type(const char *filename) {
    const char *extension = strrchr(filename, '.');
    if (!extension) return "application/octet-stream";
    
    // Common MIME types
    if (strcasecmp(extension, ".txt") == 0) return "text/plain";
    if (strcasecmp(extension, ".pdf") == 0) return "application/pdf";
    if (strcasecmp(extension, ".doc") == 0) return "application/msword";
    if (strcasecmp(extension, ".docx") == 0) return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    if (strcasecmp(extension, ".jpg") == 0 || strcasecmp(extension, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(extension, ".png") == 0) return "image/png";
    if (strcasecmp(extension, ".gif") == 0) return "image/gif";
    if (strcasecmp(extension, ".mp4") == 0) return "video/mp4";
    if (strcasecmp(extension, ".mp3") == 0) return "audio/mpeg";
    if (strcasecmp(extension, ".zip") == 0) return "application/zip";
    if (strcasecmp(extension, ".json") == 0) return "application/json";
    if (strcasecmp(extension, ".xml") == 0) return "application/xml";
    if (strcasecmp(extension, ".html") == 0) return "text/html";
    if (strcasecmp(extension, ".css") == 0) return "text/css";
    if (strcasecmp(extension, ".js") == 0) return "application/javascript";
    if (strcasecmp(extension, ".py") == 0) return "text/x-python";
    if (strcasecmp(extension, ".c") == 0) return "text/x-c";
    if (strcasecmp(extension, ".cpp") == 0 || strcasecmp(extension, ".cc") == 0) return "text/x-c++";
    if (strcasecmp(extension, ".h") == 0) return "text/x-c";
    
    return "application/octet-stream";
}

struct ProgressData {
    char filename[256];
    struct timespec start_time;
    struct timespec last_update_time;
};

int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal; // Suppress unused parameter warning
    (void)dlnow;   // Suppress unused parameter warning
    
    struct ProgressData *progress = (struct ProgressData *)clientp;

    if (ultotal > 0) {
        struct timespec current_time;
        clock_gettime_mono(&current_time);

        // Initialize on first call
        if (progress->start_time.tv_sec == 0) {
            clock_gettime_mono(&progress->start_time);
            progress->last_update_time = current_time;
        }

        // Throttle updates to about 10 per second (100ms) to prevent flickering
        double elapsed_since_last_update_ms = (current_time.tv_sec - progress->last_update_time.tv_sec) * 1000.0 + 
                                             (current_time.tv_nsec - progress->last_update_time.tv_nsec) / 1000000.0;

        if (elapsed_since_last_update_ms < 100.0 && ulnow < ultotal) {
            return 0;
        }

        progress->last_update_time = current_time;

        double percentage = (double)ulnow / ultotal * 100.0;

        // Use the same spinner characters as the rest of the app for consistency
        static const char* spinner_chars[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        static int spinner_frame = 0;
        spinner_frame = (spinner_frame + 1) % (sizeof(spinner_chars)/sizeof(char*));

        // Clear the entire line and print the progress bar
        fprintf(stderr, "\r\033[K");

        fprintf(stderr, "%s%s%s", COLOR_YELLOW, spinner_chars[spinner_frame], COLOR_RESET);
        fprintf(stderr, " Uploading ");
        fprintf(stderr, "%s%s%s", COLOR_BOLD, progress->filename, COLOR_RESET);
        fprintf(stderr, "... %.1f%%", percentage);

        // Calculate and display ETA based on average speed
        double elapsed_s = (current_time.tv_sec - progress->start_time.tv_sec) +
                           (current_time.tv_nsec - progress->start_time.tv_nsec) / 1e9;

        if (elapsed_s > 0.5 && ulnow > 0) {
            double speed = (double)ulnow / elapsed_s; // Average speed in bytes/sec
            double remaining_bytes = (ultotal > ulnow) ? (double)(ultotal - ulnow) : 0;
            double raw_eta = (speed > 0) ? (remaining_bytes / speed) : 0;
            int eta_seconds = (raw_eta > 86400.0) ? 86400 : (int)raw_eta;

            fprintf(stderr, " (ETA: ");
            if (eta_seconds >= 86400) {
                fprintf(stderr, ">24h");
            } else if (eta_seconds < 60) {
                fprintf(stderr, "%ds", eta_seconds);
            } else {
                fprintf(stderr, "%dm %ds", eta_seconds / 60, eta_seconds % 60);
            }
            fprintf(stderr, ")");
        }
        fflush(stderr);
    }

    return 0;
}

char *escape_drive_query(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *escaped = malloc(len * 2 + 1);
    if (!escaped) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\'' || str[i] == '\\') {
            escaped[j++] = '\\';
        }
        escaped[j++] = str[i];
    }
    escaped[j] = '\0';
    return escaped;
}

static void format_display_name(const char *name, char *buf, size_t buf_len, size_t max_len) {
    if (!name || buf_len == 0) {
        if (buf_len > 0) buf[0] = '\0';
        return;
    }
    size_t len = strlen(name);
    if (len <= max_len) {
        snprintf(buf, buf_len, "%s", name);
    } else {
        if (max_len > 3) {
            size_t keep = max_len - 3;
            if (keep >= buf_len) keep = buf_len - 4;
            snprintf(buf, buf_len, "%.*s...", (int)keep, name);
        } else {
            snprintf(buf, buf_len, "%.*s", (int)max_len, name);
        }
    }
}

int cdrive_search(const char *query) {
    APIResponse response = {0};

    if (load_tokens(&g_tokens) != 0) {
        print_error("Not authenticated. Run 'cdrive auth login' first.");
        return -1;
    }

    char *escaped_query = escape_drive_query(query);
    if (!escaped_query) {
        print_error("Failed to process search query");
        return -1;
    }

    char *encoded_query = url_encode(escaped_query);
    free(escaped_query);
    if (!encoded_query) {
        print_error("Failed to encode search query");
        return -1;
    }

    char url[2048];
    snprintf(url, sizeof(url),
             "%s?q=name%%20contains%%20%%27%s%%27%%20and%%20trashed=false&fields=files(id,name,mimeType,size,modifiedTime)&pageSize=1000",
             DRIVE_API_URL, encoded_query);
    free(encoded_query);

    if (cdrive_api_get(url, &response) != 0) {
        print_error("Search failed");
        return -1;
    }

    if (response.data) {
        json_object *root = json_tokener_parse(response.data);
        if (root) {
            json_object *files_array;
            if (json_object_object_get_ex(root, "files", &files_array) &&
                json_object_get_type(files_array) == json_type_array) {
                int num_files = json_object_array_length(files_array);

                if (g_json_mode) {
                    printf("[");
                    for (int i = 0; i < num_files; i++) {
                        json_object *file_obj = json_object_array_get_idx(files_array, i);
                        printf("%s", json_object_to_json_string(file_obj));
                        if (i < num_files - 1) printf(",");
                    }
                    printf("]");
                } else {
                    printf("\n");
                    if (num_files > 0) {
                        print_colored("TYPE\tNAME\t\t\t\t\tID\n", COLOR_BOLD);
                        print_colored("----\t----\t\t\t\t\t--\n", COLOR_BOLD);

                        for (int i = 0; i < num_files; i++) {
                            json_object *file_obj = json_object_array_get_idx(files_array, i);
                            json_object *id_obj, *name_obj, *mime_type_obj;

                            if (json_object_object_get_ex(file_obj, "id", &id_obj) &&
                                json_object_object_get_ex(file_obj, "name", &name_obj) &&
                                json_object_object_get_ex(file_obj, "mimeType", &mime_type_obj)) {
                                const char *mime_type = json_object_get_string(mime_type_obj);
                                if (strcmp(mime_type, "application/vnd.google-apps.folder") == 0) {
                                    print_colored("[DIR] ", COLOR_CYAN);
                                } else {
                                    print_colored("[FILE]", COLOR_WHITE);
                                }
                                char display_name[64];
                                format_display_name(json_object_get_string(name_obj), display_name, sizeof(display_name), 40);
                                printf("\t%-40.40s\t", display_name);
                                print_colored(json_object_get_string(id_obj), COLOR_YELLOW);
                                printf("\n");
                            }
                        }
                    } else {
                        print_info("No files found matching the search query.");
                    }
                }
            }
            json_object_put(root);
        }
        free(response.data);
    }

    return 0;
}

int cdrive_share(const char *file_id, const char *email, const char *role) {
    if (!role || (strcmp(role, "reader") != 0 &&
                  strcmp(role, "writer") != 0 &&
                  strcmp(role, "commenter") != 0 &&
                  strcmp(role, "owner") != 0 &&
                  strcmp(role, "organizer") != 0 &&
                  strcmp(role, "fileOrganizer") != 0)) {
        print_error("Invalid role. Must be one of: reader, writer, commenter, owner, organizer, fileOrganizer");
        return -1;
    }

    APIResponse response = {0};

    if (load_tokens(&g_tokens) != 0) {
        print_error("Not authenticated. Run 'cdrive auth login' first.");
        return -1;
    }

    char url[1024];
    snprintf(url, sizeof(url), "https://www.googleapis.com/drive/v3/files/%s/permissions", file_id);

    json_object *share_obj = json_object_new_object();
    json_object_object_add(share_obj, "type", json_object_new_string("user"));
    json_object_object_add(share_obj, "role", json_object_new_string(role));
    json_object_object_add(share_obj, "emailAddress", json_object_new_string(email));
    const char *post_data = json_object_to_json_string(share_obj);

    CURLcode res = CURLE_FAILED_INIT;
    long http_code = 0;

    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt > 0) {
            if (refresh_access_token(&g_tokens) != 0 || save_tokens(&g_tokens) != 0) break;
        }

        CURL *curl = curl_easy_init();
        if (!curl) break;

        char auth_header[MAX_HEADER_SIZE];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
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

    json_object_put(share_obj);

    if (res != CURLE_OK || http_code != 200) {
        print_error("Failed to share file");
        if (response.data) {
            json_object *root = json_tokener_parse(response.data);
            if (root) {
                json_object *error_obj, *message_obj;
                if (json_object_object_get_ex(root, "error", &error_obj) &&
                    json_object_object_get_ex(error_obj, "message", &message_obj)) {
                    fprintf(stderr, "API Error: %s\n", json_object_get_string(message_obj));
                }
                json_object_put(root);
            }
        }
        if (response.data) free(response.data);
        return -1;
    }

    if (response.data) free(response.data);

    if (g_json_mode) {
        printf("{\"status\":\"success\",\"file_id\":\"%s\",\"shared_with\":\"%s\",\"role\":\"%s\"}\n",
               file_id, email, role);
    } else {
        print_success("File shared successfully!");
        printf("  File ID: %s\n", file_id);
        printf("  Shared with: %s\n", email);
        printf("  Role: %s\n", role);
    }

    return 0;
}

// Portable glob expansion
#ifdef _WIN32
    #include <io.h>
    int cdrive_glob(const char *pattern, char ***results, int *count) {
        struct _finddata_t fd;
        intptr_t handle = _findfirst(pattern, &fd);
        if (handle == -1) return -1;

        // Extract directory prefix if pattern contains path separators
        char dir_prefix[MAX_PATH_SIZE] = {0};
        const char *last_sep1 = strrchr(pattern, '/');
        const char *last_sep2 = strrchr(pattern, '\\');
        const char *last_sep = NULL;
        if (last_sep1 && last_sep2) {
            last_sep = (last_sep1 > last_sep2) ? last_sep1 : last_sep2;
        } else {
            last_sep = last_sep1 ? last_sep1 : last_sep2;
        }
        if (last_sep) {
            size_t prefix_len = (size_t)(last_sep - pattern + 1);
            if (prefix_len < sizeof(dir_prefix)) {
                memcpy(dir_prefix, pattern, prefix_len);
                dir_prefix[prefix_len] = '\0';
            }
        }

        int capacity = 16;
        *results = malloc(sizeof(char *) * (size_t)capacity);
        *count = 0;

        do {
            if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0) continue;
            if (*count >= capacity) {
                capacity *= 2;
                char **new_results = realloc(*results, sizeof(char *) * (size_t)capacity);
                if (!new_results) {
                    for (int j = 0; j < *count; j++) free((*results)[j]);
                    free(*results);
                    _findclose(handle);
                    return -1;
                }
                *results = new_results;
            }
            char full_path[MAX_PATH_SIZE];
            if (dir_prefix[0]) {
                snprintf(full_path, sizeof(full_path), "%s%s", dir_prefix, fd.name);
            } else {
                snprintf(full_path, sizeof(full_path), "%s", fd.name);
            }
            (*results)[*count] = strdup(full_path);
            if (!(*results)[*count]) {
                for (int j = 0; j < *count; j++) free((*results)[j]);
                free(*results);
                _findclose(handle);
                return -1;
            }
            (*count)++;
        } while (_findnext(handle, &fd) == 0);

        _findclose(handle);

        if (*count == 0) { free(*results); *results = NULL; return -1; }
        return 0;
    }
#else
    #include <glob.h>
    int cdrive_glob(const char *pattern, char ***results, int *count) {
        glob_t g;
        int ret = glob(pattern, GLOB_MARK, NULL, &g);
        if (ret != 0) return -1;

        *count = (int)g.gl_pathc;
        *results = malloc(sizeof(char *) * (size_t)(*count));
        if (!*results) { globfree(&g); return -1; }

        for (int i = 0; i < *count; i++) {
            (*results)[i] = strdup(g.gl_pathv[i]);
            if (!(*results)[i]) {
                for (int j = 0; j < i; j++) free((*results)[j]);
                free(*results); globfree(&g); return -1;
            }
        }

        globfree(&g);
        return 0;
    }
#endif

int cdrive_upload(const char *source_path, const char *target_folder) {
    CURL *curl;
    CURLcode res;
    APIResponse response = {0};
    LoadingSpinner setup_spinner = {0};
    curl_mime *mime = NULL;
    curl_mimepart *part;
    
    // Check if the source file exists and is a regular file
    struct stat path_stat;
    if (stat(source_path, &path_stat) != 0) {
        print_error("File not found or cannot be accessed");
        perror(source_path);
        return -1;
    }

    if (!S_ISREG(path_stat.st_mode)) {
        print_error("The specified path is not a regular file.");
        return -1;
    }

    // Extract filename from path
    const char *filename_fslash = strrchr(source_path, '/');
    const char *filename_bslash = strrchr(source_path, '\\');
    const char *filename = NULL;
    if (filename_fslash && filename_bslash) {
        filename = (filename_fslash > filename_bslash) ? filename_fslash : filename_bslash;
    } else {
        filename = filename_fslash ? filename_fslash : filename_bslash;
    }
    if (filename && *filename) {
        filename++; // Skip the '/'
    } else {
        filename = source_path;
    }
    
    // Clean any trailing slash/backslash
    size_t fn_len = strlen(filename);
    while (fn_len > 0 && (filename[fn_len - 1] == '/' || filename[fn_len - 1] == '\\')) {
        fn_len--;
    }
    char safe_filename[256];
    if (fn_len == 0) {
        strncpy(safe_filename, "uploaded_file", sizeof(safe_filename) - 1);
        safe_filename[sizeof(safe_filename) - 1] = '\0';
    } else {
        size_t copy_len = (fn_len < sizeof(safe_filename) - 1) ? fn_len : sizeof(safe_filename) - 1;
        memcpy(safe_filename, filename, copy_len);
        safe_filename[copy_len] = '\0';
    }

    // Get file MIME type
    const char *mime_type = get_file_mime_type(source_path);

    // Prepare metadata JSON safely using json-c
    json_object *meta_obj = json_object_new_object();
    json_object_object_add(meta_obj, "name", json_object_new_string(safe_filename));
    if (target_folder && strlen(target_folder) > 0 && strcmp(target_folder, "root") != 0) {
        json_object *parents_arr = json_object_new_array();
        json_object_array_add(parents_arr, json_object_new_string(target_folder));
        json_object_object_add(meta_obj, "parents", parents_arr);
    }
    const char *metadata_str = json_object_to_json_string(meta_obj);
    
    long http_code = 0;

    if (!g_json_mode) start_spinner(&setup_spinner, "Preparing upload...");

    // Load tokens from file
    if (load_tokens(&g_tokens) != 0) {
        if (!g_json_mode) stop_spinner(&setup_spinner);
        print_error("Not authenticated. Run 'cdrive auth login' first.");
        json_object_put(meta_obj);
        return -1;
    }

    // Validate token before upload by making a quick API call
    CURL *test_curl = curl_easy_init();
    if (test_curl) {
        APIResponse test_response = {0};
        char auth_header[MAX_HEADER_SIZE];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
        struct curl_slist *test_headers = NULL;
        test_headers = curl_slist_append(test_headers, auth_header);
        
        curl_easy_setopt(test_curl, CURLOPT_URL, "https://www.googleapis.com/drive/v3/about?fields=user");
        curl_easy_setopt(test_curl, CURLOPT_HTTPHEADER, test_headers);
        curl_easy_setopt(test_curl, CURLOPT_WRITEFUNCTION, write_response_callback);
        curl_easy_setopt(test_curl, CURLOPT_WRITEDATA, &test_response);
        
        CURLcode test_res = curl_easy_perform(test_curl);
        long test_http_code = 0;
        curl_easy_getinfo(test_curl, CURLINFO_RESPONSE_CODE, &test_http_code);
        
        curl_slist_free_all(test_headers);
        curl_easy_cleanup(test_curl);
        if (test_response.data) free(test_response.data);
        
        // If token is expired, refresh it before upload
        if (test_res == CURLE_OK && (test_http_code == 401 || test_http_code == 403)) {
            if (!g_json_mode) stop_spinner(&setup_spinner);
            printf("\n");
            print_info("Access token expired. Refreshing...");
            if (refresh_access_token(&g_tokens) != 0 || save_tokens(&g_tokens) != 0) {
                print_error("Failed to refresh token. Please re-authenticate with 'cdrive auth login'.");
                json_object_put(meta_obj);
                return -1;
            }
            print_success("Token refreshed successfully");
            if (!g_json_mode) start_spinner(&setup_spinner, "Preparing upload...");
        }
    }

    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt == 1) {
            // Second attempt, try to refresh the token
            printf("\n");
            print_info("Upload failed due to authentication. Attempting to refresh token...");
            if (refresh_access_token(&g_tokens) != 0 || save_tokens(&g_tokens) != 0) {
                print_error("Failed to refresh token. Please re-authenticate with 'cdrive auth login'.");
                break; // Exit loop, will fail with the previous error code
            }
            print_info("Token refreshed. Retrying upload...");
        }

        curl = curl_easy_init();
        if (!curl) {
            print_error("Error initializing curl");
            break;
        }

        // Create the form
        mime = curl_mime_init(curl);
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "metadata");
        curl_mime_data(part, metadata_str, CURL_ZERO_TERMINATED);
        curl_mime_type(part, "application/json; charset=UTF-8");
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "media");
        curl_mime_filedata(part, source_path);
        curl_mime_type(part, mime_type);

        // Set up authorization header
        char auth_header[MAX_HEADER_SIZE];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, auth_header);

        // Set up progress tracking
        struct ProgressData progress_data = {0};
        snprintf(progress_data.filename, sizeof(progress_data.filename), "%s", safe_filename);

        // Configure curl options
        curl_easy_setopt(curl, CURLOPT_URL, "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        if (!g_json_mode) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
        } else {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        }

        if (attempt == 0 && !g_json_mode) stop_spinner(&setup_spinner);

        res = curl_easy_perform(curl);
        fprintf(stderr, "\r\033[K"); // Clear progress line

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        // Clean up for this attempt
        curl_slist_free_all(headers);
        curl_mime_free(mime);
        curl_easy_cleanup(curl);

        if (res == CURLE_OK && http_code == 200) {
            break; // Success, exit loop
        }

        if (res != CURLE_OK || (http_code != 401 && http_code != 403)) {
            break; // A non-auth error occurred, exit loop
        }

        // If we are here, it was a 401/403 error, loop will try to refresh
        if (response.data) {
            free(response.data);
            response.data = NULL;
            response.size = 0;
        }
    }
    
    // After the loop, check the final result
    if (res != CURLE_OK) {
        printf("\n");
        fprintf(stderr, "upload failed: %s\n", curl_easy_strerror(res));
        if (response.data) free(response.data);
        json_object_put(meta_obj);
        return -1;
    }
    
    // Check for HTTP errors from the API
    if (http_code != 200) {
        printf("\n"); // Newline after progress bar
        print_error("Upload failed due to an API error");
        fprintf(stderr, "HTTP Error: %ld\n", http_code);
        if (response.data) {
            // Try to parse for a more specific error message from Google
            json_object *root = json_tokener_parse(response.data);
            if (root) {
                json_object *error_obj, *message_obj;
                if (json_object_object_get_ex(root, "error", &error_obj) &&
                    json_object_object_get_ex(error_obj, "message", &message_obj)) {
                    fprintf(stderr, "API Message: %s\n", json_object_get_string(message_obj));
                }
                json_object_put(root);
            }
        }
        if (http_code == 401 || http_code == 403) {
            print_warning("Authentication token may be invalid or expired. Please run 'cdrive auth login' again.");
        }
        if (response.data) free(response.data);
        json_object_put(meta_obj);
        return -1;
    }

    // Parse response to get file info
    int upload_ok = 0;
    if (response.data) {
        json_object *root = json_tokener_parse(response.data);
        if (root) {
            json_object *id_obj;
            const char *file_id = NULL;
            
            if (json_object_object_get_ex(root, "id", &id_obj)) {
                file_id = json_object_get_string(id_obj);
                if (file_id && strlen(file_id) > 0) {
                    // Generate direct download link and web view link
                    char download_link[MAX_URL_SIZE];
                    char web_link[MAX_URL_SIZE];
                    snprintf(download_link, sizeof(download_link), 
                            "https://drive.google.com/uc?export=download&id=%s", file_id);
                    snprintf(web_link, sizeof(web_link),
                            "https://drive.google.com/file/d/%s/view", file_id);
                    
                    // Store web link in global variable for potential future use
                    strncpy(g_last_upload_link, web_link, MAX_URL_SIZE - 1);
                    g_last_upload_link[MAX_URL_SIZE - 1] = '\0';
                    
                    if (g_json_mode) {
                        printf("{\"status\":\"success\",\"file_id\":\"%s\",\"download_link\":\"%s\",\"web_link\":\"%s\"}\n",
                               file_id, download_link, web_link);
                    } else {
                        print_success("Upload complete!");
                        printf("Web Link:      %s\n", web_link);
                        printf("Download Link: %s\n\n", download_link);
                    }
                    upload_ok = 1;
                }
            }
            
            json_object_put(root);
        }
        free(response.data);
    }
    
    json_object_put(meta_obj);

    if (!upload_ok) {
        print_error("Failed to retrieve file ID from upload response.");
        return -1;
    }
    
    return 0;
}

int cdrive_list_files(const char *folder_id) {
    APIResponse response = {0};
    LoadingSpinner list_spinner = {0};
    
    if (!g_json_mode) {
        start_spinner(&list_spinner, "Fetching files from Google Drive...");
    }
    
    // Load tokens
    if (load_tokens(&g_tokens) != 0) {
        if (!g_json_mode) stop_spinner(&list_spinner);
        print_error("Not authenticated. Run 'cdrive auth login' first.");
        return -1;
    }
    
    // Build URL
    char url[1024];
    char *escaped_folder = (strcmp(folder_id, "root") == 0) ? strdup("root") : escape_drive_query(folder_id);
    char *encoded_folder = url_encode(escaped_folder ? escaped_folder : folder_id);
    if (escaped_folder) free(escaped_folder);
    snprintf(url, sizeof(url), 
            "%s?q=%%27%s%%27%%20in%%20parents%%20and%%20trashed=false&fields=files(id,name,mimeType,size,modifiedTime)&pageSize=1000", 
            DRIVE_API_URL, encoded_folder ? encoded_folder : folder_id);
    if (encoded_folder) free(encoded_folder);
    
    int api_result = cdrive_api_get(url, &response);
    if (!g_json_mode) stop_spinner(&list_spinner);
    
    if (api_result != 0) {
        print_error("Failed to list files. Check authentication or folder ID.");
        return -1;
    }
    
    // Parse and display results
    if (response.data) {
        json_object *root = json_tokener_parse(response.data);
        if (root) {
            json_object *files_array;
            if (json_object_object_get_ex(root, "files", &files_array) && 
                json_object_get_type(files_array) == json_type_array) {
                int num_files = json_object_array_length(files_array);
                
                if (g_json_mode) {
                    printf("[");
                    for (int i = 0; i < num_files; i++) {
                        json_object *file_obj = json_object_array_get_idx(files_array, i);
                        printf("%s", json_object_to_json_string(file_obj));
                        if (i < num_files - 1) printf(",");
                    }
                    printf("]\n");
                } else {
                    printf("\n");
                    if (num_files > 0) {
                        print_colored("TYPE\tNAME\t\t\t\t\tID\n", COLOR_BOLD);
                        print_colored("----\t----\t\t\t\t\t--\n", COLOR_BOLD);

                        for (int i = 0; i < num_files; i++) {
                            json_object *file_obj = json_object_array_get_idx(files_array, i);
                            json_object *id_obj, *name_obj, *mime_type_obj;
                            
                            if (json_object_object_get_ex(file_obj, "id", &id_obj) &&
                                json_object_object_get_ex(file_obj, "name", &name_obj) &&
                                json_object_object_get_ex(file_obj, "mimeType", &mime_type_obj)) 
                            {
                                const char *mime_type = json_object_get_string(mime_type_obj);
                                
                                if (strcmp(mime_type, "application/vnd.google-apps.folder") == 0) {
                                    print_colored("[DIR] ", COLOR_CYAN);
                                } else {
                                    print_colored("[FILE]", COLOR_WHITE);
                                }
                                
                                char display_name[64];
                                format_display_name(json_object_get_string(name_obj), display_name, sizeof(display_name), 40);
                                printf("\t%-40.40s\t", display_name);
                                print_colored(json_object_get_string(id_obj), COLOR_YELLOW);
                                printf("\n");
                            }
                        }
                        printf("\n");
                    } else {
                        print_info("This folder is empty.");
                    }
                }
            }
            json_object_put(root);
        }
        free(response.data);
    }
    
    return 0;
}

int cdrive_create_folder(const char *folder_name, const char *parent_id) {
    APIResponse response = {0};
    
    // Load tokens
    if (load_tokens(&g_tokens) != 0) {
        print_error("Not authenticated. Run 'cdrive auth login' first.");
        return -1;
    }
    
    // Prepare JSON data safely using json-c
    json_object *folder_obj = json_object_new_object();
    json_object_object_add(folder_obj, "name", json_object_new_string(folder_name));
    json_object_object_add(folder_obj, "mimeType", json_object_new_string("application/vnd.google-apps.folder"));
    if (parent_id && strlen(parent_id) > 0 && strcmp(parent_id, "root") != 0) {
        json_object *parents_arr = json_object_new_array();
        json_object_array_add(parents_arr, json_object_new_string(parent_id));
        json_object_object_add(folder_obj, "parents", parents_arr);
    }
    const char *json_data = json_object_to_json_string(folder_obj);
    
    CURLcode res = CURLE_FAILED_INIT;
    long http_code = 0;

    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt > 0) {
            if (refresh_access_token(&g_tokens) != 0 || save_tokens(&g_tokens) != 0) break;
        }

        CURL *curl = curl_easy_init();
        if (!curl) break;

        char auth_header[MAX_HEADER_SIZE];
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, auth_header);
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, DRIVE_API_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
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
    
    json_object_put(folder_obj);
    
    if (res != CURLE_OK || http_code != 200) {
        print_error("Failed to create folder");
        if (http_code != 200 && http_code != 0) fprintf(stderr, "HTTP Error: %ld\n", http_code);
        if (res != CURLE_OK) printf("Details: %s\n", curl_easy_strerror(res));
        if (response.data) free(response.data);
        return -1;
    }
    
    // Parse response
    if (response.data) {
        json_object *root = json_tokener_parse(response.data);
        if (root) {
            json_object *id_obj, *name_obj;
            
            if (json_object_object_get_ex(root, "id", &id_obj) &&
                json_object_object_get_ex(root, "name", &name_obj)) 
            {
                if (g_json_mode) {
                    printf("{\"status\":\"success\",\"name\":\"%s\",\"id\":\"%s\"}\n",
                           json_object_get_string(name_obj), json_object_get_string(id_obj));
                } else {
                    printf("\n");
                    print_colored("  Name: ", COLOR_BOLD); printf("%s\n", json_object_get_string(name_obj));
                    print_colored("  ID:   ", COLOR_BOLD); printf("%s\n\n", json_object_get_string(id_obj));
                }
            }
            
            json_object_put(root);
        }
        free(response.data);
    }
    
    return 0;
}
