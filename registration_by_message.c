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

// Store SP function
void store_sp(sp_entry_t **head, const char *client_id, const char *topic, const char *sp_filter) {
    sp_entry_t *new_entry = malloc(sizeof(sp_entry_t));
    new_entry->client_id = strdup(client_id);
    new_entry->topic = strdup(topic);
    new_entry->sp_filter = strdup(sp_filter);

    // Expand the SP filter
    new_entry->sp_purposes = expand_purpose_filter(sp_filter, &(new_entry->sp_purpose_count));

    new_entry->next = *head;
    *head = new_entry;
}

// Find SP entry function
sp_entry_t* find_sp_entry(sp_entry_t *head, const char *client_id, const char *topic) {
    sp_entry_t *current = head;
    while (current) {
        if (strcmp(current->client_id, client_id) == 0 && strcmp(current->topic, topic) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Free SP list
void free_sp_list(sp_entry_t **head) {
    sp_entry_t *current = *head;
    while(current) {
        sp_entry_t *next = current->next;
        free(current->client_id);
        free(current->topic);
        free(current->sp_filter);
        free_purpose_list(current->sp_purposes, current->sp_purpose_count);
        free(current);
        current = next;
    }
    *head = NULL;
}

// Store MP function
void store_mp(mp_entry_t **head, const char *topic, const char *mp_filter) {
    mp_entry_t *new_entry = malloc(sizeof(mp_entry_t));
    new_entry->topic = strdup(topic);
    new_entry->mp_filter = strdup(mp_filter);

    // Expand the MP filter
    new_entry->mp_purposes = expand_purpose_filter(mp_filter, &(new_entry->mp_purpose_count));

    new_entry->next = *head;
    *head = new_entry;
}

// Find MP entry function
mp_entry_t* find_mp_entry(mp_entry_t *head, const char *topic) {
    mp_entry_t *current = head;
    while (current) {
        if (strcmp(current->topic, topic) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

// Free MP list
void free_mp_list(mp_entry_t **head) {
    mp_entry_t *current = *head;
    while(current) {
        mp_entry_t *next = current->next;
        free(current->topic);
        free(current->mp_filter);
        free_purpose_list(current->mp_purposes, current->mp_purpose_count);
        free(current);
        current = next;
    }
    *head = NULL;
}

// Callback for SUBSCRIBE events
int callback_subscribe(int event, void *event_data, void *userdata) {
    struct mosquitto_evt_subscribe *ed = event_data;
    const struct mosquitto_property *prop = ed->properties;

    const char *client_id = mosquitto_client_id(ed->client);

    const char *sp_filter = NULL;
    while(prop) {
        if(mosquitto_property_identifier(prop) == MQTT_PROP_USER_PROPERTY) {
            const char *key = NULL, *value = NULL;
            mosquitto_property_read_string_pair(prop, &key, &value, false);
            if(strncmp(key, "SP:", 3) == 0) {
                sp_filter = value;
                const char *topic = key + 3; // Extract topic from "SP:<topic>"
                pthread_mutex_lock(&pbac_mutex);
                store_sp(&sp_list, client_id, topic, sp_filter);
                pthread_mutex_unlock(&pbac_mutex);
            }
        }
        prop = mosquitto_property_next(prop);
    }

    return MOSQ_ERR_SUCCESS;
}

// Callback for PUBLISH events
int callback_publish(int event, void *event_data, void *userdata) {
    struct mosquitto_evt_publish *ed = event_data;
    const char *topic = ed->topic;

    if(strcmp(topic, "$priv/purpose_management") == 0) {
        // Handle MP registration
        const struct mosquitto_property *prop = ed->properties;
        const char *mp_filter = NULL;
        const char *mp_topic = NULL;

        while(prop) {
            if(mosquitto_property_identifier(prop) == MQTT_PROP_USER_PROPERTY) {
                const char *key = NULL, *value = NULL;
                mosquitto_property_read_string_pair(prop, &key, &value, false);
                if(strncmp(key, "MP:", 3) == 0) {
                    mp_topic = key + 3; // Skip "MP:"
                    mp_filter = value;
                    break;
                }
            }
            prop = mosquitto_property_next(prop);
        }

        if(mp_topic && mp_filter) {
            pthread_mutex_lock(&pbac_mutex);
            store_mp(&mp_list, mp_topic, mp_filter);
            pthread_mutex_unlock(&pbac_mutex);
        }

        return MOSQ_ERR_SUCCESS;
    }

    // For regular messages, retrieve stored MP
    pthread_mutex_lock(&pbac_mutex);
    mp_entry_t *mp_entry = find_mp_entry(mp_list, topic);
    pthread_mutex_unlock(&pbac_mutex);

    if(!mp_entry) {
        // No MP registered for this topic
        // Handle according to your policy
        return MOSQ_ERR_SUCCESS;
    }

    check_purpose_compatibility_registration_by_message(ed, mp_entry);

    return MOSQ_ERR_SUCCESS;
}

// Function to check purpose compatibility and handle mismatch
void check_purpose_compatibility_registration_by_message(struct mosquitto_evt_publish *ed, mp_entry_t *mp_entry) {
    pthread_mutex_lock(&pbac_mutex);

    struct mosquitto_client *client = NULL;
    mosquitto_broker_subscribers_for_topic(ed->topic, &client);
    while(client) {
        const char *client_id = mosquitto_client_id(client);

        // Find SP entry
        sp_entry_t *sp_entry = find_sp_entry(sp_list, client_id, ed->topic);

        bool compatible = false;
        if (sp_entry && sp_entry->sp_purposes && mp_entry->mp_purposes) {
            // Check if any MP purpose matches any SP purpose
            compatible = purposes_match(mp_entry->mp_purposes, mp_entry->mp_purpose_count, sp_entry->sp_purposes, sp_entry->sp_purpose_count);
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

        client = client->next;
    }

    pthread_mutex_unlock(&pbac_mutex);
}

// Plugin initialization
int mosquitto_plugin_init(mosquitto_plugin_id_t *identifier, void **userdata, struct mosquitto_opt *options, int option_count) {
    mosquitto_callback_register(identifier, MOSQ_EVT_SUBSCRIBE, callback_subscribe, NULL, NULL);
    mosquitto_callback_register(identifier, MOSQ_EVT_PUBLISH, callback_publish, NULL, NULL);
    return MOSQ_ERR_SUCCESS;
}

// Plugin cleanup
int mosquitto_plugin_cleanup(void *userdata, struct mosquitto_opt *options, int option_count) {
    pthread_mutex_lock(&pbac_mutex);
    free_sp_list(&sp_list);
    free_mp_list(&mp_list);
    pthread_mutex_unlock(&pbac_mutex);
    pthread_mutex_destroy(&pbac_mutex);
    return MOSQ_ERR_SUCCESS;
}
