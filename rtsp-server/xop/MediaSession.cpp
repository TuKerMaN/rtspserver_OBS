// PHZ
// 2018-9-30
// Scott Xu
// 2020-12-5 Add IPv6 Support.

#include "MediaSession.h"
#include "RtpConnection.h"
#include <cstring>
#include <ctime>
#include <map>
#include <forward_list>
#include <utility>
#include "net/Logger.h"

using namespace xop;
using namespace std;

std::atomic_uint MediaSession::last_session_id_(1);

MediaSession::MediaSession(std::string url_suffix,
			   const uint8_t max_channel_count)
	: max_channel_count_(max_channel_count),
	  session_id_(++last_session_id_),
	  suffix_(std::move(url_suffix)),
	  media_sources_(max_channel_count),
	  buffer_(max_channel_count),
	  multicast_port_(max_channel_count, 0),
	  has_new_client_(false)
{
}

MediaSession *MediaSession::CreateNew(std::string url_suffix,
				      const uint32_t max_channel_count)
{
	return new MediaSession(std::move(url_suffix), max_channel_count);
}

MediaSession::~MediaSession()
{
	if (!multicast_ip_.empty()) {
		MulticastAddr::instance().Release(multicast_ip_);
	}
}

void MediaSession::AddNotifyConnectedCallback(
	const NotifyConnectedCallback &callback)
{
	notify_connected_callbacks_.push_back(callback);
}

void MediaSession::AddNotifyDisconnectedCallback(
	const NotifyDisconnectedCallback &callback)
{
	notify_disconnected_callbacks_.push_back(callback);
}

bool MediaSession::AddSource(MediaChannelId channel_id, MediaSource *source)
{
	if (static_cast<uint8_t>(channel_id) >= max_channel_count_)
		return false;
	source->SetSendFrameCallback([this](const MediaChannelId channel_id,
					    const RtpPacket &pkt) {
		std::lock_guard lock(map_mutex_);

		std::forward_list<std::shared_ptr<RtpConnection>> clients;
		std::map<int, RtpPacket> packets;

		for (auto iter = clients_.begin(); iter != clients_.end();) {
			if (auto conn = iter->second.lock(); conn == nullptr) {
				clients_.erase(iter++);
			} else {
				if (int id = conn->GetId(); id >= 0) {
					if (packets.find(id) == packets.end()) {
						RtpPacket tmp_pkt;
						memcpy(tmp_pkt.data.get(),
						       pkt.data.get(),
						       pkt.size);
						tmp_pkt.size = pkt.size;
						tmp_pkt.last = pkt.last;
						tmp_pkt.timestamp =
							pkt.timestamp;
						tmp_pkt.type = pkt.type;
						packets.emplace(id, tmp_pkt);
					}
					clients.emplace_front(conn);
				}
				++iter;
			}
		}

		int count = 0;
		for (const auto &iter : clients) {
			if (int id = iter->GetId(); id >= 0) {
				if (auto iter2 = packets.find(id);
				    iter2 != packets.end()) {
					int ret = 0;
					count++;
					ret = iter->SendRtpPacket(
						channel_id, iter2->second);
					if (is_multicast_ && ret == 0) {
						break;
					}
				}
			}
		}
		return true;
	});

	media_sources_[static_cast<uint8_t>(channel_id)].reset(source);
	return true;
}

bool MediaSession::RemoveSource(MediaChannelId channel_id)
{
	media_sources_[static_cast<uint8_t>(channel_id)] = nullptr;
	return true;
}

bool MediaSession::StartMulticast()
{
	if (is_multicast_) {
		return true;
	}

	multicast_ip_ = MulticastAddr::instance().GetAddr();
	if (multicast_ip_.empty()) {
		return false;
	}

	std::random_device rd;
	for (uint32_t chn = 0; chn < max_channel_count_; chn++)
		multicast_port_[chn] = htons(rd() & 0xfffe);

	is_multicast_ = true;
	return true;
}

std::string MediaSession::GetSdpMessage(const std::string ip,
					const std::string &session_name,
					const bool ipv6) const
{
	if (media_sources_.empty())
		return "";

	//std::string ip = NetInterface::GetLocalIPAddress(ipv6);
	char buf[2048] = {0};

	snprintf(buf, sizeof(buf),
		 "v=0\r\n"
		 "o=- 9%ld 1 IN IP%d %s\r\n"
		 "t=0 0\r\n"
		 "a=control:*\r\n",
		 static_cast<long>(std::time(nullptr)), ipv6 ? 6 : 4,
		 ip.c_str());

	if (!session_name.empty()) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			 "s=%s\r\n", session_name.c_str());
	}

	if (is_multicast_) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			 "a=type:broadcast\r\n"
			 "a=rtcp-unicast: reflection\r\n");
	}

	for (uint32_t chn = 0; chn < max_channel_count_; chn++) {
		if (media_sources_[chn]) {
			if (is_multicast_) {
				snprintf(buf + strlen(buf),
					 sizeof(buf) - strlen(buf), "%s\r\n",
					 media_sources_[chn]
						 ->GetMediaDescription(
							 multicast_port_[chn])
						 .c_str());

				snprintf(buf + strlen(buf),
					 sizeof(buf) - strlen(buf),
					 "c=IN IP%d %s/255\r\n", ipv6 ? 6 : 4,
					 multicast_ip_.c_str());
			} else {
				snprintf(buf + strlen(buf),
					 sizeof(buf) - strlen(buf), "%s\r\n",
					 media_sources_[chn]
						 ->GetMediaDescription(0)
						 .c_str());
			}

			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
				 "%s\r\n",
				 media_sources_[chn]->GetAttribute().c_str());

			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
				 "a=control:track%d\r\n", chn);
		}
	}

	return buf;
}

MediaSource *MediaSession::GetMediaSource(MediaChannelId channel_id) const
{
	if (static_cast<uint8_t>(channel_id) < max_channel_count_) {
		return media_sources_[static_cast<uint8_t>(channel_id)].get();
	}

	return nullptr;
}

bool MediaSession::HandleFrame(MediaChannelId channelId, AVFrame frame)
{
	std::lock_guard lock(mutex_);

	if (static_cast<uint8_t>(channelId) < max_channel_count_) {
		media_sources_[static_cast<uint8_t>(channelId)]->HandleFrame(
			channelId, std::move(frame));
	} else {
		return false;
	}

	return true;
}

bool MediaSession::AddClient(const SOCKET rtspfd,
			     const std::shared_ptr<RtpConnection> &rtp_conn)
{
	std::lock_guard lock(map_mutex_);

	if (const auto iter = clients_.find(rtspfd); iter == clients_.end()) {
		std::weak_ptr rtp_conn_weak_ptr = rtp_conn;
		clients_.emplace(rtspfd, rtp_conn_weak_ptr);
		for (auto &callback : notify_connected_callbacks_)
			callback(
				session_id_, rtp_conn->GetIp(),
				rtp_conn->GetPort()); /* 回调通知当前客户端数量 */

		has_new_client_ = true;
		return true;
	}

	return false;
}

void MediaSession::RemoveClient(const SOCKET rtspfd)
{
	std::lock_guard lock(map_mutex_);

	if (const auto iter = clients_.find(rtspfd); iter != clients_.end()) {
		if (const auto conn = iter->second.lock()) {
			for (auto &callback : notify_disconnected_callbacks_)
				callback(
					session_id_, conn->GetIp(),
					conn->GetPort()); /* 回调通知当前客户端数量 */
		}
		clients_.erase(iter);
	}
}
