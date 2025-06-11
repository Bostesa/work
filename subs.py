import paho.mqtt.client as mqtt

# Broker configuration
BROKER_ADDRESS = 'visionpc01.cs.umbc.edu'  # Change if needed
BROKER_PORT = 1883

# Topic
TOPIC = 'data/location'

# Subscription Purpose (PF-SP)
PF_SP = 'ads/{third-party,targeted}'

def subscriber():
    def on_connect(client, userdata, flags, rc, properties=None):
        print(f"[Subscriber] Connected with result code {rc}")

        # Prepare properties for the SUBSCRIBE message
        properties = mqtt.Properties(mqtt.PacketTypes.SUBSCRIBE)
        properties.UserProperty = [("PF-SP", PF_SP)]

        # Subscribe to the topic with properties
        client.subscribe(TOPIC, qos=1, options=None, properties=properties)
        print(f"[Subscriber] Subscribed to topic '{TOPIC}' with PF-SP '{PF_SP}'")

    def on_message(client, userdata, message):
        print("\n[Subscriber] Received message:")
        print(f"Topic: {message.topic}")
        print(f"Payload: {message.payload.decode()}")
        if message.properties and message.properties.UserProperty:
            print("User Properties:")
            for prop in message.properties.UserProperty:
                print(f" - {prop[0]}: {prop[1]}")
        else:
            print("No User Properties.")

    client = mqtt.Client(client_id='subscriber', protocol=mqtt.MQTTv5)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER_ADDRESS, BROKER_PORT)
    client.loop_forever()

if __name__ == "__main__":
    subscriber()
