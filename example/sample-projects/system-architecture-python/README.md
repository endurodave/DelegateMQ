Python application shows how Python can communicate with a C++ server application 
using DelegateMQ remote delegates using ZeroMQ transport and MessagePack.

DelegateMQ is not implemented in Python. DelegateMQ is a C++ library. This Python app
demonstrates how Python communicates to a C++ DelegateMQ remote application using 
ZeroMQ and MessagePack.

This Python client app example connects to the C++ server app located here:

`DelegateMQ\example\sample-projects\system-architecture\server`

To run the Python client and C++ server applications:

1. Start the C++ server app
2. Run this Python client app `main.py`
3. Python client and C++ server communicate and output data to the console