/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::Heartbeat;
using csce438::ClusterInfo;
using csce438::ServerInfo;
using csce438::SNSService;

std::unique_ptr<SNSService::Stub> stub_;
std::unique_ptr<SNSService::Stub> slave_stub_;

std::string database;
std::string type;

//Vector that stores every client that has been created
std::vector<std::string> users;

void SendHeartbeat(std::string id) {
  
  while (1) {
    ClientContext context;
    Heartbeat heartbeat;
    Reply reply;
  
    heartbeat.set_id(id);
    heartbeat.set_type(type);
    
    stub_->Ping(&context, heartbeat, &reply);
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
  }
}

void SlaveLogin(std::string id) {
  ClientContext context;
  Request request;
  Reply reply;
  
  request.set_id(id);
  slave_stub_->Login(&context, request, &reply);
}

void SlaveFollow(const Request* request, Reply* reply) {
  ClientContext context;
  slave_stub_->Follow(&context, *request, reply);
}

void SlaveTimeline(std::string id, std::vector<std::string> lines) {
  ClientContext context;
  Request request;
  Reply reply;
  
  request.set_id(id);
  for (std::string line : lines) {
    request.add_arguments(line);
  }
  
  slave_stub_->UpdateTimeline(&context, request, &reply);
}

class SNSServiceImpl final : public SNSService::Service {
  
  Status List(ServerContext* context, const Request* request, Reply* reply) override {
        // ------------------------------------------------------------
        // In this function, you are to write code that handles 
        // LIST request from the user. Ensure that both the fields
        // all_users & following_users are populated
        // ------------------------------------------------------------
        
        std::string username = request->id();
        std::string follower, user;
        
        std::ifstream file1(database + "/Followers/"+username+"_followers.txt");
        
        if(!file1.is_open()) {
            std::cout << "shits fucked" << std::endl;
        }
        
        reply->add_followers(username);
        
        while (std::getline(file1, follower)) {
            reply->add_followers(follower.substr(0, follower.size()));
        }
        
        file1.close();
        
        std::ifstream file2(database+"/all_users.txt");
        
        if(!file2.is_open()) {
            std::cout << "shits fucked" << std::endl;
        }
        
        while(std::getline(file2, user)) {
            reply->add_all_users(user.substr(0, user.size()));
        }
        
        file2.close();
        
        reply->set_msg("SUCCESS");
        
        return Status::OK;
    }
    
  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
      // ------------------------------------------------------------
      // In this function, you are to write code that handles 
      // request from a user to follow one of the existing
      // users
      // ------------------------------------------------------------
      
      std::string user = request->arguments(0);
      std::string username = request->id();
      std::string existing_user;
      
      std::ifstream ifile = std::ifstream(database+"/Followers/" + user + "_followers.txt");
      
      if (ifile.is_open() && username != user) {
        
          std::vector<std::string> followers;
          
          bool already_following = false;
          while (std::getline(ifile, existing_user)) {
              if (existing_user == username) {
                  already_following = true;
                  break;
              } 
              followers.push_back(existing_user);
          }
          ifile.close();
          
          if (!already_following) {
              std::ofstream ofile(database+"/Followers/" + user + "_followers.txt");
              
              for (int i = 0; i < followers.size(); ++i) {
                  ofile << followers.at(i) << "\n";
              }
              
              ofile << username << "\n";
              
              ofile.close();
              
              reply->set_msg("SUCCESS");
          }
          else {
              reply->set_msg("FAILURE_ALREADY_EXISTS");
          }
          
      }
      else {
          reply->set_msg("FAILURE_INVALID_USERNAME");
      }
      
      if (type == "master") {
        SlaveFollow(request, reply);
      }
      
      return Status::OK; 
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
      // ------------------------------------------------------------
      // In this function, you are to write code that handles 
      // a new user and verify if the username is available
      // or already taken
      // ------------------------------------------------------------
      
      std::string username = request->id();
      
      if (users.empty()) {
          users.push_back(username);
      }
      else {
        for (int i = 0; i < users.size(); ++i) {
          if (users.at(i) == username) {
            reply->set_msg("FAILURE_ALREADY_EXISTS");
            return Status::OK;
          }
        }
      }
      
      std::ifstream ifile(database+"/all_users.txt");
      
      std::string user;
      std::vector<std::string> all_users_vec;
      
      while(std::getline(ifile, user)) {
        all_users_vec.push_back(user);
      }
      
      ifile.close();
      
      std::ofstream ofile(database+"/all_users.txt");
      
      bool already_exists = false;
      for (int i = 0; i < all_users_vec.size(); ++i) {
          if (all_users_vec.at(i) == username) {
              already_exists = true;
          }
          ofile << all_users_vec.at(i) << "\n";
      }
      
      if (!already_exists) {
          ofile << username << "\n";
      }
      
      ofile.close();
      
      std::ofstream newfile;
      
      ifile = std::ifstream(database+"/Followers/"+username+"_followers.txt");
      if (!ifile.is_open()) {
        newfile = std::ofstream(database+"/Followers/"+username+"_followers.txt");
        newfile.close();
      }
      ifile.close();
      
      ifile = std::ifstream(database+"/Timelines/"+username+"_timeline.txt");
      if (!ifile.is_open()) {
        newfile = std::ofstream(database+"/Timelines/"+username+"_timeline.txt");
        newfile.close();
      }
      ifile.close();
      
      reply->set_msg("SUCCESS");
      
      if (type == "master") {
        SlaveLogin(username);
      }
      
      return Status::OK;
      
      
  }
  
  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    Message setup_message;
    stream->Read(&setup_message);
    
    std::string username = setup_message.id();
    
    std::thread readThread([stream, username]() {
      Message read_message;
      std::ifstream followers_ifile, self_ifile, fol_timeline_ifile;
      std::ofstream self_ofile, fol_timeline_ofile;
      
      std::string user, line;
      std::vector<std::string> followers;
      std::vector<std::string> messages;
      
      while (stream->Read(&read_message)) {
        std::string time_str = google::protobuf::util::TimeUtil::ToString(read_message.timestamp());
        
        followers_ifile = std::ifstream(database+"/Followers/"+username+"_followers.txt");
        while (std::getline(followers_ifile, user)) {
          user = user.substr(0);
          messages.push_back(username + "(" + time_str + ")" + read_message.msg());
          
          fol_timeline_ifile = std::ifstream(database+"/Timelines/"+user+"_timeline.txt");
          while(std::getline(fol_timeline_ifile, line)) {
            messages.push_back(line);
            if (messages.size() == 20) {
              break;
            }
          }
          fol_timeline_ifile.close();
          
          fol_timeline_ofile = std::ofstream(database+"/Timelines/"+user+"_timeline.txt");
          for (std::string m : messages) {
            if (m.back() != '\n') {
              m.push_back('\n');
            }
            fol_timeline_ofile << m;
          }
          fol_timeline_ofile.close();
          
          if (type == "master") {
            SlaveTimeline(user, messages);
          }
          
          messages.clear();
        }
        followers_ifile.close();
        
        messages.clear();
        
        messages.push_back(username + "(" + time_str + ")" + read_message.msg());
        
        self_ifile = std::ifstream(database+"/Timelines/"+username+"_timeline.txt");
        while(std::getline(self_ifile, line)) {
          messages.push_back(line);
          if (messages.size() == 20) {
            break;
          }
        }
        self_ifile.close();
        
        self_ofile = std::ofstream(database+"/Timelines/"+username+"_timeline.txt");
        for (std::string m : messages) {
          if (m.back() != '\n') {
            m.push_back('\n');
          }
          self_ofile << m;
        }
        self_ofile.close();
        
        if (type == "master") {
          SlaveTimeline(username, messages);
        }
        
        messages.clear();
      }
    });
    
    // read history
    Message write_message;
    std::vector<std::string> history_vec;
    std::string message_str;
    
    std::ifstream ifile(database+"/Timelines/"+username+"_timeline.txt");
    
    std::string prev_msg = "";
    Timestamp timestamp;
    
    while (std::getline(ifile, message_str)) {
      if (prev_msg == "") {
        prev_msg = message_str;
      }
      
      write_message.set_id(message_str.substr(0, message_str.find_first_of('(')));
     
      message_str = message_str.substr(message_str.find_first_of('(') + 1, message_str.size());
      write_message.set_msg(message_str.substr(message_str.find_first_of(')') + 1, message_str.size()));
      
      message_str = message_str.substr(0, message_str.find_first_of(')'));
      google::protobuf::util::TimeUtil::FromString(message_str, &timestamp);
      write_message.set_allocated_timestamp(&timestamp);
      
      stream->Write(write_message);
      
      write_message.release_timestamp();
    }
    ifile.close();

    std::string new_message;
    while (1) {
      ifile = std::ifstream(database+"/Timelines/"+username+"_timeline.txt");
      while (std::getline(ifile, new_message)) {
        if (new_message != prev_msg) {
          history_vec.push_back(new_message);
        }
        else {
          break;
        }
      }
      ifile.close();
      
      if (!history_vec.empty()) {
        prev_msg = history_vec.at(0);
        for (int i = 0; i < history_vec.size(); ++i) {
          message_str = history_vec.at(i);
          if (username != message_str.substr(0, message_str.find_first_of('('))) {
            write_message.set_id(message_str.substr(0, message_str.find_first_of('(')));
            
            message_str = message_str.substr(message_str.find_first_of('(') + 1, message_str.size());
            write_message.set_msg(message_str.substr(message_str.find_first_of(')') + 1, message_str.size()));
            
            message_str = message_str.substr(0, message_str.find_first_of(')'));
            google::protobuf::util::TimeUtil::FromString(message_str, &timestamp);
            write_message.set_allocated_timestamp(&timestamp);
            
            stream->Write(write_message);
            
            write_message.release_timestamp();
          }
        }
      }
      history_vec.clear();
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    return Status::OK;
  }
  
  Status UpdateTimeline(ServerContext *context, const Request* request, Reply* reply) override {
    std::ofstream file = std::ofstream(database+"/Timelines/"+request->id()+"_timeline.txt");
    
    for (std::string line : request->arguments()) {
      if (line.back() != '\n') {
        line.push_back('\n');
      }
      file << line;
    }
    
    file.close();
    
    return Status::OK;
  }
  
  Status ConnectSlave(ServerContext* context, const ServerInfo* server_info, ClusterInfo* cluster_info) override {
    std::cout << database << " connecting slave " << server_info->port() << std::endl;
    slave_stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel("localhost:" + server_info->port(), grpc::InsecureChannelCredentials())));
    std::cout << database << " connected slave" << std::endl;
    return Status::OK;
  }
  
  /*
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    Client user = client_db[find_user(request->id())];
    int index = 0;
    for(Client c : client_db){
      list_reply->add_all_users(c.username);
    }
    std::vector<Client*>::const_iterator it;
    for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
      list_reply->add_followers((*it)->username);
    }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    std::string username1 = request->id();
    std::string username2 = request->arguments(0);
    int join_index = find_user(username2);
    if(join_index < 0 || username1 == username2)
      reply->set_msg("Join Failed -- Invalid Username");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[join_index];
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end()){
	reply->set_msg("Join Failed -- Already Following User");
        return Status::OK;
      }
      user1->client_following.push_back(user2);
      user2->client_followers.push_back(user1);
      reply->set_msg("Join Successful");
    }
    return Status::OK; 
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    Client c;
    std::string username = request->id();
    
    std::cout << "logging in " << username << std::endl;
    
    int user_index = find_user(username);
    if(user_index < 0){
      c.username = username;
      client_db.push_back(c);
      reply->set_msg("Login Successful!");
    }
    else{ 
      Client *user = &client_db[user_index];
      if(user->connected)
        reply->set_msg("Invalid Username");
      else{
        std::string msg = "Welcome Back " + user->username;
	      reply->set_msg(msg);
        user->connected = true;
      }
    }
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    Message message;
    Client *c;
    while(stream->Read(&message)) {
      std::string username = message.id();
      int user_index = find_user(username);
      c = &client_db[user_index];
 
      //Write the current message to "username.txt"
      std::string filename = username+".txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      google::protobuf::Timestamp temptime = message.timestamp();
      std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
      std::string fileinput = time+" :: "+message.id()+":"+message.msg()+"\n";
      //"Set Stream" is the default message from the client to initialize the stream
      if(message.msg() != "Set Stream")
        user_file << fileinput;
      //If message = "Set Stream", print the first 20 chats from the people you follow
      else{
        if(c->stream==0)
      	  c->stream = stream;
        std::string line;
        std::vector<std::string> newest_twenty;
        std::ifstream in(username+"following.txt");
        int count = 0;
        //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
        while(getline(in, line)){
          if(c->following_file_size > 20){
	          if(count < c->following_file_size-20){
              count++;
	            continue;
            }
          }
          newest_twenty.push_back(line);
        }
        Message new_msg; 
 	      //Send the newest messages to the client to be displayed
	      for(int i = 0; i<newest_twenty.size(); i++){
	        new_msg.set_msg(newest_twenty[i]);
          stream->Write(new_msg);
        }    
        continue;
      }
      //Send the message to each follower's stream
      std::vector<Client*>::const_iterator it;
      for(it = c->client_followers.begin(); it!=c->client_followers.end(); it++){
        Client *temp_client = *it;
      	if(temp_client->stream!=0 && temp_client->connected)
	        temp_client->stream->Write(message);
        //For each of the current user's followers, put the message in their following.txt file
        std::string temp_username = temp_client->username;
        std::string temp_file = temp_username + "following.txt";
	      std::ofstream following_file(temp_file,std::ios::app|std::ios::out|std::ios::in);
	      following_file << fileinput;
        temp_client->following_file_size++;
	      std::ofstream user_file(temp_username + ".txt",std::ios::app|std::ios::out|std::ios::in);
        user_file << fileinput;
      }
    }
    //If the client disconnected from Chat Mode, set connected to false
    c->connected = false;
    return Status::OK;
  }
  */
};

void RunServer(std::string port_no) {
  std::string server_address = "localhost:"+port_no;
  
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

void RegisterWithCoordinator(std::string port, std::string cip, std::string cp, std::string id) {
  std::string coord_info = cip + ":" + cp;
  stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(coord_info, grpc::InsecureChannelCredentials())));
  
  ClientContext context;
  ServerInfo server_info;
  ClusterInfo cluster_info;
  
  server_info.set_id(id);
  server_info.set_type(type);
  server_info.set_port(port);
  
  stub_->RegisterServer(&context, server_info, &cluster_info);
  
  if (cluster_info.port() != "") {
    slave_stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel("localhost:" + cluster_info.port(), grpc::InsecureChannelCredentials())));
  }
  
}

int main(int argc, char** argv) {
  
  std::string port = "8010";
  std::string cip = "localhost";
  std::string cp = "8000";
  std::string id = "1";
  
  for (int i = 1; i < argc; i += 2) {
    if (strcmp(argv[i], "-cip") == 0) {
        cip = argv[i+1];
    } else if (strcmp(argv[i], "-cp") == 0) {
        cp = argv[i+1];
    } else if (strcmp(argv[i], "-p") == 0) {
        port = argv[i+1];
    } else if (strcmp(argv[i], "-id") == 0) {
        id = argv[i+1];
    } else if (strcmp(argv[i], "-t") == 0) {
        type = argv[i+1];
    }
  }
  
  RegisterWithCoordinator(port, cip, cp, id);
  
  database = type + "_" + id;
  mkdir(database.c_str(), 0777);
  mkdir((database + "/Followers").c_str(), 0777);
  mkdir((database + "/Timelines").c_str(), 0777);
  
  std::ofstream(database+"/all_users.txt");
  
  std::thread thread(SendHeartbeat, id);
  
  RunServer(port);
  
  thread.join();
  
  return 0;
}
