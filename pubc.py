import time
import paho.mqtt.client as mqtt

# Broker configuration
BROKER_ADDRESS = 'visionpc01.cs.umbc.edu'  # Change if needed
BROKER_PORT = 1883

# Topic
TOPIC = 'data/location'

# Message Purpose Filter (PF-MP)
PF_MP = 'ads/{third-party,targeted}'

def publisher():
    def on_connect(client, userdata, flags, rc, properties=None):
        print(f"[Publisher] Connected with result code {rc}")

        # Start publishing messages in a loop
        while True:
            # Prepare properties for the PUBLISH message
            properties = mqtt.Properties(mqtt.PacketTypes.PUBLISH)
            properties.UserProperty = [("PF-MP", PF_MP)]

            # Publish a message with PF-MP
            payload = f"Location data payload at {time.strftime('%Y-%m-%d %H:%M:%S')}"
            client.publish(TOPIC, payload, qos=1, properties=properties)
            print(f"[Publisher] Published message with PF-MP '{PF_MP}' to topic '{TOPIC}'")

            # Wait for a certain interval before publishing the next message
            time.sleep(5)  # Publish every 5 seconds

    client = mqtt.Client(client_id='publisher', protocol=mqtt.MQTTv5)
    client.on_connect = on_connect

    client.connect(BROKER_ADDRESS, BROKER_PORT)
    client.loop_forever()

if __name__ == "__main__":
    publisher()
