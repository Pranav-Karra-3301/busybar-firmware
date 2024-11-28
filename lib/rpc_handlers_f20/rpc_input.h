#pragma once

#include <rpc_common/rpc.h>

void* rpc_system_input_alloc(RpcSession* session);

void rpc_system_input_free(void* context);
