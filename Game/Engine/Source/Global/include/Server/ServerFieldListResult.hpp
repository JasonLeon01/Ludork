#pragma once
#include <Server/ServerFieldEntry.hpp>
#include <Server/ServerResult.hpp>
#include <optional>
#include <vector>

BIND_CLASS()
class LUDORK_GLOBAL_API ServerFieldListResult : public ServerResult {
public:
    LUDORK_CAST_DERIVED(ServerFieldListResult, ServerResult)
    ServerFieldListResult(ServerErrorCode code, std::string message,
                          std::vector<ServerFieldEntry> entries = {},
                          std::optional<std::string> nextCursor = {});
    BIND_METHOD(property = "entries")
    std::vector<ServerFieldEntry> getEntries() const;
    BIND_METHOD(property = "nextCursor")
    std::optional<std::string> getNextCursor() const;

private:
    std::vector<ServerFieldEntry> entries_;
    std::optional<std::string> nextCursor_;
};
