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
using csce438::Heartbeat;
using csce438::ClusterInfo;
using csce438::ServerInfo;
using csce438::SyncInfo;
using csce438::SNSService;


struct Cluster {
  int id;
  
  std::string master_port = "";
  std::string slave_port = "";
  std::string synchronizer_port = "";
  
  bool m_active = false;
  bool s_active = false;
  
  time_t m_timestamp;
  time_t s_timestamp;
};


std::vector<Cluster> clusters;

int findClusterId(int id) {
  if (!clusters.empty()) {
    for (int i = 0; i < clusters.size(); ++i) {
      if (clusters[i].id == id) {
        return i;
      }
    }
  }
  return -1;
}


class SNSServiceImpl final : public SNSService::Service {
  
  Status ConnectClient(ServerContext* context, const Request* request, Reply* reply) override {
    int i = stoi(request->id()) % clusters.size();
    
    std::string destination = "localhost:";
    
    time_t now = time(nullptr);
    
    if (clusters[i].m_timestamp >= (now - 20)) {
      destination += clusters[i].master_port;
    }
    else {
      destination += clusters[i].slave_port;
    }
    
    std::cout << destination << std::endl;
    
    reply->set_msg(destination);
    
    return Status::OK;
  }
  
  Status Ping(ServerContext* context, const Heartbeat* heartbeat, Reply* reply) override {
    int cluster_id = stoi(heartbeat->id());
    std::string type = heartbeat->type();
    
    int i = findClusterId(cluster_id);
    
    if (type == "master") {
      clusters[i].m_timestamp = time(nullptr);
    }
    else {
      clusters[i].s_timestamp = time(nullptr);
    }
    
    std::cout << "heartbeat from " << type << "_" << cluster_id << std::endl;
    
    return Status::OK;
  }
  
  Status RegisterServer(ServerContext* context, const ServerInfo* server_info, ClusterInfo* cluster_info) override {
    int cluster_id = stoi(server_info->id());
    
    int i = findClusterId(cluster_id);
    
    if (i < 0) {
      Cluster cluster;
      cluster.id = cluster_id;
      clusters.push_back(cluster);
      i = clusters.size() - 1;
    }
    
    cluster_info->set_port("");
    
    if (server_info->type() == "master") {
      clusters[i].master_port = server_info->port();
      clusters[i].m_active = true;
      
      if (clusters[i].s_active) {
        //std::cout << "found slave " << clusters[i].slave_port << " with no master" << std::endl;
        
        cluster_info->set_port(clusters[i].slave_port);
      }
    }
    else if (server_info->type() == "slave") {
      clusters[i].slave_port = server_info->port();
      clusters[i].s_active = true;
      
      if (clusters[i].m_active) {
        ClientContext C_context;
        ServerInfo server_info2;
        ClusterInfo cluster_info2;
        
        server_info2.set_port(server_info->port());
        
        std::string address = "localhost:" + clusters[i].master_port;
        std::unique_ptr<SNSService::Stub> stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel(address, grpc::InsecureChannelCredentials())));
        
        stub_->ConnectSlave(&C_context, server_info2, &cluster_info2);
      }
    }
    
    return Status::OK;
  }
  
  Status RegisterSync(ServerContext* context, const SyncInfo* sync_info, ClusterInfo* cluster_info) override {
    int cluster_id = stoi(sync_info->id());
    
    int i = findClusterId(cluster_id);
    
    if (i < 0) {
      Cluster cluster;
      cluster.id = cluster_id;
      clusters.push_back(cluster);
      i = clusters.size() - 1;
    }
    
    for (Cluster cluster : clusters) {
      if (cluster.synchronizer_port != "") {
        cluster_info->add_syncs("localhost:"+cluster.synchronizer_port);
        
        std::unique_ptr<SNSService::Stub> stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(grpc::CreateChannel("localhost:"+cluster.synchronizer_port, grpc::InsecureChannelCredentials())));
        
        ClientContext C_context;
        ClusterInfo cluster_info2;
        stub_->AddSync(&C_context, *sync_info, &cluster_info2);
        
      }
    }
    
    clusters[i].synchronizer_port = sync_info->port();
    
    return Status::OK;
  }

};

void RunCoordinator(std::string port_no) {
  std::string coordinator_address = "localhost:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(coordinator_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Coordinator listening on " << coordinator_address << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  
  std::string port = "8000";
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
  RunCoordinator(port);

  return 0;
}
