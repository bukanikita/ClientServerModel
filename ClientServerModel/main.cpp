// I have decided to put everything to one file, because I don't see really any reason to create few files for such small programm and it will be easier to review


// This define is needed, because in task was two points:
// 1. Use exactly struct tagTDATA.
// 2. Make it portable to different OS.
// But, it is not portable, when we using tagTDATA, so, I made this switch
#define MAKE_IT_PORTABLE 1

#include <iostream>
#include <chrono>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <fstream>
#include <memory>
#include <atomic>
#if !MAKE_IT_PORTABLE
#include <wtypes.h>

#ifdef max
#undef max
#endif
#endif

using priorityType = unsigned char;
using clientIdType = unsigned long;

#if MAKE_IT_PORTABLE

class Message
{
public:
	friend std::ostream& operator<<(std::ostream& os, const Message& message);

	Message(clientIdType clientId, priorityType priority, const std::string &data) : m_clientId(clientId), m_priority(priority), m_data(data)
	{
		m_timeOfRequest = std::chrono::system_clock::now();
	}

	bool operator<(const Message &another) const
	{
		return std::make_pair(m_priority, m_timeOfRequest) < std::make_pair(another.m_priority, another.m_timeOfRequest);
	}
private:
	clientIdType m_clientId;
	priorityType m_priority;
	std::string m_data;
	decltype(std::chrono::system_clock::now()) m_timeOfRequest;
};

std::ostream& operator<<(std::ostream& os, const Message& message)
{
	// assume that for processing one request we need 3 seconds (to make it the most similar to real program).
	std::this_thread::sleep_for(std::chrono::seconds(3));
	os << std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) << " : "
		<< "Client id - " << message.m_clientId << "; "
		<< "Request Priority - " << static_cast<int>(message.m_priority) << "; "
		<< "Request time - " << std::chrono::system_clock::to_time_t(message.m_timeOfRequest) << "; "
		<< "Data - " << message.m_data << ";";
	return os;
}

#else

typedef struct tagTDATA
{
	friend std::ostream& operator<<(std::ostream& os, const tagTDATA& message);

	tagTDATA(DWORD clientId, BYTE priority, const std::string &data) : dwClientId(clientId), cPriority(priority)
	{
		dwTicks = GetTickCount();
		strcpy_s(Data, data.c_str());
	}

	bool operator<(const tagTDATA &another) const
	{
		return std::make_pair(cPriority, dwTicks) < std::make_pair(another.cPriority, another.dwTicks);
	}
	BYTE cPriority;
	DWORD dwTicks;
	DWORD dwClientId;
	char Data[255];
} TDATA, *PTDATA;

std::ostream& operator<<(std::ostream& os, const tagTDATA& message)
{
	// assume that for processing one request we need 3 seconds (to make it the most similar to real program).
	std::this_thread::sleep_for(std::chrono::seconds(3));
	os << GetTickCount() << " : "
		<< "Client id - " << message.dwClientId << "; "
		<< "Request Priority - " << static_cast<int>(message.cPriority) << "; "
		<< "Request time - " << message.dwTicks << "; "
		<< "Data - " << message.Data << ";";
	return os;
}

using Message = tagTDATA;

#endif

class ClientServerModel
{
public:
	ClientServerModel(clientIdType numberOfClients) :
		m_priorityDistribution(0, std::numeric_limits<priorityType>::max()),
		m_threadSleepDistribution(m_minSleepInterval, m_maxSleepInterval),
		m_numberOfClients(numberOfClients)
	{
		m_logFile.open("logFile.log");
		if (!m_logFile.is_open())
		{
			// we can throw exception here, because we did not acquire any resources and it is safe.
			throw std::exception("Log file can not be opened.");
		}
		m_active = true;
	}

	~ClientServerModel()
	{
		m_logFile.close();
		stop();
	}

	void start()
	{
		m_server = std::thread(&ClientServerModel::server, this);
		for (clientIdType i = 0; i < m_numberOfClients; ++i)
		{
			m_clients.emplace_back(&ClientServerModel::client, this, i);
		}
	}

	void client(int id)
	{
		while (m_active)
		{
			std::this_thread::sleep_for(std::chrono::seconds(m_threadSleepDistribution(m_randomEngine)));

			std::unique_lock<std::mutex> lck(m_queueMutex);

			m_queue.emplace(id, m_priorityDistribution(m_randomEngine), "Information from client " + std::to_string(id));

			m_cvQueue.notify_all();
		}
	}

	void server()
	{
		while (m_active)
		{
			std::unique_lock<std::mutex> lck(m_queueMutex);
			m_cvQueue.wait(lck, [&]()-> bool { return !m_active || !m_queue.empty(); });

			if (m_queue.empty())
			{
				continue;
			}

			const Message message = m_queue.top();
			m_queue.pop();
			lck.unlock();

			processMessage(message);
		}
	}

	void processMessage(const Message &message)
	{
		m_logFile << message << std::endl;
	}

	void stop()
	{
		if (m_active)
		{
			m_active = false;
			m_cvQueue.notify_all();

			m_server.join();

			for (std::thread &client : m_clients)
			{
				client.join();
			}
		}
	}

private:
	// min and max time of client sleep in seconds
	static const int m_minSleepInterval = 3;
	static const int m_maxSleepInterval = 20;

	std::ofstream m_logFile;

	std::priority_queue<Message> m_queue;
	std::mutex m_queueMutex;
	std::condition_variable m_cvQueue;

	std::default_random_engine m_randomEngine;
	std::uniform_int_distribution<> m_priorityDistribution;
	std::uniform_int_distribution<> m_threadSleepDistribution;

	std::vector<std::thread> m_clients;
	std::thread m_server;

	clientIdType m_numberOfClients;

	std::atomic<bool> m_active;
};

int main()
{
	std::cout << "Please, enter number of clients: ";
	clientIdType numberOfClients = 0;

	while (!(std::cin >> numberOfClients) || numberOfClients == 0)
	{
		std::cin.clear();
		std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		std::cout << "Please, enter valid number of clients: ";
	}
	std::cin.clear();
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	std::unique_ptr<ClientServerModel> clientServerModel;
	try
	{
		clientServerModel.reset(new ClientServerModel(numberOfClients));
	}
	catch (std::exception &e)
	{
		std::cout << e.what() << std::endl << "Finishing Program." << std::endl;
		return 1;
	}
	catch (...)
	{
		std::cout << "Something was wrong.\nFinishing Program." << std::endl;
		return 2;
	}

	clientServerModel->start();

	std::string command;
	while (true)
	{
		std::cout << "Client-server model started to run. To stop, please, enter \"stop\".\n";
		std::getline(std::cin, command);

		if (command == "stop")
		{
			std::cout << "Stopping...\n";
			clientServerModel->stop();
			std::cout << "Client-server model stopped.\n";
			break;
		}
		else
		{
			std::cout << "Wrong command.\n";
		}

	}

	return 0;
}