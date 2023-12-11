//#include <utility>
//#include "fstream"
#include "thread"
#include "yaml-cpp/yaml.h"
#include "iceflow/Producer.hpp"



class Compute
{

public:
    void compute(std::string &pub_syncPrefix,
				 std::string &userPrefix_data_main,
                 std::vector<int> nDataStreams)
    {
	    auto producer = new iceflow::Producer(pub_syncPrefix, userPrefix_data_main, nDataStreams);
        int i=0;
        while(true){
            std::string data = "Hello"+ std::to_string(i);
            std::cout<<"Data: "<<data<<std::endl;
            i++;
            producer->push(data);
        }
    }
};

void
DataFlow(std::string &pub_syncPrefix,
		 std::string &userPrefix_data_main,
         std::vector<int> &nDataStreams)
{
    auto *compute = new Compute();
    std::vector<std::thread> ProducerThreads;
    ProducerThreads.emplace_back(&Compute::compute, compute,std::ref(pub_syncPrefix) ,std::ref(userPrefix_data_main), std::ref(nDataStreams));

    for (auto &t: ProducerThreads) {
        t.join();
    }
}


int
main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "usage: " << argv[0] << " "
                  << "<config-file>" << std::endl;
        return 1;
    }

    std::vector<std::thread> ProducerThreads;

    YAML::Node config = YAML::LoadFile(argv[1]);

    auto pub_syncPrefix          = config["Producer"]["pub_syncPrefix"].as<std::string>();
    auto userPrefix_data_main    = config["Producer"]["userPrefix_data_main"].as<std::string>();
	auto nDataStreams        = config["Producer"]["nDataStreams"].as<std::vector<int>>();


    try {
        DataFlow(pub_syncPrefix
                , userPrefix_data_main
                , nDataStreams);

    }
    catch (const std::exception &e) {
        std::cout << (e.what()) << std::endl;
    }
}