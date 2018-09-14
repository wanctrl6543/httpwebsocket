#include "WebSocketServer.h"
#include "config/asio_no_tls.hpp"
#include "server.hpp"
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>

typedef websocketpp::server<websocketpp::config::asio> server;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using websocketpp::lib::mutex;
using websocketpp::lib::lock_guard;

mutex m_action_lock;
typedef server::message_ptr message_ptr;
server web_server;

std::map<void*, websocketpp::connection_hdl> g_maphdl;

void on_open(server*s, websocketpp::connection_hdl hdl)
{
	lock_guard<mutex> guard(m_action_lock);
	void* pFirst = hdl.lock().get();
	g_maphdl[pFirst] = hdl;
}

void on_close(server*s, websocketpp::connection_hdl hdl)
{
	lock_guard<mutex> guard(m_action_lock);	
	void* pFirst = hdl.lock().get();
	std::map<void*, websocketpp::connection_hdl>::iterator it = g_maphdl.find(pFirst);
	if (it != g_maphdl.end())
	{
		g_maphdl.erase(it);
	}
}

void on_message(server* s, websocketpp::connection_hdl hdl, message_ptr msg)
{
	std::cout << "on_message called with hdl: " << hdl.lock().get()
		<< " and message: " << msg->get_payload()
		<< std::endl;

	if (msg->get_payload() == "stop-listening")
	{
		s->stop_listening();
		return;
	}
}



CWebSocketServer* g_pWebSocketServer = NULL;
int g_nport = 9002;

CWebSocketServer::CWebSocketServer()
	: m_strurl("")
	, m_nport(9002)
	, m_bstartserver(false)
{
	g_pWebSocketServer = this;
}


CWebSocketServer::~CWebSocketServer()
{
}

CWebSocketServer* CWebSocketServer::GetWebSocketServer(bool bCreate /*= false*/)
{
	if (NULL == g_pWebSocketServer && bCreate)
	{
		new CWebSocketServer();
	}
	return g_pWebSocketServer;
}

void CWebSocketServer::DestroyWebSocketServer()
{
	if (NULL != g_pWebSocketServer)
	{
		delete g_pWebSocketServer;
		g_pWebSocketServer = NULL;
	}
}

void CWebSocketServer::thread()
{
	try
	{
		web_server.set_access_channels(websocketpp::log::alevel::all);
		web_server.clear_access_channels(websocketpp::log::alevel::frame_payload);
		web_server.init_asio();
		web_server.set_open_handler(bind(&on_open, &web_server, ::_1));
		web_server.set_close_handler(bind(&on_close, &web_server, ::_1));
		web_server.set_message_handler(bind(&on_message, &web_server, ::_1, ::_2));
		web_server.listen(g_nport);
		web_server.start_accept();
		web_server.run();
	}
	catch (websocketpp::exception const & e)
	{
		std::cout << e.what() << std::endl;

		return;
	}
	catch (...)
	{
		std::cout << "other exception" << std::endl;

		return;
	}
	return;
}

int CWebSocketServer::startserver(std::string strurl, int nport /*= 9002*/)
{
	m_strurl = strurl;
	m_nport = nport;
	g_nport = m_nport;
	m_bstartserver = true;

	boost::thread t(CWebSocketServer::thread);
	t.timed_join(boost::posix_time::seconds(1));
	return 0;
}

void CWebSocketServer::stopserver()
{
	web_server.stop();
	g_maphdl.clear();
}

void CWebSocketServer::sendmsgtoweb(const char* strmsg)
{
	if (!m_bstartserver)
	{
		return;
	}
	lock_guard<mutex> guard(m_action_lock);
	std::map<void*, websocketpp::connection_hdl>::iterator it = g_maphdl.begin();
	while (it != g_maphdl.end())
	{

		websocketpp::connection_hdl hdl = it->second;
		try
		{
			server::connection_type::ptr ptr = web_server.get_con_from_hdl(hdl);
			if (1 == ptr->get_state())
			{
				web_server.send(hdl, strmsg, websocketpp::frame::opcode::text);
			}

			if (1 == ptr->get_state())
			{
				//echo_server.send(hdl, data, websocketpp::frame::opcode::text);
			}
		}
		catch (const websocketpp::lib::error_code& e)
		{
			std::cout << "Echo failed because: " << e
				<< "(" << e.message() << ")" << std::endl;
		}
		++it;
	}
}
