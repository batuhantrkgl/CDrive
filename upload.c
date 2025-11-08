#define _GNU_SOURCE
#include "cdrive.h"

char *get_file_mime_type(const char *filename) {
    const char *extension = strrchr(filename, '.');
    if (!extension) return strdup("application/octet-stream");
    
    // Common MIME types
    if (strcasecmp(extension, ".txt") == 0) return strdup("text/plain");
    if (strcasecmp(extension, ".pdf") == 0) return strdup("application/pdf");
    if (strcasecmp(extension, ".doc") == 0) return strdup("application/msword");
    if (strcasecmp(extension, ".docx") == 0) return strdup("application/vnd.openxmlformats-officedocument.wordprocessingml.document");
    if (strcasecmp(extension, ".jpg") == 0 || strcasecmp(extension, ".jpeg") == 0) return strdup("image/jpeg");
    if (strcasecmp(extension, ".png") == 0) return strdup("image/png");
    if (strcasecmp(extension, ".gif") == 0) return strdup("image/gif");
    if (strcasecmp(extension, ".mp4") == 0) return strdup("video/mp4");
    if (strcasecmp(extension, ".mp3") == 0) return strdup("audio/mpeg");
    if (strcasecmp(extension, ".zip") == 0) return strdup("application/zip");
    if (strcasecmp(extension, ".json") == 0) return strdup("application/json");
    if (strcasecmp(extension, ".xml") == 0) return strdup("application/xml");
    if (strcasecmp(extension, ".html") == 0) return strdup("text/html");
    if (strcasecmp(extension, ".css") == 0) return strdup("text/css");
    if (strcasecmp(extension, ".js") == 0) return strdup("application/javascript");
    if (strcasecmp(extension, ".py") == 0) return strdup("text/x-python");
    if (strcasecmp(extension, ".c") == 0) return strdup("text/x-c");
    if (strcasecmp(extension, ".cpp") == 0 || strcasecmp(extension, ".cc") == 0) return strdup("text/x-c++");
    if (strcasecmp(extension, ".h") == 0) return strdup("text/x-c");
    
    return strdup("application/octet-stream");
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
        clock_gettime(CLOCK_MONOTONIC, &current_time);

        // Initialize on first call
        if (progress->start_time.tv_sec == 0) {
            clock_gettime(CLOCK_MONOTONIC, &progress->start_time);
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
        printf("\r\033[K"); 
        
        print_colored(spinner_chars[spinner_frame], COLOR_YELLOW);
        printf(" Uploading ");
        print_colored(progress->filename, COLOR_BOLD);
        printf("... %.1f%%", percentage);

        // Calculate and display ETA based on average speed
        double elapsed_s = (current_time.tv_sec - progress->start_time.tv_sec) + 
                           (current_time.tv_nsec - progress->start_time.tv_nsec) / 1e9;
        
        if (elapsed_s > 0.5 && ulnow > 0) {
            double speed = (double)ulnow / elapsed_s; // Average speed in bytes/sec
            double remaining_bytes = ultotal - ulnow;
            int eta_seconds = (int)(remaining_bytes / speed);

            printf(" (ETA: ");
            if (eta_seconds < 60) {
                printf("%ds", eta_seconds);
            } else {
                printf("%dm %ds", eta_seconds / 60, eta_seconds % 60);
            }
            printf(")");
        }
        fflush(stdout);
    }
    
    return 0;
}

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
    const char *filename = filename_fslash > filename_bslash ? filename_fslash : filename_bslash;
    if (filename) {
        filename++; // Skip the '/'
    } else {
        filename = source_path;
    }
    
    // Get file MIME type
    char *mime_type = get_file_mime_type(source_path);

    // Prepare metadata JSON
    char metadata_str[512];
    if (strcmp(target_folder, "root") == 0) {
        snprintf(metadata_str, sizeof(metadata_str), 
                "{\"name\":\"%s\"}", filename);
    } else {
        snprintf(metadata_str, sizeof(metadata_str), 
                "{\"name\":\"%s\",\"parents\":[\"%s\"]}", filename, target_folder);
    }
    
    long http_code = 0;

    start_spinner(&setup_spinner, "Preparing upload...");

    // Load tokens from file
    if (load_tokens(&g_tokens) != 0) {
        stop_spinner(&setup_spinner);
        print_error("Not authenticated. Run 'cdrive auth login' first.");
        free(mime_type);
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
            stop_spinner(&setup_spinner);
            printf("\n");
            print_info("Access token expired. Refreshing...");
            if (refresh_access_token(&g_tokens) != 0 || save_tokens(&g_tokens) != 0) {
                print_error("Failed to refresh token. Please re-authenticate with 'cdrive auth login'.");
                free(mime_type);
                return -1;
            }
            print_success("Token refreshed successfully");
            start_spinner(&setup_spinner, "Preparing upload...");
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
        strncpy(progress_data.filename, filename, sizeof(progress_data.filename) - 1);

        // Configure curl options
        curl_easy_setopt(curl, CURLOPT_URL, "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);

        if (attempt == 0) stop_spinner(&setup_spinner);

        res = curl_easy_perform(curl);
        printf("\r\033[K"); // Clear progress line

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
        free(mime_type);
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
        free(mime_type);
        return -1;
    }

    // Parse response to get file info
    if (response.data) {
        json_object *root = json_tokener_parse(response.data);
        if (root) {
            json_object *id_obj;
            const char *file_id = NULL;
            
            if (json_object_object_get_ex(root, "id", &id_obj)) {
                file_id = json_object_get_string(id_obj);
                
                // Generate direct download link
                char download_link[MAX_URL_SIZE];
                snprintf(download_link, sizeof(download_link), 
                        "https://drive.google.com/uc?export=download&id=%s", file_id);
                
                // Store in global variable for potential future use
                strncpy(g_last_upload_link, download_link, MAX_URL_SIZE - 1);
                g_last_upload_link[MAX_URL_SIZE - 1] = '\0';
                
                // Simple, clean output like GitHub CLI
                print_success("Upload complete!");
                printf("\n%s\n\n", download_link);
            }
            
            json_object_put(root);
        }
        free(response.data);
    }
    
    free(mime_type);
    
    return 0;
}

int cdrive_list_files(const char *folder_id) {
    CURL *curl;
    CURLcode res;
    APIResponse response = {0};
    LoadingSpinner list_spinner = {0};
    
    start_spinner(&list_spinner, "Fetching files from Google Drive...");
    
    // Load tokens
    if (load_tokens(&g_tokens) != 0) {
        stop_spinner(&list_spinner);
        print_error("Not authenticated. Run 'cdrive auth login' first.");
        return -1;
    }
    
    curl = curl_easy_init();
    if (!curl) {
        stop_spinner(&list_spinner);
        print_error("Error initializing curl");
        return -1;
    }
    
    // Build URL
    char url[1024];
    if (strcmp(folder_id, "root") == 0) {
        snprintf(url, sizeof(url), 
                "%s?q=%%27root%%27%%20in%%20parents%%20and%%20trashed=false&fields=files(id,name,mimeType,size,modifiedTime)", 
                DRIVE_API_URL);
    } else {
        snprintf(url, sizeof(url), 
                "%s?q=%%27%s%%27%%20in%%20parents%%20and%%20trashed=false&fields=files(id,name,mimeType,size,modifiedTime)", 
                DRIVE_API_URL, folder_id);
    }
    
    // Set up authorization header
    char auth_header[MAX_HEADER_SIZE];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    
    // Configure curl
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    stop_spinner(&list_spinner);
    
    if (res != CURLE_OK) {
        print_error("Failed to list files");
        printf("Details: %s\n", curl_easy_strerror(res));
        if (response.data) free(response.data);
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
                            
                            printf("\t%-40.40s\t", json_object_get_string(name_obj));
                            print_colored(json_object_get_string(id_obj), COLOR_YELLOW);
                            printf("\n");
                        }
                    }
                    printf("\n");
                } else {
                    print_info("This folder is empty.");
                }
            }
            json_object_put(root);
        }
        free(response.data);
    }
    
    return 0;
}

int cdrive_create_folder(const char *folder_name, const char *parent_id) {
    CURL *curl;
    CURLcode res;
    APIResponse response = {0};
    
    // Load tokens
    if (load_tokens(&g_tokens) != 0) {
        print_error("Not authenticated. Run 'cdrive auth login' first.");
        return -1;
    }
    
    curl = curl_easy_init();
    if (!curl) {
        print_error("Error initializing curl");
        return -1;
    }
    
    // Prepare JSON data
    char json_data[512];
    if (strcmp(parent_id, "root") == 0) {
        snprintf(json_data, sizeof(json_data), 
                "{\"name\":\"%s\",\"mimeType\":\"application/vnd.google-apps.folder\"}", 
                folder_name);
    } else {
        snprintf(json_data, sizeof(json_data), 
                "{\"name\":\"%s\",\"mimeType\":\"application/vnd.google-apps.folder\",\"parents\":[\"%s\"]}", 
                folder_name, parent_id);
    }
    
    // Set up headers
    char auth_header[MAX_HEADER_SIZE];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    // Configure curl
    curl_easy_setopt(curl, CURLOPT_URL, DRIVE_API_URL);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    // Clean up
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK) {
        print_error("Failed to create folder");
        printf("Details: %s\n", curl_easy_strerror(res));
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
                printf("\n");
                print_colored("  Name: ", COLOR_BOLD); printf("%s\n", json_object_get_string(name_obj));
                print_colored("  ID:   ", COLOR_BOLD); printf("%s\n\n", json_object_get_string(id_obj));
            }
            
            json_object_put(root);
        }
        free(response.data);
    }
    
    return 0;
}
