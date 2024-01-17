#pragma once
#include <asio.hpp>
#include <iostream>
#include <functional>
#include <queue>
#include "ThreadSafeQueue.hpp"
#include "Message.hpp"
#include "OwnedMessage.hpp"
using asio::ip::tcp;


template <class T>
class TcpConnection : public std::enable_shared_from_this<TcpConnection<T>>
{

public:


	explicit TcpConnection(asio::io_context& asioIoContext, tcp::socket sock, tsqueue<owned_message<T>>& qIn, std::uint32_t client_id)
		: ioContext(asioIoContext), m_socket(std::move(sock)), messagesInQueue(qIn), id(client_id)
	{}

	virtual ~TcpConnection()
	{}

	void Start()
	{
		ReadHeader();
	}

	void Post(const Message<T>& msg)
	{
		asio::post(ioContext,
			[this, msg]()
			{
				bool notWritingMessage = messagesOutQueue.empty();
				messagesOutQueue.push_back(msg);
				if (notWritingMessage)
				{
					WriteHeader();
				}
			}
		);
	}

	std::uint32_t GetID()
	{
		return id;
	}

	tcp::socket& Socket()
	{
		return m_socket;
	}

	bool IsConnected() const
	{
		return m_socket.is_open();
	}

	void Disconnect()
	{
		if (IsConnected())
			asio::post(ioContext, [this]() { m_socket.close(); });
	}

private:


	void ReadHeader()
	{
		asio::async_read(m_socket, asio::buffer(&temp_messageIn.header, sizeof(MessageHeader<T>)),
			[this](asio::error_code ec, size_t len)
			{
				if (!ec)
				{
					if (temp_messageIn.header.size > 0)
					{
						//Resize to the new size
						temp_messageIn.body.resize(temp_messageIn.header.size);
						//Read the body
						ReadBody();
					}
					else
					{
						//Should add to the messageIn queue
						AddMessageToIncomingMessageQueue();
					}
				}
				else
				{
					std::cout << "[" << this->GetID() << "] ReadHeader failed! closing the connection..." << std::endl;
					m_socket.close();
				}
			}
		);
	}

	void ReadBody()
	{
		asio::async_read(m_socket, asio::buffer(temp_messageIn.body.data(), temp_messageIn.body.size()),
			[this](const asio::error_code ec, size_t len)
			{
				if (!ec)
				{
					//Add message to incoming message queue
					AddMessageToIncomingMessageQueue();
				}
				else
				{
					std::cout << "[" << this->GetID() << "] ReadBody failed! closing the connection..." << std::endl;
					m_socket.close();
				}
			}
		);
	}


	void WriteHeader()
	{
		asio::async_write(m_socket, asio::buffer(&messagesOutQueue.front().header, sizeof(MessageHeader<T>)),
			[this](const asio::error_code ec, size_t len)
			{
				if (!ec)
				{
					if (messagesOutQueue.front().body.size() > 0)
					{
						WriteBody();
					}
					else
					{
						messagesOutQueue.pop_front();

						if (!messagesOutQueue.empty())
						{
							WriteHeader();
						}
					}
				}
				else
				{
					std::cout << "[" << this->GetID() << "] WriteHeader failed! closing the connection..." << std::endl;
					m_socket.close();
				}
			}
		);
	}


	void WriteBody()
	{
		asio::async_write(m_socket, asio::buffer(messagesOutQueue.front().body.data(), messagesOutQueue.front().body.size()),
			[this](asio::error_code ec, size_t bytesTransferred)
			{
				if (!ec)
				{
					messagesOutQueue.pop_front();

					if (!messagesOutQueue.empty())
					{
						WriteHeader();
					}
				}
				else
				{
					std::cout << "[" << this->GetID() << "] WriteBody failed! closing the connection..." << std::endl;
					m_socket.close();
				}
			}
		); 
	}


	void AddMessageToIncomingMessageQueue()
	{
		//owned_message newMessage{ temp_messageIn, this->shared_from_this() };
		messagesInQueue.push_back({ temp_messageIn, this->shared_from_this() });

		ReadHeader();
	}

private:

	asio::io_context& ioContext;
	tcp::socket m_socket;

	uint32_t id = 0;

	Message<T> temp_messageIn;
	tsqueue<Message<T>> messagesOutQueue;
	tsqueue<owned_message<T>>& messagesInQueue;
};