#include <stdio.h>
#include <unistd.h>
#include <pinc.h>

#include "globals.h"
#include "ipc_rabbithole.h"
#include "ipc_event.h"
#include "logger.h"
#include "limbo_man.h"

IPCRabbithole* rabbithole;

PCL int OnInit()
{
	// Plugin arguments
	uint s = Plugin_Cmd_Argc();
	for(uint i = 0; i < s; i++)
	{
		char* argv = Plugin_Cmd_Argv(i);
		
		if(strcmp(argv, "-d") == 0)
		{
			WONDERLANDX_DBG = true;
			
			Logger::LogInfo("Debug mode enabled");
		}
	}
	
	Logger::LogInfo("Initialising WonderlandX\n");
	
	LimboMan::Instance()->SetLimboLimit(Plugin_GetSlotCount());
	
	int serverPort = Plugin_Cvar_VariableIntegerValue("net_port");
	rabbithole = new IPCRabbithole(serverPort);
	
	return 0;
}

PCL void OnInfoRequest(pluginInfo_t *info)
{
	// =====  MANDATORY FIELDS  =====
	info->handlerVersion.major = PLUGIN_HANDLER_VERSION_MAJOR;
	info->handlerVersion.minor = PLUGIN_HANDLER_VERSION_MINOR;  // Requested handler version

	// =====  OPTIONAL  FIELDS  =====
	info->pluginVersion.major = PLUGIN_VERSION_MAJOR;
	info->pluginVersion.minor = PLUGIN_VERSION_MINOR;
	strncpy(info->fullName,"WonderlandX", sizeof(info->fullName));
	strncpy(info->shortDescription, "Wonderland for CoD4X17a", sizeof(info->shortDescription));
}

PCL void OnPlayerJoinReq(int clientnum, netadr_t* netaddress, char* pbguid,
                         char* userinfo, int authstatus, char* deniedmsg,
                         int deniedmsgbufmaxlen, qboolean* wait)
{
#ifdef WONDERLANDX_STANDALONE
    *wait = qfalse;
    LimboMan::Instance()->Reset(clientnum);
    return;
#endif

    bool isWaiting = LimboMan::Instance()->IsWaiting(clientnum);
    bool isDenied  = LimboMan::Instance()->IsDenied(clientnum);

    // 1) If denied, send message and let engine drop the client
	if(isDenied)
	{
		char* reason = LimboMan::Instance()->GetDenyReason(clientnum);
		size_t deniedLen = strnlen(reason, MAX_STRING_CHARS);

		strncpy(deniedmsg, reason, deniedLen);
		deniedmsg[deniedLen] = '\0';  // ensure null-termination

		*wait = qfalse;               // let engine process the deny
		LimboMan::Instance()->Reset(clientnum);
		return;
	}

    // 2) If not waiting anymore, bot accepted → release
    if(!isWaiting)
    {
        *wait = qfalse;
        LimboMan::Instance()->Reset(clientnum);
        return;
    }

    // 3) Still waiting → hold in limbo and emit JOINREQ
    *wait = qtrue;

    const char* ipAddr = Plugin_NET_AdrToStringShort(netaddress);

    IPCEvent* event = new IPCEvent("JOINREQ");
    event->AddArgument((void*) clientnum, IPCTypes::uint);
    event->AddArgument((void*) ipAddr, IPCTypes::ch);
    event->AddArgument((void*) Plugin_GetPlayerGUID(clientnum), IPCTypes::ch);
    event->AddArgument((void*) userinfo, IPCTypes::ch);

    rabbithole->SetEventForBroadcast(event);
    rabbithole->SignalEventSend();
}

PCL void OnPlayerConnect(int clientnum, netadr_t* netaddress, char* pbguid, char* userinfo, int authstatus, char* deniedmsg, int deniedmsgbufmaxlen)
{
	const char* ipAddr = Plugin_NET_AdrToStringShort(netaddress);
	
	IPCEvent* event = new IPCEvent("CONNECT");
	event->AddArgument((void*) clientnum, IPCTypes::uint);
	event->AddArgument((void*) ipAddr, IPCTypes::ch);
	event->AddArgument((void*) Plugin_GetPlayerGUID(clientnum), IPCTypes::ch);
	event->AddArgument((void*) userinfo, IPCTypes::ch);
	
	rabbithole->SetEventForBroadcast(event);
	rabbithole->SignalEventSend();
}

PCL void OnClientEnterWorld(client_t* client)
{
	int clientnum = client - clientbase;

	IPCEvent* event = new IPCEvent("ENTER_WORLD");
	event->AddArgument((void*) clientnum, IPCTypes::uint);
	event->AddArgument((void*) client->userinfo, IPCTypes::ch);

	rabbithole->SetEventForBroadcast(event);
	rabbithole->SignalEventSend();
}

PCL void OnClientUserinfoChanged(client_t *client)
{
	int clientnum = client - clientbase;

	IPCEvent* event = new IPCEvent("USERINFO_CHANGED");
	event->AddArgument((void*) clientnum, IPCTypes::uint);
	event->AddArgument((void*) client->userinfo, IPCTypes::ch);

	rabbithole->SetEventForBroadcast(event);
	rabbithole->SignalEventSend();
}

PCL void OnClientSpawn(gentity_t* ent) {
    int clientnum = ent->s.clientNum;
	int team = Plugin_GetClientTeam(clientnum);

	IPCEvent* event = new IPCEvent("SPAWN");
	event->AddArgument((void*) clientnum, IPCTypes::uint);
	event->AddArgument((void*) team, IPCTypes::uint);

	rabbithole->SetEventForBroadcast(event);
	rabbithole->SignalEventSend();
}


PCL void OnMessageSent(char* message, int slot, qboolean *show, int mode)
{
	// Force the server not to forward the message on to the CoD4 clients
	// because Alice will deal with them.
	*show = qfalse;
	
	IPCEvent* event = new IPCEvent("CHAT");
	event->AddArgument((void*) slot, IPCTypes::uint);
	event->AddArgument((void*) message, IPCTypes::ch);
	event->AddArgument((void*) *show, IPCTypes::uint);
	event->AddArgument((void*) mode, IPCTypes::uint);

	
	rabbithole->SetEventForBroadcast(event);
	rabbithole->SignalEventSend();
}

PCL void OnPlayerDC(client_t* client, const char* reason)
{
	int clientnum = client - clientbase;
	
	IPCEvent* event = new IPCEvent("DC");
	event->AddArgument((void*) clientnum, IPCTypes::uint);
	
	rabbithole->SetEventForBroadcast(event);
	rabbithole->SignalEventSend();
}