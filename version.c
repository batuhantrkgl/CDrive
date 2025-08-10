#define _GNU_SOURCE
#include "cdrive.h"

// Forward declarations
static int save_update_cache(const UpdateInfo *update_info);
static int load_update_cache(UpdateInfo *update_info);

// Function to force check for updates (bypass cache)
int force_check_for_updates(UpdateInfo *update_info) {
    // Delete cache file to force fresh check
    char cache_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    
    if (home_dir) {
        snprintf(cache_path, sizeof(cache_path), "%s%s%s%s%s", 
                 home_dir, PATH_SEP, CONFIG_DIR, PATH_SEP, UPDATE_CACHE_FILE);
        unlink(cache_path); // Remove cache file
    }
    
    int result = check_for_updates(update_info);
    
    // Save new result to cache if successful
    if (result == 0) {
        save_update_cache(update_info);
    }
    
    return result;
}

// Cache functions for update checking
static int save_update_cache(const UpdateInfo *update_info) {
    char cache_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    
    if (!home_dir) return -1;
    
    snprintf(cache_path, sizeof(cache_path), "%s%s%s%s%s", 
             home_dir, PATH_SEP, CONFIG_DIR, PATH_SEP, UPDATE_CACHE_FILE);
    
    FILE *file = fopen(cache_path, "w");
    if (!file) return -1;
    
    time_t now = time(NULL);
    
    fprintf(file, "{\n");
    fprintf(file, "  \"timestamp\": %ld,\n", now);
    fprintf(file, "  \"version\": \"%s\",\n", update_info->version);
    fprintf(file, "  \"release_date\": \"%s\",\n", update_info->release_date);
    fprintf(file, "  \"tag_name\": \"%s\",\n", update_info->tag_name);
    fprintf(file, "  \"download_url\": \"%s\",\n", update_info->download_url);
    fprintf(file, "  \"is_newer\": %s\n", update_info->is_newer ? "true" : "false");
    fprintf(file, "}\n");
    
    fclose(file);
    return 0;
}

static int load_update_cache(UpdateInfo *update_info) {
    char cache_path[MAX_PATH_SIZE];
    const char *home_dir = getenv(HOME_ENV);
    
    if (!home_dir) return -1;
    
    snprintf(cache_path, sizeof(cache_path), "%s%s%s%s%s", 
             home_dir, PATH_SEP, CONFIG_DIR, PATH_SEP, UPDATE_CACHE_FILE);
    
    FILE *file = fopen(cache_path, "r");
    if (!file) return -1;
    
    char buffer[2048];
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    fclose(file);
    buffer[bytes_read] = '\0';
    
    json_object *root = json_tokener_parse(buffer);
    if (!root) return -1;
    
    // Check if cache is still valid
    json_object *timestamp_obj;
    if (json_object_object_get_ex(root, "timestamp", &timestamp_obj)) {
        time_t cached_time = json_object_get_int64(timestamp_obj);
        time_t now = time(NULL);
        
        // If cache is older than UPDATE_CACHE_EXPIRE_HOURS, invalidate it
        if (now - cached_time > UPDATE_CACHE_EXPIRE_HOURS * 3600) {
            json_object_put(root);
            return -1;
        }
    } else {
        json_object_put(root);
        return -1;
    }
    
    // Load cached data
    json_object *version_obj, *release_date_obj, *tag_name_obj, *download_url_obj, *is_newer_obj;
    
    if (json_object_object_get_ex(root, "version", &version_obj)) {
        strncpy(update_info->version, json_object_get_string(version_obj), 
                sizeof(update_info->version) - 1);
    }
    
    if (json_object_object_get_ex(root, "release_date", &release_date_obj)) {
        strncpy(update_info->release_date, json_object_get_string(release_date_obj), 
                sizeof(update_info->release_date) - 1);
    }
    
    if (json_object_object_get_ex(root, "tag_name", &tag_name_obj)) {
        strncpy(update_info->tag_name, json_object_get_string(tag_name_obj), 
                sizeof(update_info->tag_name) - 1);
    }
    
    if (json_object_object_get_ex(root, "download_url", &download_url_obj)) {
        strncpy(update_info->download_url, json_object_get_string(download_url_obj), 
                sizeof(update_info->download_url) - 1);
    }
    
    if (json_object_object_get_ex(root, "is_newer", &is_newer_obj)) {
        update_info->is_newer = json_object_get_boolean(is_newer_obj);
    }
    
    json_object_put(root);
    return 0;
}

void print_version(void) {
    print_colored("CDrive", COLOR_BOLD);
    printf(" version ");
    print_colored(CDRIVE_VERSION " (" CDRIVE_RELEASE_DATE ")", COLOR_GREEN);
    printf("\n");
    print_colored(GITHUB_RELEASES_URL "\n", COLOR_BLUE);
    printf("A professional Google Drive command-line interface\n");
    printf("Built with C Programming Language Using Love\n");
    printf("Made By: ");
    print_colored("Batuhantrkgl\n", COLOR_CYAN);
}

void print_version_with_update_check(void) {
    // Print basic version info
    print_version();
    
    printf("\n");
    
    UpdateInfo update_info = {0};
    
    // Try to load from cache first
    int cache_result = load_update_cache(&update_info);
    
    if (cache_result == 0) {
        // We have valid cached data
        print_colored("[*] ", COLOR_BLUE);
        printf("Using cached update information...\n");
        
        if (update_info.is_newer) {
            printf("\n");
            print_colored("[+] ", COLOR_GREEN);
            print_colored("Update Available! ", COLOR_BOLD);
            printf("Version %s", update_info.version);
            if (strlen(update_info.release_date) > 0) {
                printf(" (%s)", update_info.release_date);
            }
            printf("\n");
            
            print_colored("[*] ", COLOR_CYAN);
            printf("Run ");
            print_colored("cdrive update --auto", COLOR_YELLOW);
            printf(" to install pre-compiled binary\n");
            
            print_colored("[*] ", COLOR_CYAN);
            printf("Or run ");
            print_colored("cdrive update --compile", COLOR_YELLOW);
            printf(" to automatically compile it on your machine\n");
            
            print_colored("[*] ", COLOR_CYAN);
            printf("View releases: ");
            print_colored(GITHUB_RELEASES_URL "\n", COLOR_BLUE);
        } else {
            printf("\n");
            print_colored("[+] ", COLOR_GREEN);
            printf("You're running the latest version!\n");
        }
        
        print_colored("[*] ", COLOR_BLUE);
        printf("Use ");
        print_colored("cdrive update --check", COLOR_YELLOW);
        printf(" to force refresh\n");
        return;
    }
    
    // Cache miss or expired, check online
    LoadingSpinner spinner = {0};
    start_spinner(&spinner, "Checking for updates...");
    
    int update_result = check_for_updates(&update_info);
    
    stop_spinner(&spinner);
    
    if (update_result == 0) {
        // Save successful result to cache
        save_update_cache(&update_info);
        
        if (update_info.is_newer) {
            printf("\n");
            print_colored("[+] ", COLOR_GREEN);
            print_colored("Update Available! ", COLOR_BOLD);
            printf("Version %s", update_info.version);
            if (strlen(update_info.release_date) > 0) {
                printf(" (%s)", update_info.release_date);
            }
            printf("\n");
            
            print_colored("[*] ", COLOR_CYAN);
            printf("Run ");
            print_colored("cdrive update --auto", COLOR_YELLOW);
            printf(" to install pre-compiled binary\n");
            
            print_colored("[*] ", COLOR_CYAN);
            printf("Or run ");
            print_colored("cdrive update --compile", COLOR_YELLOW);
            printf(" to automatically compile it on your machine\n");
            
            print_colored("[*] ", COLOR_CYAN);
            printf("View releases: ");
            print_colored(GITHUB_RELEASES_URL "\n", COLOR_BLUE);
        } else {
            printf("\n");
            print_colored("[+] ", COLOR_GREEN);
            printf("You're running the latest version!\n");
        }
    } else if (update_result == -2) {
        printf("\n");
        print_colored("[!] ", COLOR_YELLOW);
        printf("GitHub API rate limit exceeded. Try again later.\n");
    } else if (update_result == -3) {
        printf("\n");
        print_colored("[!] ", COLOR_RED);
        printf("Repository not found or releases not available.\n");
        print_colored("[*] ", COLOR_CYAN);
        printf("Check: ");
        print_colored(GITHUB_RELEASES_URL "\n", COLOR_BLUE);
    } else {
        printf("\n");
        print_colored("[!] ", COLOR_YELLOW);
        printf("Could not check for updates. Please check your internet connection.\n");
        print_colored("[*] ", COLOR_CYAN);
        printf("Manual check: ");
        print_colored(GITHUB_RELEASES_URL "\n", COLOR_BLUE);
    }
}

int compare_versions(const char *current, const char *latest) {
    // Simple version comparison (assumes semantic versioning x.y.z)
    // Returns: 1 if latest > current, -1 if latest < current, 0 if equal
    
    if (!current || !latest) return 0;
    
    int current_major = 0, current_minor = 0, current_patch = 0;
    int latest_major = 0, latest_minor = 0, latest_patch = 0;
    
    // Parse current version with more flexible parsing
    int current_parsed = sscanf(current, "%d.%d.%d", &current_major, &current_minor, &current_patch);
    if (current_parsed < 1) return 0; // Invalid format
    
    // Parse latest version (remove 'v' prefix if present)
    const char *version_str = latest;
    if (latest[0] == 'v' || latest[0] == 'V') {
        version_str = latest + 1;
    }
    
    int latest_parsed = sscanf(version_str, "%d.%d.%d", &latest_major, &latest_minor, &latest_patch);
    if (latest_parsed < 1) return 0; // Invalid format
    
    // Handle cases where minor or patch versions are missing
    if (current_parsed < 2) current_minor = 0;
    if (current_parsed < 3) current_patch = 0;
    if (latest_parsed < 2) latest_minor = 0;
    if (latest_parsed < 3) latest_patch = 0;
    
    // Compare versions
    if (latest_major > current_major) return 1;
    if (latest_major < current_major) return -1;
    
    if (latest_minor > current_minor) return 1;
    if (latest_minor < current_minor) return -1;
    
    if (latest_patch > current_patch) return 1;
    if (latest_patch < current_patch) return -1;
    
    return 0; // Equal
}

int check_for_updates(UpdateInfo *update_info) {
    CURL *curl;
    CURLcode res;
    APIResponse response = {0};
    
    curl = curl_easy_init();
    if (!curl) {
        return -1;
    }
    
    // Set up headers for GitHub API
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: cdrive/1.0.1 (+https://github.com/batuhantrkgl/CDrive)");
    headers = curl_slist_append(headers, "Accept: application/vnd.github.v3+json");
    headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
    
    // Configure curl with better options
    curl_easy_setopt(curl, CURLOPT_URL, GITHUB_REPO_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L); // 15 second timeout
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); // 10 second connect timeout
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    // Perform request
    res = curl_easy_perform(curl);
    
    // Check HTTP status
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    // Clean up curl
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    // Handle different error cases
    if (res != CURLE_OK) {
        if (response.data) free(response.data);
        if (res == CURLE_OPERATION_TIMEDOUT) {
            return -1; // Timeout
        }
        return -1; // Other curl error
    }
    
    // Handle HTTP error codes
    if (http_code == 403) {
        if (response.data) free(response.data);
        return -2; // Rate limited
    } else if (http_code == 404) {
        if (response.data) free(response.data);
        return -3; // Repository not found
    } else if (http_code != 200) {
        if (response.data) free(response.data);
        return -1; // Other HTTP error
    }
    
    if (!response.data || response.size == 0) {
        if (response.data) free(response.data);
        return -1;
    }
    
    // Parse JSON response
    json_object *root = json_tokener_parse(response.data);
    if (!root) {
        free(response.data);
        return -1;
    }
    
    // Check if response has error message
    json_object *message_obj;
    if (json_object_object_get_ex(root, "message", &message_obj)) {
        const char *message = json_object_get_string(message_obj);
        if (strstr(message, "rate limit") || strstr(message, "Rate limit")) {
            json_object_put(root);
            free(response.data);
            return -2;
        }
    }
    
    // Extract release information
    json_object *tag_name_obj, *published_at_obj, *assets_obj, *prerelease_obj;
    
    // Check if this is a prerelease (skip prereleases)
    if (json_object_object_get_ex(root, "prerelease", &prerelease_obj)) {
        if (json_object_get_boolean(prerelease_obj)) {
            json_object_put(root);
            free(response.data);
            return -3; // Skip prereleases
        }
    }
    
    if (json_object_object_get_ex(root, "tag_name", &tag_name_obj)) {
        const char *tag_name = json_object_get_string(tag_name_obj);
        if (tag_name && strlen(tag_name) > 0) {
            strncpy(update_info->tag_name, tag_name, sizeof(update_info->tag_name) - 1);
            update_info->tag_name[sizeof(update_info->tag_name) - 1] = '\0';
            
            // Extract version (remove 'v' prefix if present)
            const char *version_str = tag_name;
            if (tag_name[0] == 'v' || tag_name[0] == 'V') {
                version_str = tag_name + 1;
            }
            strncpy(update_info->version, version_str, sizeof(update_info->version) - 1);
            update_info->version[sizeof(update_info->version) - 1] = '\0';
        }
    }
    
    if (json_object_object_get_ex(root, "published_at", &published_at_obj)) {
        const char *published_at = json_object_get_string(published_at_obj);
        // Extract date from ISO 8601 format (YYYY-MM-DDTHH:MM:SSZ)
        if (published_at && strlen(published_at) >= 10) {
            snprintf(update_info->release_date, sizeof(update_info->release_date), 
                     "%.4s-%.2s-%.2s", published_at, published_at + 5, published_at + 8);
        }
    }
    
    // Look for download assets with better platform detection
    if (json_object_object_get_ex(root, "assets", &assets_obj)) {
        int assets_len = json_object_array_length(assets_obj);
        const char *platform_keyword = NULL;
        const char *arch_keyword = NULL;
        
        // Determine platform and architecture
#ifdef _WIN32
        platform_keyword = "windows";
        #ifdef _WIN64
            arch_keyword = "x86_64";
        #else
            arch_keyword = "i386";
        #endif
#elif defined(__APPLE__)
        platform_keyword = "darwin";
        #ifdef __aarch64__
            arch_keyword = "arm64";
        #else
            arch_keyword = "x86_64";
        #endif
#else
        platform_keyword = "linux";
        #ifdef __aarch64__
            arch_keyword = "arm64";
        #elif defined(__arm__)
            arch_keyword = "armv7";
        #elif defined(__i386__)
            arch_keyword = "i386";
        #else
            arch_keyword = "x86_64";
        #endif
#endif
        
        for (int i = 0; i < assets_len; i++) {
            json_object *asset = json_object_array_get_idx(assets_obj, i);
            json_object *name_obj, *download_url_obj;
            
            if (json_object_object_get_ex(asset, "name", &name_obj) &&
                json_object_object_get_ex(asset, "browser_download_url", &download_url_obj)) {
                
                const char *asset_name = json_object_get_string(name_obj);
                
                // Look for appropriate binary for current platform
                if (platform_keyword && strstr(asset_name, platform_keyword)) {
                    // Check architecture if specified
                    if (arch_keyword && strstr(asset_name, arch_keyword)) {
                        const char *download_url = json_object_get_string(download_url_obj);
                        strncpy(update_info->download_url, download_url, sizeof(update_info->download_url) - 1);
                        update_info->download_url[sizeof(update_info->download_url) - 1] = '\0';
                        break;
                    } else if (!arch_keyword) {
                        // If no specific arch requirement, take first platform match
                        const char *download_url = json_object_get_string(download_url_obj);
                        strncpy(update_info->download_url, download_url, sizeof(update_info->download_url) - 1);
                        update_info->download_url[sizeof(update_info->download_url) - 1] = '\0';
                        break;
                    }
                }
            }
        }
    }
    
    // Compare versions only if we have valid version strings
    if (strlen(update_info->version) > 0) {
        update_info->is_newer = (compare_versions(CDRIVE_VERSION, update_info->version) > 0);
    } else {
        json_object_put(root);
        free(response.data);
        return -1; // No valid version found
    }
    
    json_object_put(root);
    free(response.data);
    
    return 0;
}

// Progress callback for download
struct DownloadProgress {
    LoadingSpinner *spinner;
    size_t total_size;
    size_t downloaded;
    time_t last_update;
};

static int download_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                     curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal;
    (void)ulnow;
    
    struct DownloadProgress *progress = (struct DownloadProgress *)clientp;
    time_t now = time(NULL);
    
    // Update every second
    if (now > progress->last_update) {
        progress->last_update = now;
        
        if (dltotal > 0) {
            double percent = (double)dlnow / (double)dltotal * 100.0;
            char progress_msg[256];
            
            if (dltotal > 1024 * 1024) {
                snprintf(progress_msg, sizeof(progress_msg), 
                         "Downloading... %.1f%% (%.1f MB / %.1f MB)",
                         percent, (double)dlnow / (1024*1024), (double)dltotal / (1024*1024));
            } else if (dltotal > 1024) {
                snprintf(progress_msg, sizeof(progress_msg), 
                         "Downloading... %.1f%% (%.1f KB / %.1f KB)",
                         percent, (double)dlnow / 1024, (double)dltotal / 1024);
            } else {
                snprintf(progress_msg, sizeof(progress_msg), 
                         "Downloading... %.1f%% (%ld B / %ld B)",
                         percent, (long)dlnow, (long)dltotal);
            }
            
            // Update spinner message (we'd need to modify spinner for this to work)
            // For now, just continue with existing spinner
        }
    }
    
    return 0;
}

int download_and_install_update(const UpdateInfo *update_info, int auto_install) {
    if (!update_info->download_url[0]) {
        print_error("No download URL available for your platform");
        print_colored("[*] ", COLOR_CYAN);
        printf("Available platforms at: ");
        print_colored(GITHUB_RELEASES_URL "\n", COLOR_BLUE);
        return -1;
    }
    
    printf("\n");
    print_colored("[*] ", COLOR_YELLOW);
    printf("Downloading cdrive %s...\n", update_info->version);
    
    // Create temporary file with better naming
    char temp_file[512];
    char temp_dir[256];
    
#ifdef _WIN32
    const char *env_temp = getenv("TEMP");
    if (!env_temp) env_temp = getenv("TMP");
    if (!env_temp) env_temp = "C:\\Windows\\Temp";
    strncpy(temp_dir, env_temp, sizeof(temp_dir) - 1);
    snprintf(temp_file, sizeof(temp_file), "%s\\cdrive_%s.exe", temp_dir, update_info->version);
#else
    const char *env_temp = getenv("TMPDIR");
    if (!env_temp) env_temp = "/tmp";
    strncpy(temp_dir, env_temp, sizeof(temp_dir) - 1);
    snprintf(temp_file, sizeof(temp_file), "%s/cdrive_%s", temp_dir, update_info->version);
#endif
    
    // Download file with progress tracking
    CURL *curl = curl_easy_init();
    if (!curl) {
        print_error("Failed to initialize download");
        return -1;
    }
    
    FILE *fp = fopen(temp_file, "wb");
    if (!fp) {
        print_error("Failed to create temporary file");
        printf("Target location: %s\n", temp_file);
        curl_easy_cleanup(curl);
        return -1;
    }
    
    // Set up progress tracking
    struct DownloadProgress progress = {0};
    progress.last_update = time(NULL);
    
    // Start download spinner
    LoadingSpinner spinner = {0};
    start_spinner(&spinner, "Downloading...");
    progress.spinner = &spinner;
    
    // Configure curl for download
    curl_easy_setopt(curl, CURLOPT_URL, update_info->download_url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 600L); // 10 minute timeout for downloads
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, download_progress_callback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progress);
    
    // Add user agent and other headers
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "User-Agent: cdrive/1.0.1 (+https://github.com/batuhantrkgl/CDrive)");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    
    // Check HTTP status
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    stop_spinner(&spinner);
    fclose(fp);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res != CURLE_OK || http_code != 200) {
        print_error("Download failed");
        if (res == CURLE_OPERATION_TIMEDOUT) {
            printf("Download timed out. Please try again with a better connection.\n");
        } else if (http_code == 404) {
            printf("Download URL not found. The release might have been moved.\n");
        } else if (http_code != 200) {
            printf("HTTP error: %ld\n", http_code);
        } else {
            printf("Network error: %s\n", curl_easy_strerror(res));
        }
        unlink(temp_file);
        return -1;
    }
    
    // Verify file size
    struct stat file_stat;
    if (stat(temp_file, &file_stat) == 0) {
        if (file_stat.st_size < 1024) { // Less than 1KB is suspicious
            print_error("Downloaded file is too small. Download may have failed.");
            unlink(temp_file);
            return -1;
        }
        
        print_success("Download completed!");
        if (file_stat.st_size > 1024 * 1024) {
            printf("Downloaded %.1f MB\n", (double)file_stat.st_size / (1024*1024));
        } else {
            printf("Downloaded %.1f KB\n", (double)file_stat.st_size / 1024);
        }
    } else {
        print_error("Could not verify downloaded file");
        return -1;
    }
    
    if (auto_install) {
        printf("\n");
        print_colored("[*] ", COLOR_YELLOW);
        printf("Installing update...\n");
        
        // Get current executable path
        char current_exe[512];
        int found_exe = 0;
        
#ifdef _WIN32
        if (GetModuleFileName(NULL, current_exe, sizeof(current_exe)) > 0) {
            found_exe = 1;
        }
#else
        ssize_t len = readlink("/proc/self/exe", current_exe, sizeof(current_exe) - 1);
        if (len != -1) {
            current_exe[len] = '\0';
            found_exe = 1;
        }
#endif
        
        if (!found_exe) {
            print_warning("Could not determine current executable path");
            print_colored("[*] ", COLOR_CYAN);
            printf("Downloaded binary: %s\n", temp_file);
            printf("Please install manually by replacing your cdrive binary.\n");
            return 0;
        }
        
        // Check if we have write permission
        if (access(current_exe, W_OK) != 0 && geteuid() != 0) {
            print_warning("Permission denied for automatic installation");
            print_colored("[*] ", COLOR_CYAN);
            printf("Downloaded binary: %s\n", temp_file);
            print_colored("[*] ", COLOR_CYAN);
            printf("To install manually:\n");
#ifdef _WIN32
            printf("  move \"%s\" \"%s\"\n", temp_file, current_exe);
#else
            printf("  sudo mv \"%s\" \"%s\"\n", temp_file, current_exe);
            printf("  sudo chmod +x \"%s\"\n", current_exe);
#endif
            return 0;
        }
        
        // Create backup
        char backup_file[600];
        snprintf(backup_file, sizeof(backup_file), "%s.backup.%ld", current_exe, time(NULL));
        
        // Perform installation
        int install_success = 0;
        
#ifdef _WIN32
        // Windows installation
        if (CopyFile(current_exe, backup_file, FALSE) && 
            CopyFile(temp_file, current_exe, FALSE)) {
            install_success = 1;
        }
#else
        // Unix/Linux installation
        if (rename(current_exe, backup_file) == 0 && 
            rename(temp_file, current_exe) == 0 && 
            chmod(current_exe, 0755) == 0) {
            install_success = 1;
        }
#endif
        
        if (install_success) {
            print_success("Update installed successfully!");
            print_colored("[+] ", COLOR_GREEN);
            printf("cdrive %s is now ready to use\n", update_info->version);
            print_colored("[*] ", COLOR_CYAN);
            printf("Previous version backed up as: %s\n", backup_file);
            
            // Clean up temp file
            unlink(temp_file);
        } else {
            print_error("Installation failed");
            print_colored("[*] ", COLOR_CYAN);
            printf("Downloaded binary: %s\n", temp_file);
            printf("Manual installation required.\n");
        }
    } else {
        print_colored("[*] ", COLOR_CYAN);
        printf("Downloaded to: %s\n", temp_file);
        print_colored("[*] ", COLOR_CYAN);
        printf("To install manually, replace your current cdrive binary with this file.\n");
    }
    
    return 0;
}
