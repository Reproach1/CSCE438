#include <iostream>
#include <string>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "client.h"

#include "sns.grpc.pb.h"

class Client : public IClient
{
    public:
        Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname(hname), username(uname), port(p)
            {}
    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
    private:
        std::string hostname;
        std::string username;
        std::string port;
        
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<csce438::SNSService::Stub> stub_;
};

int main(int argc, char** argv) {

    std::string hostname = "localhost";
    std::string username = "default";
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'u':
                username = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }

    Client myc(hostname, username, port);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
	// ------------------------------------------------------------
    std::string server_address = hostname;
    server_address.append(":");
    server_address.append(port);
    
    auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
    stub_ = csce438::SNSService::NewStub(channel);
    
    grpc::ClientContext context;
    
    csce438::Request request;
    request.set_username(username);
    
    csce438::Reply reply;
    grpc::Status status;
    
    status = stub_->Login(&context, request, &reply);
    
    if (reply.msg() == "FAILURE_ALREADY_EXISTS") {
        return -1;
    }
    
    return 1;
    
}

IReply Client::processCommand(std::string& input)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse the given input
    // command and create your own message so that you call an 
    // appropriate service method. The input command will be one
    // of the followings:
	//
	// FOLLOW <username>
	// UNFOLLOW <username>
	// LIST
    // TIMELINE
	//
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
	// GUIDE 2:
	// Then, you should create a variable of IReply structure
	// provided by the client.h and initialize it according to
	// the result. Finally you can finish this function by returning
    // the IReply.
	// ------------------------------------------------------------
    
	// ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Follow" service method for FOLLOW command,
    // IReply can be set as follow:
    // 
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Follow(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //      
    //      return ire;
    // 
    // IMPORTANT: 
    // For the command "LIST", you should set both "all_users" and 
    // "following_users" member variable of IReply.
    // ------------------------------------------------------------
    
    IReply ire;
    
    grpc::ClientContext context;
    
    csce438::Request request;
    request.set_username(username);
    
    csce438::Reply reply;
    grpc::Status status;
    
    std::string action = input.substr(0, input.find_first_of(' '));
    std::string arguments = input.substr(input.find_first_of(' ')+1, input.size());
    
    request.add_arguments(arguments);
    
    if (action == "LIST") {
        status = stub_->List(&context, request, &reply);
        for (int i = 0; i < reply.all_users_size(); ++i) {
            ire.all_users.push_back(reply.all_users(i));
        }
        
        for (int i = 0; i < reply.following_users_size(); ++i) {
            ire.following_users.push_back(reply.following_users(i));
        }
    }
    else if (action == "FOLLOW") {
        status = stub_->Follow(&context, request, &reply);
    }
    else if (action == "UNFOLLOW") {
        status = stub_->UnFollow(&context, request, &reply);
    }
    
    if (action == "TIMELINE") {
        ire.comm_status = SUCCESS;
    }
    else if (status.ok()) {
        if (reply.msg() == "SUCCESS")
            ire.comm_status = SUCCESS;
        else if (reply.msg() == "FAILURE_NOT_EXISTS")
            ire.comm_status = FAILURE_NOT_EXISTS;
        else if (reply.msg() == "FAILURE_ALREADY_EXISTS")
            ire.comm_status = FAILURE_ALREADY_EXISTS;
        else if (reply.msg() == "FAILURE_INVALID_USERNAME")
            ire.comm_status = FAILURE_INVALID_USERNAME;
        else if (reply.msg() == "FAILURE_INVALID")
            ire.comm_status = FAILURE_INVALID;
    }
    else {
        ire.comm_status = FAILURE_NOT_EXISTS;
    }
    
    return ire;
}

void Client::processTimeline()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------
	grpc::ClientContext context;
    
    csce438::Message message;
    message.set_username(username);
	
	std::shared_ptr<grpc::ClientReaderWriter<csce438::Message, csce438::Message>> stream(stub_->Timeline(&context));
	message.set_msg("Gamer time");
	
	while (true) {
        stream->Write(message);
	}
    stream->WritesDone();
}
