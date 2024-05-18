MQTT_SERVER_ADDRESS="192.168.24.26"

.phony: run_individual_assignment clean mosquitto_run_broker mosquitto_sub_topic

individual_assignment: main/main.c
	idf.py build flash

run_individual_assignment: individual_assignment
	python3 send_stoud.py | idf.py monitor

mosquitto_run_broker:
	sudo mkdir -p /run/mosquitto
	sudo chown mosquitto:mosquitto /var/run/mosquitto
	sudo mosquitto -c /etc/mosquitto/mosquitto.conf

mosquitto_sub_topic:
	mosquitto_sub -h $(MQTT_SERVER_ADDRESS) -p 1883 --cafile /etc/mosquitto/ca_certificates/ca.crt --cert certs/client.crt --key certs/client.key -t aggregate -d

clean:
	idf.py fullclean