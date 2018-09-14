#pragma once
#include <string>
#include "thread.hpp"
#include "thread/mutex.hpp"

using namespace std;
class CWebSocketServer
{
public:
	CWebSocketServer();
	~CWebSocketServer();

public:
	static CWebSocketServer*GetWebSocketServer(bool bCreate = false);
	static void DestroyWebSocketServer();
	static void thread();
	int startserver(std::string strurl, int nport = 9002);
	void stopserver();
	void sendmsgtoweb(const char* strmsg);

private:
	std::string m_strurl;
	int  m_nport;
	bool m_bstartserver;
};

