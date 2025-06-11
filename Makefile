# Makefile for compiling the Mosquitto plugin

CC = gcc
CFLAGS = -fPIC -I/usr/include
LDFLAGS = -shared -lpthread -lmosquitto -lsqlite3
TARGET = intent_plugin.so
SOURCES = registration_by_message.c registration_by_topic.c per_message_declara>
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
        $(CC) -o $@ $(OBJECTS) $(LDFLAGS)

%.o: %.c
        $(CC) $(CFLAGS) -c $< -o $@

clean:
        rm -f $(OBJECTS) $(TARGET)









