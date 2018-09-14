#pragma once
#include<string>
class CWebSocketClient
{
public:
	CWebSocketClient();
	~CWebSocketClient();

	int connect(const char* struri);
	void close();

	void send(const char* message);
	void show();
};

