#pragma once
#include <asio.hpp>
#include <queue>
#include <iostream>
#include <optional>
#include <functional>
#include <unordered_set>
#include "TcpConnection.hpp"
#include "Message.hpp"
#include "OwnedMessage.hpp"

enum class Ipv
{
	V4, V6
};

using asio::ip::tcp;



template <class T>
class TcpServer
{
public:


	TcpServer(const std::string& ip, int port) : m_port(std::move(port)), m_acceptor(io_context)
	{
		try
		{
			asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(ip), m_port);
			m_acceptor.open(endpoint.protocol());
			m_acceptor.bind(endpoint);
			m_acceptor.listen();
		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << std::endl;
		}

		std::cout << "Listening to [" << m_acceptor.local_endpoint().address() << ", " << m_acceptor.local_endpoint().port() << "]" << std::endl;
	}

	~TcpServer()
	{
		Stop();
	}

	int Run()
	{
		try {
			std::cout << "Server is waiting to accept new clients..." << std::endl;
			StartAccept();

			m_threadContext = std::thread([this]() { io_context.run(); });
		}
		catch (std::exception& e) {
			std::cout << e.what() << std::endl;
			return -1;
		}

		return 0;
	}

	void Stop()
	{
		// Request the context to close
		io_context.stop();

		// Tidy up the context thread
		if (m_threadContext.joinable()) 
			m_threadContext.join();

		// Inform someone, anybody, if they care...
		std::cout << "[SERVER] Stopped!\n";
	}

	void MessageClient(std::shared_ptr<TcpConnection<T>> client, const Message<T>& message)
	{
		if (client && client->IsConnected())
		{
			client->Post(message);
		}
		else
		{
			OnClientDisconnect(client);

			client.reset();
			m_connections.erase(client);
		}
	}

	void MessageAllClients(const Message<T>& message, std::shared_ptr<TcpConnection<T>> ToIgnore = nullptr)
	{
		for (std::shared_ptr<TcpConnection<T>> connection : m_connections)
		{
			if (connection && connection->IsConnected())
			{
				if (connection != ToIgnore)
					connection->Post(message);
			}
			else
			{
				OnClientDisconnect(connection);

				connection.reset();
				m_connections.erase(connection);
			}
		}
	}

	void Update(size_t nMaxMessages = -1, bool bWait = false)
	{
		if (bWait) messagesIn.wait();

		// Process as many messages as you can up to the value
		// specified
		size_t nMessageCount = 0;
		while (nMessageCount < nMaxMessages && !messagesIn.empty())
		{
			// Grab the front message
			auto msg = messagesIn.pop_front();

			// Pass to message handler
			OnMessage(msg.connection, msg.message);

			nMessageCount++;
		}
	}

protected:

	virtual void OnMessage(std::shared_ptr<TcpConnection<T>> client, Message<T>& message)
	{
		MessageAllClients(message, client);
	}

	virtual bool OnClientConnect(std::shared_ptr<TcpConnection<T>> client)
	{
		return true;
	}

	// Called when a client appears to have disconnected
	virtual void OnClientDisconnect(std::shared_ptr<TcpConnection<T>> client)
	{

	}


private:


	void StartAccept()
	{
		m_acceptor.async_accept(
			[this](std::error_code ec, asio::ip::tcp::socket socket)
			{
				std::shared_ptr<TcpConnection<T>> connection = std::make_shared<TcpConnection<T>>(io_context, std::move(socket), messagesIn, nIDCounter++);
				
				if (!ec)
				{
					if (OnClientConnect(connection))
					{
						std::cout << "[" << connection->GetID()
							<< "] New connection has been established: " << connection->Socket().remote_endpoint() << std::endl;;
						m_connections.insert(connection);
						connection->Start();
					}
					else
					{
						std::cout << "Connection denied..." << std::endl;
					}
				}
				else
				{
					std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
				}

				StartAccept();
			}
		);
	}

private:

	int m_port;
	Ipv ip_version;

	tsqueue<owned_message<T>> messagesIn;
	std::thread m_threadContext;

	asio::io_context io_context;
	//tcp::socket m_socket;

	tcp::acceptor m_acceptor;
	std::unordered_set<std::shared_ptr<TcpConnection<T>>> m_connections;

	uint32_t nIDCounter = 1000;
};