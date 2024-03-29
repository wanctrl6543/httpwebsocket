// httpwebsocketclient.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include<iostream>
#include <string>
#include <sstream>

#include "WebSocketClient.h"
int main()
{
	bool done = false;

	std::string input;
	CWebSocketClient client;
	client.connect("ws://127.0.0.1:9002");
	while (!done)
	{
		std::cout << "Enter Command: ";
		std::getline(std::cin, input);
		if (input == "quit")
		{
			done = true;
		}
		else if(input.substr(0,4) == "send")
		{
			std::stringstream ss(input);
			std::string cmd;
			std::string message;
			ss >> cmd;
			std::getline(ss, message);
			client.send(message.c_str());
		}
		else if(input.substr(0,4) == "show")
		{
			client.show();

		}
		else
		{
			std::cout << "> Unrecognized Command" << std::endl;
		}
	}
	client.close();

    return 0;
}

