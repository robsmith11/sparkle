#include "sparkle_server.h"
#include "were/were_server_unix.h"
#include "sparkle_connection.h"


//==================================================================================================

SparkleServer::~SparkleServer()
{
    delete _server;
}

SparkleServer::SparkleServer(WereEventLoop *loop, const std::string &path)
{
    _loop = loop;
    
    _server = new WereServerUnix(_loop, path);
    
    _server->newConnection.connect(WereSimpleQueuer(loop, &SparkleServer::handleConnection, this));
}

//==================================================================================================

void SparkleServer::handleConnection()
{
    WereSocketUnix *socket = _server->accept();
    if (socket == 0)
        return;
    
    std::shared_ptr<SparkleConnection> client(new SparkleConnection(_loop, socket));
    
    std::weak_ptr<SparkleConnection> client_weak = client;
    
    client->signal_disconnected.connect([this, client_weak]()
    {
        _loop->queue(std::bind(&SparkleServer::handleDisconnection, this, client_weak.lock()));
    });

    client->signal_packet.connect([this, client_weak](std::shared_ptr<SparklePacket> packet)
    {
        _loop->queue(std::bind(&SparkleServer::handlePacket, this, client_weak.lock(), packet));
    });

    _clients.insert(client);
    
    signal_connected(client);
}

void SparkleServer::handleDisconnection(std::shared_ptr<SparkleConnection> client)
{
    _clients.erase(client);
}

void SparkleServer::handlePacket(std::shared_ptr<SparkleConnection> client, std::shared_ptr<SparklePacket> packet)
{
    signal_packet(client, packet);
}

void SparkleServer::broadcast(SparklePacket *packet)
{
    for (auto it = _clients.begin(); it != _clients.end(); ++it)
        (*it)->send(packet);
}

void SparkleServer::broadcast1(const packet_type_t *packetType, void *data)
{
    SparklePacket packet;
    packet.header()->operation = packetType->code;
    
    SparklePacketStream stream(&packet);
    stream.pWrite(&packetType->packer, data);
    
    broadcast(&packet);
}

//==================================================================================================

