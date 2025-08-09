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
};

int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal; // Suppress unused parameter warning
    (void)dlnow;   // Suppress unused parameter warning
    
    struct ProgressData *progress = (struct ProgressData *)clientp;
    
    if (ultotal > 0) {
        double percentage = (double)ulnow / ultotal * 100.0;
        
        // Only update every 25% to be minimal, and always show 100%
        if ((percentage >= 25.0 && progress->last_percentage < 25.0) ||
            (percentage >= 50.0 && progress->last_percentage < 50.0) ||
            (percentage >= 75.0 && progress->last_percentage < 75.0) ||
            (percentage >= 100.0 && progress->last_percentage < 100.0)) {
            printf("\ruploading... %.0f%%", percentage);
            fflush(stdout);
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
    
    // Load tokens
    if (load_tokens(&g_tokens) != 0) {
        print_error("Not authenticated. Run 'cdrive auth login' first.");
        return -1;
    }
    
    // Check if file exists and get file size
    FILE *file = fopen(source_path, "rb");
    if (!file) {
        print_error("File not found or cannot be opened");
        return -1;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fclose(file);
    
    curl = curl_easy_init();
    if (!curl) {
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
    strncpy(progress_data.filename, filename, sizeof(progress_data.filename) - 1);
    
    // Configure curl options
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress_data);
    
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
    
    // Build URL
    char url[1024];
    if (strcmp(folder_id, "root") == 0) {
        snprintf(url, sizeof(url), 
                "%s?q='root' in parents and trashed=false&fields=files(id,name,mimeType,size,modifiedTime)", 
                DRIVE_API_URL);
    } else {
        snprintf(url, sizeof(url), 
                "%s?q='%s' in parents and trashed=false&fields=files(id,name,mimeType,size,modifiedTime)", 
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
                                print_colored("%.1f KB", COLOR_CYAN);
                                printf(")", (double)size / 1024);
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
