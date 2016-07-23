#pragma once
#include <boost/asio.hpp>
#include <array>
#include <cstdint>
#include <utility>
#include <iostream> //debug
#include <functional>
#include <ctime>

#include "protocol/detail/protocol_detail.hpp"
#include "event/event.hpp"
#include "protocol/detail/basic_async_reader.hpp"
#include "protocol/detail/basic_async_writer.hpp"
#include "protocol/detail/serializers/text_iserializer.hpp"
#include "protocol/detail/serializers/text_oserializer.hpp"

namespace t3o
{

namespace detail
{
	using namespace boost::asio;
	using boost::asio::ip::tcp;
	using std::array;
	using async_text_reader = basic_async_reader<text_iserializer>;
	using async_text_writer = basic_async_writer<text_oserializer>;
}

class game_session
{
public:
	explicit game_session(detail::io_service& service) :
		_socket{service},
		_symbol{0},
		_reader{_socket},
		_writer{_socket},
		_is_closed{false},
		_is_logged{false},
		_is_alive{true},
		_is_ran{false}
	{
		_reader.event_disconnected() 
			+= std::bind(&game_session::_handle_disconnected, this);
		_writer.event_disconnected() 
			+= std::bind(&game_session::_handle_disconnected, this);
	}

	~game_session()
	{
		close();
	}

	void async_run()
	{
		//do a harlemshake 
		_is_ran = true;
		using namespace std::placeholders;
		auto binder = std::bind(&game_session::_handle_handshake, this, _1);
		_reader.async_read<detail::protocol::client_handshake>(binder);
	}
			
	void async_send_field_set(unsigned field, unsigned x, unsigned y, 
			std::function<void()> handler = nullptr)
	{
		detail::protocol::field_set_packet packet;
		packet.x = x;
		packet.y = y;
		packet.field = field;
		_writer.async_write(packet, handler);
	}

	bool keepalive()
	{
		if(!_is_alive) return false;
		_is_alive = false;
		detail::protocol::keepalive packet;
		time(&packet.timestamp);
		_writer.async_write(packet);
		return true;
	}

	void close()
	{
		if(_is_closed) return;
		_disconnected_event();
		_socket.close();
		_is_closed = true;
		_is_logged = false;
	}

	void async_begin_game(uint8_t symbol, uint8_t width, uint8_t height,
			std::function<void()> handler = nullptr)
	{
		detail::protocol::server_handshake packet;
		packet.symbol = _symbol = symbol;
		packet.width = width;
		packet.height = height;
		_writer.async_write(packet, handler);
	}

	void async_end_game(uint8_t result,
			std::function<void()> handler = nullptr)
	{
		detail::protocol::game_end packet;
		packet.result = result;
		_writer.async_write(packet, handler);
	}
			
	auto& socket() { return _socket; }
	const auto& name() const { return _name; }
	auto symbol() const	{ return _symbol; }

	//flags
	auto is_closed() const { return _is_closed; }
	auto is_logged() const { return _is_logged;	}
	auto is_ran() const { return _is_ran; }
	auto is_working() const	{ return _is_ran && !_is_closed; }

	//events
	auto& event_field_set() { return _field_set_event; }
	auto& event_disconnected() { return _disconnected_event; }
	auto& event_logged() { return _logged_event; }

private:
	void _handle_disconnected()
	{
		close();
		_disconnected_event();
	}

	void _handle_handshake(const detail::protocol::client_handshake& data)
	{
		_name.assign(std::begin(data.name), std::end(data.name));
		_send_feedback(0, [this]{
			_is_logged = true;
			_logged_event(); 
			//std::cout << "[DEBUG]: LISTEN FOR DATA BEGIN" << std::endl;
			_listen_for_data();		
		}); 
	}

	void _listen_for_data()
	{
		using namespace std::placeholders;
		auto field_set_binder = std::bind(&game_session::_handle_field_set, this, _1);
		auto keepalive_binder = std::bind(&game_session::_handle_keepalive, this, _1);
		_reader.async_read<
			detail::protocol::field_set_packet,
			detail::protocol::keepalive
		>(field_set_binder, keepalive_binder);
	}

	void _handle_field_set(const detail::protocol::field_set_packet& data)
	{
		_field_set_event(static_cast<unsigned>(data.x),
				static_cast<unsigned>(data.y));
		_listen_for_data();
	}

	void _handle_keepalive(const detail::protocol::keepalive& data)
	{
		//std::cout << "on keep alive" << std::endl;
		_is_alive = true;
		_listen_for_data();
	}

	void _send_feedback(uint8_t result, std::function<void()> callback)
	{
		detail::protocol::feedback packet;
		packet.result = result;
		_writer.async_write(packet, callback);
	}

	void _recv_feedback(std::function<void(const detail::protocol::feedback&)> callback)
	{
		_reader.async_read<detail::protocol::feedback>(callback);
	}

	detail::tcp::socket _socket;
	std::string _name;
	uint8_t _symbol;
			
	detail::async_text_reader _reader;
	detail::async_text_writer _writer;

	//flags
	bool _is_closed;
	bool _is_logged;
	bool _is_alive;
	bool _is_ran;

	//events
	event<void()> _logged_event;
	event<void(unsigned x, unsigned y)> _field_set_event;
	event<void()> _disconnected_event;
};

}

