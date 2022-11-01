// chat_server.cpp
// ~~~~~~~~~~~~~~~
//借鉴了asio库的官网demo
// Copyright (c) 2003-2018 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma warning (disable : 4996)
#define ASIO_STANDALONE
#ifndef CHAT_SERVER
#define CHAT_SERVER
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include "asio.hpp"
#include "chat_message.hpp"
#include "GameMessageWrap.h"
using asio::ip::tcp;

//----------------------------------------------------------------------

typedef std::deque<chat_message> chat_message_queue;

//----------------------------------------------------------------------
class chat_server;

/*
*   @brief  tcp�˿ڹ�����
*           ���𴢴�ͻ��˶˿ں�, ͬ��д��Ϣ����Ϣ
*/
class TcpConnection
	: public std::enable_shared_from_this<TcpConnection>
{
public:
	typedef std::shared_ptr<TcpConnection> pointer;
	
	/**
	* @brief                        �ӷ���˷���һ���ͻ���ָ��
	*
	* @param io_service             ���ͻ��˷���һ���������
	* @param parent                 ���صĿͻ������Է���˶���
	*/
	static pointer create(asio::io_service& io_service, chat_server* parent);
	
	/**
	* @brief                        �����˿�ʹ��Ȩ
	*/
	tcp::socket& socket();

	/**
	* @brief                        �ӷ���˵õ����׽���
	*
	* @param socket                 �Ѷ˿ںŴ�serverת��tcpconnection
	*/
	void get_socket(tcp::socket);

	/**
	* @brief                        ��ʼ��ȡ����
	*/
	void start();
	
	/**
	* @brief                        д���ݸ������
	*/
	void write_data(std::string s);

	/**
	* @brief                        ��ȡ���ݵľ������
	*
	*/
	std::string read_data();

	/**
	* @brief                        ȷ����û�г���
	*
	*/
	bool error()const { return error_flag_; }

	/**
	* @brief                        �رս��̺Ͷ˿ں�
	*
	*/
	void do_close();
private:
	
	/**
	* @brief                        ������Ϣ��������
	*
	*/
	void handle_read_header(const asio::error_code& error);
	
	/**
	* @brief                        ������Ϣ��������
	*
	*/
	void handle_read_body(const asio::error_code& error);

	/**
	* @brief                        ����һ��tcp����
	*
	* @param io_service             ���ͻ��˷���һ���������
	* @param parent                 ���صĿͻ������Է���˶���
	*/
	TcpConnection(asio::io_service& io_service, chat_server * parent);;

	tcp::socket socket_;             //�ѽ������ӵ��׽��ּ�¼����

	chat_server* parent;             //�����ָ��

	bool error_flag_{ false };       //�����־

	chat_message read_msg_;          //��ǰ��ȡ��Ϣ
	std::deque<chat_message> read_msg_deque_; //��ȡ��ϢҪ�������������
	std::condition_variable data_cond_;  //��������
	std::mutex mut_;                 //������
	//	asio::steady_timer steady_timer_;

};
/**
* @brief                        �����
*
*/
class chat_server
{
public:
	/**
	* @brief                    ��ͼ��
	*
	*/
	int map = 2;
	
	/**
	* @brief                        ��������һ���������
	*
	* @param io_service             ������˷���һ���������
	* @param endpoint               ���ܵĶ˿ں�
	*/
	chat_server(int port) :
		acceptor_(*io_service_, tcp::endpoint(tcp::v4(), port))
	{
		do_accept();
	}

	/**
	* @brief                      ����һ�������ָ��
	*
	* @param port                 ��ȡ�Ķ˿ں�
	*/
	static chat_server *create(int port = 1024) {
		auto s = new chat_server(port);
		s->thread_ = new std::thread(
			std::bind(static_cast<std::size_t(asio::io_service::*)()>(&asio::io_service::run),
				io_service_));
	
		return s;
	}
	
	/**
	* @brief                        ���͸��ͻ��˵���ҳ�ʼ��Ϣ
	*
	*/
	void button_start()
	{
		acceptor_.close();
		using namespace std; // For sprintf and memcpy.
		char total[4 + 1] = "";
		sprintf(total, "%4d", static_cast<int>(connections_.size()));
		char camp[4 + 1] = "";
		for (auto i = 0; i < connections_.size(); i++) {
			sprintf(camp, "%4d", i + 1);
			connections_[i]->write_data("PLAYER" + std::string(total) + std::string(camp) + std::to_string(map));
		}
		this->button_thread_ = new std::thread(std::bind(&chat_server::loop_process, this));
		button_thread_->detach();
	}
	int connection_num() const
	{
		return connections_.size();
	}
private:
	/**
	* @brief                        �첽���Խ������Կͻ��˵Ľ�������
	*
	*/
	void do_accept()
	{
		acceptor_.async_accept(
			[this](std::error_code ec, tcp::socket socket)
		{
			if (!ec)
			{
				TcpConnection::pointer new_connection = TcpConnection::create(acceptor_.get_io_context(), this);
				connections_.push_back(new_connection);
				auto ep_ = socket.remote_endpoint();
				new_connection->get_socket(std::move(socket));
				std::cout << "client : " << ep_.port() << " enter this room" << std::endl;
				new_connection->start();
				//std::make_shared<TcpConnection>(std::move(socket))->start();
			}
			do_accept();
		});
	}
	
	/**
	* @brief                        ���ͻ���ͬ��������Ϣ
	*
	*/
	void loop_process()
	{
		while (true)
		{
			std::unique_lock<std::mutex> lock(delete_mutex_);
			std::vector<std::string> ret;
			for (auto r : connections_)
			{
				if (r->error())
					//				break;
					error_flag_ |= r->error();
				ret.push_back(r->read_data());
			}
			auto game_msg = GameMessageWrap::combine_message(ret);

			for (auto r : connections_)
				r->write_data(game_msg);
		}
	}
	std::vector<TcpConnection::pointer> connections_;           //���tcp���ӵ�����
	static asio::io_service * io_service_;                      //������˵�һ��
	tcp::acceptor acceptor_;                                    //������
	std::thread *thread_, *button_thread_{ nullptr };           //��ʼ��Ϸ��Ϣд��Ľ���
	std::mutex delete_mutex_;                                   //��
	bool error_flag_{ false };                                  //�����־
	std::condition_variable data_cond_;                         //��������
	//chat_room room_;
};
#endif
//----------------------------------------------------------------------
