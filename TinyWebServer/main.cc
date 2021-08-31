#include <getopt.h>
#include <string>

#include "EventLoop.h"
#include "Server.h"
#include "base/Logger.h"

int main(int argc, char *argv[])
{
    int threadNum = 4;  // default threadNum = 4
    int port = 80;
    std::string logPath = "./server.log";

    // parse arguments
    int opt;
    const char *str = "t:l:p:h:";
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt) {
            case 't':
                threadNum = atoi(optarg);
                break;
            case 'l':
                logPath = optarg;
                if (logPath.size() < 2 || optarg[0] != '/') {
                    printf("logPath should start with \"/\"\n");
                    abort();
                }
                break;
            case 'p':
                port = atoi(optarg);
                break;
            case 'h':
                printf("usage: %s [-p port_num] [-t thread_num] [-l log_path] [-h]", argv[0]);
                break;
            default:
                break;
        }
    }

    Logger::setLogFileName(logPath);

    EventLoop mainLoop;
    Server myServer(&mainLoop, threadNum, port);
    myServer.start();
    mainLoop.loop();

    return 0;
}