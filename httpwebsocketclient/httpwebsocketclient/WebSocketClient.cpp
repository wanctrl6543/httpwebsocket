#include "WebSocketClient.h"
#include "config/asio_no_tls_client.hpp"
#include "client.hpp"
#include "common/thread.hpp"
#include "common/memory.hpp"
#include<cstdlib>
#include<iostream>
#include<map>
#include<string>
#include<sstream>

typedef websocketpp::client<websocketpp::config::asio_client> ws_client;

class connection_metadata {
public:
	typedef websocketpp::lib::shared_ptr<connection_metadata> metadataptr;
	connection_metadata(websocketpp::connection_hdl hdl,std::string uri)
		: m_hdl(hdl)
		, m_status("Connecting")
		, m_uri(uri)
		, m_server("N/A")
	{}

	void on_open(ws_client*client, websocketpp::connection_hdl hdl)
	{
		m_status = "Open";
		ws_client::connection_ptr con = client->get_con_from_hdl(hdl);
		m_server = con->get_response_header("Server");
	}
	void on_fail(ws_client*client ,websocketpp::connection_hdl hdl)
	{
		m_status = "Failed";
		ws_client::connection_ptr con = client->get_con_from_hdl(hdl);
		m_server = con->get_response_header("Server");
		m_error_reason = con->get_ec().message();
	}

	void on_close(ws_client* client, websocketpp::connection_hdl hdl)
	{
		m_status = "Closed";
		ws_client::connection_ptr con = client->get_con_from_hdl(hdl);
		std::stringstream s;
		s << "close code: " << con->get_remote_close_code() << "(" << websocketpp::close::status::get_string(con->get_remote_close_code()) << "),close reason: " << con->get_remote_close_reason();
		m_error_reason = s.str();
	}

	void on_message(websocketpp::connection_hdl hdl, ws_client::message_ptr msg)
	{
		if (msg->get_opcode() == websocketpp::frame::opcode::text)
		{
			m_messages.push_back("<< " + msg->get_payload());
		}
		else
			m_messages.push_back("<< " + websocketpp::utility::to_hex(msg->get_payload()));
	}

	websocketpp::connection_hdl get_hdl() const
	{
		return m_hdl;
	}

	std::string get_status() const
	{
		return m_status;
	}

	std::string get_uri() const
	{
		return m_uri;
	}

	void record_send_message(std::string message)
	{
		m_messages.push_back(">> " + message);
	}

	friend std::ostream & operator<< (std::ostream & out, connection_metadata const&data);

private:
	websocketpp::connection_hdl m_hdl;
	std::string m_status;
	std::string m_uri;
	std::string m_server;
	std::string m_error_reason;
	std::vector<std::string> m_messages;

};

std::ostream & operator<<(std::ostream&out, connection_metadata const&data)
{
	out << "> URI: " << data.m_uri << "\n"
		<< "> Status: " << data.m_status << "\n"
		<< "> Remote Server: " << (data.m_server.empty() ? "None Specofied" : data.m_server) << "\n"
		<< "> Error/close reason: " << (data.m_error_reason.empty() ? "N/A" : data.m_error_reason) << "\n";
	out << "> Message Processed:(" << data.m_messages.size() << ")\n";
	std::vector<std::string>::const_iterator it;
	for (it = data.m_messages.begin();it != data.m_messages.end();++it)
	{
		out << *it << "\n";
	}
	return out;
}

ws_client g_wsEndpoint;
connection_metadata::metadataptr g_wsClientConnection;
websocketpp::lib::shared_ptr<websocketpp::lib::thread> g_threadws;

CWebSocketClient::CWebSocketClient()
{
	g_wsEndpoint.clear_access_channels(websocketpp::log::alevel::all);
	g_wsEndpoint.clear_error_channels(websocketpp::log::elevel::all);

	g_wsEndpoint.init_asio();
	g_wsEndpoint.start_perpetual();

	g_threadws = websocketpp::lib::make_shared<websocketpp::lib::thread>(&ws_client::run, &g_wsEndpoint);
}


CWebSocketClient::~CWebSocketClient()
{
	g_wsEndpoint.stop_perpetual();
	if (g_wsClientConnection->get_status() == "Open")
	{
		websocketpp::lib::error_code ec;
		g_wsEndpoint.close(g_wsClientConnection->get_hdl(), websocketpp::close::status::going_away, "", ec);
		if (ec)
		{
			std::cout << "> Error closing ws connection " << g_wsClientConnection->get_uri() << " :" << ec.message() << std::endl;

		}
	}
	g_threadws->join();
}

int CWebSocketClient::connect(const char* struri)
{
	websocketpp::lib::error_code ec;
	ws_client::connection_ptr pConnection = g_wsEndpoint.get_connection(struri, ec);
	if (ec)
	{
		std::cout << "> Connect initialization error: " << ec.message() << std::endl;
		return -1;
	}

	g_wsClientConnection = websocketpp::lib::make_shared<connection_metadata>(pConnection->get_handle(),struri);
	pConnection->set_open_handler(websocketpp::lib::bind(&connection_metadata::on_open, g_wsClientConnection, &g_wsEndpoint, websocketpp::lib::placeholders::_1));
	pConnection->set_fail_handler(websocketpp::lib::bind(&connection_metadata::on_fail, g_wsClientConnection, &g_wsEndpoint, websocketpp::lib::placeholders::_1));
	pConnection->set_close_handler(websocketpp::lib::bind(&connection_metadata::on_close, g_wsClientConnection, &g_wsEndpoint, websocketpp::lib::placeholders::_1));
	pConnection->set_message_handler(websocketpp::lib::bind(&connection_metadata::on_message, g_wsClientConnection, websocketpp::lib::placeholders::_1, websocketpp::lib::placeholders::_2));
	g_wsEndpoint.connect(pConnection);
	return 0;
}

void CWebSocketClient::close()
{
	if (g_wsClientConnection->get_status() == "Open")
	{
		int close_code = websocketpp::close::status::normal;
		websocketpp::lib::error_code ec;
		g_wsEndpoint.close(g_wsClientConnection->get_hdl(), close_code, "", ec);
		if (ec)
		{
			std::cout << "> Error initiating close: " << ec.message() << std::endl;
		}
	}
}

void CWebSocketClient::send(const char* message)
{
	websocketpp::lib::error_code ec;
	g_wsEndpoint.send(g_wsClientConnection->get_hdl(), message, websocketpp::frame::opcode::text, ec);
	if (ec)
	{
		std::cout << "> Error sending message: " << ec.message() << std::endl;
		return;
	}
	g_wsClientConnection->record_send_message(message);
}

void CWebSocketClient::show()
{
	std::cout << *g_wsClientConnection << std::endl;
}
