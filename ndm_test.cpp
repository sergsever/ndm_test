/*
** selectserver.c -- a cheezy multiperson chat server
*/
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <vector>
#include <thread>
#include <mutex>
#include <ctime>
#include <map>
#include <numeric>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio.hpp>
#include <memory>
#include<atomic>

#define PORT "9034"   // port we're listening on/

/*
 * Return a listening socket
 */



int get_listener_socket(void)
{
    struct addrinfo hints, *ai, *p;
    int yes=1;    // for setsockopt() SO_REUSEADDR, below
    int rv;
    int listener;

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }

    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    return listener;
}


std::map<int, unsigned> clients;
std::mutex mutex_clients;
std::atomic<bool> stop = false;
std::mutex stop_mutex;
const int BUFF_SIZE = 256;

/*commands*/

const std::string cmdTime("/time");
const std::string cmdStats("/stats");
const std::string cmdShutdown("/shutdown");

int processClient(int clientSock)
{
	char buff[BUFF_SIZE];
	int recived = 0;
	mutex_clients.lock();
	
	clients[clientSock] = ++clients[clientSock];
	mutex_clients.unlock();
	memset(&buff, 0, BUFF_SIZE); 
	std::cout << "process client" << std::endl;
	recived = recv(clientSock, buff , BUFF_SIZE, 0); 
	std::cout << "recived: " << recived << " chars: " << buff << std::endl;  
	if (recived > 0)
	{
		buff[recived] = 0;
	}
	else
	{
		std::cout << "reicive error " << recived << " errno: " << errno << std::endl; 
	}

	
	if (strlen(buff) > 0)
	{
		if (buff[strlen(buff) -1] == '\n')
			buff[strlen(buff) -1] = 0;
		std::string input = std::string(buff);
		if (input[0] == '/')
		{
			/*command*/
			char output[BUFF_SIZE] = {0};
			std::time_t time = std::time(nullptr);	
			if (input.compare(cmdTime) == 0) 
			{
				if (std::strftime(output, BUFF_SIZE, "%F %T", std::localtime(&time))){
				}
			} 
			else if(input.compare(cmdStats) == 0)
			{
			memset(output, 0, BUFF_SIZE);
				mutex_clients.lock();	
				int clientsCount = clients.size();
				int sessions = std::accumulate(
        std::begin(clients),
        std::end(clients),
        0, // initial value of the sum
        [](const unsigned previous, const std::pair<const int,unsigned >& p)
        { return previous + p.second; }
    	);
		mutex_clients.unlock();
			snprintf(output, BUFF_SIZE, "all clients: %d, all sessions:%d\n",clientsCount,
			sessions); 
		}			
		else if(input.compare(cmdShutdown) == 0)
		{
			std::cout << "command shutdown" << std::endl;
			snprintf(output, BUFF_SIZE, "-bye! ");
			//stop_mutex.lock();
			stop = true;
			//stop_mutex.unlock();
		}
			
		if(strlen(output) > 0)
		{
			output[strlen(output) -1] = '\n';
			send(clientSock, output, strlen(output) , 0);	
		}
		}
		else 
		{
			/*not command*/
			send(clientSock, buff, strlen(buff), 0); 
		}
		}
	shutdown(clientSock, 2);
	close(clientSock);
	return 0;	     
}
/*
 * Main
 */
int run_events(std::shared_ptr<boost::asio::thread_pool> pool)
{
    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    //int fdmax;        // maximum file descriptor number

    int listener;     // listening socket descriptor
    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);
	int epoll_descriptor = 0;	
//    boost::asio::thread_pool pool(5);

	std::cout << "get socket" << std::endl;
    listener = get_listener_socket();
	epoll_descriptor = epoll_create1(0);
	if (epoll_descriptor == -1)
	{
		std::cout << "error creating epool: " << errno << std::endl;
		exit(1); 
	}
	std::cout << "epoll created" << std::endl;

	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = listener;
	if ( epoll_ctl(epoll_descriptor,EPOLL_CTL_ADD, listener, &event) == -1 )
	{
		std::cout << "error epoll_ctl: " << errno << std::endl;
		exit(2);
	}
	std::cout << "waiting events" << std::endl;  
	const int MAX_EVENTS = 64;
	//stop_mutex.lock();
	stop = false;
	//stop_mutex.unlock();
	while(stop == false)
	{
		struct epoll_event events[MAX_EVENTS];
		int events_count = epoll_wait(epoll_descriptor, events, MAX_EVENTS, -1);
		std::cout << "events count: " << events_count << std::endl;
		if( events_count == -1)
		{
			std::cout << "waiting error: " << errno << std::endl;
			exit(3); 
		}
		
		for(int i = 0; i < events_count; ++i)
		{
			if (events[i].data.fd == listener)
			{
			struct sockaddr_in addr;
			socklen_t addrlen;
				int client = accept(listener, (struct sockaddr *) &addr, &addrlen); 
				if( client == -1)
				{
				std::cout << "accept client: " << client << " errno: " << errno   << std::endl;
				exit(4);
				}
				//result = std::async(std::launch::async, processClient, client);
//				std::thread thread(processClient, client);
				boost::asio::post(*pool, [client]{ processClient(client);} );
				std::cout << "add to pool" << std::endl;
				
				std::cout << "back in events loop" << std::endl;
				//pool->join();
			} 

		} 
	}
	std::cout << "end events loop" << std::endl;
return 0;
} 

int main(void)
{
std::shared_ptr<boost::asio::thread_pool> pool = std::make_shared<boost::asio::thread_pool>(5);
		boost::asio::post(*pool, [pool]{ run_events(pool);});
		pool->join();
		std::cout << "end all threads" << std::endl;
}
	

