# ABXClient_for_exchange_server

This project develops a C++ client application capable of requesting and receiving stock ticker data from the ABX exchange server.The goal of this C++ client is to generate a JSON file as output. This JSON file contains an array of objects, where each object represents a packet of data with increasing sequences.

**Steps to run the project:**
1. Clone the repository to your local machine.
2. Open command prompt and navigate to the folder.
3. Run the command: g++ -o client client.cpp -lws2_32
4. The above command will generate a client.exe file.
5. Now open another terminal and run the server : node main.js
6. In other terminal run the exe file: ./client.exe

Following these steps will generate a output.json file.
