#pragma once
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <functional>
#include <vector>
#include <thread>
#include <functional>
#include <utility>
#include <algorithm>
#include <chrono>

#include "game_session.hpp"
#include "event/event.hpp"

namespace t3o
{

namespace detail
{
	using namespace boost::asio;
	using boost::asio::ip::tcp;
	using std::function;
	using std::vector;
	using session_ptr = std::shared_ptr<game_session>;
	using duration_t = boost::posix_time::time_duration;
}

class game_server 
{
public:
	explicit game_server(detail::io_service& io_service, short port,
			detail::duration_t keepalive_duration = boost::posix_time::seconds(5)) :
		_io_service{io_service}, 
		_acceptor{_io_service, boost::asio::ip::tcp::endpoint(
				boost::asio::ip::tcp::v4(), port)},
		_is_listening{false},
		_keepalive_duration{keepalive_duration},
		_keepalive_timer{io_service}
	{
		using namespace std::placeholders;
		auto binder = std::bind(&game_server::_distribute_keepalive, this, _1);
		_keepalive_timer.expires_from_now(_keepalive_duration);
		_keepalive_timer.async_wait(binder);
	}

	void set_field(unsigned x, unsigned y, unsigned field)
	{
		for(auto& session : _sessions)
		{
			session->async_send_field_set(x, y, field, []{/*do nothing*/});
		}
	}

	void start_listen_for_players()
	{
		_is_listening = true;
		_create_session();
	}

	void stop_listen_for_players()
	{
		_is_listening = false;
		_acceptor.cancel();
	}

	const auto& sessions() const { return _sessions; }

	//events
	auto& event_session_started() {	return _session_started_event; }
	auto& event_session_ended()	{ return _session_ended_event; }
			
private:
	void _distribute_keepalive(const boost::system::error_code&)
	{
		//std::cout << "[DEBUG]: distribute keepalive" << std::endl;
		for(auto& s : _sessions) if(s->is_working())
		{
			auto result = s->keepalive();
			if(!result) 
			{
				s->close();
				//std::cout << "keepalive not recived" << std::endl;
			}
		}
		using namespace std::placeholders;
		auto binder = std::bind(&game_server::_distribute_keepalive, this, _1);
		_keepalive_timer.expires_from_now(_keepalive_duration);
		_keepalive_timer.async_wait(binder);
	}

	void _create_session()
	{
		auto session = std::make_shared<game_session>(_io_service);
		_sessions.push_back(session);
		using namespace std::placeholders;
		_acceptor.async_accept(session->socket(), 
				std::bind(&game_server::_on_accepted, this, session, _1));
	}

	void _on_accepted(detail::session_ptr& session, 
			const boost::system::error_code& code)
	{
		if(code != boost::system::errc::success) return;

		using namespace std::placeholders;
		session->event_disconnected() += 
			std::bind(&game_server::_on_client_disconnected, this, session);
		_session_started_event(*session);
		session->async_run();
		_create_session();
	}

	void _on_client_disconnected(detail::session_ptr& session)
	{
		auto it = std::find(std::begin(_sessions), std::end(_sessions), session);
		if(it != std::end(_sessions)){
			_sessions.erase(it);
			_session_ended_event(*session);
		}
	}

	detail::io_service& _io_service;
	detail::tcp::acceptor _acceptor;
	detail::vector<detail::session_ptr> _sessions;
	bool _is_listening;
	detail::duration_t _keepalive_duration;
	detail::deadline_timer _keepalive_timer;

	//events
	event<void(game_session&)> _session_started_event;
	event<void(game_session&)> _session_ended_event;
};

}

