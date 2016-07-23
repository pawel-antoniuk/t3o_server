#include <iostream>
#include <boost/asio.hpp>
#include <string>
#include <iterator>
#include <cstdio>
#include <thread>
#include <chrono>

#include "game_server.hpp"
#include "game_board.hpp"

namespace t3o
{

class server_controller
{
public:
	server_controller(boost::asio::io_service& service,
			unsigned short port) :
		_server(service, port),
		_is_game_on(false),
		_board(3, 3)
	{
		using namespace std::placeholders;
		_server.event_session_started() 
			+= std::bind(&server_controller::_handle_session_started,
					this, _1);
		_server.event_session_ended()
			+= std::bind(&server_controller::_handle_session_ended,
					this, _1);
	}

	void begin_listen()
	{
		_server.start_listen_for_players();
	}

private:
	void _handle_session_started(t3o::game_session& session)
	{
		std::cout << "new connection" << std::endl;
		using namespace std::placeholders;
		session.event_logged() 
			+= std::bind(&server_controller::_handle_sign_in,
					this, std::ref(session));
		session.event_field_set() 
			+= std::bind(&server_controller::_handle_field_set,
					this, std::ref(session), _1, _2);
	}

	void _handle_session_ended(t3o::game_session& session)
	{
		_end_game();
		std::cout << "disconnection" << std::endl;
		for(auto& s : _server.sessions()) if(s->is_logged())
		{
			s->async_end_game(0);
		}
	}

	void _handle_sign_in(t3o::game_session& session)
	{
		std::cout << "logged as: " 
			<< session.name().c_str() << std::endl;
		auto players_online = _count_online_players();
		std::cout << "players online: " << players_online << std::endl;
		if(players_online == 2)
		{
			_begin_game();
			std::cout << "game has begun" << std::endl;
			unsigned symbol = 1;
			for(auto& s : _server.sessions()) if(s->is_logged())
			{
				s->async_begin_game(symbol++, 3, 3);				
			}
		}

	}

	void _handle_field_set(t3o::game_session& session,
			unsigned x, unsigned y)
	{
		std::cout << '[' << x << "][" << y << "]=" 
			<< (int)session.symbol() << std::endl;
		for(auto& s : _server.sessions()) if(s->is_logged())
		{
			s->async_send_field_set(session.symbol(), x, y);
		}
		_board.set_field(session.symbol(), x, y);
		auto winner = _board.who_won();
		if(winner) _handle_player_won(winner);
	}

	void _handle_player_won(unsigned winner)
	{
		std::cout << winner << " won!" << std::endl;
		_end_game();
		for(auto& s : _server.sessions()) if(s->is_logged())
		{
			s->async_end_game(winner, [&s]{ s->close(); });
		}
		_board.clear();
	}
	
	unsigned _count_online_players()
	{
		unsigned counter = 0;
		for(auto& s : _server.sessions()) counter += s->is_logged();
		return counter;
	}

	void _begin_game()
	{
		if(!_is_game_on)
		{
			_is_game_on = true;
			_server.stop_listen_for_players();
		}
	}

	void _end_game()
	{
		if(_is_game_on)
		{
			_is_game_on = false;
			_server.start_listen_for_players();
		}
	}

	t3o::game_server _server;
	bool _is_game_on;
	game_board _board;
};

}
