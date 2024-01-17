#pragma once
#include "Message.hpp"
#include "TcpConnection.hpp"


template <class T>
struct TcpConnection;

template <class T>
struct owned_message
{
	Message<T> message;
	std::shared_ptr<TcpConnection<T>> connection;
};
