#pragma once
#include <vector>


template <class T>
struct MessageHeader
{
	T id{};
	size_t size = 0;
};


template <class T>
struct Message
{
	MessageHeader<T> header{};
	std::vector<uint8_t> body;


	size_t size() const
	{
		return body.size();
	}

	template<typename DataType>
	friend Message<T>& operator << (Message<T>& msg, const DataType& data)
	{
		static_assert(std::is_standard_layout<DataType>::value, "Data is too complex for a vector");

		size_t currentSize = msg.body.size();
		msg.body.resize(currentSize + sizeof(DataType));

		//Copy from the last index(that contains data). The vector could have data before the user uses this function,
		//there for we add the currentSize of the vector to msg.body.data() to tell memcpy that we want to copy from the last index
		//(usually the vector doesn't contain data so current size would be 0)
		std::memcpy(msg.body.data() + currentSize, &data, sizeof(DataType));

		//Update current size in the header
		msg.header.size = msg.size();

		return msg;
	}


	template<typename DataType>
	friend Message<T>& operator >> (Message<T>& msg, DataType& data)
	{
		static_assert(std::is_standard_layout<DataType>::value, "Data is too complex for a vector");

		size_t index = msg.body.size() - sizeof(DataType);
		std::memcpy(&data, msg.body.data() - index, sizeof(DataType));
		msg.body.resize(index);

		//Update current size in the header
		msg.header.size = msg.size();

		return msg;
	}

	void CreateStringMessage(const std::string& message)
	{
		this->body.resize(message.size());
		std::memcpy(this->body.data(), message.data(), message.size());

		this->header.size = this->size();
	}

	const std::string GetStringMessage()
	{
		std::string str(this->body.begin(), this->body.end());
		return str;
	}
};