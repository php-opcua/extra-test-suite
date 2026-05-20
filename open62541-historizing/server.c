/*
 * php-opcua/extra-test-suite — open62541-historizing
 *
 * Minimal open62541 server with HistoryRead + HistoryUpdate enabled on a
 * single Double variable, used by php-opcua/opcua-client integration tests
 * for the HistoryUpdate service.
 *
 * Exposes:
 *   - ns=2;s=Historizing.Counter   (Double, historizing, read+write+historyread+historywrite)
 *
 * No security, anonymous access only.
 */

#include <open62541/plugin/historydata/history_data_backend_memory.h>
#include <open62541/plugin/historydata/history_data_gathering_default.h>
#include <open62541/plugin/historydata/history_database_default.h>
#include <open62541/plugin/historydatabase.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <signal.h>
#include <stdio.h>

static volatile UA_Boolean running = true;

static void stopHandler(int sig) {
    (void) sig;
    running = false;
}

static UA_StatusCode addHistorizingCounter(UA_Server *server,
                                           UA_HistoryDataGathering gathering) {
    UA_UInt16 ns = UA_Server_addNamespace(server, "urn:php-opcua:historizing");

    UA_VariableAttributes attr = UA_VariableAttributes_default;

    UA_Double initial = 0.0;
    UA_Variant_setScalar(&attr.value, &initial, &UA_TYPES[UA_TYPES_DOUBLE]);

    attr.displayName = UA_LOCALIZEDTEXT("en-US", "Counter");
    attr.description = UA_LOCALIZEDTEXT("en-US", "Historizing double counter for HistoryUpdate tests");
    attr.dataType    = UA_TYPES[UA_TYPES_DOUBLE].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ
                     | UA_ACCESSLEVELMASK_WRITE
                     | UA_ACCESSLEVELMASK_HISTORYREAD
                     | UA_ACCESSLEVELMASK_HISTORYWRITE;
    attr.userAccessLevel = attr.accessLevel;
    attr.historizing = true;

    UA_NodeId counterId      = UA_NODEID_STRING(ns, "Historizing.Counter");
    UA_QualifiedName name    = UA_QUALIFIEDNAME(ns, "Counter");
    UA_NodeId parentId       = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentRef      = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_NodeId typeDef        = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE);

    UA_StatusCode rc = UA_Server_addVariableNode(
        server, counterId, parentId, parentRef, name, typeDef, attr,
        NULL, NULL);
    if (rc != UA_STATUSCODE_GOOD) {
        return rc;
    }

    UA_HistorizingNodeIdSettings setting;
    setting.historizingBackend         = UA_HistoryDataBackend_Memory(3, 1024);
    setting.maxHistoryDataResponseSize = 1024;
    setting.historizingUpdateStrategy  = UA_HISTORIZINGUPDATESTRATEGY_USER;

    return gathering.registerNodeId(server, gathering.context, &counterId, setting);
}

int main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);

    UA_HistoryDataGathering gathering = UA_HistoryDataGathering_Default(1);
    config->historyDatabase = UA_HistoryDatabase_default(gathering);

    UA_StatusCode rc = addHistorizingCounter(server, gathering);
    if (rc != UA_STATUSCODE_GOOD) {
        fprintf(stderr, "[historizing] failed to add Counter node: %s\n",
                UA_StatusCode_name(rc));
        UA_Server_delete(server);
        return 1;
    }

    fprintf(stderr, "[historizing] open62541 historizing server starting on :4840\n");
    fprintf(stderr, "[historizing] exposed node: ns=<urn:php-opcua:historizing>;s=Historizing.Counter (Double, history enabled)\n");

    rc = UA_Server_run(server, &running);
    UA_Server_delete(server);
    return rc == UA_STATUSCODE_GOOD ? 0 : 1;
}
