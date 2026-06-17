#ifndef INSTFUNC_H
#define INSTFUNC_H

#include <Arduino.h>
#include <WiFiS3.h>
#include "config.h"

class InstClass{
    private:
        WiFiClient client;
        IPAddress serverIP;
        WiFiUDP udp;

    public:
        InstClass();
        bool isRegistered;
        int ready;
        void Starts();
        void WiFiStart();
        void startUDP();
        bool connectToServer();
        void connection();
        void sendIdentification(const char* name);
        String receiveTCP(WiFiClient &c);
        String receiveUDP(WiFiUDP &u);

        // 新機能
        void recieveStart();
        int recieveLevel();

};
#endif