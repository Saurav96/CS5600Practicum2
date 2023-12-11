# CS5600_Practicum2

## Make the Project:
>> make clean
>> make 

## Setup the ROOT_DIRECTORY in util.h
## Setup the CLIENT_DIRECTORY in util.h


## Run the server:
>> cd server
>> ./server

## To run the client:
>> cd client
>> ./client


To run individual commands on the client:
// No. of arguments as per the command.
>> ./rfs COMMAND ARG1 ARG2 

eg1: /rfs LS client_test/pUe.ttx
eg2: ./rfs GET client_test/pUe.ttx test/a.txxclear
eg3: ./rfs GET client_test/pUe.ttx

eg4: ./rfs WRITE client_test/pUe.ttx hello/a.txxclear
eg5: ./rfs WRITE client_test/pUe.ttx

eg6: ./rfs RM client_test/pUe.ttx
eg7: ./rfs RM filr.txt


