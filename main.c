#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <winsock2.h>
#include "lib/cjson/cJSON.h"
#include "api.h"
#include "ws.h"
#include "server.h"

#define DllExport(returnType) __declspec(dllexport) returnType __stdcall

const char* PLUGIN_INFO = 
    "pluginID=websocket.protocol;\r\n"
    "pluginName=WebSocket Protocol;\r\n"
    "pluginBrief="
        "Enable you to use QQLight API in any language you like via WebSocket.\r\n\r\n"
        "GitHub:\r\nhttps://github.com/Chocolatl/qqlight-websocket;\r\n"
    "pluginVersion=0.1.2;\r\n"
    "pluginSDK=3;\r\n"
    "pluginAuthor=Chocolatl;\r\n"
    "pluginWindowsTitle=;"
;

char authCode[64];
char pluginPath[1024];

void pluginLog(const char* type, const char* format, ...) {
	char buff[512];
	va_list arg;

	va_start(arg, format);
	vsnprintf(buff, sizeof(buff) - 1, format, arg);
	va_end(arg);

	QL_printLog(type, buff, 0, authCode);
}

// 返回转换后数据地址，记得free
char* GBKToUTF8(const char* str) {
    
    // GB18030代码页
    const int CODE_PAGE = 54936;

    int n = MultiByteToWideChar(CODE_PAGE, 0, str, -1, NULL, 0);
    wchar_t u16str[n + 1];
    MultiByteToWideChar(CODE_PAGE, 0, str, -1, u16str, n);

    n = WideCharToMultiByte(CP_UTF8, 0, u16str, -1, NULL, 0, NULL, NULL);
    char* u8str = malloc(n + 1);
    WideCharToMultiByte(CP_UTF8, 0, u16str, -1, u8str, n, NULL, NULL);

    return u8str;
}

// 返回转换后数据地址，记得free
char* UTF8ToGBK(const char* str) {

    // GB18030代码页
    const int CODE_PAGE = 54936;

    int n = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
    wchar_t u16str[n + 1];
    MultiByteToWideChar(CP_UTF8, 0, str, -1, u16str, n);

    n = WideCharToMultiByte(CODE_PAGE, 0, u16str, -1, NULL, 0, NULL, NULL);
    char* gbstr = malloc(n + 1);
    WideCharToMultiByte(CODE_PAGE,0, u16str, -1, gbstr, n, NULL, NULL);

    return gbstr;
}

void wsClientTextDataHandle(const char* payload, uint64_t payloadLen, SOCKET socket) {
    
    // 注意，payload的文本数据不是以\0结尾
    pluginLog("wsClientDataHandle","Payload data is %.*s", payloadLen > 128 ? 128 : (unsigned int)payloadLen, payload);

    const char* parseEnd;

    cJSON *json = cJSON_ParseWithOpts(payload, &parseEnd, 0);

    if(json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            pluginLog("jsonParse", "Error before: %d", error_ptr - payload);
        }
        return;
    }

    // 公有字段
    const cJSON* j_id     = cJSON_GetObjectItemCaseSensitive(json, "id");        // cJSON_GetObjectItemCaseSensitive获取不存在的字段时会返回NULL
    const cJSON* j_method = cJSON_GetObjectItemCaseSensitive(json, "method");
    const cJSON* j_params = cJSON_GetObjectItemCaseSensitive(json, "params");

    const cJSON_bool e_id     = cJSON_IsString(j_id);        // 如果j_xx的值为NULL的时候也会返回FALSE，所以e_xx为TRUE时可以保证字段存在且类型正确
    const cJSON_bool e_method = cJSON_IsString(j_method);
    const cJSON_bool e_params = cJSON_IsString(j_params);

    const char* v_id     = e_id     ?  j_id->valuestring      : NULL;
    const char* v_method = e_method ?  j_method->valuestring  : NULL;
    const char* v_params = e_params ?  j_params->valuestring  : NULL;

    if(!e_method) {
        cJSON_Delete(json);
        return;
    }

    // 参数字段
    const cJSON* j_type    = cJSON_GetObjectItemCaseSensitive(j_params, "type");        // 即使j_params为NULL也是安全的，返回的结果也是NULL
    const cJSON* j_group   = cJSON_GetObjectItemCaseSensitive(j_params, "group");
    const cJSON* j_qq      = cJSON_GetObjectItemCaseSensitive(j_params, "qq");
    const cJSON* j_content = cJSON_GetObjectItemCaseSensitive(j_params, "content");
    const cJSON* j_msgid   = cJSON_GetObjectItemCaseSensitive(j_params, "msgid");
    const cJSON* j_message = cJSON_GetObjectItemCaseSensitive(j_params, "message");
    const cJSON* j_object  = cJSON_GetObjectItemCaseSensitive(j_params, "object");
    const cJSON* j_data    = cJSON_GetObjectItemCaseSensitive(j_params, "data");
    const cJSON* j_name    = cJSON_GetObjectItemCaseSensitive(j_params, "name");

    const cJSON_bool e_type    = cJSON_IsNumber(j_type);
    const cJSON_bool e_group   = cJSON_IsString(j_group);
    const cJSON_bool e_qq      = cJSON_IsString(j_qq);
    const cJSON_bool e_content = cJSON_IsString(j_content);
    const cJSON_bool e_msgid   = cJSON_IsString(j_msgid);
    const cJSON_bool e_message = cJSON_IsString(j_message);
    const cJSON_bool e_object  = cJSON_IsString(j_object);
    const cJSON_bool e_data    = cJSON_IsString(j_data);
    const cJSON_bool e_name    = cJSON_IsString(j_name);

    int         v_type    = e_type    ?  j_type->valueint        :  -1;
    const char* v_group   = e_group   ?  j_group->valuestring    :  NULL;
    const char* v_qq      = e_qq      ?  j_qq->valuestring       :  NULL;
    const char* v_content = e_content ?  j_content->valuestring  :  NULL;
    const char* v_msgid   = e_msgid   ?  j_msgid->valuestring    :  NULL;
    const char* v_message = e_message ?  j_message->valuestring  :  NULL;
    const char* v_object  = e_object  ?  j_object->valuestring   :  NULL;
    const char* v_data    = e_data    ?  j_data->valuestring     :  NULL;
    const char* v_name    = e_name    ?  j_name->valuestring     :  NULL;
 
    pluginLog("jsonRPC", "Client call '%s' method", v_method);

    #define PARAMS_CHECK(condition) if(!(condition)) {pluginLog("jsonParse", "Invalid data"); goto RPCParseEnd;}
    #define METHOD_IS(name) (strcmp(name, v_method) == 0)
    
    if(METHOD_IS("sendMessage")) {

        PARAMS_CHECK(e_type && e_content && (e_qq || e_group));

        char* gbkText = UTF8ToGBK(v_content);
        QL_sendMessage(v_type, e_content ? v_group : "", e_qq ? v_qq : "", gbkText, authCode);
        free((void*)gbkText);

    } else if (METHOD_IS("withdrawMessage")) {

        PARAMS_CHECK(e_group && e_msgid);

        QL_withdrawMessage(v_group, v_msgid, authCode);

    } else if (METHOD_IS("getFriendList")) {

        PARAMS_CHECK(e_id);

        cJSON* root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(v_id));

        const char* friendList = GBKToUTF8(QL_getFriendList(authCode));
        cJSON_AddItemToObject(root, "result", cJSON_Parse(friendList));

        const char* jsonStr = cJSON_PrintUnformatted(root);
        wsFrameSend(socket, jsonStr, strlen(jsonStr), frameType_text);

        cJSON_Delete(root);
        free((void*)friendList);
        free((void*)jsonStr);

    } else if (METHOD_IS("addFriend")) {

        PARAMS_CHECK(e_id);

        if(!e_message) {
            QL_addFriend(v_qq, "", authCode);
        } else {
            const char* text = UTF8ToGBK(v_message);
            QL_addFriend(v_qq, text, authCode);
            free((void*)text);
        }

    } else if (METHOD_IS("deleteFriend")) {

        PARAMS_CHECK(e_id);

        QL_deleteFriend(v_qq, authCode);

    } else if (METHOD_IS("getGroupList")) {

        PARAMS_CHECK(e_id);

        cJSON* root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(v_id));

        const char* groupList = GBKToUTF8(QL_getGroupList(authCode));
        cJSON_AddItemToObject(root, "result", cJSON_Parse(groupList));

        const char* jsonStr = cJSON_PrintUnformatted(root);
        wsFrameSend(socket, jsonStr, strlen(jsonStr), frameType_text);

        cJSON_Delete(root);
        free((void*)groupList);
        free((void*)jsonStr);

    } else if (METHOD_IS("getGroupMemberList")) {

        PARAMS_CHECK(e_id && e_group);

        cJSON* root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(v_id));

        const char* groupMemberList = GBKToUTF8(QL_getGroupMemberList(v_group, authCode));
        cJSON_AddItemToObject(root, "result", cJSON_Parse(groupMemberList));

        const char* jsonStr = cJSON_PrintUnformatted(root);
        wsFrameSend(socket, jsonStr, strlen(jsonStr), frameType_text);

        cJSON_Delete(root);
        free((void*)groupMemberList);
        free((void*)jsonStr);

    } else if (METHOD_IS("addGroup")) {

        PARAMS_CHECK(e_group);

        if(!e_message) {
            QL_addGroup(v_group, "", authCode);
        } else {
            const char* text = UTF8ToGBK(v_message);
            QL_addGroup(v_group, text, authCode);
            free((void*)text);
        }

    } else if (METHOD_IS("quitGroup")) {

        PARAMS_CHECK(e_group);

        QL_quitGroup(v_group, authCode);

    } else if (METHOD_IS("getGroupCard")) {

        PARAMS_CHECK(e_id && e_group && e_qq);

        cJSON* root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(v_id));

        const char* groupCard = GBKToUTF8(QL_getGroupCard(v_group, v_qq, authCode));
        cJSON_AddItemToObject(root, "result", cJSON_CreateString(groupCard));

        const char* jsonStr = cJSON_PrintUnformatted(root);
        wsFrameSend(socket, jsonStr, strlen(jsonStr), frameType_text);

        cJSON_Delete(root);
        free((void*)groupCard);
        free((void*)jsonStr);

    } else if (METHOD_IS("uploadImage")) {

        PARAMS_CHECK(e_id && e_type && e_object && e_data);

        cJSON* root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(v_id));

        const char* guid = QL_uploadImage(v_type, v_object, v_data, authCode);
        cJSON_AddItemToObject(root, "result", cJSON_CreateString(guid));

        const char* jsonStr = cJSON_PrintUnformatted(root);
        wsFrameSend(socket, jsonStr, strlen(jsonStr), frameType_text);

        cJSON_Delete(root);
        free((void*)jsonStr);

    } else if (METHOD_IS("getQQInfo")) {

        PARAMS_CHECK(e_id && e_qq);

        cJSON* root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(v_id));

        const char* info = GBKToUTF8(QL_getQQInfo(v_qq, authCode));
        cJSON_AddItemToObject(root, "result", cJSON_Parse(info));

        const char* jsonStr = cJSON_PrintUnformatted(root);
        wsFrameSend(socket, jsonStr, strlen(jsonStr), frameType_text);

        cJSON_Delete(root);
        free((void*)info);
        free((void*)jsonStr);

    } else if (METHOD_IS("getGroupInfo")) {

        PARAMS_CHECK(e_id && e_group);

        cJSON* root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(v_id));

        const char* info = GBKToUTF8(QL_getGroupInfo(v_group, authCode));
        cJSON_AddItemToObject(root, "result", cJSON_Parse(info));

        const char* jsonStr = cJSON_PrintUnformatted(root);
        wsFrameSend(socket, jsonStr, strlen(jsonStr), frameType_text);

        cJSON_Delete(root);
        free((void*)info);
        free((void*)jsonStr);

    } else if (METHOD_IS("inviteIntoGroup")) {

        PARAMS_CHECK(e_qq && e_group);

        QL_inviteIntoGroup(v_group, v_qq, authCode);

    } else if (METHOD_IS("setGroupCard")) {

        PARAMS_CHECK(e_qq && e_group && e_name);

        QL_setGroupCard(v_group, v_qq, v_name, authCode);

    } else if (METHOD_IS("getLoginAccount")) {

        PARAMS_CHECK(e_id);

        cJSON* root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(v_id));

        const char* account = GBKToUTF8(QL_getLoginAccount(authCode));
        cJSON_AddItemToObject(root, "result", cJSON_CreateString(account));

        const char* jsonStr = cJSON_PrintUnformatted(root);
        wsFrameSend(socket, jsonStr, strlen(jsonStr), frameType_text);

        cJSON_Delete(root);
        free((void*)account);
        free((void*)jsonStr);

    } else if (METHOD_IS("setSignature")) {

        PARAMS_CHECK(e_content);

        QL_setSignature(v_content, authCode);

    } else if (METHOD_IS("getNickname")) {

        PARAMS_CHECK(e_id && e_qq);

        cJSON* root = cJSON_CreateObject();
        cJSON_AddItemToObject(root, "id", cJSON_CreateString(v_id));

        const char* nickname = GBKToUTF8(QL_getNickname(v_qq, authCode));
        cJSON_AddItemToObject(root, "result", cJSON_CreateString(nickname));

        const char* jsonStr = cJSON_PrintUnformatted(root);
        wsFrameSend(socket, jsonStr, strlen(jsonStr), frameType_text);

        cJSON_Delete(root);
        free((void*)nickname);
        free((void*)jsonStr);

    } else {
        pluginLog("jsonRPC", "Unknown method '%s'", v_method);
    }

    RPCParseEnd:

    cJSON_Delete(json);
}

DllExport(const char*) Information(const char* _authCode) {
    
    // 获取authCode
    strncpy(authCode, _authCode, sizeof(authCode) - 1);
    
    return PLUGIN_INFO;
}

DllExport(int) Event_Initialization(void) {
    
    // 获取插件目录
    strncpy(pluginPath, QL_getPluginPath(authCode), sizeof(authCode) - 1);
    
    return 0;
}

DllExport(int) Event_pluginStart(void) {
    
    int result = serverStart();
    
    if(result != 0) {
        pluginLog("Event_pluginStart", "WebSocket server startup failed");
    } else {
        pluginLog("Event_pluginStart", "WebSocket server startup success");
    }
    
    return 0;
} 

DllExport(int) Event_pluginStop(void) {
    
    serverStop();
    
    pluginLog("Event_pluginStop", "WebSocket server stopped"); 
    
    return 0;
}

DllExport(int) Event_GetNewMsg (
    int type,              // 1=好友消息 2=群消息 3=群临时消息 4=讨论组消息 5=讨论组临时消息 6=QQ临时消息
    const char* group,     // 类型为1或6的时候，此参数为空字符串，其余情况下为群号或讨论组号
    const char* qq,        // 消息来源QQ号 "10000"都是来自系统的消息(比如某人被禁言或某人撤回消息等)
    const char* msg,       // 消息内容
    const char* msgid      // 消息id，撤回消息的时候会用到，群消息会存在，其余情况下为空  
) {

    const char* u8Content = GBKToUTF8(msg);

    cJSON* root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "event", cJSON_CreateString("message"));

    cJSON* params = cJSON_CreateObject();
    cJSON_AddItemToObject(params, "type", cJSON_CreateNumber(type));
    cJSON_AddItemToObject(params, "msgid", cJSON_CreateString(msgid));
    cJSON_AddItemToObject(params, "group", cJSON_CreateString(group));
    cJSON_AddItemToObject(params, "qq", cJSON_CreateString(qq));
    cJSON_AddItemToObject(params, "content", cJSON_CreateString(u8Content));

    cJSON_AddItemToObject(root, "params", params);

    const char* jsonStr = cJSON_PrintUnformatted(root);

    wsFrameSendToAll(jsonStr, strlen(jsonStr), frameType_text);

    cJSON_Delete(root);
    free((void*)u8Content);
    free((void*)jsonStr);

    return 0;    // 返回0下个插件继续处理该事件，返回1拦截此事件不让其他插件执行
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    
    if(loadQQLightAPI() != 0) {
        MessageBox(NULL, "The message.dll load failed", "error", MB_OK);
        return FALSE;
    }
    
    /* Returns TRUE on success, FALSE on failure */
    return TRUE;
}
