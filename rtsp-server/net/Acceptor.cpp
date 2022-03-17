//Scott Xu
//2020-12-6 Add IPv6 support.
#include "Acceptor.h"
#include "EventLoop.h"
#include "SocketUtil.h"
#include "Logger.h"

using namespace xop;

Acceptor::Acceptor(EventLoop *eventLoop)
	: event_loop_(eventLoop), tcp_socket_(new TcpSocket())
{
}

Acceptor::~Acceptor() = default;

int Acceptor::Listen(const std::string &ip, const uint16_t port)
{
	std::lock_guard locker(mutex_);

	if (tcp_socket_->GetSocket() > 0) {
		tcp_socket_->Close();
	}
	const SOCKET sockfd =
		tcp_socket_->Create(SocketUtil::IsIpv6Address(ip));
	channel_ptr_.reset(new Channel(sockfd));
	SocketUtil::SetReuseAddr(sockfd);
	SocketUtil::SetReusePort(sockfd);
	SocketUtil::SetNonBlock(sockfd);

	if (!tcp_socket_->Bind(ip, port)) {
		return -1;
	}

	if (!tcp_socket_->Listen(1024)) {
		return -1;
	}

	channel_ptr_->SetReadCallback([this]() { this->OnAccept(); });
	channel_ptr_->EnableReading();
	event_loop_->UpdateChannel(channel_ptr_);
	return 0;
}

void Acceptor::Close()
{
	std::lock_guard locker(mutex_);

	if (tcp_socket_->GetSocket() > 0) {
		event_loop_->RemoveChannel(channel_ptr_);
		tcp_socket_->Close();
	}
}

void Acceptor::OnAccept()
{
	std::lock_guard locker(mutex_);
	if (const auto sockfd = tcp_socket_->Accept(); sockfd > 0) {
		if (new_connection_callback_) {
			new_connection_callback_(sockfd);
		} else {
			SocketUtil::Close(sockfd);
		}
	}
}
