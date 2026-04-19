#pragma once

#include <string>
#include <vector>

#include "Core/GameplayNotification.hpp"
#include "Core/GameSessionConfig.hpp"
#include "Multiplayer/MultiplayerClient.hpp"
#include "Multiplayer/MultiplayerServer.hpp"

class SaveManager;
struct SaveData;

struct MultiplayerJoinCredentials {
    std::string host;
    int port = 0;
    std::string password;
};

struct MultiplayerReconnectState {
    bool available = false;
    bool awaitingReconnect = false;
    bool reconnectAttemptInProgress = false;
    MultiplayerJoinCredentials request;
    std::string lastErrorMessage;
};

class MultiplayerRuntime {
public:
    void resetConnections(const std::string& hostDisconnectReason = "Host closed the multiplayer session.");
    void clearReconnectState();
    void cacheReconnectRequest(const MultiplayerJoinCredentials& request);

    bool startHostIfNeeded(const GameSessionConfig& session,
                           const std::string& saveName,
                           std::string* errorMessage = nullptr);
    bool pushSnapshotIfConnected(bool lanHost,
                                 const std::string& serializedSaveData,
                                 const std::vector<GameplayNotification>& notifications,
                                 std::string* errorMessage = nullptr);
    bool pushSnapshotIfConnected(bool lanHost,
                                 const std::string& serializedSaveData,
                                 std::string* errorMessage = nullptr);
    bool publishTurnPreview(bool lanHost,
                            const MultiplayerTurnPreview& preview,
                            std::string* errorMessage = nullptr);
    bool submitTurnSubmission(const std::vector<TurnCommand>& commands,
                              std::string* errorMessage = nullptr);
    bool sendTurnRejected(const std::string& reason,
                          std::string* errorMessage = nullptr);
    bool join(const MultiplayerJoinCredentials& request,
              SaveManager& saveManager,
              SaveData& snapshotData,
              std::string* errorMessage = nullptr);
    void update();

    bool serverIsRunning() const { return m_server.isRunning(); }
    bool hostHasAuthenticatedClient() const { return m_server.hasAuthenticatedClient(); }
    bool clientIsAuthenticated() const { return m_client.isAuthenticated(); }
    bool clientIsConnected() const { return m_client.isConnected(); }
    bool hasPendingServerEvent() const { return m_server.hasPendingEvent(); }
    bool hasPendingClientEvent() const { return m_client.hasPendingEvent(); }
    MultiplayerServer::Event popNextServerEvent() { return m_server.popNextEvent(); }
    MultiplayerClient::Event popNextClientEvent() { return m_client.popNextEvent(); }

    bool lanHostRemoteSessionEstablished() const { return m_lanHostRemoteSessionEstablished; }
    void setLanHostRemoteSessionEstablished(bool established) { m_lanHostRemoteSessionEstablished = established; }
    const std::string& hostJoinHint() const { return m_hostJoinHint; }

    bool hasReconnectRequest() const { return m_reconnectState.available; }
    bool awaitingReconnect() const { return m_reconnectState.awaitingReconnect; }
    bool reconnectAttemptInProgress() const { return m_reconnectState.reconnectAttemptInProgress; }
    void setReconnectAttemptInProgress(bool inProgress) { m_reconnectState.reconnectAttemptInProgress = inProgress; }
    const MultiplayerJoinCredentials& reconnectRequest() const { return m_reconnectState.request; }
    const std::string& reconnectLastErrorMessage() const { return m_reconnectState.lastErrorMessage; }
    void noteReconnectAwaiting(const std::string& message);
    void noteReconnectRecovered();

private:
    MultiplayerServer m_server;
    MultiplayerClient m_client;
    bool m_lanHostRemoteSessionEstablished = false;
    std::string m_hostJoinHint;
    MultiplayerReconnectState m_reconnectState;
};