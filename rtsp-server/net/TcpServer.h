// PHZ
// 2018-11-10
// Scott Xu
// 2020-12-04 Add multiple socket support.

#ifndef XOP_TCPSERVER_H
#define XOP_TCPSERVER_H

#include <memory>
#include <string>
#include <mutex>
#include <unordered_map>
#include "EventLoop.h"
#include "TcpConnection.h"

namespace xop {

class Acceptor;
class EventLoop;

class TcpServer {
public:
	explicit TcpServer(EventLoop *event_loop);
	virtual ~TcpServer();

	virtual bool Start(const std::string &ip, uint16_t port);
	virtual void Stop();

	/*std::string GetIPAddress() const
	{ return ip_; }

	uint16_t GetPort() const 
	{ return port_; }*/

protected:
	virtual TcpConnection::Ptr OnConnect(SOCKET sockfd);
	virtual void AddConnection(SOCKET sockfd, TcpConnection::Ptr tcpConn);
	virtual void RemoveConnection(SOCKET sockfd);

	EventLoop *event_loop_;
	//uint16_t port_;
	//std::string ip_;
	std::vector<std::unique_ptr<Acceptor>> acceptors_;
	//bool is_started_;
	std::mutex mutex_;
	std::unordered_map<SOCKET, std::shared_ptr<TcpConnection>> connections_;
};

}

#endif
