run_individual_asignment_hardcoded: individual_assignment
	iotlab-node --update bin/individual_assignment.elf
	sleep 1
	echo '' | nc 'm3-$(NODE_ID).grenoble.iot-lab.info' 20000

run_individual_asignment_python_stdout: individual_assignment
	iotlab-node --update bin/individual_assignment.elf
	sleep 1
	python3 send_stdout.py | nc 'm3-$(NODE_ID).grenoble.iot-lab.info' 20000

run_individual_asignment_python_tcp: individual_assignment
	iotlab-node --update bin/individual_assignment.elf
	sleep 1
	python3 send_tcp.py

run_mqtt_listener:
	mosquitto_sub --cafile /opt/iot-lab-ca.pem -h mqtt4.iot-lab.info -p 8883 -t iotlab/agheorgh/individual_assignment_aggregation

plot_consumption:
	plot_oml_consum -p -i ~/.iot-lab/last/consumption/m3_$(NODE_ID).oml
