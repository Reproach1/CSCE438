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
#include <thread>
#include <chrono>
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
        
        std::ifstream file1("Database/Following/"+username+"_following.txt");
        
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
        
        std::ifstream ifile = std::ifstream("Database/Following/" + user + "_following.txt");
        
        if (ifile.is_open() || username == user) {
            ifile.close();
            
            ifile = std::ifstream("Database/Following/" + username + "_following.txt");
            std::vector<std::string> following;
            
            while (std::getline(ifile, existing_user)) {
                following.push_back(existing_user);
            }
            
            ifile.close();
            
            std::ofstream ofile("Database/Following/" + username + "_following.txt");
            
            for (int i = 0; i < following.size(); ++i) {
                ofile << following.at(i) << "\n";
            }
            
            ofile << user << "\n";
            
            ofile.close();
            
            ifile = std::ifstream("Database/Followers/" + user + "_followers.txt");
            std::vector<std::string> followers;
            
            while (std::getline(ifile, existing_user)) {
                followers.push_back(existing_user);
            }
            
            ifile.close();
            
            ofile = std::ofstream("Database/Followers/" + user + "_followers.txt");
            
            for (int i = 0; i < followers.size(); ++i) {
                ofile << followers.at(i) << "\n";
            }
            
            ofile << username << "\n";
            
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
        
        std::ifstream ifile = std::ifstream("Database/Following/" + username + "_following.txt");
        std::vector<std::string> following;
        
        bool exists = false;
        while (std::getline(ifile, existing_user)) {
            if (existing_user != user) {
                following.push_back(existing_user);
            }
            else {
                exists = true;
            }
        }
        
        ifile.close();
        
        if (exists) {
            
            std::ofstream ofile("Database/Following/" + username + "_following.txt");
        
            for (int i = 0; i < following.size(); ++i) {
                ofile << following.at(i) << "\n";
            }
            ofile.close();
            
            ifile = std::ifstream("Database/Followers/" + user + "_followers.txt");
            std::vector<std::string> followers;
            
            while (std::getline(ifile, existing_user)) {
                followers.push_back(existing_user);
            }
            
            ifile.close();
            
            ofile = std::ofstream("Database/Followers/" + user + "_followers.txt");
            
            for (int i = 0; i < followers.size(); ++i) {
                if (followers.at(i) != username) {
                    ofile << followers.at(i) << "\n";
                }
            }
            
            ofile.close();
            
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
        
        std::ofstream newfile;
        
        ifile = std::ifstream("Database/Following/"+username+"_following.txt");
        if (!ifile.is_open()) {
            newfile = std::ofstream("Database/Following/"+username+"_following.txt");
            newfile.close();
        }
        ifile.close();
        
        ifile = std::ifstream("Database/Followers/"+username+"_followers.txt");
        if (!ifile.is_open()) {
            newfile = std::ofstream("Database/Followers/"+username+"_followers.txt");
            newfile.close();
        }
        ifile.close();
        
        ifile = std::ifstream("Database/Timelines/"+username+"_timeline.txt");
        if (!ifile.is_open()) {
            newfile = std::ofstream("Database/Timelines/"+username+"_timeline.txt");
            newfile.close();
        }
        ifile.close();
        
        reply->set_msg("SUCCESS");
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
        
        std::string username = setup_message.username();
        
        std::thread readThread([stream, username]() {
            Message read_message;
            std::ifstream followers_ifile, self_ifile, fol_timeline_ifile;
            std::ofstream self_ofile, fol_timeline_ofile;
            
            std::string user, line;
            std::vector<std::string> followers;
            std::vector<std::string> messages;
            
            while (stream->Read(&read_message)) {
                std::string time_str = google::protobuf::util::TimeUtil::ToString(read_message.timestamp());
                
                followers_ifile = std::ifstream("Database/Followers/"+username+"_followers.txt");
                while (std::getline(followers_ifile, user)) {
                    user = user.substr(0);
                    fol_timeline_ifile = std::ifstream("Database/Timelines/"+user+"_timeline.txt");
                    while(std::getline(fol_timeline_ifile, line)) {
                        messages.push_back(line);
                    }
                    fol_timeline_ifile.close();
                    
                    fol_timeline_ofile = std::ofstream("Database/Timelines/"+user+"_timeline.txt");
                    fol_timeline_ofile << username << "(" << time_str << ")" << read_message.msg();
                    
                    if (!messages.empty()) {
                        if (messages.size() == 20) {
                            messages.erase(messages.begin()+19);
                        }
                        for (int i = 0; i < messages.size(); ++i) {
                            fol_timeline_ofile << messages.at(i) << "\n";
                        }
                    }
                    fol_timeline_ofile.close();
                }
                followers_ifile.close();
                
                messages.clear();
                self_ifile = std::ifstream("Database/Timelines/"+username+"_timeline.txt");
                while(std::getline(self_ifile, line)) {
                    messages.push_back(line);
                }
                self_ifile.close();
                
                self_ofile = std::ofstream("Database/Timelines/"+username+"_timeline.txt");
                self_ofile << username << "(" << time_str << ")" << read_message.msg();
                
                if (!messages.empty()) {
                    if (messages.size() == 20) {
                        messages.erase(messages.begin()+19);
                    }
                    for (int i = 0; i < messages.size(); ++i) {
                        self_ofile << messages.at(i) << "\n";
                    }
                }
                self_ofile.close();
                messages.clear();
            }
        });
        
        // read history
        Message write_message;
        std::vector<std::string> history_vec;
        std::string message_str;
        
        std::ifstream ifile("Database/Timelines/"+username+"_timeline.txt");
        
        std::string prev_msg;
        Timestamp timestamp;
        
        while (std::getline(ifile, message_str)) {
            prev_msg = message_str;
            
            write_message.set_username(message_str.substr(0, message_str.find_first_of('(')));
           
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
            ifile = std::ifstream("Database/Timelines/"+username+"_timeline.txt");
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
                for (int i = history_vec.size() - 1; i >= 0; --i) {
                    message_str = history_vec.at(i);
                    if (username != message_str.substr(0, message_str.find_first_of('('))) {
                        write_message.set_username(message_str.substr(0, message_str.find_first_of('(')));
                        
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
