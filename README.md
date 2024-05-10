# Individual Assignment, IoT 2024
Adrian Gheorghiu - 2162607
## Implementation details
Initially, I implemented the task on IoT-LAB, because it offers
power draw measurement tools

Both sending the data real-time from a python script through UART and
hardcoding the signal were implemented for IoT-LAB
- First method (real-time UART/TCP) observations:
    - We have to decide on a sending frequency.
    - We notice that we have to introduce a delay between flashing the board
      and starting to send the signal otherwise the board misses the initial bytes sent.
    - At first, I tried to do print it to STDOUT and pipe it through netcat(nc),
      but the results where really bad and the sampled signal looks very blocky. It had
      nothing to do with the network delay inconsistency, but rather the fact that
      netcat uses Nagle's alglorithm which introduces delay into the TCP sends
      and waits for data to accumulate to send bigger packets at once. To fix this,
      the connection had to be made through sockets directly with the TCP option
      TCP_NODELAY set to true.
- Second method (hardcoded signal) observations:
    - Even with the hardcoded signal, FFT result is not perfect due to spectral
      leakage (from sampling incomplete period). In fact, the results are very similar
      to that of the first method.

Problem with IoT-LAB is that there is no MQTT library and also the nodes seem
isolated with the only method of communication being through the TCP broker that sends
the message through UART. Hence why, I also implemented it for the physical device,
where I have MQTT but no power draw measurement tool. To keep it simple, for the
physical device I only implemented the hardcoded version.

Physical device implementation observations:

- By default, FreeRTOS tickrate is set to 100Hz. I increased it to 1kHz to be able
to achieve 1kHz sample rate. I could in theory use timers but for the sake of keeping
the code simple, I decided not to.
- For the aggregate, an optimised rolling window average of the signal is calculated
- For MQTT, I use a secure Mosquitto MQTTS server running on my laptop, with RSA keys and
  certificates for the CA, server, and client generated using OpenSSL locally. I don't
  use any authentication method, so clients are anonymous.