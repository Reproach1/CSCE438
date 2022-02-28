#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
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
using csce438::SNSService;

std::vector<std::string> users;

class SNSServiceImpl final : public SNSService::Service {
    
    Status List(ServerContext* context, const Request* request, Reply* reply) override {
        // ------------------------------------------------------------
        // In this function, you are to write code that handles 
        // LIST request from the user. Ensure that both the fields
        // all_users & following_users are populated
        // ------------------------------------------------------------
        
        std::string username = request->username();
        std::string following, user;
        
        std::ifstream file1("Database/Following/"+username+".txt");
        
        if(!file1.is_open()) {
            std::cout << "shits fucked" << std::endl;
        }
        
        reply->add_following_users(username);
        
        while (std::getline(file1, following)) {
            reply->add_following_users(following.substr(0, following.size()));
        }
        
        file1.close();
        
        std::ifstream file2("Database/all_users.txt");
        
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
        std::string username = request->username();
        std::string existing_user;
        
        std::ifstream ifile("Database/all_users.txt");
        
        bool user_exists = false;
        while(std::getline(ifile, existing_user)) {
            if (user == existing_user) {
                user_exists = true;
                break;
            }
        }
        
        ifile.close();
        
        if (user_exists) {
            ifile = std::ifstream("Database/Following/" + username + ".txt");
            std::vector<std::string> following;
            
            while (std::getline(ifile, existing_user)) {
                following.push_back(existing_user);
            }
            
            ifile.close();
            
            std::ofstream ofile("Database/Following/" + username + ".txt");
            
            for (int i = 0; i < following.size(); ++i) {
                ofile << following.at(i) << "\n";
            }
            
            ofile << user << "\n";
            
            ofile.close();
            
            reply->set_msg("SUCCESS");
            
        }
        else {
            reply->set_msg("FAILURE_INVALID_USERNAME");
        }
        
        
        return Status::OK; 
    }
    
    Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
        // ------------------------------------------------------------
        // In this function, you are to write code that handles 
        // request from a user to unfollow one of his/her existing
        // followers
        // ------------------------------------------------------------
        
        std::string username = request->username();
        std::string user = request->arguments(0);
        std::string existing_user;
        
        std::ifstream ifile = std::ifstream("Database/Following/" + username + ".txt");
        std::vector<std::string> following;
        
        while (std::getline(ifile, existing_user)) {
            following.push_back(existing_user);
        }
        
        ifile.close();
        
        std::ofstream ofile("Database/Following/" + username + ".txt");
        
        bool exists = false;
        for (int i = 0; i < following.size(); ++i) {
            if (following.at(i) != user) {
                ofile << following.at(i) << "\n";
            }
            else {
                exists = true;
            }
        }
        
        ofile.close();
        
        if (exists) {
            reply->set_msg("SUCCESS");
        }
        else {
            reply->set_msg("FAILURE_INVALID_USERNAME");
        }
        
        return Status::OK;
    }
    
    Status Login(ServerContext* context, const Request* request, Reply* reply) override {
        // ------------------------------------------------------------
        // In this function, you are to write code that handles 
        // a new user and verify if the username is available
        // or already taken
        // ------------------------------------------------------------
        
        std::string username = request->username();
        
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
        
        std::ifstream ifile("Database/all_users.txt");
        
        std::string user;
        std::vector<std::string> all_users_vec;
        
        while(std::getline(ifile, user)) {
            all_users_vec.push_back(user);
        }
        
        ifile.close();
        
        std::ofstream ofile("Database/all_users.txt");
        
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
        
        std::ofstream newfile("Database/Following/"+username+".txt");
        newfile.close();
        
        reply->set_msg("SUCCESS");
        return Status::OK;
        
        
    }
    
    Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
        // ------------------------------------------------------------
        // In this function, you are to write code that handles 
        // receiving a message/post from a user, recording it in a file
        // and then making it available on his/her follower's streams
        // ------------------------------------------------------------
        Message message;
        
        while (stream->Read(&message)) {
            std::cout << "something happened";
            std::cout << message.msg() << std::endl;
        }
        return Status::OK;
    }

};

void RunServer(std::string port_no) {
    // ------------------------------------------------------------
    // In this function, you are to write code 
    // which would start the server, make it listen on a particular
    // port number.
    // ------------------------------------------------------------
    std::string server_address = "0.0.0.0:";
    server_address.append(port_no);
    SNSServiceImpl service;
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<Server> server(builder.BuildAndStart());
    server->Wait();
}

int main(int argc, char** argv) {
  
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "p:")) != -1){
        switch(opt) {
            case 'p':
                port = optarg;
                break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }
    RunServer(port);
    return 0;
}
