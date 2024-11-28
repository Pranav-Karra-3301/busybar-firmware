#include "rpc_dummy.h"

#include <furi.h>

#include <rpc_common/rpc_i.h>

typedef struct {
    uint8_t dummy;
} RpcSystemDummy;

void* rpc_system_dummy_alloc(RpcSession* session) {
    UNUSED(session);
    RpcSystemDummy* instance = malloc(sizeof(RpcSystemDummy));
    return instance;
}

void rpc_system_dummy_free(void* context) {
    UNUSED(context);
    free(context);
}
