#pragma once

struct lua_State;

namespace ludork::runtime::node_graph_detail {

struct StackRestore {
    lua_State* state;
    int top;

    ~StackRestore();
};

}  // namespace ludork::runtime::node_graph_detail
