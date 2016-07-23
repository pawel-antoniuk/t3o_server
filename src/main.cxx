#include <iostream>

#include "server_controller.hpp"

int main()
{
	boost::asio::io_service service;
	t3o::server_controller controller(service, 6667);
	std::cout << "server has beend started v1" 
		<< std::endl;
	controller.begin_listen();
	service.run();
}
