# FoodWasteReducer

## :ledger: Index
- [Description](#beginner-description)
- [Instructions](#zap-instructions)
- [Functionality](#gear-functionality)
- [Structure](#gear-structure)

## :beginner: Description
This is a project written in C that facilitates food donations and requests, as an intermediary between suppliers and charitable organizations. The system presents a reliable solution for users to donate unsold food or request donations efficiently and securely.

## :zap: Instructions
For the setup, you must proceed as follows:
- Install SQLite from https://www.sqlite.org/download.html
- Clone this repository in a local folder: ```git clone https://github.com/zmirch/FoodWasteReducer.git```
- Run the server ```./server```
- Run the client ```./client 0.0.0.0 2911```
- type 'help' for a list of commands.


## :gear: Functionality
Users access the app using the command-line. If the server is running, client(s) will be able to connect to it and it will handle their requests.
Clients have the following commands at their disposal, as revealed by 'help':

![image](https://github.com/user-attachments/assets/1c14411c-e9ec-4db4-9ceb-932571c1aa1c)

After they register as a donor or receiver, the user can then call the appropriate commands.
Food that can be donated is divided into multiple categories: meat, produce, dairy, and bread; When a donor contributes with an amount of food, it will be inserted into the 'Products' table of the database as a new row, using the SQLite library:

![image](https://github.com/user-attachments/assets/26fb9980-5f2d-4ada-a88c-f5330239e14a)

Afterwards, when a client will request an amount of that category of food, the system will subtract the amount from the oldest donation(s), to prevent food spoilage.

## :gear: Structure
The application uses the TCP/IP architectural model. For concurrency, it uses LWPs (Light Weight Processes) for each client. The reason behind this design choice is the performance that it offers compared to a design that is pre-forked or which creates a new child process for every new client.

Client processes, concurrently connected to the server, take user input and send commands to the server, which validates their syntax and the user permissions.

Clients who are authenticated as receivers declare food requests to the server, waiting for the server to confirm the donation, and those who are of the supplier type declare the quantities and types available, waiting for the server to confirm the data insertion into the database.


