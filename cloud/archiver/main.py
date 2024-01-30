import logging
logging.basicConfig(format="%(asctime)s\t%(levelname)s\t%(message)s", level=logging.INFO, datefmt='%Y-%m-%d %H:%M:%S')

from paho.mqtt.client import Client, MQTTMessage

from influxdb_client import InfluxDBClient, Point, WriteApi


def main():  
  logging.info("Archiver started")

  mqtt = Client()
  mqtt.on_connect = mqtt_connected
  mqtt.on_message = handle_message

  influx = InfluxDBClient(url="http://database:8086", org="ws-org", token="ws-super-secret-auth-token")
  db = influx.write_api()
  mqtt.user_data_set(db)

  mqtt.connect(host="broker", port=1883)
  mqtt.loop_forever()


def mqtt_connected(client: Client, userdata, flags, rc):
  """ 
  This function is executed when the client tries to connect to MQTT broker.
  """

  if rc > 0:
    logging.error(f"Connection refused, rc={rc}")
    client.disconnect()
  else:
    logging.info("Connected to broker")
    client.subscribe("ws/+/temp")
    client.subscribe("ws/+/hum")


def handle_message(client: Client, db: WriteApi, message: MQTTMessage):
  """ 
  This function is executed when new message is received on MQTT topic.
  - db is passed as userdata from main function
  """
  
  logging.info(f"New message on {message.topic}")
  try:

    _, client_id, sensor = message.topic.split("/")
    value = float(message.payload)
    logging.info(f"Decoded message: {client_id=}, {sensor=}, {value=}")

    point = Point("sensors")
    point.field(sensor, value)
    point.tag("client_id", client_id)
    db.write(bucket="ws-bucket", record=point)    

  except Exception as ex:
    # NB! for demonstration purposes only
    logging.error(f"Failed to handle the message: {ex}")


if __name__ == "__main__":
  main()