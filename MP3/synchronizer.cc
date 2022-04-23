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
#include <sys/stat.h>
#include <thread>
#include <ctime>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::ClientContext;
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
using csce438::ClusterInfo;
using csce438::SyncInfo;
using csce438::FileInfo;
using csce438::SNSService;

struct File {
  std::string id;
  time_t prev_t = 0;
};

std::string id;

std::vector<std::string> syncs;
std::unique_ptr<SNSService::Stub> stub_;

File all_users;
std::vector<File> timelines;
std::vector<File> followers;

std::vector<std::string> users;

bool new_user(std::string id) {
  for (std::string user : users) {
    if (user == id) {
      return false;
    }
  }
  return true;
}

class SNSServiceImpl final : public SNSService::Service {
  Status AddSync(ServerContext* context, const SyncInfo* sync_info, ClusterInfo* cluster_info) override {
    
    syncs.push_back("localhost:" + sync_info->port());
    
    return Status::OK;
  }
  
  Status SyncFile(ServerContext* context, const FileInfo* file_info, Reply* reply) override {
    std::string user_id = file_info->id();
    if (file_info->action() == "follow") {
      std::ofstream ofile_slave("slave_"+id+"/Followers/"+user_id+"_followers.txt");
      std::ofstream ofile_master("master_"+id+"/Followers/"+user_id+"_followers.txt");
      
      for (std::string line: file_info->lines()) {
        ofile_slave << line << "\n";
        ofile_master << line << "\n";
      }
      ofile_master.close();
      ofile_slave.close();
    }
    else if (file_info->action() == "users") {
      std::ofstream ofile_slave("slave_"+id+"/all_users.txt");
      std::ofstream ofile_master("master_"+id+"/all_users.txt");
      
      for (std::string user : file_info->lines()) {
        std::cout << user << std::endl;
        if (new_user(user)) {
          users.push_back(user);
        }
      }
      
      for (std::string user : users) {
        ofile_slave << user << "\n";
        ofile_master << user << "\n";
      }
      
      ofile_master.close();
      ofile_slave.close();
    }
    else if (file_info->action() == "timeline") {
      std::ofstream ofile_slave("slave_"+id+"/Timelines/"+user_id+"_timeline.txt");
      std::ofstream ofile_master("master_"+id+"/Timelines/"+user_id+"_timeline.txt");
      
      for (std::string line: file_info->lines()) {
        ofile_slave << line << "\n";
        ofile_master << line << "\n";
      }
      ofile_master.close();
      ofile_slave.close();
    }
    
    return Status::OK;
  }

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
  SyncInfo sync_info;
  ClusterInfo cluster_info;
  
  sync_info.set_id(id);
  sync_info.set_port(port);
  
  stub_->RegisterSync(&context, sync_info, &cluster_info);
  
  for (std::string s : cluster_info.syncs()) {
    syncs.push_back(s);
  }
}

void CheckForUpdates() {
  // initialize values once
  int n_users = 0;
  struct stat sfile;
  
  std::ifstream ifile("slave_"+id+"/all_users.txt");
      
  std::string user;
  
  while(std::getline(ifile, user)) {
    File f_file;
    File t_file;
    
    f_file.id = user;
    t_file.id = user;
    
    followers.push_back(f_file);
    timelines.push_back(t_file);
    
    users.push_back(user);
    
    n_users++;
  }
  ifile.close();
  
  stat(("slave_"+id+"/all_users.txt").c_str(), &sfile);
  all_users.prev_t = sfile.st_mtime;
  
  for (int i = 0; i < followers.size(); ++i) {
    if (stat(("slave_"+id+"/Followers/"+followers[i].id+"_followers.txt").c_str(), &sfile) != -1) {
      followers[i].prev_t = sfile.st_mtime;
    }
  }
  
  for (int i = 0; i < timelines.size(); ++i) {
    if (stat(("slave_"+id+"/Timelines/"+followers[i].id+"_timeline.txt").c_str(), &sfile) != -1) {
      timelines[i].prev_t = sfile.st_mtime;
    }
  }
  
  std::unique_ptr<SNSService::Stub> sync_stub_;
  
  while (1) {
    std::this_thread::sleep_for(std::chrono::seconds(30));
    
    struct stat all_file;
    struct stat follow_file;
    struct stat timeline_file;
    
    stat(("slave_"+id+"/all_users.txt").c_str(), &all_file);
    if (all_file.st_mtime != all_users.prev_t) {
      all_users.prev_t = sfile.st_mtime;
      FileInfo file_info;
      file_info.set_action("users");
      
      ifile = std::ifstream("slave_"+id+"/all_users.txt");
      while(std::getline(ifile, user)) {
        file_info.add_lines(user);
        if (new_user(user)) {
          File f_file;
          File t_file;
          
          f_file.id = user;
          t_file.id = user;
          
          followers.push_back(f_file);
          timelines.push_back(t_file);
          
          users.push_back(user);
        }
      }
      ifile.close();
      
      for (std::string a : syncs) {
        std::cout << id << " sending new user to " << a << std::endl;
        std::cout << file_info.lines(0) << std::endl;
        ClientContext context;
        Reply reply;
        sync_stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(a, grpc::InsecureChannelCredentials())));
        sync_stub_->SyncFile(&context, file_info, &reply);
      }
    }
    
    for (int i = 0; i < followers.size(); ++i) {
      if (stat(("slave_"+id+"/Followers/"+followers[i].id+"_followers.txt").c_str(), &follow_file) != -1) {
        if (followers[i].prev_t != follow_file.st_mtime) {
          followers[i].prev_t = follow_file.st_mtime;
          
          FileInfo file_info;
          file_info.set_action("follow");
          file_info.set_id(followers[i].id);
          
          ifile = std::ifstream("slave_"+id+"/Followers/"+followers[i].id+"_followers.txt");
          while(std::getline(ifile, user)) {
            file_info.add_lines(user);
          }
          ifile.close();
          
          for (std::string a : syncs) {
            ClientContext context;
            Reply reply;
            sync_stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(a, grpc::InsecureChannelCredentials())));
            sync_stub_->SyncFile(&context, file_info, &reply);
          }
        }
      }
    }
    
    for (int i = 0; i < timelines.size(); ++i) {
      if (stat(("slave_"+id+"/Timelines/"+timelines[i].id+"_timeline.txt").c_str(), &timeline_file) != -1) {
        if (timelines[i].prev_t != timeline_file.st_mtime) {
          timelines[i].prev_t = timeline_file.st_mtime;
          
          FileInfo file_info;
          file_info.set_action("timeline");
          file_info.set_id(timelines[i].id);
          
          ifile = std::ifstream("slave_"+id+"/Timelines/"+followers[i].id+"_timeline.txt");
          while(std::getline(ifile, user)) {
            file_info.add_lines(user);
          }
          ifile.close();
          
          for (std::string a : syncs) {
            ClientContext context;
            Reply reply;
            sync_stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(a, grpc::InsecureChannelCredentials())));
            sync_stub_->SyncFile(&context, file_info, &reply);
          }
        }
      }
    }
  }
}

int main(int argc, char** argv) {
  
  std::string port = "8020";
  std::string cip = "localhost";
  std::string cp = "8000";
  
  
  for (int i = 1; i < argc; i += 2) {
    if (strcmp(argv[i], "-cip") == 0) {
        cip = argv[i+1];
    } else if (strcmp(argv[i], "-cp") == 0) {
        cp = argv[i+1];
    } else if (strcmp(argv[i], "-p") == 0) {
        port = argv[i+1];
    } else if (strcmp(argv[i], "-id") == 0) {
        id = argv[i+1];
    }
  }
  
  RegisterWithCoordinator(port, cip, cp, id);
  
  std::thread thread(CheckForUpdates);
  
  RunServer(port);
  
  thread.join();

  return 0;
}
