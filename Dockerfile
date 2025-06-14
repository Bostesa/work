FROM eclipse-mosquitto:latest

# Install necessary packages including SQLite
RUN apk update && \
    apk add --no-cache \
        build-base \
        openssl-dev \
        mosquitto-dev \
        sqlite-dev


# Copy Mosquitto headers for plugin development
ADD mosquitto_headers/mosquitto_plugin.h /usr/include/
ADD mosquitto_headers/mosquitto_broker.h /usr/include/

# Debug step: List contents of the headers directory
RUN ls -l /usr/include/

# Set up build directory for plugins
RUN mkdir -p /build/intent_plugin

# Copy plugin source files and Makefile for intent management plugin
ADD mosquitto_plugin  /build/intent_plugin

# Set working directory for intent management plugin and build
WORKDIR /build/intent_plugin
RUN make

# Create plugins directory in the final image
RUN mkdir -p /mosquitto/plugins

# Copy the built intent management plugin to the plugins directory
RUN cp /build/intent_plugin/per_message_declaration.so /mosquitto/plugins/
RUN cp /build/intent_plugin/registration_by_message.so /mosquitto/plugins/
RUN cp /build/intent_plugin/registaration_by_topic.so /mosquitto/plugins/

# Create a directory for SQLite database files
RUN mkdir -p /opt/mosquitto/plugins/db

# Copy the configuration file for Mosquitto
ADD mosquitto.conf /mosquitto/config/mosquitto.conf

# Expose default MQTT ports
EXPOSE 1883

# Start Mosquitto broker with the custom configuration
CMD ["/usr/sbin/mosquitto", "-c", "/mosquitto/config/mosquitto.conf"]

