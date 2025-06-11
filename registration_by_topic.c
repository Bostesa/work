// registration_by_topic.c

#include "mqtt_pbac.h"
#include <sqlite3.h>

pthread_mutex_t pbac_mutex = PTHREAD_MUTEX_INITIALIZER;
sqlite3 *db = NULL;

#include "mqtt_pbac.h"

pthread_mutex_t pbac_mutex = PTHREAD_MUTEX_INITIALIZER;
sp_entry_t *sp_list = NULL; // Head of the SP linked list

// Function to expand purpose filters
char** expand_purpose_filter(const char *filter, int *count) {
    // This function parses the purpose filter and expands it into all described purposes.
    char **purposes = NULL;
    *count = 0;

    // Replace braces with parentheses for easier processing
    char *filter_copy = strdup(filter);
    for (char *p = filter_copy; *p; p++) {
        if (*p == '{') *p = '(';
        if (*p == '}') *p = ')';
    }

    // Function to recursively expand purposes
    void expand(const char *str, char **results, int *res_count) {
        char *left_brace = strchr(str, '(');
        if (!left_brace) {
            // No more braces, add the string to results
            results[*res_count] = strdup(str);
            (*res_count)++;
            return;
        }
        char *right_brace = strchr(left_brace, ')');
        if (!right_brace) {
            // Mismatched braces
            return;
        }
        // Get the options within the braces
        int prefix_len = left_brace - str;
        char prefix[256];
        strncpy(prefix, str, prefix_len);
        prefix[prefix_len] = '\0';

        int suffix_len = strlen(right_brace + 1);
        char suffix[256];
        strncpy(suffix, right_brace + 1, suffix_len);
        suffix[suffix_len] = '\0';

        char options[256];
        int options_len = right_brace - left_brace - 1;
        strncpy(options, left_brace + 1, options_len);
        options[options_len] = '\0';

        // Split options by comma
        char *options_copy = strdup(options);
        char *option = strtok(options_copy, ",");
        while (option) {
            char new_str[512];
            snprintf(new_str, sizeof(new_str), "%s%s%s", prefix, option, suffix);
            expand(new_str, results, res_count);
            option = strtok(NULL, ",");
        }
        free(options_copy);
    }

    // Allocate memory for results
    purposes = malloc(100 * sizeof(char *)); // Adjust size as needed
    expand(filter_copy, purposes, count);

    free(filter_copy);
    return purposes;
}

// Function to free purpose list
void free_purpose_list(char **purposes, int count) {
    if (purposes) {
        for (int i = 0; i < count; i++) {
            free(purposes[i]);
        }
        free(purposes);
    }
}

// Function to check if any MP purpose matches any SP purpose
bool purposes_match(char **mp_purposes, int mp_count, char **sp_purposes, int sp_count) {
    for (int i = 0; i < mp_count; i++) {
        for (int j = 0; j < sp_count; j++) {
            if (strcmp(mp_purposes[i], sp_purposes[j]) == 0) {
                return true;
            }
        }
    }
    return false;
}

// Function to serialize purposes into a string
char* serialize_purposes(char **purposes, int count) {
    if (count == 0) return NULL;
    size_t total_length = 0;
    for (int i = 0; i < count; i++) {
        total_length += strlen(purposes[i]) + 1; // +1 for delimiter
    }
    char *result = malloc(total_length);
    result[0] = '\0';
    for (int i = 0; i < count; i++) {
        strcat(result, purposes[i]);
        if (i < count -1) {
            strcat(result, ";"); // Use semicolon as delimiter
        }
    }
    return result;
}

// Function to deserialize purposes from a string
char** deserialize_purposes(const char *str, int *count) {
    if (!str || strlen(str) == 0) {
        *count = 0;
        return NULL;
    }
    // Count the number of purposes
    int num_purposes = 1;
    for (const char *p = str; *p; p++) {
        if (*p == ';') num_purposes++;
    }
    char **purposes = malloc(num_purposes * sizeof(char *));
    char *str_copy = strdup(str);
    char *token = strtok(str_copy, ";");
    int idx = 0;
    while (token) {
        purposes[idx++] = strdup(token);
        token = strtok(NULL, ";");
    }
    free(str_copy);
    *count = num_purposes;
    return purposes;
}

// Initialize SQLite database
int init_database() {
    int rc = sqlite3_open("mqtt_pbac.db", &db);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    char *err_msg = NULL;

    const char *sql = "CREATE TABLE IF NOT EXISTS MPs (Topic TEXT PRIMARY KEY, MP TEXT);"
                      "CREATE TABLE IF NOT EXISTS SPs (ClientID TEXT, Topic TEXT, SP TEXT, PRIMARY KEY(ClientID, Topic));";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if(rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
    return rc;
}

// Store MP in SQLite DB
void store_mp_in_db(const char *topic, const char *mp_filter) {
    // Expand MP filter
    int mp_purpose_count = 0;
    char **mp_purposes = expand_purpose_filter(mp_filter, &mp_purpose_count);

    // Serialize purposes into a single string
    char *mp_purposes_str = serialize_purposes(mp_purposes, mp_purpose_count);

    // Store in DB
    char *err_msg = NULL;
    char sql[1024];
    snprintf(sql, sizeof(sql), "INSERT OR REPLACE INTO MPs (Topic, MP) VALUES ('%s', '%s');", topic, mp_purposes_str);

    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if(rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error in store_mp_in_db: %s\n", err_msg);
        sqlite3_free(err_msg);
    }

    free_purpose_list(mp_purposes, mp_purpose_count);
    free(mp_purposes_str);
}

// Get MP from SQLite DB
char** get_mp_from_db(const char *topic, int *mp_count) {
    sqlite3_stmt *stmt;
    char **mp_purposes = NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT MP FROM MPs WHERE Topic='%s';", topic);

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    rc = sqlite3_step(stmt);
    if(rc == SQLITE_ROW) {
        const char *mp_purposes_str = (const char*)sqlite3_column_text(stmt, 0);
        mp_purposes = deserialize_purposes(mp_purposes_str, mp_count);
    }
    sqlite3_finalize(stmt);
    return mp_purposes;
}

// Store SP in SQLite DB
void store_sp_in_db(const char *client_id, const char *topic, const char *sp_filter) {
    // Expand SP filter
    int sp_purpose_count = 0;
    char **sp_purposes = expand_purpose_filter(sp_filter, &sp_purpose_count);

    // Serialize purposes into a single string
    char *sp_purposes_str = serialize_purposes(sp_purposes, sp_purpose_count);

    // Store in DB
    char *err_msg = NULL;
    char sql[1024];
    snprintf(sql, sizeof(sql), "INSERT OR REPLACE INTO SPs (ClientID, Topic, SP) VALUES ('%s', '%s', '%s');", client_id, topic, sp_purposes_str);

    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if(rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error in store_sp_in_db: %s\n", err_msg);
        sqlite3_free(err_msg);
    }

    free_purpose_list(sp_purposes, sp_purpose_count);
    free(sp_purposes_str);
}

// Get SP from SQLite DB
char** get_sp_from_db(const char *client_id, const char *topic, int *sp_count) {
    sqlite3_stmt *stmt;
    char **sp_purposes = NULL;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT SP FROM SPs WHERE ClientID='%s' AND Topic='%s';", client_id, topic);

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    rc = sqlite3_step(stmt);
    if(rc == SQLITE_ROW) {
        const char *sp_purposes_str = (const char*)sqlite3_column_text(stmt, 0);
        sp_purposes = deserialize_purposes(sp_purposes_str, sp_count);
    }
    sqlite3_finalize(stmt);
    return sp_purposes;
}

// Parse and store MP
void parse_and_store_mp_registration_by_topic(const char *mp_topic_part) {
    char *mp_copy = strdup(mp_topic_part);
    char *open_bracket = strchr(mp_copy, '[');
    char *close_bracket = strchr(mp_copy, ']');
    if(open_bracket && close_bracket) {
        *open_bracket = '\0'; // Null-terminate the topic part
        const char *topic = mp_copy;
        char *mp_filter = open_bracket + 1;
        *close_bracket = '\0';
        // Replace '|' with '/'
        for(char *p = mp_filter; *p; p++) {
            if(*p == '|') *p = '/';
        }
        store_mp_in_db(topic, mp_filter);
    }
    free(mp_copy);
}

// Parse and store SP
void parse_and_store_sp_registration_by_topic(const char *client_id, const char *sp_topic_part) {
    char *sp_copy = strdup(sp_topic_part);
    char *open_bracket = strchr(sp_copy, '[');
    char *close_bracket = strchr(sp_copy, ']');
    if(open_bracket && close_bracket) {
        *open_bracket = '\0'; // Null-terminate the topic part
        const char *topic = sp_copy;
        char *sp_filter = open_bracket + 1;
        *close_bracket = '\0';
        // Replace '|' with '/'
        for(char *p = sp_filter; *p; p++) {
            if(*p == '|') *p = '/';
        }
        store_sp_in_db(client_id, topic, sp_filter);
    }
    free(sp_copy);
}

// Callback for PUBLISH events
int callback_publish(int event, void *event_data, void *userdata) {
    struct mosquitto_evt_publish *ed = event_data;
    const char *topic = ed->topic;

    if(strncmp(topic, "$priv/MP_registration/", 22) == 0) {
        // Handle MP registration
        const char *mp_topic_part = topic + 22; // Skip prefix
        pthread_mutex_lock(&pbac_mutex);
        parse_and_store_mp_registration_by_topic(mp_topic_part);
        pthread_mutex_unlock(&pbac_mutex);
        return MOSQ_ERR_SUCCESS;
    } else if(strncmp(topic, "$priv/SP_registration/", 22) == 0) {
        // Handle SP registration
        const char *sp_topic_part = topic + 22; // Skip prefix
        const char *client_id = mosquitto_client_id(ed->client);
        pthread_mutex_lock(&pbac_mutex);
        parse_and_store_sp_registration_by_topic(client_id, sp_topic_part);
        pthread_mutex_unlock(&pbac_mutex);
        return MOSQ_ERR_SUCCESS;
    }

    // Regular message handling
    pthread_mutex_lock(&pbac_mutex);

    int mp_count = 0;
    char **mp_purposes = get_mp_from_db(topic, &mp_count);

    if(!mp_purposes) {
        // No MP registered for this topic
        pthread_mutex_unlock(&pbac_mutex);
        return MOSQ_ERR_SUCCESS;
    }

    struct mosquitto_client *client = NULL;
    mosquitto_broker_subscribers_for_topic(ed->topic, &client);
    while(client) {
        const char *client_id = mosquitto_client_id(client);

        int sp_count = 0;
        char **sp_purposes = get_sp_from_db(client_id, ed->topic, &sp_count);

        bool compatible = false;
        if (sp_purposes && mp_purposes) {
            compatible = purposes_match(mp_purposes, mp_count, sp_purposes, sp_count);
        }

        if (!compatible) {
            // Send message to client stating that the intent is not allowed
            char *payload = strdup("Intent is not allowed");
            mosquitto_broker_publish(
                client_id,
                "intent/not_allowed",
                strlen(payload),
                payload,
                1, // qos
                false, // retain
                NULL // properties
            );

            // Kick the client
            mosquitto_kick_client_by_clientid(client_id, MOSQ_ERR_PROTOCOL);

            free(payload);
        }

        // Free sp_purposes
        if(sp_purposes) {
            free_purpose_list(sp_purposes, sp_count);
        }

        client = client->next;
    }

    // Free mp_purposes
    free_purpose_list(mp_purposes, mp_count);

    pthread_mutex_unlock(&pbac_mutex);

    return MOSQ_ERR_SUCCESS;
}

// Plugin initialization
int mosquitto_plugin_init(mosquitto_plugin_id_t *identifier, void **userdata, struct mosquitto_opt *options, int option_count) {
    int rc = init_database();
    if(rc != SQLITE_OK) {
        return MOSQ_ERR_UNKNOWN;
    }

    mosquitto_callback_register(identifier, MOSQ_EVT_PUBLISH, callback_publish, NULL, NULL);
    return MOSQ_ERR_SUCCESS;
}

// Plugin cleanup
int mosquitto_plugin_cleanup(void *userdata, struct mosquitto_opt *options, int option_count) {
    pthread_mutex_lock(&pbac_mutex);
    if(db) {
        sqlite3_close(db);
        db = NULL;
    }
    pthread_mutex_unlock(&pbac_mutex);
    pthread_mutex_destroy(&pbac_mutex);
    return MOSQ_ERR_SUCCESS;
}
