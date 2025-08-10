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
    double last_percentage;
    char filename[256];
    struct timespec start_time;
    curl_off_t last_bytes;
    LoadingSpinner *spinner;
    int spinner_active;
    int spinner_frame;
};

int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal; // Suppress unused parameter warning
    (void)dlnow;   // Suppress unused parameter warning
    
    struct ProgressData *progress = (struct ProgressData *)clientp;
    
    if (ultotal > 0) {
        double percentage = (double)ulnow / ultotal * 100.0;
        
        // Initialize start time on first call
        if (progress->start_time.tv_sec == 0 && progress->start_time.tv_nsec == 0) {
            clock_gettime(CLOCK_MONOTONIC, &progress->start_time);
            progress->last_bytes = ulnow;
        }
        
        // Update more frequently for real-time display (every 0.1% change or every 100ms)
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        
        // Calculate elapsed time in milliseconds
        double elapsed_ms = (current_time.tv_sec - progress->start_time.tv_sec) * 1000.0 + 
                           (current_time.tv_nsec - progress->start_time.tv_nsec) / 1000000.0;
        
        // Spinner characters for manual rotation
        const char* spinner_chars[] = {"|", "/", "-", "\\", "|", "/", "-", "\\", "|", "/"};
        const int spinner_count = 10;
        
        // Update if percentage changed by 0.1% or if 100ms have passed
        if (fabs(percentage - progress->last_percentage) >= 0.1 || elapsed_ms >= 100) {
            // Rotate spinner frame
            progress->spinner_frame = (progress->spinner_frame + 1) % spinner_count;
            
            if (percentage >= 100.0) {
                // Stop spinner before showing completion (only once)
                if (progress->spinner_active) {
                    progress->spinner_active = 0;
                    
                    printf("\r\033[K");
                    print_colored("[+] ", COLOR_GREEN);
                    print_colored("Uploading... ", COLOR_BLUE);
                    printf("%.1f%% ", percentage);
                    print_colored("(completed)", COLOR_GREEN);
                    fflush(stdout);
                }
            } else if (elapsed_ms > 500 && ulnow > 0) { // Wait 500ms before showing ETA
                // Calculate ETA based on current upload speed
                double speed = (double)ulnow / (elapsed_ms / 1000.0); // bytes per second
                double remaining_bytes = ultotal - ulnow;
                double eta_seconds = remaining_bytes / speed;
                
                // Clear line completely and show progress with manual spinner
                printf("\r\033[K%s ", spinner_chars[progress->spinner_frame]);
                print_colored("Uploading... ", COLOR_BLUE);
                printf("%.1f%% ", percentage);
                print_colored("(ETA: ", COLOR_CYAN);
                
                if (eta_seconds < 60) {
                    // Show seconds only for short durations
                    int seconds = (int)eta_seconds;
                    printf(COLOR_YELLOW "%ds" COLOR_RESET, seconds);
                } else if (eta_seconds < 3600) {
                    // Show minutes and seconds (XXm XXs)
                    int minutes = (int)(eta_seconds / 60);
                    int seconds = (int)(eta_seconds - minutes * 60);
                    printf(COLOR_YELLOW "%dm %ds" COLOR_RESET, minutes, seconds);
                } else if (eta_seconds < 86400) {
                    // Show hours, minutes, and seconds (XXh XXm XXs)
                    int hours = (int)(eta_seconds / 3600);
                    int minutes = (int)((eta_seconds - hours * 3600) / 60);
                    int seconds = (int)(eta_seconds - hours * 3600 - minutes * 60);
                    printf(COLOR_YELLOW "%dh %dm %ds" COLOR_RESET, hours, minutes, seconds);
                } else {
                    // Show days, hours, and minutes (XXd XXh XXm)
                    int days = (int)(eta_seconds / 86400);
                    int hours = (int)((eta_seconds - days * 86400) / 3600);
                    int minutes = (int)((eta_seconds - days * 86400 - hours * 3600) / 60);
                    printf(COLOR_YELLOW "%dd %dh %dm" COLOR_RESET, days, hours, minutes);
                }
                print_colored(")", COLOR_CYAN);
                fflush(stdout);
                progress->spinner_active = 1;
            } else {
                // Clear line completely and show progress with manual spinner
                printf("\r\033[K%s ", spinner_chars[progress->spinner_frame]);
                print_colored("Uploading... ", COLOR_BLUE);
                printf("%.1f%% ", percentage);
                print_colored("(calculating...)", COLOR_MAGENTA);
                fflush(stdout);
                progress->spinner_active = 1;
            }
            
            progress->last_percentage = percentage;
        }
    }
    
    return 0;
}

int cdrive_upload(const char *source_path, const char *target_folder) {
    CURL *curl;
    CURLcode res;
    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    APIResponse response = {0};
    LoadingSpinner setup_spinner = {0};
    
    start_spinner(&setup_spinner, "Preparing upload...");
    
    // Load tokens
    if (load_tokens(&g_tokens) != 0) {
        stop_spinner(&setup_spinner);
        print_error("Not authenticated. Run 'cdrive auth login' first.");
        return -1;
    }
    
    // Check if file exists and get file size
    FILE *file = fopen(source_path, "rb");
    if (!file) {
        stop_spinner(&setup_spinner);
        print_error("File not found or cannot be opened");
        return -1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);
    
    curl = curl_easy_init();
    if (!curl) {
        stop_spinner(&setup_spinner);
        print_error("Error initializing curl");
        return -1;
    }
    
    // Get file MIME type
    char *mime_type = get_file_mime_type(source_path);
    
    // Extract filename from path
    const char *filename = strrchr(source_path, '/');
    if (filename) {
        filename++; // Skip the '/'
    } else {
        filename = source_path;
    }
    
    // Prepare metadata JSON
    char metadata_str[512];
    if (strcmp(target_folder, "root") == 0) {
        snprintf(metadata_str, sizeof(metadata_str), 
                "{\"name\":\"%s\"}", filename);
    } else {
        snprintf(metadata_str, sizeof(metadata_str), 
                "{\"name\":\"%s\",\"parents\":[\"%s\"]}", filename, target_folder);
    }
    
    // Create form data
    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, "metadata",
                 CURLFORM_COPYCONTENTS, metadata_str,
                 CURLFORM_CONTENTTYPE, "application/json; charset=UTF-8",
                 CURLFORM_END);
    
    curl_formadd(&formpost, &lastptr,
                 CURLFORM_COPYNAME, "media",
                 CURLFORM_FILE, source_path,
                 CURLFORM_CONTENTTYPE, mime_type,
                 CURLFORM_END);
    
    // Set up authorization header
    char auth_header[1024];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", g_tokens.access_token);
    
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    
    // Set up progress tracking
    struct ProgressData progress_data = {0};
    LoadingSpinner upload_spinner = {0};
    strncpy(progress_data.filename, filename, sizeof(progress_data.filename) - 1);
    progress_data.spinner = &upload_spinner;
    
    // Configure curl options
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
    
    stop_spinner(&setup_spinner);
    
    print_colored("-> ", COLOR_CYAN);
    print_colored("Starting upload: ", COLOR_YELLOW);
    print_colored(filename, COLOR_GREEN);
    printf("\n");
    
    // Perform upload
    res = curl_easy_perform(curl);
    
    // Clean up
    curl_formfree(formpost);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    printf("\n"); // New line after progress
    
    if (res != CURLE_OK) {
        fprintf(stderr, "upload failed: %s\n", curl_easy_strerror(res));
        if (response.data) free(response.data);
        return -1;
    }
    
    // Parse response to get file info
    if (response.data) {
        json_object *root = json_tokener_parse(response.data);
        if (root) {
            json_object *id_obj, *web_view_link_obj;
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
                printf("%s\n\n", download_link);
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
    char auth_header[1024];
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
            if (json_object_object_get_ex(root, "files", &files_array)) {
                int num_files = json_object_array_length(files_array);
                
                if (num_files == 0) {
                    print_info("No files found in this folder.");
                } else {
                    printf("\n");
                    print_colored("📁 Files in folder:\n\n", COLOR_BOLD);
                    
                    for (int i = 0; i < num_files; i++) {
                        json_object *file_obj = json_object_array_get_idx(files_array, i);
                        json_object *id_obj, *name_obj, *mime_type_obj, *size_obj;
                        
                        if (json_object_object_get_ex(file_obj, "id", &id_obj) &&
                            json_object_object_get_ex(file_obj, "name", &name_obj) &&
                            json_object_object_get_ex(file_obj, "mimeType", &mime_type_obj)) {
                            
                            const char *mime_type = json_object_get_string(mime_type_obj);
                            
                            // Choose icon based on MIME type
                            if (strstr(mime_type, "folder")) {
                                print_colored("📁 ", COLOR_BLUE);
                            } else if (strstr(mime_type, "image")) {
                                print_colored("🖼️  ", COLOR_MAGENTA);
                            } else if (strstr(mime_type, "video")) {
                                print_colored("🎥 ", COLOR_RED);
                            } else if (strstr(mime_type, "audio")) {
                                print_colored("🎵 ", COLOR_YELLOW);
                            } else {
                                print_colored("📄 ", COLOR_WHITE);
                            }
                            
                            print_colored(json_object_get_string(name_obj), COLOR_BOLD);
                            
                            if (json_object_object_get_ex(file_obj, "size", &size_obj)) {
                                long long size = json_object_get_int64(size_obj);
                                printf(" (");
                                printf(COLOR_CYAN "%.1f KB" COLOR_RESET, (double)size / 1024);
                                printf(")");
                            }
                            
                            printf("\n");
                            print_colored("   ID: ", COLOR_YELLOW);
                            printf("%s\n\n", json_object_get_string(id_obj));
                        }
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
    char auth_header[1024];
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
            json_object *id_obj, *name_obj, *web_view_link_obj;
            
            if (json_object_object_get_ex(root, "id", &id_obj)) {
                print_colored("🆔 Folder ID: ", COLOR_BLUE);
                printf("%s\n", json_object_get_string(id_obj));
            }
            
            if (json_object_object_get_ex(root, "name", &name_obj)) {
                print_colored("📁 Name: ", COLOR_BLUE);
                printf("%s\n", json_object_get_string(name_obj));
            }
            
            if (json_object_object_get_ex(root, "webViewLink", &web_view_link_obj)) {
                print_colored("🔗 View Link: ", COLOR_BLUE);
                print_colored(json_object_get_string(web_view_link_obj), COLOR_CYAN);
                printf("\n");
            }
            
            json_object_put(root);
        }
        free(response.data);
    }
    
    return 0;
}
