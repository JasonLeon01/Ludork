#include <Server/ServerFieldListResult.hpp>
#include <utility>
ServerFieldListResult::ServerFieldListResult(
    ServerErrorCode code, std::string message,
    std::vector<ServerFieldEntry> entries,
    std::optional<std::string> nextCursor)
    : ServerResult(code, std::move(message)),
      entries_(std::move(entries)),
      nextCursor_(std::move(nextCursor)) {}
std::vector<ServerFieldEntry> ServerFieldListResult::getEntries() const {
    return entries_;
}
std::optional<std::string> ServerFieldListResult::getNextCursor() const {
    return nextCursor_;
}
